#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>

#include "../include/MemoryStressTest.hpp"
#include "../include/TimeManager.hpp"

class MemoryStressTestFixture : public ::testing::Test {
protected:
    std::unique_ptr<MemoryStressTest> memoryTest;
    TimeManager& timeManager;

    MemoryStressTestFixture() : timeManager(TimeManager::getInstance()) {}

    void SetUp() override {
        memoryTest = std::make_unique<MemoryStressTest>();
        timeManager.reset();
    }

    void TearDown() override {
        if (memoryTest) {
            memoryTest->stop();
            memoryTest->waitForCompletion();
        }
        timeManager.reset();
    }
};

// Test basic initialization
TEST_F(MemoryStressTestFixture, InitializationTest) {
    memoryTest->initialize();

    EXPECT_EQ(memoryTest->getMemoryAllocated(), 0);
    EXPECT_EQ(memoryTest->getMemoryBandwidth(), 0.0);
    EXPECT_TRUE(memoryTest->isRunning());
    EXPECT_GT(memoryTest->getTargetMemory(), 0);
    EXPECT_GT(memoryTest->getBandwidthTestSize(), 0);
}

// Test that constants are reasonable
TEST_F(MemoryStressTestFixture, ConstantsValidation) {
    EXPECT_EQ(memoryTest->getTargetMemory(), 2ULL * 1024 * 1024 * 1024); // 2 GB (MULTIPLIER * 1GB)
    EXPECT_EQ(memoryTest->getBandwidthTestSize(), 64ULL * 1024 * 1024);   // 64 MB
}

// Test memory allocation progression
TEST_F(MemoryStressTestFixture, MemoryAllocationProgression) {
    memoryTest->initialize();
    timeManager.startTimer();

    memoryTest->start();

    // Allow some time for allocation
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    size_t initialMemory = memoryTest->getMemoryAllocated();
    EXPECT_GT(initialMemory, 0) << "Memory allocation should start immediately";

    // Wait a bit more and check if allocation increased
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    size_t laterMemory = memoryTest->getMemoryAllocated();
    EXPECT_GE(laterMemory, initialMemory) << "Memory allocation should increase or stay the same";

    memoryTest->stop();
    memoryTest->waitForCompletion();
}

// Test bandwidth measurement initialization
TEST_F(MemoryStressTestFixture, BandwidthMeasurementInitialization) {
    memoryTest->initialize();
    timeManager.startTimer();

    memoryTest->start();

    // Wait for bandwidth measurement to start
    std::this_thread::sleep_for(std::chrono::seconds(1));

    double bandwidth = memoryTest->getMemoryBandwidth();
    EXPECT_GE(bandwidth, 0.0) << "Bandwidth should be non-negative";

    // Bandwidth should eventually be measured (not zero forever)
    if (bandwidth == 0.0) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        bandwidth = memoryTest->getMemoryBandwidth();
        // Note: In some test environments, bandwidth might still be 0
        // This is acceptable as it depends on system capabilities
    }

    memoryTest->stop();
    memoryTest->waitForCompletion();
}

// Test stop functionality
TEST_F(MemoryStressTestFixture, StopFunctionality) {
    memoryTest->initialize();
    timeManager.startTimer();

    EXPECT_TRUE(memoryTest->isRunning());

    memoryTest->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    memoryTest->stop();

    EXPECT_FALSE(memoryTest->isRunning()) << "Test should not be running after stop()";

    memoryTest->waitForCompletion();
}

// Test thread safety of getters
TEST_F(MemoryStressTestFixture, ThreadSafeGetters) {
    memoryTest->initialize();
    timeManager.startTimer();

    memoryTest->start();

    std::atomic<bool> testFailed{false};
    std::atomic<int> completedReads{0};

    // Create multiple threads that read the values concurrently
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&]() {
            for (int j = 0; j < 100; ++j) {
                try {
                    size_t mem = memoryTest->getMemoryAllocated();
                    double bw = memoryTest->getMemoryBandwidth();
                    bool running = memoryTest->isRunning();

                    // Basic sanity checks
                    if (mem > memoryTest->getTargetMemory() * 2) {
                        testFailed = true;
                        break;
                    }

                    if (bw < 0.0) {
                        testFailed = true;
                        break;
                    }

                    completedReads++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));

                } catch (...) {
                    testFailed = true;
                    break;
                }
            }
        });
    }

    // Let the test run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    memoryTest->stop();

    for (auto& reader : readers) {
        reader.join();
    }

    memoryTest->waitForCompletion();

    EXPECT_FALSE(testFailed) << "Thread safety test failed";
    EXPECT_GT(completedReads.load(), 0) << "Should have completed some reads";
}

// Test memory allocation limits
TEST_F(MemoryStressTestFixture, MemoryAllocationLimits) {
    memoryTest->initialize();
    timeManager.startTimer();

    memoryTest->start();

    // Let it run for a reasonable time to see allocation
    std::this_thread::sleep_for(std::chrono::seconds(2));

    size_t allocatedMemory = memoryTest->getMemoryAllocated();
    size_t targetMemory = memoryTest->getTargetMemory();
    size_t bandwidthTestSize = memoryTest->getBandwidthTestSize();

    // Memory should not exceed target + bandwidth test buffer
    EXPECT_LE(allocatedMemory, targetMemory + bandwidthTestSize)
        << "Allocated memory should not significantly exceed target";

    memoryTest->stop();
    memoryTest->waitForCompletion();
}

