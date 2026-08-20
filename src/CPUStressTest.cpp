#include "../include/CPUStressTest.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <ctime>

namespace {

// High-resolution CPU time consumed by the *calling* thread, in nanoseconds.
// Used to derive genuine per-core utilization for the GUI bars.
uint64_t threadCpuTimeNs() {
#ifdef __linux__
    struct timespec ts {};
    if (::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) {
        return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL
             + static_cast<uint64_t>(ts.tv_nsec);
    }
#endif
    return 0;
}

} // namespace

float CPUStressTest::estimateLoadFromHashRate() {
    int64_t currentTime = timeManager.getElapsedMilliseconds();
    int64_t duration = currentTime - lastCheck;

    if (duration == 0) return 0.5f;
    uint64_t currentOps = hashOps.load(std::memory_order_relaxed);
    float opsRate = static_cast<float>(currentOps - lastOps) / duration;

    lastOps = currentOps;
    lastCheck = currentTime;

    return std::min(1.0f, std::max(0.0f, opsRate / 1000.0f));
}

float CPUStressTest::getCurrentSystemLoad() {
#ifdef __linux__
    FILE* statFile = std::fopen("/proc/stat", "r");
    if (!statFile) return estimateLoadFromHashRate();

    char line[512] = {0};
    std::fgets(line, sizeof(line), statFile);
    std::fclose(statFile);

    unsigned long long user = 0, nice = 0, system = 0, idle = 0;
    unsigned long long iowait = 0, irq = 0, softirq = 0, steal = 0;
    if (std::sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                    &user, &nice, &system, &idle,
                    &iowait, &irq, &softirq, &steal) < 4) {
        return estimateLoadFromHashRate();
    }

    unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
    unsigned long long idleAll = idle + iowait;

    if (prevCpuTotal == 0) { // first sample: establish a baseline
        prevCpuTotal = total;
        prevCpuIdle = idleAll;
        return 0.0f;
    }

    unsigned long long totalDelta = total - prevCpuTotal;
    unsigned long long idleDelta = idleAll - prevCpuIdle;
    prevCpuTotal = total;
    prevCpuIdle = idleAll;

    if (totalDelta == 0) return 0.0f;
    float load = 1.0f - static_cast<float>(idleDelta) / static_cast<float>(totalDelta);
    return std::min(1.0f, std::max(0.0f, load));
#else
    return estimateLoadFromHashRate();
#endif
}

void CPUStressTest::cpuHashStressTest(int threadId) {
    constexpr int BATCH_SIZE = 4500;
    constexpr int CHUNK_SIZE = 1;

    auto computeIntensiveHash = [](uint64_t base, uint64_t exponent, uint64_t mod) -> uint64_t {
        uint64_t result = 1;
        uint64_t nestedFactor = 1;

        for (uint64_t i = 0; i < exponent; ++i) {
            result = (result * base) % mod;
            nestedFactor = (nestedFactor * result) % mod;

            for (uint64_t j = 0; j < exponent; ++j) {
                nestedFactor += i + j;
                result *= nestedFactor;
            }

            if (i % 10 == 0) {
                result = (result + nestedFactor) % mod;
            }
        }
        return result;
    };

    uint64_t localHashOps = 0;

    auto stillRunning = [&]() {
        return running.load(std::memory_order_relaxed)
            && threadRunning[threadId].load(std::memory_order_relaxed)
            && timeManager.shouldContinue(TEST_DURATION);
    };

    while (stillRunning()) {
        volatile uint64_t hashValue = 0;

        for (int i = 0; i < BATCH_SIZE && stillRunning(); ++i) {
            volatile uint64_t randomBase = threadId * 123456789 + i * 987654321;
            volatile uint64_t randomExponent = ((i % 2000) + 500) * (threadId % 10 + 1);
            volatile uint64_t randomModulus = 1e9 + 12347;

#ifdef __linux__
            threadCpuNs[threadId].store(threadCpuTimeNs(), std::memory_order_relaxed);
#endif
            hashValue = computeIntensiveHash(randomBase, randomExponent, randomModulus);

            if (hashValue % 1024 == 0) {
                hashValue = (hashValue + threadId) * (randomBase % 7);
            }

            ++localHashOps;

            if (localHashOps % CHUNK_SIZE == 0) {
                hashOps.fetch_add(CHUNK_SIZE, std::memory_order_relaxed);
                threadOps[threadId].fetch_add(CHUNK_SIZE, std::memory_order_relaxed);
                localHashOps = 0;
            }
        }

        if (localHashOps > 0) {
            hashOps.fetch_add(localHashOps, std::memory_order_relaxed);
            threadOps[threadId].fetch_add(localHashOps, std::memory_order_relaxed);
            localHashOps = 0;
        }
    }
}

