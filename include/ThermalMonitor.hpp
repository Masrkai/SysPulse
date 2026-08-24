#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include "ConsoleColors.hpp"

class ThermalMonitor {
private:
    std::atomic<bool> monitoring{false};
    std::atomic<double> maxTempC{0.0};
    std::thread monitorThread;
    double maxSafeTempC;

    void monitorLoop() {
        while (monitoring) {
            double currentMax = 0.0;
            // Scan thermal zones on Linux
            for (int i = 0; i < 16; ++i) {
                std::string path = "/sys/class/thermal/thermal_zone" + std::to_string(i) + "/temp";
                std::ifstream file(path);
                if (file.is_open()) {
                    long milliC = 0;
                    if (file >> milliC) {
                        double tempC = static_cast<double>(milliC) / 1000.0;
                        if (tempC > currentMax) {
                            currentMax = tempC;
                        }
                    }
                }
            }
            if (currentMax > 0.0) {
                maxTempC.store(currentMax, std::memory_order_relaxed);
                if (currentMax >= maxSafeTempC) {
                    std::cout << "\n" << ConsoleColors::RED 
                              << "[WARNING] Thermal safety threshold exceeded (" << currentMax 
                              << "°C >= " << maxSafeTempC << "°C)! Aborting stress test to protect hardware."
                              << ConsoleColors::RESET << std::endl;
                    monitoring.store(false);
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

public:
    explicit ThermalMonitor(double safeLimitC = 95.0) : maxSafeTempC(safeLimitC) {}

    void start() {
        if (monitoring.load()) return;
        monitoring.store(true);
        monitorThread = std::thread(&ThermalMonitor::monitorLoop, this);
    }

    void stop() {
        monitoring.store(false);
        if (monitorThread.joinable()) {
            monitorThread.join();
        }
    }

    double getMaxTemp() const {
        return maxTempC.load(std::memory_order_relaxed);
    }

    bool isOverheated() const {
        return maxTempC.load(std::memory_order_relaxed) >= maxSafeTempC;
    }

    ~ThermalMonitor() {
        stop();
    }
};
