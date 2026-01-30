#pragma once

namespace Config4D {
    // Thermal Physics
    inline double CRITICAL_TEMP = 39.23;
    inline double SECOND_CRITICAL_TEMP = 2.66;
    inline double MIN_TEMP = 0.001 * CRITICAL_TEMP;   // 0.03923
    inline double COOLING_RATE = 0.9999994;
    inline double INITIAL_TEMP = 100000.0;
    inline double REHEAT_TEMP_THRESHOLD = 100.0;
    inline double SUM_MODE_TEMP_SCALE = 1000.0;

    // Stagnation & Reseed
    inline int RESEED_STAGNATION_THRESHOLD = 4000000;

    // Hybrid Mode Parameters
    inline int HYBRID_CYCLE_TOTAL = 500000;
    inline int HYBRID_CYCLE_SWITCH = 250000;

    // Initial Mutation Probabilities (P0-P9: Specialized, P10: Random)
    inline double INIT_PROB_SPECIALIZED = 0.05;
    inline double INIT_PROB_DEFAULT = 0.50;

    // Database & Pool Management
    inline int DB_CLEANUP_THRESHOLD = 3500;
    inline int POOL_UPDATE_INTERVAL_SEC = 60;
    inline int ELITE_POOL_SIZE = 10;
    inline int DB_SAVE_INTERVAL_SEC = 100;
    inline double POOL_RESEED_PROB = 1.00; 

    // Basis Set Configuration
    inline constexpr int BASIS_MAX_RANGE = 13000;
    inline constexpr bool BASIS_USE_PADDING = false;
    inline constexpr int BASIS_PADDING_WIDTH = 4;
}
