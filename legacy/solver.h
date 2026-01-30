#pragma once
#include <array>
#include <random>
#include <vector>
#include <chrono>
#include <functional>
#include "logic.h"
#include "shared_mem.h"

class SAIsland {
public:
    using MutationFunc = std::function<std::vector<std::tuple<int, int, int>>()>;
    struct BestState {
        std::array<std::array<int, 14>, 8> board;
        int param;
        int freq;
        int sum;
    };

    SAIsland(int id, std::array<std::array<int, 14>, 8> initial_board, int lineage, int mode);
    void run();

private:
    // Core State
    int thread_id;
    int mode; // 0: Length, 1: Freq, 2: Sum, 3: Hybrid (Freq/Sum)
    int effective_mode;
    int lineage_id;
    
    std::array<std::array<int, 14>, 8> current_board;
    std::array<std::array<int, 14>, 8> local_best_board;
    FastBoard current_fb;
    
    /*
    int current_param;
    int current_freq;
    int current_sum;
    int current_hybrid_val;
    int current_hybrid_sqrt_val;
    */
    long long current_richness;

    /*
    int local_best_param;
    int local_best_freq;
    int local_best_sum;
    int local_best_hybrid_val;
    int local_best_hybrid_sqrt_val;
    */
    long long local_best_richness;
    
    // Hybrid Persistence
    BestState hybrid_bests[2];

    // Search State
    double temp;
    int stagnation_count;
    int total_iter;
    std::mt19937 rng;
    std::chrono::steady_clock::time_point last_db_save;
    std::vector<MutationFunc> mutation_operators;

    // Helper Methods
    void apply_mutation();
    void evaluate_and_accept(const std::vector<std::tuple<int, int, int>>& backup);
    void update_bests();
    void handle_periodic_tasks();
    void handle_reseed();
    void handle_reheat();
    void update_monitor();
    
    // Legacy support
    void legacy_handle_mode_switching();

    // Helpers
    long long calculate_score(int m, const FastBoard& fb) const;
    double get_temp_scale(int m) const;

    // Score accessors based on effective_mode
    long long get_current_score() const { 
        if (effective_mode == 0) return current_richness;
        /*
        if (effective_mode == 1) return current_freq;
        if (effective_mode == 2) return current_sum;
        if (effective_mode == 4) return current_hybrid_val;
        if (effective_mode == 5) return current_hybrid_sqrt_val;
        if (effective_mode == 6) return current_richness;
        */
        return current_richness; 
    }
    long long get_best_score() const { 
        if (effective_mode == 0) return local_best_richness;
        /*
        if (effective_mode == 1) return local_best_freq;
        if (effective_mode == 2) return local_best_sum;
        if (effective_mode == 4) return local_best_hybrid_val;
        if (effective_mode == 5) return local_best_hybrid_sqrt_val;
        if (effective_mode == 6) return local_best_richness;
        */
        return local_best_richness;
    }
};
