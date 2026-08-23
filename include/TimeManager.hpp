#pragma once

#include <chrono>
#include <atomic>
#include <mutex>

/*
 * Global time management class for precise timing control
 * Provides centralized time tracking for the entire stress test duration
 */
class TimeManager {
private:
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;
    std::atomic<bool> testStarted{false};
    std::atomic<bool> testEnded{false};
    mutable std::mutex timeMutex;

    // Private constructor for singleton pattern
    TimeManager() = default;

public:
    // Meyers' Singleton implementation (thread-safe in C++11 and later)
    static TimeManager& getInstance() {
        static TimeManager instance;
        return instance;
    }

    // Start the global timer
    void startTimer() {
        std::lock_guard<std::mutex> lock(timeMutex);
        if (!testStarted.load(std::memory_order_acquire)) {
            startTime = std::chrono::steady_clock::now();
            testStarted.store(true, std::memory_order_release);
        }
    }

    // End the global timer
    void endTimer() {
        std::lock_guard<std::mutex> lock(timeMutex);
        if (testStarted.load(std::memory_order_acquire) && !testEnded.load(std::memory_order_acquire)) {
            endTime = std::chrono::steady_clock::now();
            testEnded.store(true, std::memory_order_release);
        }
    }

    // Get elapsed time in seconds (double precision)
    double getElapsedSeconds() const {
        std::lock_guard<std::mutex> lock(timeMutex);
        if (!testStarted.load(std::memory_order_acquire)) {
            return 0.0;
        }

        auto currentTime = testEnded.load(std::memory_order_acquire) ? endTime : std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - startTime);
        return duration.count() / 1000000.0; // Convert microseconds to seconds
    }

    // Get elapsed time in milliseconds
    int64_t getElapsedMilliseconds() const {
        std::lock_guard<std::mutex> lock(timeMutex);
        if (!testStarted.load(std::memory_order_acquire)) {
            return 0;
        }

        auto currentTime = testEnded.load(std::memory_order_acquire) ? endTime : std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
    }

    // Get elapsed time in integer seconds (for display purposes)
    int getElapsedSecondsInt() const {
        return static_cast<int>(getElapsedSeconds());
    }

    // Check if the test should continue based on duration
    bool shouldContinue(int maxDurationSeconds) const {
        return getElapsedSeconds() < maxDurationSeconds;
    }

    // Check if test has started
    bool hasStarted() const {
        return testStarted.load(std::memory_order_acquire);
    }

    // Check if test has ended
    bool hasEnded() const {
        return testEnded.load(std::memory_order_acquire);
    }

    // Reset the timer (for testing purposes)
    void reset() {
        std::lock_guard<std::mutex> lock(timeMutex);
        testStarted.store(false, std::memory_order_release);
        testEnded.store(false, std::memory_order_release);
    }

    // Get precise start time
    std::chrono::steady_clock::time_point getStartTime() const {
        std::lock_guard<std::mutex> lock(timeMutex);
        return startTime;
    }

    // Get precise end time
    std::chrono::steady_clock::time_point getEndTime() const {
        std::lock_guard<std::mutex> lock(timeMutex);
        return endTime;
    }

    // Compatibility cleanup function (resets state for existing test suites)
    static void cleanup() {
        getInstance().reset();
    }

    // Destructor
    ~TimeManager() = default;

    // Delete copy constructor and assignment operator
    TimeManager(const TimeManager&) = delete;
    TimeManager& operator=(const TimeManager&) = delete;
};