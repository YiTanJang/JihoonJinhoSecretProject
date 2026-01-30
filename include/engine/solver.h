#ifndef SOLVER_4D_H
#define SOLVER_4D_H

#include <array>
#include <random>
#include <vector>
#include <chrono>
#include <functional>
#include "core/scoring.h"
#include "data/shared_mem.h"
#include "data/db_manager.h"
#include "core/board.h"
#include <atomic>
#include "utils/config.h"

extern std::atomic<bool> g_terminate_all;

class SAIsland4D {
public:
    using MutationFunc = std::function<std::vector<std::tuple<int, int, int>>()>;
    struct BestState {
        std::array<std::array<int, 14>, 8> board;
        int param;
        int freq;
        int sum;
    };

    SAIsland4D(int id, int mode);
    void run();

private:
    // Core State
    int thread_id;
    int solver_mode; // 0: Pure SA (No Load/Reheat), 1: Elite Reheat
    int lineage_id;
    int cycle_count;
    int accepted_bad_in_physics_window;
    int total_bad_in_physics_window;
    int accepted_total_in_physics_window;
    int physics_window_iter;
    
    // Detailed Physics Tracking
    double sum_bad_proposed_delta;
    int count_bad_proposed;
    double sum_bad_accepted_delta;
    int count_bad_accepted;

    std::array<std::array<int, 14>, 8> current_board;
    std::array<std::array<int, 14>, 8> local_best_board;
    
    double base_initial_temp;
    int current_basis_count; // Added to track real count for win condition
    double current_score;
    double local_best_score;
    double last_cycle_best_score;
    int consecutive_fails;
    int cycle_stagnation_count;

    // Search State
    double temp;
    int stagnation_count;
    int total_iter;
    std::mt19937 rng;
    std::vector<MutationFunc> mutation_operators;
    std::vector<double> action_weights;
    
    // ALNS State
    std::vector<double> segment_scores; 
    std::vector<int> segment_counts;
    int iter_in_segment;
    bool macro_enabled; // [New] Control macro moves

    // Physics Logging
    std::chrono::steady_clock::time_point last_dump_time;
    std::chrono::steady_clock::time_point last_print_time;
    std::vector<PhysicsLogRecord> physics_buffer;
    std::vector<int> action_total_bad_counts;
    std::vector<int> action_accepted_bad_counts;
    std::vector<double> action_energy_deltas; 
    std::vector<double> action_energy_sq_deltas; 
    int last_action_idx;

    // Helper Methods
    void apply_mutation();
    void evaluate_and_accept(const std::vector<std::tuple<int, int, int>>& backup);
    void update_weights(); // ALNS Update
    void update_bests();
    void initialize_lineage(bool is_start);
    void apply_heat_guided_perturbation(const std::vector<std::pair<int, int>>& targets);
    void apply_box_perturbation(int r_start, int c_start, int height, int width);
    void apply_lns_repair_mutation(int r_start, int c_start, int height, int width);
    void run_healing_burst(int iterations, double target_ar, bool use_bad_ar, bool skip_calibration = false);
    void update_monitor();
    void run_standard_sa();
    void run_lns_sa();
    void run_polishing_sa(); // [New] Restricted SA

    // Helpers
    double calculate_score(const std::array<std::array<int, 14>, 8>& board) const;
    double calculate_initial_temperature();

    // Score accessors
    double get_current_score() const { 
        return current_score; 
    }
    double get_best_score() const { 
        return local_best_score;
    }
};

#endif