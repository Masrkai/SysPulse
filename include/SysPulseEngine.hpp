#pragma once

#include "CPUStressTest.hpp"
#include "MemoryStressTest.hpp"
#include "TimeManager.hpp"
#include "ThermalMonitor.hpp"

class SysPulseEngine {
private:
    CPUStressTest cpuTest;
    MemoryStressTest memoryTest;
    TimeManager& timeManager;
    ThermalMonitor thermalMonitor;
    int testDurationSeconds;
    bool isRunning{false};

public:
    explicit SysPulseEngine(int durationSeconds = 30)
        : timeManager(TimeManager::getInstance()),
          thermalMonitor(100.0),
          testDurationSeconds(durationSeconds) {}

    void setTestMode(CPUStressTest::TestMode mode) {
        cpuTest.setTestMode(mode);
    }

    CPUStressTest::TestMode getTestMode() const {
        return cpuTest.getTestMode();
    }

    void setTestDuration(int seconds) {
        testDurationSeconds = seconds;
    }

    int getTestDuration() const {
        return testDurationSeconds;
    }

    void initialize() {
        cpuTest.initialize();
        memoryTest.initialize();
    }

    void start() {
        if (isRunning) return;
        memoryTest.measureMemoryBandwidth();
        timeManager.reset();
        timeManager.startTimer();
        thermalMonitor.start();
        cpuTest.start();
        memoryTest.start();
        isRunning = true;
    }

    void stop() {
        if (!isRunning) return;
        cpuTest.stop();
        memoryTest.stop();
        thermalMonitor.stop();
        timeManager.endTimer();
        isRunning = false;
    }

    void waitForCompletion() {
        cpuTest.waitForCompletion();
        memoryTest.waitForCompletion();
        thermalMonitor.stop();
    }

    bool shouldContinue() const {
        if (thermalMonitor.isOverheated()) {
            timeManager.endTimer();
            return false;
        }
        bool cont = timeManager.shouldContinue(testDurationSeconds);
        if (!cont && !timeManager.hasEnded()) {
            timeManager.endTimer();
        }
        return cont;
    }

    double getElapsedSeconds() const {
        return timeManager.getElapsedSeconds();
    }

    int getCoreCount() const {
        return cpuTest.getCoreCount();
    }

    uint64_t getHashOperations() const {
        return cpuTest.getHashOperations();
    }

    size_t getMemoryAllocated() const {
        return memoryTest.getMemoryAllocated();
    }

    double getMemoryBandwidth() const {
        return memoryTest.getMemoryBandwidth();
    }

    size_t getTargetMemory() const {
        return memoryTest.getTargetMemory();
    }

    std::vector<float> getThreadLoads() {
        return cpuTest.getThreadLoads();
    }

    BenchmarkResults getBenchmarkResults() const {
        return cpuTest.getBenchmarkResults();
    }

    ScoreBreakdown getScoreBreakdown() const {
        return cpuTest.getScoreBreakdown();
    }
};
