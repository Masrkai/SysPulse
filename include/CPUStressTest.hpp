#pragma once

#include <mutex>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdint>

#include "TimeManager.hpp"
#include "ScoreCalculator.hpp"

class CPUStressTest {
public:
    enum class TestMode {
        SingleCore,
        MultiCore
    };

private:
    static constexpr int TEST_DURATION = 30; // seconds

    // Shared atomic variables to track CPU metrics across sub-tests
    std::atomic<uint64_t> aluOps{0};        // ALU operations
    std::atomic<uint64_t> fpuOps{0};        // FPU / SIMD operations
    std::atomic<uint64_t> cryptoBytes{0};   // Crypto bytes processed
    std::atomic<uint64_t> compBytes{0};     // Compression bytes processed
    std::atomic<double>   cacheLatencyNs{0.0}; // Measured cache latency in ns
    std::atomic<bool>     running{true};     // Global stop flag for the whole test

    int numCores = 0;
    TestMode testMode = TestMode::MultiCore;
    int testDurationSeconds = 30;

    std::mutex threadPoolMutex;
    std::vector<std::thread> cpuThreads;
    std::vector<std::atomic<bool>> threadRunning;

    // Per-thread counters and CPU-time snapshots
    std::vector<std::atomic<uint64_t>> threadOps;
    std::vector<std::atomic<uint64_t>> threadCpuNs;
    std::vector<uint64_t> lastThreadCpuNs;
    std::vector<float>    lastThreadLoads;
    int64_t lastThreadPollMs = 0;

    TimeManager& timeManager;

    void cpuAluStressTest(int threadId, double maxElapsedSeconds = TEST_DURATION);
    void cpuFpuStressTest(int threadId, double maxElapsedSeconds = TEST_DURATION);
    void cpuCacheLatencyTest(int threadId, double maxElapsedSeconds = TEST_DURATION);
    void cpuCryptoStressTest(int threadId, double maxElapsedSeconds = TEST_DURATION);
    void cpuCompressionStressTest(int threadId, double maxElapsedSeconds = TEST_DURATION);
    void runCombinedWorkload(int threadId);

public:
    CPUStressTest() : timeManager(TimeManager::getInstance()) {}
    ~CPUStressTest() = default;

    void initialize();
    void start();
    void stop();
    void waitForCompletion();

    void setTestMode(TestMode mode) { testMode = mode; }
    TestMode getTestMode() const { return testMode; }

    void setTestDuration(int seconds) { testDurationSeconds = seconds; }
    int getTestDuration() const { return testDurationSeconds; }

    uint64_t getHashOperations() const {
        return aluOps.load(std::memory_order_relaxed) +
               fpuOps.load(std::memory_order_relaxed) +
               (cryptoBytes.load(std::memory_order_relaxed) / 64) +
               (compBytes.load(std::memory_order_relaxed) / 1000);
    }
    uint64_t getAluOps() const { return aluOps.load(std::memory_order_relaxed); }
    uint64_t getFpuOps() const { return fpuOps.load(std::memory_order_relaxed); }
    double getCacheLatencyNs() const { return cacheLatencyNs.load(std::memory_order_relaxed); }
    double getCryptoMbPerSec() const;
    double getCompressionMbPerSec() const;

    BenchmarkResults getBenchmarkResults() const;
    ScoreBreakdown getScoreBreakdown() const;

    int getCoreCount() const { return numCores; }
    int getActiveThreadCount() const { return static_cast<int>(cpuThreads.size()); }
    bool isRunning() const { return running.load(); }

    std::vector<float> getThreadLoads();

    CPUStressTest(const CPUStressTest&) = delete;
    CPUStressTest& operator=(const CPUStressTest&) = delete;
};
