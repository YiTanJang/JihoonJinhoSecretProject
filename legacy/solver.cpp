#include "solver.h"
#include "config.h"
#include "globals.h"
#include "db_manager.h"
#include "mutations.h"
#include <cmath>
#include <algorithm>
#include <print>

SAIsland::SAIsland(int id, std::array<std::array<int, 14>, 8> initial_board, int lineage, int m)
    : thread_id(id), mode(m), lineage_id(lineage), current_board(initial_board), 
      local_best_board(initial_board), total_iter(0), stagnation_count(0) 
{
    // Mode 3 (Old Switching) is now mapped to Mode 4 (Direct Hybrid Score)
    if (mode == 3) {
        mode = 4;
        std::println("Thread {} [INFO] Mode 3 (Legacy Switching) auto-mapped to Mode 4 (Direct Hybrid).", thread_id);
    }

    rng.seed(std::random_device{}() ^ (thread_id << 16));
    rebuild_fast_board(current_board, current_fb);

    effective_mode = mode; // No more dynamic switching by default
    
    /*
    current_param = get_score_param_bit(current_fb);
    current_freq  = get_frequency_score_bit(current_fb);
    current_sum   = get_sum_score(current_fb);
    current_hybrid_val = get_hybrid_score(current_fb);
    current_hybrid_sqrt_val = get_hybrid_sqrt_score(current_fb);
    */
    current_richness = get_richness_score(current_board);
    
    /*
    local_best_param = current_param;
    local_best_freq = current_freq;
    local_best_sum = current_sum;
    local_best_hybrid_val = current_hybrid_val;
    local_best_hybrid_sqrt_val = current_hybrid_sqrt_val;
    */
    local_best_richness = current_richness;

    // hybrid_bests[0] = {current_board, current_param, current_freq, local_best_sum};
    // hybrid_bests[1] = {current_board, current_param, current_freq, local_best_sum};

    temp = Config4D::INITIAL_TEMP;
    last_db_save = std::chrono::steady_clock::now();

    // Initialize Mutation Operators Container
    mutation_operators = {
        [this]() { return apply_adjacent_swap(current_board, rng); },
        [this]() { return apply_double_cell_mutation(current_board, rng); },
        [this]() { return apply_triple_cycle_swap(current_board, rng); },
        [this]() { return apply_random_2_cell_swap(current_board, rng); }
    };

    // Check Global Best
    int my_score = get_current_score();
    std::lock_guard<std::mutex> lock(console_mtx);
    if (my_score > global_best_score.load()) {
        global_best_score = my_score;
        global_best_board = current_board;
        global_best_lineage_id = lineage_id;
        std::println("Thread {} [INIT] Set Global Best to {}", thread_id, my_score);
    }
}

void SAIsland::run() {
    auto& my_status = g_monitor_ptr->status[thread_id];
    int num_ops = (int)mutation_operators.size();
    if (my_status.params[0] == 0) {
        for(int k = 0; k < num_ops; ++k) my_status.params[k] = Config4D::INIT_PROB_SPECIALIZED;
        my_status.params[num_ops] = Config4D::INIT_PROB_DEFAULT; // Fallback / Random Swap
        for(int k = num_ops + 1; k < 16; ++k) my_status.params[k] = 0.0;
    }

    while (true) {
        ++total_iter;
        // legacy_handle_mode_switching(); // Disabled: Mode 3 is now Direct Hybrid
        apply_mutation();
        handle_periodic_tasks();
        update_monitor();
        temp *= Config4D::COOLING_RATE;
    }
}

// Helper: Centralized Score Calculation
long long SAIsland::calculate_score(int m, const FastBoard& fb) const {
    switch(m) {
        case 0: return get_richness_score(current_board);
        /*
        case 1: return get_frequency_score_bit(fb);
        case 2: return get_sum_score(fb);
        case 4: return get_hybrid_score(fb);
        case 5: return get_hybrid_sqrt_score(fb);
        case 6: return get_richness_score(current_board);
        */
        default: return get_richness_score(current_board);
    }
}

// Helper: Centralized Temperature Scaling
double SAIsland::get_temp_scale(int m) const {
    switch(m) {
        case 0: return 1.0; 
        /*
        case 1: return TEMP_SCALE_FREQ;
        case 2: return TEMP_SCALE_SUM;
        case 4: return TEMP_SCALE_HYBRID;
        case 5: return TEMP_SCALE_HYBRID_SQRT;
        case 6: return 10000.0; // Scaled for richness penalty
        */
        default: return 1.0;
    }
}

