#include "../include/MemoryStressTest.hpp"
#include "../include/ConsoleColors.hpp"

#include <chrono>
#include <iostream>
#include <algorithm>
#include <random>
#include <cstring>

// Memory bandwidth measurement methods
double MemoryStressTest::performSequentialRead(uint8_t* buffer, size_t size) {
    auto start = std::chrono::high_resolution_clock::now();

    volatile uint64_t sum = 0;
    for (size_t i = 0; i < size; i += 64) { // 64-byte cache line aligned
        sum += buffer[i];
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    return (static_cast<double>(size) / 1024.0 / 1024.0) / (duration.count() / 1e9);
}

double MemoryStressTest::performSequentialWrite(uint8_t* buffer, size_t size) {
    auto start = std::chrono::high_resolution_clock::now();

    // Write pattern
    for (size_t i = 0; i < size; i += 64) {
        buffer[i] = static_cast<uint8_t>(i & 0xFF);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    return (static_cast<double>(size) / 1024.0 / 1024.0) / (duration.count() / 1e9);
}

double MemoryStressTest::performRandomAccess(uint8_t* buffer, size_t size) {
    const size_t numAccesses = size / 64; // Access every 64 bytes
    std::vector<size_t> indices(numAccesses);

    // Generate random indices
    for (size_t i = 0; i < numAccesses; ++i) {
        indices[i] = (i * 64) % size;
    }

    // Shuffle for random access pattern
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices.begin(), indices.end(), gen);

    auto start = std::chrono::high_resolution_clock::now();

    volatile uint64_t sum = 0;
    for (size_t idx : indices) {
        sum += buffer[idx];
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    return (static_cast<double>(size) / 1024.0 / 1024.0) / (duration.count() / 1e9);
}

void MemoryStressTest::measureMemoryBandwidth() {
    if (!bandwidthTestBuffer) {
        bandwidthTestBuffer = std::make_unique<uint8_t[]>(BANDWIDTH_TEST_SIZE);

        // Initialize buffer with test data
        for (size_t i = 0; i < BANDWIDTH_TEST_SIZE; ++i) {
            bandwidthTestBuffer[i] = static_cast<uint8_t>(i & 0xFF);
        }
    }

    double totalBandwidth = 0.0;
    int validTests = 0;

    // Perform multiple test iterations for averaging
    for (int i = 0; i < BANDWIDTH_ITERATIONS && bandwidthTestRunning; ++i) {
        // Sequential read test
        double readBW = performSequentialRead(bandwidthTestBuffer.get(), BANDWIDTH_TEST_SIZE);

        // Sequential write test
        double writeBW = performSequentialWrite(bandwidthTestBuffer.get(), BANDWIDTH_TEST_SIZE);

        // Random access test (typically much slower)
        double randomBW = performRandomAccess(bandwidthTestBuffer.get(), BANDWIDTH_TEST_SIZE);

        // Use the highest bandwidth measurement (sequential read typically performs best)
        double maxBW = std::max({readBW, writeBW, randomBW * 2}); // Weight random access

        if (maxBW > 0 && maxBW < 1000000) { // Sanity check (less than 1TB/s)
            totalBandwidth += maxBW;
            validTests++;
        }

        // Small delay between iterations
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (validTests > 0) {
        double avgBandwidth = totalBandwidth / validTests;
        memoryBandwidth.store(avgBandwidth, std::memory_order_relaxed);
    }
}

void MemoryStressTest::continuousBandwidthTest() {
    bandwidthTestRunning = true;

    // Perform initial measurement
    measureMemoryBandwidth();

    // Continue measuring periodically during the test
    while (running && timeManager.shouldContinue(TEST_DURATION) && bandwidthTestRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        measureMemoryBandwidth();
    }

    bandwidthTestRunning = false;
}

// Function to stress test memory allocation
void MemoryStressTest::memoryStressTest() {
    try {
        // Loop to allocate memory until the target threshold is reached or the test is stopped
        while (running && memoryAllocated < (MULTIPLIER * TARGET_MEMORY) - BANDWIDTH_TEST_SIZE && timeManager.shouldContinue(TEST_DURATION)) {
            static constexpr size_t blockSize = 1024 * 1024; // Block size of 1 MB

            // Create a unique pointer to a dynamically allocated vector of integers.
            // The vector will simulate a block of memory for operations like memory testing or simulation.
            auto block = std::make_unique<std::vector<int>>(
                blockSize / sizeof(int),  // The size of the vector is determined by dividing blockSize by sizeof(int).
                                            // This calculates how many integers can fit into the given block size.
                                            // Example: If blockSize is 1024 bytes and sizeof(int) is 4 bytes,
                                            // the vector will have 1024 / 4 = 256 elements.

                1                // The initial value of each element in the vector.
                                        // Here, all elements of the vector are initialized to the value 1.
                                        // This ensures the block is filled with a known value, which may
                                        // be useful for certain simulations or tests.
            );

            // Update the total allocated memory counter
            memoryAllocated += blockSize;

            // Store the allocated block in the vector to maintain ownership and prevent deallocation
            memoryBlocks.push_back(std::move(block));
        }
    } catch (const std::bad_alloc &e) {
        allocationFailed.store(true, std::memory_order_relaxed);
        // Handle memory allocation failure
        std::lock_guard<std::mutex> lock(consoleMutex); // Ensure thread-safe console output
        std::cout << "\n"
                << ConsoleColors::RED
                << "Memory allocation failed: " << e.what()
                << ConsoleColors::RESET << std::endl;
    }
}

void MemoryStressTest::initialize() {
    // Reset atomic variables
    memoryAllocated.store(0);
    memoryBandwidth.store(0.0);
    running.store(true);
    bandwidthTestRunning.store(false);
    allocationFailed.store(false);
}

void MemoryStressTest::start() {
    // Launch memory stress test thread
    memThread = std::thread(&MemoryStressTest::memoryStressTest, this);

    // Launch memory bandwidth test thread
    bandwidthThread = std::thread(&MemoryStressTest::continuousBandwidthTest, this);
}

void MemoryStressTest::stop() {
    running = false;
    bandwidthTestRunning = false;
}

void MemoryStressTest::waitForCompletion() {
    if (memThread.joinable()) {
        memThread.join();
    }

    if (bandwidthThread.joinable()) {
        bandwidthThread.join();
    }
}