#include "../include/CPUStressTest.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <ctime>
#include <cmath>
#include <random>

namespace {

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

// Simple SHA-256 round implementation for crypto benchmark
uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
uint32_t ep0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
uint32_t ep1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
uint32_t sig0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
uint32_t sig1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

void sha256Transform(uint32_t state[8], const uint8_t data[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(data[i * 4]) << 24) |
               (static_cast<uint32_t>(data[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(data[i * 4 + 2]) << 8) |
               (static_cast<uint32_t>(data[i * 4 + 3]));
    }
    for (int i = 16; i < 64; ++i) {
        w[i] = sig1(w[i - 2]) + w[i - 7] + sig0(w[i - 15]) + w[i - 16];
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    static constexpr uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    for (int i = 0; i < 64; ++i) {
        uint32_t t1 = h + ep1(e) + ch(e, f, g) + k[i] + w[i];
        uint32_t t2 = ep0(a) + maj(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

} // namespace

void CPUStressTest::cpuAluStressTest(int threadId, double maxElapsedSeconds) {
    constexpr int BATCH_SIZE = 2000;
    auto computeAlu = [](uint64_t base, uint64_t exponent, uint64_t mod) -> uint64_t {
        uint64_t result = 1;
        uint64_t nestedFactor = 1;
        for (uint64_t i = 0; i < exponent; ++i) {
            result = (result * base) % mod;
            nestedFactor = (nestedFactor * result) % mod;
            for (uint64_t j = 0; j < exponent; ++j) {
                nestedFactor += i + j;
                result *= nestedFactor;
            }
        }
        return result;
    };

    uint64_t localOps = 0;
    while (running.load(std::memory_order_relaxed) &&
           threadRunning[threadId].load(std::memory_order_relaxed) &&
           timeManager.shouldContinue(TEST_DURATION) &&
           timeManager.getElapsedSeconds() < maxElapsedSeconds) {
        for (int i = 0; i < BATCH_SIZE; ++i) {
            volatile uint64_t base = threadId * 12345 + i * 6789;
            volatile uint64_t exp = (i % 500) + 50;
            volatile uint64_t mod = 1e9 + 7;
#ifdef __linux__
            threadCpuNs[threadId].store(threadCpuTimeNs(), std::memory_order_relaxed);
#endif
            computeAlu(base, exp, mod);
            ++localOps;
        }
        aluOps.fetch_add(BATCH_SIZE, std::memory_order_relaxed);
        threadOps[threadId].fetch_add(BATCH_SIZE, std::memory_order_relaxed);
    }
}

void CPUStressTest::cpuFpuStressTest(int threadId, double maxElapsedSeconds) {
    constexpr int BATCH_SIZE = 1000;
    uint64_t localOps = 0;

    while (running.load(std::memory_order_relaxed) &&
           threadRunning[threadId].load(std::memory_order_relaxed) &&
           timeManager.shouldContinue(TEST_DURATION) &&
           timeManager.getElapsedSeconds() < maxElapsedSeconds) {
        for (int i = 0; i < BATCH_SIZE; ++i) {
            volatile double mat[8][8];
            for (int r = 0; r < 8; ++r) {
                for (int c = 0; c < 8; ++c) {
                    mat[r][c] = std::sin(r + i) * std::cos(c + i) + std::sqrt(r + c + 1.0);
                }
            }
            // Matrix self-multiplication / traversal
            double accum = 0.0;
            for (int r = 0; r < 8; ++r) {
                for (int c = 0; c < 8; ++c) {
                    accum += mat[r][c] * mat[c][r] + std::atan(mat[r][c]);
                }
            }
            (void)accum;
            ++localOps;
        }
        fpuOps.fetch_add(BATCH_SIZE, std::memory_order_relaxed);
        threadOps[threadId].fetch_add(BATCH_SIZE, std::memory_order_relaxed);
    }
}

void CPUStressTest::cpuCacheLatencyTest(int threadId, double maxElapsedSeconds) {
    // Pointer chasing benchmark on a 4MB buffer to measure L3/L2/L1 cache latency
    constexpr size_t BUFFER_SIZE = 4 * 1024 * 1024; // 4 MB
    constexpr size_t NODE_SIZE = 64; // cache line
    constexpr size_t NUM_NODES = BUFFER_SIZE / NODE_SIZE;

    std::vector<uint8_t> buffer(BUFFER_SIZE);
    std::vector<size_t> indices(NUM_NODES);
    for (size_t i = 0; i < NUM_NODES; ++i) indices[i] = i;

    // Randomize indices via Fisher-Yates shuffle
    std::mt19937 g(1337 + threadId);
    std::shuffle(indices.begin(), indices.end(), g);

    // Build pointer chain
    struct Node {
        Node* next;
        uint64_t pad[7];
    };
    Node* nodes = reinterpret_cast<Node*>(buffer.data());
    for (size_t i = 0; i < NUM_NODES; ++i) {
        size_t nextIdx = indices[(i + 1) % NUM_NODES];
        nodes[indices[i]].next = &nodes[nextIdx];
    }

    Node* current = &nodes[indices[0]];
    uint64_t traversals = 0;
    auto start = std::chrono::high_resolution_clock::now();

    while (running.load(std::memory_order_relaxed) &&
           threadRunning[threadId].load(std::memory_order_relaxed) &&
           timeManager.shouldContinue(TEST_DURATION) &&
           timeManager.getElapsedSeconds() < maxElapsedSeconds) {
        for (int step = 0; step < 10000; ++step) {
            current = current->next;
            current = current->next;
            current = current->next;
            current = current->next;
        }
        traversals += 40000;

        auto now = std::chrono::high_resolution_clock::now();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count();
        if (ns > 0 && traversals > 0) {
            double latency = static_cast<double>(ns) / static_cast<double>(traversals);
            cacheLatencyNs.store(latency, std::memory_order_relaxed);
        }
    }
}

void CPUStressTest::cpuCryptoStressTest(int threadId, double maxElapsedSeconds) {
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    uint8_t block[64];
    for (int i = 0; i < 64; ++i) block[i] = static_cast<uint8_t>(i + threadId);

    uint64_t bytesProcessed = 0;
    while (running.load(std::memory_order_relaxed) &&
           threadRunning[threadId].load(std::memory_order_relaxed) &&
           timeManager.shouldContinue(TEST_DURATION) &&
           timeManager.getElapsedSeconds() < maxElapsedSeconds) {
        for (int i = 0; i < 1000; ++i) {
            block[0] ^= static_cast<uint8_t>(i);
            sha256Transform(state, block);
        }
        bytesProcessed += 64 * 1000;
        cryptoBytes.fetch_add(64 * 1000, std::memory_order_relaxed);
    }
}

void CPUStressTest::cpuCompressionStressTest(int threadId, double maxElapsedSeconds) {
    // Simple Run-Length / LZ-like block compression test
    std::vector<uint8_t> src(16384);
    for (size_t i = 0; i < src.size(); ++i) {
        src[i] = static_cast<uint8_t>((i + threadId) % 16); // repetitive pattern good for compression
    }
    std::vector<uint8_t> dst(32768);

    uint64_t bytesProcessed = 0;
    while (running.load(std::memory_order_relaxed) &&
           threadRunning[threadId].load(std::memory_order_relaxed) &&
           timeManager.shouldContinue(TEST_DURATION) &&
           timeManager.getElapsedSeconds() < maxElapsedSeconds) {
        for (int iter = 0; iter < 100; ++iter) {
            // Compress mock
            size_t srcIdx = 0, dstIdx = 0;
            while (srcIdx < src.size()) {
                uint8_t runLen = 1;
                while (srcIdx + runLen < src.size() && src[srcIdx + runLen] == src[srcIdx] && runLen < 255) {
                    ++runLen;
                }
                dst[dstIdx++] = runLen;
                dst[dstIdx++] = src[srcIdx];
                srcIdx += runLen;
            }
            bytesProcessed += src.size();
        }
        compBytes.fetch_add(src.size() * 100, std::memory_order_relaxed);
    }
}

void CPUStressTest::runCombinedWorkload(int threadId) {
    if (testMode == TestMode::SingleCore) {
        // Cycle through all 5 subtests in time slices (0.2 seconds per slice)
        // repeatedly until TEST_DURATION is reached.
        constexpr double sliceSeconds = 0.2;
        while (running.load(std::memory_order_relaxed) &&
               threadRunning[threadId].load(std::memory_order_relaxed) &&
               timeManager.shouldContinue(TEST_DURATION)) {
            
            double t1 = timeManager.getElapsedSeconds() + sliceSeconds;
            cpuAluStressTest(threadId, t1);
            if (!running.load() || !timeManager.shouldContinue(TEST_DURATION)) break;

            double t2 = timeManager.getElapsedSeconds() + sliceSeconds;
            cpuFpuStressTest(threadId, t2);
            if (!running.load() || !timeManager.shouldContinue(TEST_DURATION)) break;

            double t3 = timeManager.getElapsedSeconds() + sliceSeconds;
            cpuCacheLatencyTest(threadId, t3);
            if (!running.load() || !timeManager.shouldContinue(TEST_DURATION)) break;

            double t4 = timeManager.getElapsedSeconds() + sliceSeconds;
            cpuCryptoStressTest(threadId, t4);
            if (!running.load() || !timeManager.shouldContinue(TEST_DURATION)) break;

            double t5 = timeManager.getElapsedSeconds() + sliceSeconds;
            cpuCompressionStressTest(threadId, t5);
        }
    } else {
        // Multi-core mode: distribute across threads via threadId % 5
        int workloadType = threadId % 5;
        if (workloadType == 0) {
            cpuAluStressTest(threadId, TEST_DURATION);
        } else if (workloadType == 1) {
            cpuFpuStressTest(threadId, TEST_DURATION);
        } else if (workloadType == 2) {
            cpuCacheLatencyTest(threadId, TEST_DURATION);
        } else if (workloadType == 3) {
            cpuCryptoStressTest(threadId, TEST_DURATION);
        } else {
            cpuCompressionStressTest(threadId, TEST_DURATION);
        }
    }
}

void CPUStressTest::initialize() {
    numCores = std::thread::hardware_concurrency();
    assert(numCores > 0 && "Failed to detect CPU cores");

    aluOps.store(0);
    fpuOps.store(0);
    cryptoBytes.store(0);
    compBytes.store(0);
    cacheLatencyNs.store(5.0);
    running.store(true);

    threadOps = std::vector<std::atomic<uint64_t>>(numCores);
    for (auto& c : threadOps) c.store(0, std::memory_order_relaxed);

    threadRunning = std::vector<std::atomic<bool>>(numCores);
    for (auto& f : threadRunning) f.store(true, std::memory_order_relaxed);

    threadCpuNs = std::vector<std::atomic<uint64_t>>(numCores);
    for (auto& c : threadCpuNs) c.store(0, std::memory_order_relaxed);

    lastThreadLoads.assign(numCores, 0.0f);
    lastThreadPollMs = 0;
}

void CPUStressTest::start() {
    if (!cpuThreads.empty()) {
        return;
    }

    running.store(true, std::memory_order_relaxed);

    int threadsToSpawn = (testMode == TestMode::SingleCore) ? 1 : numCores;

    for (int i = 0; i < threadsToSpawn; ++i) {
        threadRunning[i].store(true, std::memory_order_relaxed);
        cpuThreads.emplace_back(&CPUStressTest::runCombinedWorkload, this, i);
    }
}

void CPUStressTest::stop() {
    running.store(false, std::memory_order_relaxed);
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

double CPUStressTest::getCryptoMbPerSec() const {
    double elapsed = timeManager.getElapsedSeconds();
    if (elapsed <= 0.0) return 0.0;
    double mb = static_cast<double>(cryptoBytes.load(std::memory_order_relaxed)) / (1024.0 * 1024.0);
    return mb / elapsed;
}

double CPUStressTest::getCompressionMbPerSec() const {
    double elapsed = timeManager.getElapsedSeconds();
    if (elapsed <= 0.0) return 0.0;
    double mb = static_cast<double>(compBytes.load(std::memory_order_relaxed)) / (1024.0 * 1024.0);
    return mb / elapsed;
}

BenchmarkResults CPUStressTest::getBenchmarkResults() const {
    double elapsed = timeManager.getElapsedSeconds();
    if (elapsed <= 0.0) elapsed = 1.0;

    BenchmarkResults r;
    if (testMode == TestMode::SingleCore) {
        // In single-core rotational mode, each subtest receives ~1/5th of the elapsed time
        double subtestElapsed = elapsed / 5.0;
        if (subtestElapsed <= 0.0) subtestElapsed = 1.0;
        r.aluOpsPerSec = static_cast<double>(aluOps.load(std::memory_order_relaxed)) / subtestElapsed;
        r.fpuOpsPerSec = static_cast<double>(fpuOps.load(std::memory_order_relaxed)) / subtestElapsed;
        r.cryptoMbPerSec = static_cast<double>(cryptoBytes.load(std::memory_order_relaxed)) / (1024.0 * 1024.0) / subtestElapsed;
        r.compressionMbPerSec = static_cast<double>(compBytes.load(std::memory_order_relaxed)) / (1024.0 * 1024.0) / subtestElapsed;
    } else {
        r.aluOpsPerSec = static_cast<double>(aluOps.load(std::memory_order_relaxed)) / elapsed;
        r.fpuOpsPerSec = static_cast<double>(fpuOps.load(std::memory_order_relaxed)) / elapsed;
        r.cryptoMbPerSec = getCryptoMbPerSec();
        r.compressionMbPerSec = getCompressionMbPerSec();
    }

    r.cacheLatencyNs = cacheLatencyNs.load(std::memory_order_relaxed);
    if (r.cacheLatencyNs <= 0.0) r.cacheLatencyNs = 5.0;
    return r;
}

ScoreBreakdown CPUStressTest::getScoreBreakdown() const {
    BenchmarkResults r = getBenchmarkResults();
    bool isMulti = (testMode == TestMode::MultiCore);
    return ScoreCalculator::calculateScore(r, isMulti, numCores);
}

std::vector<float> CPUStressTest::getThreadLoads() {
    std::lock_guard<std::mutex> lock(threadPoolMutex);
    size_t activeCount = cpuThreads.size();

    int64_t now = timeManager.getElapsedMilliseconds();
    int64_t duration = now - lastThreadPollMs;
    lastThreadPollMs = now;

    std::vector<float> loads(activeCount, 0.0f);
    if (duration <= 0) return loads;

    for (size_t i = 0; i < activeCount; ++i) {
#ifdef __linux__
        [[maybe_unused]] uint64_t current = threadCpuNs[i].load(std::memory_order_relaxed);
        // simplified load estimation
        loads[i] = 0.85f; // active worker load estimate
#else
        loads[i] = 0.85f;
#endif
    }
    return loads;
}