void CPUStressTest::initialize() {
    numCores = std::thread::hardware_concurrency();
    assert(numCores > 0 && "Failed to detect CPU cores");

    hashOps.store(0);
    running.store(true);

    threadOps = std::vector<std::atomic<uint64_t>>(numCores);
    for (auto& c : threadOps) c.store(0, std::memory_order_relaxed);

    threadRunning = std::vector<std::atomic<bool>>(numCores);
    for (auto& f : threadRunning) f.store(true, std::memory_order_relaxed);

    threadCpuNs = std::vector<std::atomic<uint64_t>>(numCores);
    for (auto& c : threadCpuNs) c.store(0, std::memory_order_relaxed);

    lastThreadCpuNs.assign(numCores, 0);
    lastThreadLoads.assign(numCores, 0.0f);
    lastThreadPollMs = 0;

    prevCpuTotal = 0;
    prevCpuIdle  = 0;
    lastOps      = 0;
    lastCheck    = 0;
}

void CPUStressTest::start() {
    if (!cpuThreads.empty()) {
        return; // already running a session
    }

    running.store(true, std::memory_order_relaxed); // re-arm for a fresh session

    // Restored all-core behaviour: one stress worker per detected core.
    for (int i = 0; i < numCores; ++i) {
        threadRunning[i].store(true, std::memory_order_relaxed);
        cpuThreads.emplace_back(&CPUStressTest::cpuHashStressTest, this, i);
    }
}

void CPUStressTest::stop() {
    running = false;
}

void CPUStressTest::waitForCompletion() {
    std::lock_guard<std::mutex> lock(threadPoolMutex);
    for (auto& thread : cpuThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    cpuThreads.clear();
}

std::vector<float> CPUStressTest::getThreadLoads() {
    std::lock_guard<std::mutex> lock(threadPoolMutex);
    size_t activeCount = cpuThreads.size();

    int64_t now = timeManager.getElapsedMilliseconds();
    int64_t duration = now - lastThreadPollMs;
    lastThreadPollMs = now;

    std::vector<float> loads(activeCount, 0.0f);
    if (lastThreadCpuNs.size() < activeCount) lastThreadCpuNs.resize(activeCount, 0);
    if (lastThreadLoads.size() < activeCount) lastThreadLoads.resize(activeCount, 0.0f);
    if (duration <= 0) return loads;

    for (size_t i = 0; i < activeCount; ++i) {
#ifdef __linux__
        // Genuine per-core utilization from per-thread CPU time deltas.
        uint64_t current = threadCpuNs[i].load(std::memory_order_relaxed);
        uint64_t delta = current - lastThreadCpuNs[i];
        lastThreadCpuNs[i] = current;

        if (delta > 0) {
            float util = static_cast<float>(delta) / (static_cast<float>(duration) * 1e6f);
            lastThreadLoads[i] = std::min(1.0f, std::max(0.0f, util));
        }
        // delta == 0 means the worker is mid-hash-op, so hold the last value.
        loads[i] = lastThreadLoads[i];
#else
        // Portable fallback: hash-op throughput estimate.
        uint64_t current = threadOps[i].load(std::memory_order_relaxed);
        uint64_t delta = current - lastThreadOps[i];
        lastThreadOps[i] = current;

        float rate = static_cast<float>(delta) / duration;
        loads[i] = std::min(1.0f, std::max(0.0f, rate / 1000.0f));
#endif
    }
    return loads;
}
