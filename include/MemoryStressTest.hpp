#pragma once

#include <mutex>
#include <thread>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "TimeManager.hpp"

class MemoryStressTest {
private:
    std::vector<std::unique_ptr<std::vector<int>>> memoryBlocks;

    static constexpr int    MULTIPLIER = 2;                     // Memory multiplier for stress test (resulting in a 2 GB Max Allocation)
    int                     testDurationSeconds = 30;           // seconds
    static constexpr size_t TARGET_MEMORY = 1024 * 1024 * 1024; // 1 GB

    // Memory bandwidth measurement constants
    static constexpr size_t BANDWIDTH_TEST_SIZE = 64 * 1024 * 1024; // 64MB test buffer
    static constexpr int    BANDWIDTH_ITERATIONS = 5;               // Number of iterations for averaging

    // Shared atomic variables to track memory metrics
    std::atomic<bool>     running{true};        // Flag to indicate if the test is running
    std::atomic<size_t>   memoryAllocated{0};   // Memory allocated in bytes
    std::atomic<double>   memoryBandwidth{0.0}; // Memory bandwidth in MB/s
    std::atomic<bool>     allocationFailed{false}; // OOM / allocation failure flag

    std::mutex consoleMutex;

    // Reference to global time manager
    TimeManager& timeManager;

    // Memory bandwidth measurement variables
    std::unique_ptr<uint8_t[]> bandwidthTestBuffer;
    std::atomic<bool> bandwidthTestRunning{false};

    // Memory stress test methods
    void memoryStressTest();

    // Memory bandwidth measurement methods
    double performSequentialRead(uint8_t* buffer, size_t size);
    double performSequentialWrite(uint8_t* buffer, size_t size);
    double performRandomAccess(uint8_t* buffer, size_t size);
    void   continuousBandwidthTest();

public:
    MemoryStressTest() : timeManager(TimeManager::getInstance()) {}
    ~MemoryStressTest() = default;

    // Public interface
    void initialize();
    void start();
    void stop();
    void waitForCompletion();

    void setTestDuration(int seconds) { testDurationSeconds = seconds; }
    int getTestDuration() const { return testDurationSeconds; }

    // Memory bandwidth measurement methods
    void measureMemoryBandwidth();

    // Getters for monitoring
    size_t getMemoryAllocated() const { return memoryAllocated.load(std::memory_order_relaxed); }
    double getMemoryBandwidth() const { return memoryBandwidth.load(std::memory_order_relaxed); }
    size_t getTargetMemory() const { return TARGET_MEMORY * MULTIPLIER; }
    size_t getBandwidthTestSize() const { return BANDWIDTH_TEST_SIZE; }
    bool isRunning() const { return running.load(); }
    bool hasAllocationFailed() const { return allocationFailed.load(std::memory_order_relaxed); }

    // Disable copy constructor and assignment
    MemoryStressTest(const MemoryStressTest&) = delete;
    MemoryStressTest& operator=(const MemoryStressTest&) = delete;

private:
    std::thread memThread;
    std::thread bandwidthThread;
};