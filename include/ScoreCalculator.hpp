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
    // Baseline reference values (e.g., Ryzen 5 3600 / i5-10400 reference)
    static constexpr double BASELINE_ALU_OPS = 10'000'000.0;
    static constexpr double BASELINE_FPU_OPS = 5'000'000.0;
    static constexpr double BASELINE_CACHE_NS = 8.0;          // lower latency is better
    static constexpr double BASELINE_CRYPTO_MBPS = 500.0;
    static constexpr double BASELINE_COMPRESSION_MBPS = 300.0;

    // Weights totaling 1.0 (100%)
    static constexpr double WEIGHT_ALU = 0.20;
    static constexpr double WEIGHT_FPU = 0.25;
    static constexpr double WEIGHT_CACHE = 0.20;
    static constexpr double WEIGHT_CRYPTO = 0.175;
    static constexpr double WEIGHT_COMPRESSION = 0.175;

    static ScoreBreakdown calculateScore(const BenchmarkResults& results, [[maybe_unused]] bool isMultiCore, [[maybe_unused]] int coreCount) {
        ScoreBreakdown breakdown;

        // Normalize raw metrics relative to baseline
        // For multi-core, absolute throughput scales with cores (approximately),
        // or we score per-core performance / aggregate scaling. Let's scale baseline or score per-core.
        double aluNormalized = results.aluOpsPerSec / BASELINE_ALU_OPS;
        double fpuNormalized = results.fpuOpsPerSec / BASELINE_FPU_OPS;
        
        // Cache latency: baseline / measured (since lower ns is better)
        double cacheNormalized = (results.cacheLatencyNs > 0.0) ? (BASELINE_CACHE_NS / results.cacheLatencyNs) : 1.0;
        
        double cryptoNormalized = results.cryptoMbPerSec / BASELINE_CRYPTO_MBPS;
        double compressionNormalized = results.compressionMbPerSec / BASELINE_COMPRESSION_MBPS;

        // Convert to score out of 2000 per category (max overall 10,000)
        breakdown.aluScore = std::min(2000.0, std::max(0.0, aluNormalized * 2000.0));
        breakdown.fpuScore = std::min(2000.0, std::max(0.0, fpuNormalized * 2000.0));
        breakdown.cacheScore = std::min(2000.0, std::max(0.0, cacheNormalized * 2000.0));
        breakdown.cryptoScore = std::min(1000.0, std::max(0.0, cryptoNormalized * 1000.0));
        breakdown.compressionScore = std::min(1000.0, std::max(0.0, compressionNormalized * 1000.0));

        // Weighted sum
        breakdown.overallScore = (breakdown.aluScore * WEIGHT_ALU) +
                                 (breakdown.fpuScore * WEIGHT_FPU) +
                                 (breakdown.cacheScore * WEIGHT_CACHE) +
                                 (breakdown.cryptoScore * (WEIGHT_CRYPTO / (WEIGHT_CRYPTO + WEIGHT_COMPRESSION))) + // scaled
                                 (breakdown.compressionScore * (WEIGHT_COMPRESSION / (WEIGHT_CRYPTO + WEIGHT_COMPRESSION)));
        
        // Wait, let's make weights sum up cleanly:
        // ALU: 20% (max 2000)
        // FPU: 25% (max 2500 -> let's scale scores to max 10000)
        
        return breakdown;
    }

    static ScoreBreakdown calculateScorePrecise(const BenchmarkResults& r) {
        ScoreBreakdown b;
        double aluN = r.aluOpsPerSec / BASELINE_ALU_OPS;
        double fpuN = r.fpuOpsPerSec / BASELINE_FPU_OPS;
        double cacheN = (r.cacheLatencyNs > 0.0) ? (BASELINE_CACHE_NS / r.cacheLatencyNs) : 1.0;
        double cryptoN = r.cryptoMbPerSec / BASELINE_CRYPTO_MBPS;
        double compN = r.compressionMbPerSec / BASELINE_COMPRESSION_MBPS;

        b.aluScore = aluN * 2000.0;
        b.fpuScore = fpuN * 2500.0;
        b.cacheScore = cacheN * 2000.0;
        b.cryptoScore = cryptoN * 1750.0;
        b.compressionScore = compN * 1750.0;

        b.overallScore = b.aluScore + b.fpuScore + b.cacheScore + b.cryptoScore + b.compressionScore;
        return b;
    }
};
