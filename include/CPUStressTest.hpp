#pragma once

#include <mutex>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdint>

#include "TimeManager.hpp"

class CPUStressTest {
private:
    static constexpr int TEST_DURATION = 30; // seconds

    // Shared atomic variables to track CPU metrics
    std::atomic<uint64_t> hashOps{0};        // Total hashing operations
    std::atomic<bool>     running{true};     // Global stop flag for the whole test

    int numCores = 0;

    // Guards cpuThreads while getThreadLoads() samples thread load counters;
    // cpuThreads is only mutated in start() and waitForCompletion().
    std::mutex threadPoolMutex;
    std::vector<std::thread> cpuThreads;
    std::vector<std::atomic<bool>> threadRunning; // per-thread stop flag, sized to numCores

    // Per-thread op counters and CPU-time snapshots, sized once in
    // initialize() to numCores and indexed directly by threadId.
    std::vector<std::atomic<uint64_t>> threadOps;   // hash-ops completed (rate fallback)
    std::vector<std::atomic<uint64_t>> threadCpuNs; // CLOCK_THREAD_CPUTIME_ID snapshots
    std::vector<uint64_t> lastThreadOps;            // poller-side, for hash-rate fallback
    std::vector<uint64_t> lastThreadCpuNs;          // poller-side, for CPU-time delta
    std::vector<float>    lastThreadLoads;          // held between polls (mid-op smoothing)
    int64_t lastThreadPollMs = 0;

    // System load derived from /proc/stat sampling (Linux), reset in initialize().
    uint64_t prevCpuTotal = 0;
    uint64_t prevCpuIdle  = 0;

    // Hash-rate fallback accumulator (non-Linux), reset in initialize().
    uint64_t lastOps   = 0;
    int64_t  lastCheck = 0;

    TimeManager& timeManager;

    float getCurrentSystemLoad();
    float estimateLoadFromHashRate();
    void  cpuHashStressTest(int threadId);

public:
    CPUStressTest() : timeManager(TimeManager::getInstance()) {}
    ~CPUStressTest() = default;

    void initialize();
    void start();
    void stop();
    void waitForCompletion();

    uint64_t getHashOperations() const { return hashOps.load(std::memory_order_relaxed); }
    int getCoreCount() const { return numCores; }
    int getActiveThreadCount() const { return static_cast<int>(cpuThreads.size()); }
    bool isRunning() const { return running.load(); }

    // GUI polling interface
    std::vector<float> getThreadLoads(); // one entry per *active* thread, 0..1

    CPUStressTest(const CPUStressTest&) = delete;
    CPUStressTest& operator=(const CPUStressTest&) = delete;
};