// Legacy Code: Kept for reference but disabled in main loop
void SAIsland::legacy_handle_mode_switching() {
    // Original logic for Mode 3 (Switching)
    /*
    if (mode != 3) return;

    int cycle = total_iter % Config4D::HYBRID_CYCLE_TOTAL;
    int new_eff_mode = (cycle < Config4D::HYBRID_CYCLE_SWITCH) ? 1 : 2;
    int array_idx = (new_eff_mode == 1) ? 0 : 1; 
    int current_array_idx = (effective_mode == 1) ? 0 : 1;

    if (new_eff_mode != effective_mode) {
        hybrid_bests[current_array_idx] = {local_best_board, local_best_param, local_best_freq, local_best_sum};
        effective_mode = new_eff_mode;
        
        local_best_board = hybrid_bests[array_idx].board;
        local_best_param = hybrid_bests[array_idx].param;
        local_best_freq  = hybrid_bests[array_idx].freq;
        local_best_sum   = hybrid_bests[array_idx].sum;

        if (effective_mode == 0) current_param = get_score_param_bit(current_fb);
        else if (effective_mode == 1) current_freq = get_frequency_score_bit(current_fb);
        else current_sum = get_sum_score(current_fb);
    }
    */
}

void SAIsland::apply_mutation() {
    auto& my_status = g_monitor_ptr->status[thread_id];
    int num_ops = (int)mutation_operators.size();
    int num_actions = num_ops + 1; 
    
    double total_weight = 0.0;
    for(int i = 0; i < num_actions; ++i) total_weight += my_status.params[i];

    std::uniform_real_distribution<double> dist_prob(0.0, 1.0);
    double action_roll = dist_prob(rng) * total_weight;
    double current_cumulative = 0.0;
    int selected_action = -1;

    for (int i = 0; i < num_actions; ++i) {
        current_cumulative += my_status.params[i];
        if (action_roll <= current_cumulative) {
            selected_action = i;
            break;
        }
    }

    std::vector<std::tuple<int, int, int>> backup;
    
    if (selected_action >= 0 && selected_action < num_ops) {
        backup = mutation_operators[selected_action]();
    }

    if (backup.empty()) {
        backup = apply_single_cell_mutation(current_board, rng);
    }

    for(const auto& [r, c, old_val] : backup) {
        update_fast_board(current_fb, r, c, old_val, current_board[r][c]);
    }

    evaluate_and_accept(backup);
}

void SAIsland::evaluate_and_accept(const std::vector<std::tuple<int, int, int>>& backup) {
    bool accept = false;
    long long new_score = calculate_score(effective_mode, current_fb);
    long long old_score = current_richness; 
    
    // if (effective_mode == 0) old_score = current_richness;

    if (new_score > old_score) {
        accept = true;
        stagnation_count = 0;
    } else {
        stagnation_count++;
        std::uniform_real_distribution<double> dist_prob(0.0, 1.0);
        
        double current_temp = temp * get_temp_scale(effective_mode);
        double p = std::exp((static_cast<double>(new_score) - old_score) / current_temp);
        if (dist_prob(rng) < p) accept = true;
    }

    if (accept) {
        current_richness = new_score;
        update_bests();
    } else {
        for (const auto& [r, c, old_val] : backup) {
            update_fast_board(current_fb, r, c, current_board[r][c], old_val);
            current_board[r][c] = old_val;
        }
    }
}

void SAIsland::update_bests() {
    long long score = current_richness;
    long long best = local_best_richness;

    if (score > best) {
        /*
        AllScores all = compute_all_scores(current_fb);

        local_best_param = all.param;
        local_best_freq  = all.freq;
        local_best_sum   = all.sum;
        local_best_hybrid_val = all.hybrid;
        local_best_hybrid_sqrt_val = all.hybrid_sqrt;
        */
        local_best_richness = score;
        local_best_board = current_board;
        
        save_richness_result(lineage_id, local_best_richness, local_best_board);

        if (get_best_score() > global_best_score.load()) {
            std::lock_guard<std::mutex> lock(console_mtx);
            if (get_best_score() > global_best_score.load()) {
                global_best_score = get_best_score();
                global_best_board = local_best_board;
                global_best_lineage_id = lineage_id;
                save_richness_result(lineage_id, local_best_richness, local_best_board);
            }
        }
    }
}

