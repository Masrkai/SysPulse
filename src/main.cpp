#include "../include/CPUStressTest.hpp"
#include "../include/MemoryStressTest.hpp"
#include "../include/ConsoleInitializer.hpp"
#include "../include/ConsoleColors.hpp"
#include "../include/TimeManager.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>

class SysPulseManager {
private:
    static constexpr int BAR_WIDTH = 30;
    static constexpr int TEST_DURATION = 30;

    CPUStressTest cpuTest;
    MemoryStressTest memoryTest;
    TimeManager& timeManager;

    // Helper methods for display
    void clearLine() const {
        std::cout << "\r\033[K"; // Clear the current console line
    }

    void moveCursor(int lines, bool up) const {
        std::cout << "\033[" << lines << (up ? 'A' : 'B'); // Move the cursor up or down
    }

    void displayTimeProgress() const {
        // Get current elapsed time from global time manager
        double elapsedSecondsDouble = timeManager.getElapsedSeconds();
        int elapsedSeconds = static_cast<int>(elapsedSecondsDouble);

        // Clamp elapsedSeconds to not exceed TEST_DURATION for display purposes
        int displaySeconds = std::min(elapsedSeconds, TEST_DURATION);

        // Calculate the progress as a fraction of the total test duration
        float progress = static_cast<float>(displaySeconds) / TEST_DURATION;

        // Map the progress fraction to a position on the progress bar
        int pos = static_cast<int>(BAR_WIDTH * progress);

        // Initialize the progress bar string with a label
        std::string progressBar = "Time:   [";

        // Build the visual representation of the progress bar
        for (int i = 0; i < BAR_WIDTH; ++i) {
            if (i < pos) {
                progressBar += std::string(ConsoleColors::CYAN) + "■" + ConsoleColors::RESET;
            } else {
                progressBar += "□";
            }
        }

        // Clear the current console line
        clearLine();

        // Output the progress bar, current elapsed time, and total test duration
        std::cout << progressBar << "] "
                  << displaySeconds << "s / "
                  << TEST_DURATION << "s" << std::flush;
    }

    void displayMemoryStatus() const {
        // Calculate adjusted target memory based on the multiplier
        float adjustedTargetMemory = static_cast<float>(memoryTest.getTargetMemory());
        size_t currentMemory = memoryTest.getMemoryAllocated();

        // Calculate memory usage progress as a percentage (0.0 to 1.0)
        float progress = static_cast<float>(currentMemory) / adjustedTargetMemory;

        // Calculate the number of filled positions in the progress bar
        int pos = static_cast<int>(BAR_WIDTH * progress);

        // Build the memory usage progress bar
        std::string progressBar = "Memory: [";
        for (int i = 0; i < BAR_WIDTH; ++i) {
            progressBar += (i < pos) ?
                std::string(ConsoleColors::GREEN) + "■" + ConsoleColors::RESET :
                "□";
        }

        // Clear the current console line to prepare for new output
        clearLine();

        // Display the memory progress bar and memory usage details
        std::cout << progressBar << "] "
                  << currentMemory / (1024 * 1024) << "MB / "
                  << adjustedTargetMemory / (1024 * 1024) << "MB" << std::flush;
    }

    void displayBandwidthStatus() const {
        double currentBandwidth = memoryTest.getMemoryBandwidth();

        // Clear the current console line
        clearLine();

        // Display memory bandwidth with color coding based on performance
        std::string colorCode = ConsoleColors::CYAN;
        if (currentBandwidth > 20000) {      // > 20 GB/s - excellent DDR5 performance
            colorCode = ConsoleColors::GREEN;
        } else if (currentBandwidth > 10000) { // > 10 GB/s - good performance
            colorCode = ConsoleColors::YELLOW;
        } else if (currentBandwidth > 5000) {  // > 5 GB/s - moderate performance
            colorCode = ConsoleColors::CYAN;
        } else {                               // < 5 GB/s - lower performance
            colorCode = ConsoleColors::RED;
        }

        std::cout << "RAM BW: " << colorCode << std::fixed << std::setprecision(2)
                  << currentBandwidth << " MB/s" << ConsoleColors::RESET;

        // Add estimated frequency for DDR5 (very rough approximation)
        if (currentBandwidth > 0) {
            // DDR5 dual channel rough estimation: bandwidth ≈ frequency × 2 channels × 8 bytes × efficiency
            // Assuming ~70% efficiency: frequency ≈ bandwidth / (2 × 8 × 0.7) ≈ bandwidth / 11.2
            double estimatedFreq = currentBandwidth / 11.2;
            std::cout << " (~" << static_cast<int>(estimatedFreq) << " MHz est.)" << std::flush;
        }
    }

