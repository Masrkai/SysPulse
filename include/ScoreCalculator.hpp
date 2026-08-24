#pragma once

#include <cstdint>
#include <algorithm>

struct BenchmarkResults {
    double aluOpsPerSec = 0.0;
    double fpuOpsPerSec = 0.0;
    double cacheLatencyNs = 0.0; // lower is better
    double cryptoMbPerSec = 0.0;
    double compressionMbPerSec = 0.0;
};

struct ScoreBreakdown {
    double aluScore = 0.0;
    double fpuScore = 0.0;
    double cacheScore = 0.0;
    double cryptoScore = 0.0;
    double compressionScore = 0.0;
    double overallScore = 0.0;
};

class ScoreCalculator {
public:
    // Calibrated baseline reference values per core for modern high-end CPUs
    static constexpr double BASELINE_ALU_OPS = 250'000'000.0;
    static constexpr double BASELINE_FPU_OPS = 120'000'000.0;
    static constexpr double BASELINE_CACHE_NS = 12.0;          // lower latency is better
    static constexpr double BASELINE_CRYPTO_MBPS = 15'000.0;
    static constexpr double BASELINE_COMPRESSION_MBPS = 4'000.0;

    // Weights / max score allocations totaling 10,000 max score
    static ScoreBreakdown calculateScore(const BenchmarkResults& results, bool isMultiCore, int coreCount) {
        ScoreBreakdown breakdown;

        // Dynamically scale baseline expectations for multi-core runs using efficiency scaling (Amdahl-inspired factor)
        double coreScaling = (isMultiCore && coreCount > 1) ? static_cast<double>(coreCount) * 0.85 : 1.0;

        double aluBaseline = BASELINE_ALU_OPS * coreScaling;
        double fpuBaseline = BASELINE_FPU_OPS * coreScaling;
        double cryptoBaseline = BASELINE_CRYPTO_MBPS * coreScaling;
        double compressionBaseline = BASELINE_COMPRESSION_MBPS * coreScaling;

        double aluNormalized = (aluBaseline > 0.0) ? (results.aluOpsPerSec / aluBaseline) : 0.0;
        double fpuNormalized = (fpuBaseline > 0.0) ? (results.fpuOpsPerSec / fpuBaseline) : 0.0;
        double cacheNormalized = (results.cacheLatencyNs > 0.0) ? (BASELINE_CACHE_NS / results.cacheLatencyNs) : 1.0;
        double cryptoNormalized = (cryptoBaseline > 0.0) ? (results.cryptoMbPerSec / cryptoBaseline) : 0.0;
        double compressionNormalized = (compressionBaseline > 0.0) ? (results.compressionMbPerSec / compressionBaseline) : 0.0;

        // Strict capping to category max scores (totaling 10,000 max overall)
        breakdown.aluScore = std::min(2000.0, std::max(0.0, aluNormalized * 2000.0));
        breakdown.fpuScore = std::min(2500.0, std::max(0.0, fpuNormalized * 2500.0));
        breakdown.cacheScore = std::min(2000.0, std::max(0.0, cacheNormalized * 2000.0));
        breakdown.cryptoScore = std::min(1750.0, std::max(0.0, cryptoNormalized * 1750.0));
        breakdown.compressionScore = std::min(1750.0, std::max(0.0, compressionNormalized * 1750.0));

        breakdown.overallScore = breakdown.aluScore + breakdown.fpuScore + breakdown.cacheScore + breakdown.cryptoScore + breakdown.compressionScore;
        return breakdown;
    }

    static ScoreBreakdown calculateScorePrecise(const BenchmarkResults& r) {
        return calculateScore(r, false, 1);
    }
};