void SAIsland::handle_periodic_tasks() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_db_save).count() >= Config4D::DB_SAVE_INTERVAL_SEC) {
        save_richness_result(lineage_id, local_best_richness, local_best_board);
        last_db_save = now;
    }

    if (stagnation_count >= Config4D::RESEED_STAGNATION_THRESHOLD) handle_reseed();
    handle_reheat();
}

void SAIsland::handle_reseed() {
    last_db_save = std::chrono::steady_clock::now();
    std::pair<int, std::array<std::array<int, 14>, 8>> next_seed;
    bool pool_has_data = false;
    
    {
        std::lock_guard<std::mutex> lock_pool(pool_mtx);
        if (!gene_pool.empty()) {
            std::uniform_int_distribution<size_t> d(0, gene_pool.size() - 1);
            next_seed = gene_pool[d(rng)]; 
            pool_has_data = true;
        }
    }

    std::uniform_real_distribution<double> dist_prob(0.0, 1.0);
    bool use_pool = false;
    
    if (pool_has_data) {
        if (g_load_mode == 1) {
            use_pool = true;
        } else if (g_load_mode == 2) {
            use_pool = (dist_prob(rng) < 0.5);
        } else { // g_load_mode == 0
            use_pool = false;
        }
    }

    if (use_pool) {
        lineage_id = next_seed.first;
        current_board = next_seed.second;
    } else {
        lineage_id = static_cast<int>((static_cast<long long>(rng()) << 16) | thread_id) & 0x7FFFFFFF;
        for(auto& row : current_board) for(auto& v : row) v = rng() % 10;
    }

    rebuild_fast_board(current_board, current_fb);
    /*
    current_param = get_score_param_bit(current_fb);
    current_freq  = get_frequency_score_bit(current_fb);
    current_sum   = get_sum_score(current_fb);
    current_hybrid_val = get_hybrid_score(current_fb);
    current_hybrid_sqrt_val = get_hybrid_sqrt_score(current_fb);
    */
    current_richness = get_richness_score(current_board);
    
    /*
    local_best_param = current_param;
    local_best_freq  = current_freq;
    local_best_sum   = current_sum;
    local_best_hybrid_val = current_hybrid_val;
    local_best_hybrid_sqrt_val = current_hybrid_sqrt_val;
    */
    local_best_richness = current_richness;
    local_best_board = current_board;

    /*
    if (mode == 3) {
        hybrid_bests[0] = {local_best_board, local_best_param, local_best_freq, local_best_sum};
        hybrid_bests[1] = {local_best_board, local_best_param, local_best_freq, local_best_sum};
    }
    */

    temp = Config4D::INITIAL_TEMP;
    stagnation_count = 0;
}

void SAIsland::handle_reheat() {
    if (temp < Config4D::REHEAT_TEMP_THRESHOLD) {
        temp = Config4D::INITIAL_TEMP;
    }
}

void SAIsland::update_monitor() {
    if (!g_monitor_ptr || (total_iter % 100 != 0)) return;

    g_monitor_ptr->num_threads = std::thread::hardware_concurrency();
    g_monitor_ptr->global_best_score = global_best_score.load();
    
    auto& ts = g_monitor_ptr->status[thread_id];
    ts.thread_id = thread_id;
    ts.current_score = get_current_score();
    ts.best_score = get_best_score();
    ts.temperature = temp;
    ts.total_iter = total_iter;
    ts.mode = effective_mode;

    for(int r=0; r<8; ++r) {
        for(int c=0; c<14; ++c) ts.current_board[r][c] = current_board[r][c];
    }

    if (g_monitor_ptr->cmd.processed == 0 && g_monitor_ptr->cmd.target_thread == thread_id) {
        int c_type = g_monitor_ptr->cmd.command_type;
        if (c_type == 1) stagnation_count = Config4D::RESEED_STAGNATION_THRESHOLD + 100;
        else if (c_type == 2) {
            int idx = g_monitor_ptr->cmd.param_idx;
            if (idx >= 0 && idx < 16) ts.params[idx] = g_monitor_ptr->cmd.new_value;
        }
        else if (c_type == 3) temp = g_monitor_ptr->cmd.new_value;
        g_monitor_ptr->cmd.processed = 1;
    }
}
