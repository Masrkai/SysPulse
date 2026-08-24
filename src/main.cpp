#include "../include/SysPulseEngine.hpp"
#include "../include/ConsoleInitializer.hpp"
#include "../include/ConsoleColors.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <string>

class SysPulseManager {
private:
    static constexpr int BAR_WIDTH = 30;
    static constexpr int TEST_DURATION = 30;

    SysPulseEngine engine{TEST_DURATION};

    void clearLine() const {
        std::cout << "\r\033[K";
    }

    void moveCursor(int lines, bool up) const {
        std::cout << "\033[" << lines << (up ? 'A' : 'B');
    }

    void displayTimeProgress() const {
        double elapsedSecondsDouble = engine.getElapsedSeconds();
        int elapsedSeconds = static_cast<int>(elapsedSecondsDouble);
        int displaySeconds = std::min(elapsedSeconds, engine.getTestDuration());
        float progress = static_cast<float>(displaySeconds) / engine.getTestDuration();
        int pos = static_cast<int>(BAR_WIDTH * progress);

        std::string progressBar = "Time:   [";
        for (int i = 0; i < BAR_WIDTH; ++i) {
            if (i < pos) {
                progressBar += std::string(ConsoleColors::CYAN) + "■" + ConsoleColors::RESET;
            } else {
                progressBar += "□";
            }
        }

        clearLine();
        std::cout << progressBar << "] "
                  << displaySeconds << "s / "
                  << engine.getTestDuration() << "s" << std::flush;
    }

    void displayMemoryStatus() const {
        float adjustedTargetMemory = static_cast<float>(engine.getTargetMemory());
        size_t currentMemory = engine.getMemoryAllocated();
        float progress = static_cast<float>(currentMemory) / adjustedTargetMemory;
        int pos = static_cast<int>(BAR_WIDTH * progress);

        std::string progressBar = "Memory: [";
        for (int i = 0; i < BAR_WIDTH; ++i) {
            progressBar += (i < pos) ?
                std::string(ConsoleColors::GREEN) + "■" + ConsoleColors::RESET :
                "□";
        }

        clearLine();
        std::cout << progressBar << "] "
                  << currentMemory / (1024 * 1024) << "MB / "
                  << adjustedTargetMemory / (1024 * 1024) << "MB" << std::flush;
    }

    void displayBandwidthStatus() const {
        double currentBandwidth = engine.getMemoryBandwidth();
        clearLine();

        std::string colorCode = ConsoleColors::CYAN;
        if (currentBandwidth > 20000) {
            colorCode = ConsoleColors::GREEN;
        } else if (currentBandwidth > 10000) {
            colorCode = ConsoleColors::YELLOW;
        } else if (currentBandwidth > 5000) {
            colorCode = ConsoleColors::CYAN;
        } else {
            colorCode = ConsoleColors::RED;
        }

        std::cout << "RAM BW: " << colorCode << std::fixed << std::setprecision(2)
                  << currentBandwidth << " MB/s" << ConsoleColors::RESET;

        if (currentBandwidth > 0) {
            double estimatedFreq = currentBandwidth / 11.2;
            std::cout << " (~" << static_cast<int>(estimatedFreq) << " MHz est.)" << std::flush;
        }
    }

    void updateDisplay() {
        clearLine();
        displayTimeProgress();
        std::cout << std::endl;
        displayMemoryStatus();
        std::cout << std::endl;
        displayBandwidthStatus();
        std::cout << std::endl;
        std::cout << "HASH OPS: "
                  << engine.getHashOperations()
                  << " ops" << std::flush;
    }

public:
    SysPulseManager() = default;