    void updateDisplay() {
        // Clear the current console line
        clearLine();

        // Display the elapsed time using global time manager
        displayTimeProgress();

        // Output a newline to separate sections
        std::cout << std::endl;

        // Display current memory usage status
        displayMemoryStatus();

        // Output a newline for separation
        std::cout << std::endl;

        // Display memory bandwidth status
        displayBandwidthStatus();

        // Output a newline for separation
        std::cout << std::endl;

        // Display the total number of hash operations performed
        std::cout << "HASH OPS: "
                  << cpuTest.getHashOperations()
                  << " ops" << std::flush;
    }

public:
    SysPulseManager() : timeManager(TimeManager::getInstance()) {}

    void run() {
        // Initialize the console
        ConsoleInitializer::initialize();

        // Display the start banner
        std::cout << ConsoleColors::MAGENTA
                  << "\n=== SysPulse Starting ==="
                  << ConsoleColors::RESET << std::endl;

        // Display warning message
        std::cout << ConsoleColors::YELLOW
                  << "Warning: This program will stress your system for "
                  << TEST_DURATION << " seconds."
                  << ConsoleColors::RESET << std::endl;

        // Prompt the user to continue
        std::cout << "Press Enter to continue...";
        std::cin.get();

        // Initialize both test components
        cpuTest.initialize();
        memoryTest.initialize();

        // Display detected cores
        std::cout << ConsoleColors::BLUE << "\nDetected "
                  << cpuTest.getCoreCount() << " CPU cores"
                  << ConsoleColors::RESET << std::endl;

        std::cout << "\nStarting stress test...\n\n" << std::flush;

        // Start the global timer
        timeManager.startTimer();

        // Start both stress tests
        cpuTest.start();
        memoryTest.start();

        // Monitoring loop - uses global time manager
        while (timeManager.shouldContinue(TEST_DURATION)) {
            updateDisplay();

            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            moveCursor(3, true); // Move up 3 lines
        }

        // Signal all tests to stop and end the timer
        cpuTest.stop();
        memoryTest.stop();
        timeManager.endTimer();

        // Wait for all components to complete
        cpuTest.waitForCompletion();
        memoryTest.waitForCompletion();

        // Display test results using precise timing
        std::cout << std::endl;

        std::cout << "\n\n" << ConsoleColors::MAGENTA
                  << "=== Test Results ==="
                  << ConsoleColors::RESET << std::endl;

        std::cout << ConsoleColors::CYAN
                  << "Total hashing operations: " << cpuTest.getHashOperations()
                  << " ops" << ConsoleColors::RESET << std::endl;

        std::cout << ConsoleColors::CYAN
                  << "Total execution time: " << std::fixed << std::setprecision(3)
                  << timeManager.getElapsedSeconds() << " seconds"
                  << ConsoleColors::RESET << std::endl;

        std::cout << ConsoleColors::CYAN
                  << "Maximum memory allocated: " << ((memoryTest.getMemoryAllocated() + memoryTest.getBandwidthTestSize()) / (1024 * 1024))
                  << "MB" << ConsoleColors::RESET << std::endl;

        std::cout << ConsoleColors::CYAN
                  << "Memory bandwidth: " << std::fixed << std::setprecision(2)
                  << memoryTest.getMemoryBandwidth() << " MB/s"
                  << ConsoleColors::RESET << std::endl;

        std::cout << ConsoleColors::CYAN
                  << "CPU cores utilized: " << cpuTest.getCoreCount()
                  << ConsoleColors::RESET << std::endl;

        // Cleanup
        TimeManager::cleanup();
    }
};

int main() {
    SysPulseManager testManager;
    testManager.run();
    return 0;
}