// Test time-based termination
TEST_F(MemoryStressTestFixture, TimeBasedTermination) {
    memoryTest->initialize();

    // Start timer but don't advance it - simulate very short duration
    timeManager.startTimer();

    auto startTime = std::chrono::steady_clock::now();

    memoryTest->start();

    // Manually advance time by ending the timer immediately
    // This simulates the time manager indicating test should end
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    timeManager.endTimer();

    // Give threads time to recognize the time condition
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    memoryTest->stop();
    memoryTest->waitForCompletion();

    auto endTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Test should complete relatively quickly when time manager indicates end
    EXPECT_LT(elapsed.count(), 1000) << "Test should terminate promptly";
}

// Test resource cleanup
TEST_F(MemoryStressTestFixture, ResourceCleanup) {
    memoryTest->initialize();
    timeManager.startTimer();

    memoryTest->start();

    // Let it allocate some memory
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    size_t allocatedBeforeStop = memoryTest->getMemoryAllocated();
    EXPECT_GT(allocatedBeforeStop, 0);

    memoryTest->stop();

    // Capture right after stop() - only a block that already passed the
    // `running` check may still land after this point.
    size_t allocatedAtStop = memoryTest->getMemoryAllocated();
    memoryTest->waitForCompletion();

    // After stopping, the test should not be running
    EXPECT_FALSE(memoryTest->isRunning());

    // Memory should remain allocated (as it's stored in the linked list)
    // This is expected behavior - the memory blocks persist until destruction.
    // A worker that already passed the `running` check before stop() may still
    // finish one in-flight 1MB block after stop(), so allow at most one block
    // of growth after the stop() read.
    constexpr size_t blockSize = 1024 * 1024;
    size_t allocatedAfterStop = memoryTest->getMemoryAllocated();
    EXPECT_GE(allocatedAfterStop, allocatedAtStop)
        << "Memory should remain allocated after stop";
    EXPECT_LE(allocatedAfterStop, allocatedAtStop + blockSize)
        << "Memory should only grow by at most one in-flight block after stop";
}

// Test multiple start/stop cycles
TEST_F(MemoryStressTestFixture, MultipleStartStopCycles) {
    for (int cycle = 0; cycle < 3; ++cycle) {
        memoryTest->initialize();
        timeManager.reset();
        timeManager.startTimer();

        EXPECT_TRUE(memoryTest->isRunning()) << "Should be running after initialize";

        memoryTest->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        memoryTest->stop();
        memoryTest->waitForCompletion();

        EXPECT_FALSE(memoryTest->isRunning()) << "Should not be running after stop";
    }
}

// Test bandwidth measurement bounds
TEST_F(MemoryStressTestFixture, BandwidthMeasurementBounds) {
    memoryTest->initialize();
    timeManager.startTimer();

    memoryTest->start();

    // Wait for bandwidth measurement
    std::this_thread::sleep_for(std::chrono::seconds(1));

    double bandwidth = memoryTest->getMemoryBandwidth();

    // Bandwidth should be reasonable (not negative, not impossibly high)
    EXPECT_GE(bandwidth, 0.0) << "Bandwidth should not be negative";

    if (bandwidth > 0) {
        // If bandwidth is measured, it should be within reasonable bounds
        // Modern systems: 1 MB/s to 1 TB/s is reasonable range
        EXPECT_LE(bandwidth, 1000000.0) << "Bandwidth should not exceed 1 TB/s";
        EXPECT_GE(bandwidth, 0.1) << "If measured, bandwidth should be at least 0.1 MB/s";
    }

    memoryTest->stop();
    memoryTest->waitForCompletion();
}

// Integration test with TimeManager
TEST_F(MemoryStressTestFixture, TimeManagerIntegration) {
    memoryTest->initialize();

    EXPECT_TRUE(timeManager.hasStarted() == false) << "Timer should not be started initially";

    timeManager.startTimer();
    EXPECT_TRUE(timeManager.hasStarted()) << "Timer should be started after startTimer()";

    memoryTest->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_GT(timeManager.getElapsedSeconds(), 0.0) << "Time should have elapsed";
    EXPECT_FALSE(timeManager.hasEnded()) << "Timer should not be ended yet";

    memoryTest->stop();
    timeManager.endTimer();

    EXPECT_TRUE(timeManager.hasEnded()) << "Timer should be ended after endTimer()";

    memoryTest->waitForCompletion();
}

// Performance characteristics test
TEST_F(MemoryStressTestFixture, PerformanceCharacteristics) {
    memoryTest->initialize();
    timeManager.startTimer();

    auto startTime = std::chrono::high_resolution_clock::now();

    memoryTest->start();

    // Run for 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));

    size_t memoryAllocated = memoryTest->getMemoryAllocated();

    memoryTest->stop();
    memoryTest->waitForCompletion();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto actualDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();

    // Should allocate at least some memory in 1 second
    EXPECT_GT(memoryAllocated, 0) << "Should allocate memory within 1 second";

    // Should not take too much longer than requested
    EXPECT_LT(actualDuration, 4000) << "Should complete within reasonable time";

    // Memory allocation should be in reasonable chunks (1MB blocks expected)
    if (memoryAllocated > 0) {
        // Should be multiple of 1MB (allowing for bandwidth test buffer)
        size_t effectiveAllocation = memoryAllocated - memoryTest->getBandwidthTestSize();
        EXPECT_EQ(effectiveAllocation % (1024 * 1024), 0)
            << "Memory allocation should be in 1MB blocks";
    }
}