    void run(int argc, char* argv[]) {
        int duration = TEST_DURATION;
        bool nonInteractive = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "--duration" || arg == "-d") && i + 1 < argc) {
                duration = std::stoi(argv[++i]);
                nonInteractive = true;
            } else if ((arg == "--mode" || arg == "-m") && i + 1 < argc) {
                std::string m = argv[++i];
                if (m == "single" || m == "1") {
                    engine.setTestMode(CPUStressTest::TestMode::SingleCore);
                } else {
                    engine.setTestMode(CPUStressTest::TestMode::MultiCore);
                }
                nonInteractive = true;
            } else if (arg == "--help" || arg == "-h") {
                std::cout << "Usage: SysPulse [options]\n"
                          << "Options:\n"
                          << "  -d, --duration SECS    Set test duration in seconds (default: 30)\n"
                          << "  -m, --mode MODE        Set mode ('single' or 'multi', default: multi)\n"
                          << "  -h, --help             Display this help message\n";
                return;
            }
        }

        engine.setTestDuration(duration);

        ConsoleInitializer::initialize();

        std::cout << ConsoleColors::MAGENTA
                  << "\n=== SysPulse Starting ==="
                  << ConsoleColors::RESET << std::endl;

        std::cout << ConsoleColors::YELLOW
                  << "Warning: This program will stress your system for "
                  << duration << " seconds."
                  << ConsoleColors::RESET << std::endl;

        if (!nonInteractive) {
            std::cout << "Select mode:\n"
                      << "  1. Single-Core Test\n"
                      << "  2. Multi-Core Test (All Cores)\n"
                      << "Enter choice [1/2, default 2]: ";
            std::string modeChoice;
            std::getline(std::cin, modeChoice);
            if (modeChoice == "1") {
                engine.setTestMode(CPUStressTest::TestMode::SingleCore);
                std::cout << "Selected: Single-Core Mode\n";
            } else {
                engine.setTestMode(CPUStressTest::TestMode::MultiCore);
                std::cout << "Selected: Multi-Core Mode\n";
            }

            std::cout << "Press Enter to continue...";
            std::cin.get();
        }

        engine.initialize();

        std::cout << ConsoleColors::BLUE << "\nDetected "
                  << engine.getCoreCount() << " CPU cores"
                  << ConsoleColors::RESET << std::endl;

        std::cout << "\nStarting stress test...\n\n" << std::flush;

        engine.start();

        while (engine.shouldContinue()) {
            updateDisplay();
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            moveCursor(3, true);
        }

        engine.stop();
        engine.waitForCompletion();

        std::cout << std::endl;
        std::cout << "\n\n" << ConsoleColors::MAGENTA
                  << "=== Benchmark Results ==="
                  << ConsoleColors::RESET << std::endl;

        BenchmarkResults res = engine.getBenchmarkResults();
        ScoreBreakdown score = engine.getScoreBreakdown();

        std::cout << ConsoleColors::GREEN << std::fixed << std::setprecision(1)
                  << "Overall CPU Score: " << score.overallScore << " / 10000"
                  << ConsoleColors::RESET << std::endl;

        std::cout << "----------------------------------------\n";
        std::cout << "Sub-Scores & Metrics:\n"
                  << "  - ALU Score:     " << std::fixed << std::setprecision(0) << score.aluScore << ConsoleColors::RESET << " (ALU: " << std::fixed << std::setprecision(1) << res.aluOpsPerSec / 1e6 << " M ops/s)\n"
                  << "  - FPU/SIMD Score:" << std::fixed << std::setprecision(0) << score.fpuScore << " (FPU: " << std::fixed << std::setprecision(1) << res.fpuOpsPerSec / 1e6 << " M ops/s)\n"
                  << "  - Cache Score:   " << std::fixed << std::setprecision(0) << score.cacheScore << " (Latency: " << std::fixed << std::setprecision(2) << res.cacheLatencyNs << " ns)\n"
                  << "  - Crypto Score:  " << std::fixed << std::setprecision(0) << score.cryptoScore << " (Crypto: " << std::fixed << std::setprecision(1) << res.cryptoMbPerSec << " MB/s)\n"
                  << "  - Compression:   " << std::fixed << std::setprecision(0) << score.compressionScore << " (Comp: " << std::fixed << std::setprecision(1) << res.compressionMbPerSec << " MB/s)\n";

        std::cout << ConsoleColors::CYAN
                  << "\nTotal execution time: " << std::fixed << std::setprecision(3)
                  << engine.getElapsedSeconds() << " seconds\n"
                  << "Memory bandwidth: " << std::fixed << std::setprecision(2)
                  << engine.getMemoryBandwidth() << " MB/s\n"
                  << "Test Mode: " << (engine.getTestMode() == CPUStressTest::TestMode::SingleCore ? "Single-Core" : "Multi-Core")
                  << " (" << (engine.getTestMode() == CPUStressTest::TestMode::SingleCore ? 1 : engine.getCoreCount()) << " threads)\n"
                  << ConsoleColors::RESET << std::endl;
    }
};

int main(int argc, char* argv[]) {
    SysPulseManager testManager;
    testManager.run(argc, argv);
    return 0;
}
