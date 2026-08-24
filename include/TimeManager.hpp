#pragma once

#include <chrono>
#include <atomic>

/*
 * Global time management class for precise timing control
 * Provides centralized time tracking for the entire stress test duration
 * Lock-free implementation using atomic microseconds since epoch.
 */
class TimeManager {
private:
    std::atomic<int64_t> startTimeUs{0};
    std::atomic<int64_t> endTimeUs{0};
    std::atomic<bool> testStarted{false};
    std::atomic<bool> testEnded{false};

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
        if (!testStarted.load(std::memory_order_acquire)) {
            auto now = std::chrono::steady_clock::now();
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
            startTimeUs.store(us, std::memory_order_release);
            testStarted.store(true, std::memory_order_release);
        }
    }

    // End the global timer
    void endTimer() {
        if (testStarted.load(std::memory_order_acquire) && !testEnded.load(std::memory_order_acquire)) {
            auto now = std::chrono::steady_clock::now();
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
            endTimeUs.store(us, std::memory_order_release);
            testEnded.store(true, std::memory_order_release);
        }
    }

    // Get elapsed time in seconds (double precision)
    double getElapsedSeconds() const {
        if (!testStarted.load(std::memory_order_acquire)) {
            return 0.0;
        }

        int64_t start = startTimeUs.load(std::memory_order_acquire);
        int64_t end = testEnded.load(std::memory_order_acquire) ? 
            endTimeUs.load(std::memory_order_acquire) : 
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

        int64_t diff = end - start;
        return diff > 0 ? static_cast<double>(diff) / 1000000.0 : 0.0;
    }

    // Get elapsed time in milliseconds
    int64_t getElapsedMilliseconds() const {
        return static_cast<int64_t>(getElapsedSeconds() * 1000.0);
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
        testStarted.store(false, std::memory_order_release);
        testEnded.store(false, std::memory_order_release);
        startTimeUs.store(0, std::memory_order_release);
        endTimeUs.store(0, std::memory_order_release);
    }

    // Get precise start time
    std::chrono::steady_clock::time_point getStartTime() const {
        int64_t us = startTimeUs.load(std::memory_order_acquire);
        return std::chrono::steady_clock::time_point(std::chrono::microseconds(us));
    }

    // Get precise end time
    std::chrono::steady_clock::time_point getEndTime() const {
        int64_t us = endTimeUs.load(std::memory_order_acquire);
        return std::chrono::steady_clock::time_point(std::chrono::microseconds(us));
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
