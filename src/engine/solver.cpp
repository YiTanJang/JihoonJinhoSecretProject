#include "engine/solver.h"
#include "utils/config.h"
#include "utils/globals.h"
#include "utils/physics_lookup.h"
#include "data/db_manager.h"
#include "engine/mutations.h"
#include "core/scoring.h"
#include "core/basis.h"
#include "core/board.h"
#include "legacy/common.h"
#include <cmath>
#include <algorithm>
#include <print>
#include <fstream>
#include <atomic>

std::atomic<bool> g_terminate_all{false};

SAIsland4D::SAIsland4D(int id, int mode)
    : thread_id(id), solver_mode(mode), lineage_id(0), current_board{},
      local_best_board{}, current_basis_count(0), total_iter(0), stagnation_count(0), cycle_count(0),
      accepted_bad_in_physics_window(0), total_bad_in_physics_window(0), accepted_total_in_physics_window(0), physics_window_iter(0),
      sum_bad_proposed_delta(0.0), count_bad_proposed(0), sum_bad_accepted_delta(0.0), count_bad_accepted(0),
      macro_enabled(true)
{
    last_dump_time = std::chrono::steady_clock::now();
    last_print_time = std::chrono::steady_clock::now();
    physics_buffer.reserve(1000);
    rng.seed(std::random_device{}() ^ (thread_id << 16));

    // 1. Initialize Mutation Operators Container
    mutation_operators = {
        // Micro-Moves
        [this]() { return apply_distance_1_swap(current_board, rng); },
        [this]() { return apply_distance_2_swap(current_board, rng); },
        [this]() { return apply_random_global_swap(current_board, rng); },
        [this]() { return apply_random_cell_mutation(current_board, rng); },
        
        // Meso-Moves
        [this]() { return apply_local_domino_swap(current_board, rng); },
        [this]() { return apply_global_domino_swap(current_board, rng); },
        [this]() { return apply_triangle_rotate(current_board, rng); },
        [this]() { return apply_straight_slide(current_board, rng); },
        [this]() { return apply_worm_slide(current_board, rng); },
        [this]() { return apply_variable_block_rotate(current_board, rng); }, 
        [this]() { return apply_heatmap_swap(current_board, rng); },
        [this]() { return apply_heatmap_domino_swap(current_board, rng); },
        [this]() { return apply_heatmap_mutate(current_board, rng); },
        
        // Macro-Moves
        [this]() { return apply_variable_block_swap(current_board, rng); },
        [this]() { return apply_variable_block_flip(current_board, rng); }
    };

    // 2. Initialize Weights & ALNS State (Updated for 15 operators)
    action_weights = { 10.0, 10.0, 1.0, 1.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 2.0, 2.0, 2.0, 3.0, 3.0 };
    double total_w = 0.0;
    for (double w : action_weights) total_w += w;
    for (double& w : action_weights) w /= total_w;
    
    iter_in_segment = 0;
    segment_scores.assign(mutation_operators.size(), 0.0);
    segment_counts.assign(mutation_operators.size(), 0);
    
    action_total_bad_counts.assign(mutation_operators.size(), 0);
    action_accepted_bad_counts.assign(mutation_operators.size(), 0);
    action_energy_deltas.assign(mutation_operators.size(), 0.0);
    action_energy_sq_deltas.assign(mutation_operators.size(), 0.0);
    last_action_idx = 0;

    // 3. Initialize Lineage (Setup Board & Temp)
    initialize_lineage(true);
}

double SAIsland4D::calculate_initial_temperature() {
    double t = PhysicsLookup::get_temp_for_bad_ar(0.80);
    // if (t > 8.0 * Config4D::CRITICAL_TEMP) t = 8.0 * Config4D::CRITICAL_TEMP; // No clamping needed? 
    // Usually lookup table max AR is 0.41, so 0.80 is way off chart (will return max temp ~300)
    // Actually the new table goes up to 0.80 BadAR = 307.06 Temp. So it works.
    return t;
}

void SAIsland4D::run() {
    if (solver_mode == 2) {
        run_lns_sa();
    } else {
        run_standard_sa();
    }
}

void SAIsland4D::run_standard_sa() {
    // Mode 0: Pure SA (1 cycle), Mode 1: Reheat SA
    while (!g_terminate_all) {
        int max_cycles = (solver_mode == 0 || solver_mode == 3) ? 1 : 4;
        while (cycle_count < max_cycles && !g_terminate_all) {
            // Cycle Init
            if (cycle_count > 0) {
                if (local_best_score > last_cycle_best_score) {
                    consecutive_fails = 0;
                    if (solver_mode == 1) {
                         // Restart sequence on improvement
                         cycle_count = 0;
                         last_cycle_best_score = local_best_score;
                         std::printf("[Thread %d] Improvement! Restarting loop at Cycle 0\n", thread_id);
                         // Don't continue here; let it fall through to the Temp Setup below which handles Cycle 0
                    }
                } else {
                    consecutive_fails++;
                }
                last_cycle_best_score = local_best_score;

                /* 
                if (solver_mode != 1 && consecutive_fails >= 3) {
                    std::printf("[Thread %d] Standard SA Stagnation (3 cycles). Breaking to Reseed...\n", thread_id);
                    break; 
                }
                */

                // Reset to local best board for next cycle (or Cycle 0 restart)
                current_board = local_best_board;
                get_basis_score_combined(current_board, 0.75, 0.25, current_basis_count, current_score);
            }

            // Temperature Setup (Unified for Mode 1)
            if (solver_mode == 1) {
                double target_acc = 0.40;
                if (cycle_count == 0) target_acc = 0.15;
                else if (cycle_count == 1) target_acc = 0.225;
                else if (cycle_count == 2) target_acc = 0.30;
                else if (cycle_count == 3) target_acc = 0.40;

                // Use empirical lookup table for precise reheat temperature
                temp = PhysicsLookup::get_temp_for_bad_ar(target_acc);
                
                std::printf("[Thread %d] Cycle %d Start | TargetAcc: %.3f | Temp: %.2f (Lookup)\n", thread_id, cycle_count, target_acc, temp);
            }

            // Cooling Loop
            double cycle_initial_temp = temp;
            long long dynamic_cooling_iter = 0;
            double slow_cooling_rate = std::pow(Config4D::COOLING_RATE, 0.125);
            cycle_stagnation_count = 0;
            long long iter_in_cycle = 0;
            bool hard_reset_needed = false;

            while (true) {
                ++total_iter; ++physics_window_iter; ++iter_in_segment; ++iter_in_cycle;
                
                double normal_iter = static_cast<double>(iter_in_cycle - dynamic_cooling_iter);
                temp = cycle_initial_temp * std::pow(Config4D::COOLING_RATE, normal_iter) * std::pow(slow_cooling_rate, static_cast<double>(dynamic_cooling_iter));
                if (temp < Config4D::MIN_TEMP) temp = Config4D::MIN_TEMP;

                bool in_crit1 = (temp >= 0.015625 * Config4D::CRITICAL_TEMP && temp <= 2.0 * Config4D::CRITICAL_TEMP);
                // bool in_crit2 = (temp >= 0.5 * Config4D::SECOND_CRITICAL_TEMP && temp <= 4.0 * Config4D::SECOND_CRITICAL_TEMP);
                if (in_crit1 && solver_mode != 3) dynamic_cooling_iter++;

                apply_mutation();
                update_monitor();
                if (iter_in_segment >= 100) update_weights();

                // Logging
                if (physics_window_iter >= 3000) {
                    double overall_ar = (physics_window_iter > 0) ? (double)accepted_total_in_physics_window / physics_window_iter : 0.0;
                    double bad_ar = (total_bad_in_physics_window > 0) ? (double)accepted_bad_in_physics_window / total_bad_in_physics_window : 0.0;
                    
                    std::vector<double> per_action_ars(mutation_operators.size(), 0.0);
                    std::vector<double> per_action_avg_deltas(mutation_operators.size(), 0.0);
                    double sum_e = 0, sum_e2 = 0; int total_bad = 0;
                    for(size_t i=0; i<mutation_operators.size(); ++i) {
                        sum_e += action_energy_deltas[i]; sum_e2 += action_energy_sq_deltas[i]; total_bad += action_total_bad_counts[i];
                        if (action_total_bad_counts[i] > 0) {
                            per_action_ars[i] = (double)action_accepted_bad_counts[i] / action_total_bad_counts[i];
                            per_action_avg_deltas[i] = action_energy_deltas[i] / action_total_bad_counts[i];
                        }
                    }
                    double energy_stddev = 0.0;
                    if (total_bad > 1) {
                        double mean = sum_e / total_bad;
                        double var = (sum_e2 / total_bad) - (mean * mean);
                        energy_stddev = std::sqrt(std::max(0.0, var));
                    }
                    PhysicsLogRecord rec;
                    rec.thread_id = thread_id; rec.lineage_id = lineage_id; rec.cycle = cycle_count; rec.iteration = total_iter;
                    rec.temp = temp; rec.overall_ar = overall_ar; rec.bad_ar = bad_ar; rec.energy_stddev = energy_stddev; rec.score = current_score;
                    rec.avg_bad_prop_delta = (count_bad_proposed > 0) ? sum_bad_proposed_delta / count_bad_proposed : 0.0;
                    rec.avg_bad_acc_delta = (count_bad_accepted > 0) ? sum_bad_accepted_delta / count_bad_accepted : 0.0;
                    sum_bad_proposed_delta = 0.0; count_bad_proposed = 0; sum_bad_accepted_delta = 0.0; count_bad_accepted = 0;
                    for(int i=0; i<24; ++i) {
                        if (i < (int)mutation_operators.size()) { rec.probs[i] = action_weights[i]; rec.ars[i] = per_action_ars[i]; rec.deltas[i] = per_action_avg_deltas[i]; }
                        else { rec.probs[i]=0; rec.ars[i]=0; rec.deltas[i]=0; }
                    }
                    physics_buffer.push_back(rec);
                    auto now = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::minutes>(now - last_dump_time).count() >= 15) {
                        save_physics_log_batch(physics_buffer); physics_buffer.clear(); last_dump_time = now;
                    }
                    accepted_bad_in_physics_window = 0; total_bad_in_physics_window = 0; accepted_total_in_physics_window = 0; physics_window_iter = 0;
                     for(size_t i=0; i<mutation_operators.size(); ++i) { action_total_bad_counts[i]=0; action_accepted_bad_counts[i]=0; action_energy_deltas[i]=0; action_energy_sq_deltas[i]=0; }
                }

                if (current_basis_count >= get_basis_size()) break;
                if (stagnation_count >= Config4D::RESEED_STAGNATION_THRESHOLD) { hard_reset_needed = true; break; }
                if (g_terminate_all || cycle_stagnation_count >= 10000000 || temp < Config4D::MIN_TEMP) break;
            }

            if (hard_reset_needed) break;
            save_best_board(lineage_id, base_initial_temp, local_best_score, local_best_board);
            save_best_board(lineage_id, base_initial_temp, current_score, current_board);
            if (current_basis_count >= get_basis_size()) break;
            cycle_count++;
        }
        if (g_terminate_all) break;
        initialize_lineage(false);
        cycle_count = 0;
    }
}

void SAIsland4D::run_lns_sa() {
    // Mode 2: LNS (Strategic 6x6 + Seq 5x5 Sliding Window)
    std::printf("[Thread %d] Starting LNS (Strategic 6x6 + Seq 5x5)...\n", thread_id);

    while (!g_terminate_all) {
        bool improved_in_pass = false;
        double start_pass_score = local_best_score;

        // --- Phase 1: Strategic 6x6 Optimization ---
        struct HoleCandidate { int r, c; double loss; };
        std::vector<HoleCandidate> candidates;
        candidates.reserve(27);

        // 1. Evaluate 27 positions (6x6)
        for (int r = 0; r <= 2; ++r) {
            for (int c = 0; c <= 8; ++c) {
                // Temporarily punch hole (set to -1)
                std::vector<std::tuple<int, int, int>> backup;
                for(int rr=r; rr<r+6; ++rr) {
                    for(int cc=c; cc<c+6; ++cc) {
                        backup.push_back({rr, cc, current_board[rr][cc]});
                        current_board[rr][cc] = -1;
                    }
                }
                
                int dummy_cnt; double temp_score;
                get_basis_score_combined(current_board, 0.75, 0.25, dummy_cnt, temp_score);
                double loss = local_best_score - temp_score;
                
                candidates.push_back({r, c, loss});

                // Restore
                for(const auto& b : backup) current_board[std::get<0>(b)][std::get<1>(b)] = std::get<2>(b);
            }
        }
        current_score = local_best_score; // Ensure clean state

        // 2. Select Top 3 (Least Loss)
        std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b){
            return a.loss < b.loss;
        });

        for (int i = 0; i < 3 && i < (int)candidates.size(); ++i) {
             if (g_terminate_all) break;
             int r_start = candidates[i].r;
             int c_start = candidates[i].c;
             
             // Run SA on this hole (1M iters, T=5->0.1)
             // Rate approx 0.999996 for 1M iters
             
             current_board = local_best_board;
             current_score = local_best_score;
             
             // Reset ALNS weights for clean start on this hole
             // Default sensible weights: { 10, 10, 1, 1, 5, 5, 5, 5, 5, 5, 2, 2, 2, 3, 3 }
             action_weights = { 10.0, 10.0, 1.0, 1.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 2.0, 2.0, 2.0, 3.0, 3.0 };
             double total_w = 0.0; for (double w : action_weights) total_w += w;
             for (double& w : action_weights) w /= total_w;

             std::fill(segment_scores.begin(), segment_scores.end(), 0.0);
             std::fill(segment_counts.begin(), segment_counts.end(), 0);
             iter_in_segment = 0;

             // Punch hole (Randomize)
             apply_box_perturbation(r_start, c_start, 6, 6);
             
             double temp_sa = 2.0;
             double min_temp_sa = 0.1;
             double cooling_rate_sa = std::pow(min_temp_sa / temp_sa, 1.0 / 2000000.0);
             int max_iters_sa = 2000000;
             
             for(int iter=0; iter<max_iters_sa; ++iter) {
                 total_iter++; physics_window_iter++; iter_in_segment++;
                 temp_sa *= cooling_rate_sa;
                 if (temp_sa < min_temp_sa) temp_sa = min_temp_sa;
                 temp = temp_sa; // Sync for monitor

                 // Global mutation as requested
                 apply_mutation();
                 
                 update_monitor();
                 if (iter_in_segment >= 100) update_weights();

                 if (current_basis_count >= get_basis_size()) break;
                 if (g_terminate_all) break;
             }
             
             if (local_best_score >= start_pass_score) {
                 if (local_best_score > start_pass_score) improved_in_pass = true;
                 start_pass_score = local_best_score;
             }
             if (current_basis_count >= get_basis_size()) break;
        }
        
        if (current_basis_count >= get_basis_size()) break;

        // --- Phase 2: Sliding Window (5x5) ---
        // Sliding Window: 40 positions (Rows 0-3, Cols 0-9)
        std::vector<std::pair<int, int>> windows;
        for (int r = 0; r <= 3; ++r) {
            for (int c = 0; c <= 9; ++c) {
                windows.push_back({r, c});
            }
        }
        
        // Construct Full Path: Forward then Reverse
        std::vector<std::pair<int, int>> full_path = windows; // Forward
        full_path.insert(full_path.end(), windows.rbegin(), windows.rend()); // Reverse

        for (const auto& win : full_path) {
            if (g_terminate_all) break;
            
            // Try 3 cycles per window
            for (int cycle = 0; cycle < 3; ++cycle) {
                // 1. Reset to Best Known (Greedy Base)
                current_board = local_best_board;
                current_score = local_best_score;

                // Reset ALNS weights for clean start on this window
                // Default sensible weights: { 10, 10, 1, 1, 5, 5, 5, 5, 5, 5, 2, 2, 2, 3, 3 }
                action_weights = { 10.0, 10.0, 1.0, 1.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 2.0, 2.0, 2.0, 3.0, 3.0 };
                double total_w = 0.0; for (double w : action_weights) total_w += w;
                for (double& w : action_weights) w /= total_w;

                std::fill(segment_scores.begin(), segment_scores.end(), 0.0);
                std::fill(segment_counts.begin(), segment_counts.end(), 0);
                iter_in_segment = 0;

                // 2. Perturb 4x4 Box
                apply_box_perturbation(win.first, win.second, 5, 5);

                // 3. Repair (Geometric Cooling 2.0 -> 0.1)
                temp = 2.0;
                double min_temp = 0.1;
                int max_cooling_iters = 2000000;
                // Recalculate cooling rate for new duration
                double cooling_rate = std::pow(min_temp / temp, 1.0 / max_cooling_iters);
                
                int lns_stagnation_threshold = 15000;
                int lns_cycle_stagnation = 0;
                double best_in_cycle = current_score;
                
                for (int i = 0; i < max_cooling_iters; ++i) {
                    total_iter++; physics_window_iter++; iter_in_segment++;
                    
                    temp *= cooling_rate;
                    if (temp < min_temp) temp = min_temp;

                    // Global Mutation as requested
                    apply_mutation();
                    
                    if (current_score > best_in_cycle) {
                        best_in_cycle = current_score;
                        lns_cycle_stagnation = 0;
                    } else {
                        lns_cycle_stagnation++;
                    }

                    update_monitor();
                    if (iter_in_segment >= 100) update_weights();
                    
                    if (current_basis_count >= get_basis_size()) break;
                    if (g_terminate_all) break;
                    if (lns_cycle_stagnation >= lns_stagnation_threshold) break;
                }

                if (current_basis_count >= get_basis_size()) break;
                if (g_terminate_all) break;
            }

            // 4. Check Improvement (>= allows side-stepping on plateaus)
            if (local_best_score >= start_pass_score) {
                 if (local_best_score > start_pass_score) {
                    std::printf("[Thread %d] LNS Improvement! %.1f -> %.1f (Win %d,%d)\n", 
                        thread_id, start_pass_score, local_best_score, win.first, win.second);
                    save_best_board(lineage_id, base_initial_temp, local_best_score, local_best_board);
                    improved_in_pass = true;
                }
                start_pass_score = local_best_score; 
            }

            if (current_basis_count >= get_basis_size()) break;
            
            // Clean up physics logs periodically
            if (physics_window_iter >= 3000) {
                 accepted_bad_in_physics_window = 0; total_bad_in_physics_window = 0; accepted_total_in_physics_window = 0; physics_window_iter = 0;
                 for(size_t i=0; i<mutation_operators.size(); ++i) { action_total_bad_counts[i]=0; action_accepted_bad_counts[i]=0; action_energy_deltas[i]=0; action_energy_sq_deltas[i]=0; }
            }
        }

        if (g_terminate_all) break;
        if (current_basis_count >= get_basis_size()) break;

        // Reseed if no improvement in full pass
        if (!improved_in_pass) {
            std::printf("[Thread %d] LNS Stagnation (Full Pass). Reseeding...\n", thread_id);
            initialize_lineage(false);
        }
    }
}

double SAIsland4D::calculate_score(const std::array<std::array<int, 14>, 8>& board) const {
    // Experimental Twin-Reward Scoring (Floating Point)
    return get_basis_score_with_twins(board, 0.75, 0.25);
}

void SAIsland4D::apply_mutation() {
    std::vector<std::tuple<int, int, int>> backup;

    // Adaptive selection
    std::discrete_distribution<size_t> dist_op(action_weights.begin(), action_weights.end());
    last_action_idx = (int)dist_op(rng);
    backup = mutation_operators[last_action_idx]();

    if (last_action_idx >= 0) {
        segment_counts[last_action_idx]++;
    }

    if (backup.empty()) {
        backup = apply_single_cell_mutation(current_board, rng);
    }

    evaluate_and_accept(backup);
}

void SAIsland4D::evaluate_and_accept(const std::vector<std::tuple<int, int, int>>& backup) {
    bool accept = false;
    int basis_count = 0;
    double new_score = 0;
    get_basis_score_combined(current_board, 0.75, 0.25, basis_count, new_score);
    
    double old_score = current_score; 
    bool is_bad_move = (new_score <= old_score);

    // WIN CONDITION: Only trigger if the actual BASIS COUNT is sufficient
    if (basis_count >= get_basis_size()) {
        current_score = new_score;
        update_bests();
        return; // Win found, stay at this board
    }

    if (new_score > old_score) {
        accept = true;
        stagnation_count = 0;
        cycle_stagnation_count = 0;
    } else {
        stagnation_count++;
        cycle_stagnation_count++;
        if (last_action_idx >= 0) {
            total_bad_in_physics_window++;
            action_total_bad_counts[last_action_idx]++;
        }

        // Track proposed bad moves
        double delta_val = new_score - old_score;
        sum_bad_proposed_delta += delta_val;
        count_bad_proposed++;

        std::uniform_real_distribution<double> dist_prob(0.0, 1.0);
        double current_temp = std::max(Config4D::MIN_TEMP, temp);
        double delta = new_score - old_score;
        double p = std::exp(delta / current_temp);
        if (dist_prob(rng) < p) accept = true;
    }

    if (accept) {
        accepted_total_in_physics_window++;
        if (last_action_idx >= 0) {
            if (is_bad_move) {
                accepted_bad_in_physics_window++;
                action_accepted_bad_counts[last_action_idx]++;
                double delta = new_score - old_score;
                
                // Track accepted bad moves
                sum_bad_accepted_delta += delta;
                count_bad_accepted++;

                action_energy_deltas[last_action_idx] += delta;
                action_energy_sq_deltas[last_action_idx] += (delta * delta);
            }
        }

        if (last_action_idx >= 0) {
            // ALNS Scoring (Sigma1=50, Sigma2=20, Sigma3=5)
            if (new_score > local_best_score) {
                 segment_scores[last_action_idx] += 50.0;
            } else if (new_score > old_score) {
                segment_scores[last_action_idx] += 20.0;
            } else {
                segment_scores[last_action_idx] += 5.0;
            }
        }
        current_score = new_score;
        current_basis_count = basis_count; // Update basis count
        update_bests();
    } else {
        // Reverse iteration to correctly handle overwrites
        for (auto it = backup.rbegin(); it != backup.rend(); ++it) {
            const auto& [r, c, old_val] = *it;
            current_board[r][c] = old_val;
        }
    }
}

void SAIsland4D::update_bests() {
    if (current_score >= local_best_score) {
        bool improved = (current_score > local_best_score);
        local_best_score = current_score;
        local_best_board = current_board;
        
        if (improved) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_print_time).count() >= 1) {
                std::println("[4D-Thread {}] New Best: {:.1f}", thread_id, local_best_score);
                last_print_time = now;
            }
        }
    }
}

void SAIsland4D::initialize_lineage(bool is_start) {
    if (!is_start) {
        save_physics_log_batch(physics_buffer);
        physics_buffer.clear();
    }

    lineage_id = static_cast<int>((static_cast<long long>(rng()) << 16) | thread_id) & 0x7FFFFFFF;
    bool is_elite = false;

    if (solver_mode == 1 || solver_mode == 2) {
        // Mode 1: Elite Reheat, Mode 2: LNS (also needs Elite seed)
        if (!g_loaded_elites.empty()) {
            int idx = rng() % g_loaded_elites.size();
            current_board = g_loaded_elites[idx].board;
            lineage_id = g_loaded_elites[idx].lineage_id;
            base_initial_temp = g_loaded_elites[idx].initial_temp;
            is_elite = true;
        } else {
            // Fallback if pool empty
            for(auto& row : current_board) for(auto& v : row) v = rng() % 10;
        }
    } else {
        // Mode 0: Pure SA (Random Start)
        for(auto& row : current_board) for(auto& v : row) v = rng() % 10;
        lineage_id = (static_cast<int>(rng()) & 0x7FFFFFFF);
        is_elite = false;
    }

    // Common Calibration Logic
    get_basis_score_combined(current_board, 0.75, 0.25, current_basis_count, current_score);
    local_best_score = current_score;
    local_best_board = current_board;

    if (is_elite) {
         // Calibration moved to start of run_standard_sa loop for unified logic.
         // Just set a placeholder temp here.
         temp = Config4D::CRITICAL_TEMP; 

         if (is_start) {
             std::printf("[Thread %d] Init Elite. Fingerprint T: %.2f | Deferring Reheat Calc\n", thread_id, base_initial_temp);
         } else {
             std::printf("[Thread %d] Reseed LOADED ELITE. Fingerprint T: %.2f | Deferring Reheat Calc\n", 
                thread_id, base_initial_temp);
         }
    } else {
        base_initial_temp = calculate_initial_temperature();
        temp = base_initial_temp;
        
        if (is_start) {
            std::printf("[Thread %d] Init Random board. T_base: %.2f\n", thread_id, base_initial_temp);
        } else {
            std::printf("[Thread %d] Reseed RANDOM board. T_base: %.2f\n", thread_id, base_initial_temp);
        }
    }
    
    last_cycle_best_score = local_best_score;
    consecutive_fails = 0;
    cycle_stagnation_count = 0;
    stagnation_count = 0;
}
void SAIsland4D::apply_heat_guided_perturbation(const std::vector<std::pair<int, int>>& targets) {
    std::uniform_int_distribution<int> dist(0, 9);
    
    std::printf("[Thread %d] Starting Multi-Stage Heat Perturbation (Roulette Selection)...\n", thread_id);

    struct Cell { int r, c; };
    std::vector<Cell> repair_targets;
    
    // Process each target specification {num_centers, radius}
    for (const auto& target : targets) {
        int num_centers = target.first;
        int patch_radius = target.second;

        for (int step = 0; step < num_centers; ++step) {
            int dummy_cnt;
            double base_score;
            get_basis_score_combined(current_board, 0.75, 0.25, dummy_cnt, base_score);
            
            struct Candidate {
                int r, c;
                double drop;
            };
            std::vector<Candidate> candidates;
            candidates.reserve(112);
            double total_weight = 0;
            double max_drop = -1e18;
            int best_r = -1, best_c = -1; // Fallback

            for (int r = 0; r < 8; ++r) {
                for (int c = 0; c < 14; ++c) {
                    int original = current_board[r][c];
                    if (original == -1) continue; 

                    current_board[r][c] = -1; 
                    int cur_cnt; double new_score;
                    get_basis_score_combined(current_board, 0.75, 0.25, cur_cnt, new_score);
                    double drop = base_score - new_score;
                    
                    // Fallback tracking
                    if (drop > max_drop) {
                        max_drop = drop;
                        best_r = r;
                        best_c = c;
                    }

                    // Roulette candidates (only positive drops contribute to weight)
                    if (drop > 0) {
                        candidates.push_back({r, c, drop});
                        total_weight += drop;
                    }
                    
                    current_board[r][c] = original; 
                }
            }

            int selected_r = -1;
            int selected_c = -1;

            if (total_weight > 0 && !candidates.empty()) {
                // Roulette Wheel Selection
                std::uniform_real_distribution<double> weight_dist(0.0, total_weight);
                double random_val = weight_dist(rng);
                double current_sum = 0;
                
                for (const auto& cand : candidates) {
                    current_sum += cand.drop;
                    if (current_sum > random_val) {
                        selected_r = cand.r;
                        selected_c = cand.c;
                        break;
                    }
                }
            } else {
                // Fallback to greedy if no positive drops (or purely flat/negative landscape)
                selected_r = best_r;
                selected_c = best_c;
            }

            if (selected_r != -1) {
                for (int dr = -patch_radius; dr <= patch_radius; ++dr) {
                    for (int dc = -patch_radius; dc <= patch_radius; ++dc) {
                        int nr = selected_r + dr;
                        int nc = selected_c + dc;
                        if (nr >= 0 && nr < 8 && nc >= 0 && nc < 14) {
                            bool exists = false;
                            for(const auto& cell : repair_targets) if(cell.r == nr && cell.c == nc) { exists = true; break; }
                            
                            if (!exists) {
                                repair_targets.push_back({nr, nc});
                                current_board[nr][nc] = -1; 
                            }
                        }
                    }
                }
            }
        }
    }

    // Randomized Greedy Repair
    std::shuffle(repair_targets.begin(), repair_targets.end(), rng);

    for (const auto& cell : repair_targets) {
        int r = cell.r;
        int c = cell.c;
        
        std::vector<std::pair<double, int>> candidates; 
        candidates.reserve(10);

        for (int d = 0; d < 10; ++d) {
            current_board[r][c] = d;
            int cur_cnt; double s;
            get_basis_score_combined(current_board, 0.75, 0.25, cur_cnt, s);
            candidates.push_back({s, d});
        }

        std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b){
            return a.first > b.first;
        });

        int rcl_size = std::min((int)candidates.size(), 3);
        std::uniform_int_distribution<int> rcl_dist(0, rcl_size - 1);
        int chosen_idx = rcl_dist(rng);
        
        current_board[r][c] = candidates[chosen_idx].second;
    }

    get_basis_score_combined(current_board, 0.75, 0.25, current_basis_count, current_score);
}

void SAIsland4D::apply_box_perturbation(int r_start, int c_start, int height, int width) {
    // Randomize the box area
    for (int r = r_start; r < r_start + height && r < 8; ++r) {
        for (int c = c_start; c < c_start + width && c < 14; ++c) {
             current_board[r][c] = rng() % 10;
        }
    }
    get_basis_score_combined(current_board, 0.75, 0.25, current_basis_count, current_score);
}

void SAIsland4D::apply_lns_repair_mutation(int r_start, int c_start, int height, int width) {
    std::vector<std::tuple<int, int, int>> backup;
    
    // Select between 2 mutation types
    // 0: Random mutation in 4x4 window
    // 1: Dist 1 Swap from cell in 4x4 window (to neighbor)
    
    int type = std::uniform_int_distribution<int>(0, 1)(rng);
    
    if (type == 0) {
        // Random Mutation
        int r = std::uniform_int_distribution<int>(r_start, std::min(r_start + height, 8) - 1)(rng);
        int c = std::uniform_int_distribution<int>(c_start, std::min(c_start + width, 14) - 1)(rng);
        
        int old_val = current_board[r][c];
        int new_val = std::uniform_int_distribution<int>(0, 9)(rng);
        
        if (new_val != old_val) {
            current_board[r][c] = new_val;
            backup.push_back({r, c, old_val});
        }
    } else {
        // Dist 1 Swap
        int r1 = std::uniform_int_distribution<int>(r_start, std::min(r_start + height, 8) - 1)(rng);
        int c1 = std::uniform_int_distribution<int>(c_start, std::min(c_start + width, 14) - 1)(rng);
        
        // Pick random neighbor (8 directions)
        int dr[] = {-1, -1, -1, 0, 0, 1, 1, 1};
        int dc[] = {-1, 0, 1, -1, 1, -1, 0, 1};
        int dir = std::uniform_int_distribution<int>(0, 7)(rng);
        
        int r2 = r1 + dr[dir];
        int c2 = c1 + dc[dir];
        
        // Neighbor must be within the same patch
        if (r2 >= r_start && r2 < r_start + height && r2 < 8 &&
            c2 >= c_start && c2 < c_start + width && c2 < 14) {
            int v1 = current_board[r1][c1];
            int v2 = current_board[r2][c2];
            
            if (v1 != v2) {
                current_board[r1][c1] = v2;
                current_board[r2][c2] = v1;
                backup.push_back({r1, c1, v1});
                backup.push_back({r2, c2, v2});
            }
        }
    }

    if (!backup.empty()) {
        evaluate_and_accept(backup);
    }
}

void SAIsland4D::run_healing_burst(int iterations, double target_ar, bool use_bad_ar, bool skip_calibration) {
    if (!skip_calibration) {
        // Use lookup for instant temp setting
        temp = PhysicsLookup::get_temp_for_bad_ar(target_ar);
    }
    std::printf("[Thread %d] Starting Healing Burst (%d iter, Target %s AR: %.3f)...\n", 
               thread_id, iterations, use_bad_ar ? "Bad" : "Total", target_ar);
    
    int burst_iter = 0;
    const int window_size = 100;
    int local_total = 0;
    int local_accepted = 0;
    
    while (burst_iter < iterations) {
        burst_iter++;
        total_iter++; 
        
        int prev_total = use_bad_ar ? total_bad_in_physics_window : physics_window_iter;
        int prev_acc = use_bad_ar ? accepted_bad_in_physics_window : accepted_total_in_physics_window;
        
        apply_mutation();
        
        // physics_window_iter is always incremented in the loop where apply_mutation is usually called, 
        // but here we are in a burst, so we manually track the window.
        // Actually apply_mutation doesn't increment physics_window_iter, but the main loop does.
        // So we need to be careful about what we compare.
        
        // Simplified tracking for burst:
        local_total++;
        if (use_bad_ar) {
            // Check if a bad move was proposed and accepted
            if (total_bad_in_physics_window > prev_total) {
                if (accepted_bad_in_physics_window > prev_acc) {
                    local_accepted++;
                }
            } else {
                // If no bad move was proposed, don't count this iteration for Bad AR calculation
                local_total--; 
            }
        } else {
            // Total AR
            if (accepted_total_in_physics_window > prev_acc) {
                local_accepted++;
            }
        }
        
        if (burst_iter % window_size == 0) {
            double current_ar = (local_total > 0) ? (double)local_accepted / local_total : 0.0;
            
            if (current_ar < target_ar) temp *= 1.02;
            else temp *= 0.98;
            
            if (temp < Config4D::MIN_TEMP) temp = Config4D::MIN_TEMP;
            if (temp > 8.0 * Config4D::CRITICAL_TEMP) temp = 8.0 * Config4D::CRITICAL_TEMP;
            
            local_total = 0;
            local_accepted = 0;
        }
    }
    std::printf("[Thread %d] Healing Burst Complete. Final Temp: %.2f\n", thread_id, temp);
}

void SAIsland4D::update_monitor() {
    if (!g_monitor_ptr || (total_iter % 100 != 0)) return;

    // g_monitor_ptr->num_threads = 12; // Set by main
    // g_monitor_ptr->global_best_score = ...; // Optional: 4D global best

    auto& ts = g_monitor_ptr->status[thread_id];
    ts.thread_id = thread_id;
    ts.current_score = get_current_score();
    ts.best_score = get_best_score();
    ts.temperature = temp;
    ts.total_iter = total_iter;
    ts.cycle_count = cycle_count;
    
    ts.overall_ar = (physics_window_iter > 0) ? (double)accepted_total_in_physics_window / physics_window_iter : 0.0;
    ts.bad_ar = (total_bad_in_physics_window > 0) ? (double)accepted_bad_in_physics_window / total_bad_in_physics_window : 0.0;
    
    double sum_e = 0, sum_e2 = 0;
    int total_bad = 0;
    for(size_t i=0; i<action_weights.size() && i<24; ++i) {
        ts.action_weights[i] = action_weights[i];
        sum_e += action_energy_deltas[i];
        sum_e2 += action_energy_sq_deltas[i];
        total_bad += action_total_bad_counts[i];
        
        // Calculate instantaneous/windowed stats
        if (action_total_bad_counts[i] > 0) {
            ts.action_ars[i] = (double)action_accepted_bad_counts[i] / action_total_bad_counts[i];
            ts.action_deltas[i] = action_energy_deltas[i] / action_total_bad_counts[i];
        } else {
            ts.action_ars[i] = 0.0;
            ts.action_deltas[i] = 0.0;
        }
    }

    if (total_bad > 1) {
        double mean = sum_e / total_bad;
        double var = (sum_e2 / total_bad) - (mean * mean);
        ts.energy_stddev = std::sqrt(std::max(0.0, var));
    } else {
        ts.energy_stddev = 0.0;
    }

    for(int r=0; r<8; ++r) {
        for(int c=0; c<14; ++c) ts.current_board[r][c] = current_board[r][c];
    }

    if (g_monitor_ptr->cmd.processed == 0 && g_monitor_ptr->cmd.target_thread == thread_id) {
        int c_type = g_monitor_ptr->cmd.command_type;
        if (c_type == 1) stagnation_count = Config4D::RESEED_STAGNATION_THRESHOLD + 100;
        else if (c_type == 2) {
            cycle_stagnation_count = 1000000;
            consecutive_fails = 2;
        }
        else if (c_type == 3) temp = g_monitor_ptr->cmd.new_value;
        g_monitor_ptr->cmd.processed = 1;
    }
}

void SAIsland4D::update_weights() {
    double rho = 0.1; // Reaction factor
    double total_performance = 0.0;
    std::vector<double> performance(mutation_operators.size(), 0.0);

    for (size_t i = 0; i < mutation_operators.size(); ++i) {
        if (segment_counts[i] > 0) {
            performance[i] = segment_scores[i] / segment_counts[i];
        } else {
            performance[i] = 0.0;
        }
        total_performance += performance[i];
    }

    auto get_floor = [](int i) -> double {
        if (i == 0 || i == 1) return 0.001; // dist1, dist2 -> 0.1%
        if (i == 2 || i == 3) return 0.01;  // global_swap, rand_cell_mut -> 1%
        if (i == 4 || i == 5) return 0.04;  // domino_local, domino_global -> 4%
        if (i == 6) return 0.02;            // tri_rotate -> 2%
        if (i >= 7 && i <= 9) return 0.02;  // straight_slide, worm_slide, ring_shift -> 2%
        if (i == 10) return 0.02;           // heatmap_swap -> 2%
        if (i == 11) return 0.02;           // heatmap_domino_swap -> 2%
        if (i == 12) return 0.04;           // heatmap_mutate -> 4%
        if (i == 13 || i == 14) return 0.04; // var_block_swap, var_block_flip -> 4%
        return 0.001;
    };

    double sum_floors = 0.0;
    for (int i = 0; i < (int)mutation_operators.size(); ++i) sum_floors += get_floor(i);
    double remaining_budget = 1.0 - sum_floors;

    if (total_performance > 0) {
        // 1. Calculate raw adaptive shares
        std::vector<double> adaptive_shares(mutation_operators.size(), 0.0);
        for (size_t i = 0; i < mutation_operators.size(); ++i) {
            double normalized_perf = performance[i] / total_performance;
            double f_i = get_floor((int)i);
            double adaptive_share_old = std::max(0.0, action_weights[i] - f_i);
            adaptive_shares[i] = (1.0 - rho) * adaptive_share_old + rho * (remaining_budget * normalized_perf);
        }

        // 2. Multi-pass Capping and Redistribution of SHARES
        for (int pass = 0; pass < 3; ++pass) {
            bool changed = false;
            double fixed_share_sum = 0;
            double fixed_budget_for_shares = 0;
            std::vector<bool> is_fixed(action_weights.size(), false);

            auto check_cap = [&](int idx, double cap) {
                double f_i = get_floor(idx);
                if (f_i + adaptive_shares[idx] > cap) {
                    adaptive_shares[idx] = std::max(0.0, cap - f_i);
                    is_fixed[idx] = true;
                    fixed_share_sum += adaptive_shares[idx];
                    changed = true;
                }
            };

            check_cap(0, 0.40); // dist1
            check_cap(1, 0.40); // dist2
            check_cap(2, 0.03); // global_swap
            check_cap(3, 0.03); // rand_cell
            check_cap(4, 0.15); // domino_local
            check_cap(5, 0.15); // domino_global
            
            // Refined heatmap caps: 10% for swaps, 5% for mutation
            check_cap(10, 0.10); // heatmap_swap
            check_cap(11, 0.10); // heatmap_domino_swap
            check_cap(12, 0.05); // heatmap_mutate

            if (changed) {
                double current_variable_share_sum = 0;
                for(size_t i=0; i<adaptive_shares.size(); ++i) if(!is_fixed[i]) current_variable_share_sum += adaptive_shares[i];
                double target_variable_share_sum = remaining_budget - fixed_share_sum;
                if (current_variable_share_sum > 0 && target_variable_share_sum >= 0) {
                    double scale = target_variable_share_sum / current_variable_share_sum;
                    for (size_t i = 0; i < adaptive_shares.size(); ++i) if (!is_fixed[i]) adaptive_shares[i] *= scale;
                }
            } else break;
        }

        // 3. Final Assembly
        for (size_t i = 0; i < mutation_operators.size(); ++i) {
            if (!macro_enabled && i >= 13) { // Adjusted for 15 ops (13 and 14 are macro)
                action_weights[i] = 0.0;
            } else {
                action_weights[i] = get_floor((int)i) + adaptive_shares[i];
            }
        }
    }

    // Final Re-normalize to exactly 1.0
    double final_sum = 0.0;
    for (double w : action_weights) final_sum += w;
    if (final_sum > 0) {
        for (double& w : action_weights) w /= final_sum;
    }

    // Reset Segment
    std::fill(segment_scores.begin(), segment_scores.end(), 0.0);
    std::fill(segment_counts.begin(), segment_counts.end(), 0);
    iter_in_segment = 0;
}

void SAIsland4D::run_polishing_sa() {
    std::printf("[Thread %d] Switching to Polishing SA (No Macro)...\n", thread_id);
    macro_enabled = false;
    consecutive_fails = 0;
    
    // Reset ALNS weights to clear macro bias immediately
    std::fill(action_weights.begin(), action_weights.end(), 0.0);
    for(int i=0; i<13; ++i) action_weights[i] = 1.0/13.0; 
    
    int polishing_cycles = 0;
    while (polishing_cycles < 100 && !g_terminate_all) {
        if (local_best_score > last_cycle_best_score) consecutive_fails = 0;
        else consecutive_fails++;
        last_cycle_best_score = local_best_score;

        if (consecutive_fails >= 3) {
            std::printf("[Thread %d] Polishing Stagnation. Returning to LNS...\n", thread_id);
            break; 
        }

        double target_acc = 0.20; 
        temp = PhysicsLookup::get_temp_for_bad_ar(target_acc);
        // if (temp < 1.12 * Config4D::CRITICAL_TEMP) temp = 1.12 * Config4D::CRITICAL_TEMP; 
        // if (temp > 1.6 * Config4D::CRITICAL_TEMP) temp = 1.6 * Config4D::CRITICAL_TEMP;
        
        std::printf("[Thread %d] Polishing Cycle %d | Temp: %.2f (Lookup)\n", thread_id, polishing_cycles, temp);

        double cycle_initial_temp = temp;
        long long dynamic_cooling_iter = 0;
        double slow_cooling_rate = std::pow(Config4D::COOLING_RATE, 0.2);
        
        cycle_stagnation_count = 0;
        long long iter_in_cycle = 0;

        while (true) {
            ++total_iter; ++physics_window_iter; ++iter_in_segment; ++iter_in_cycle;
            
            double normal_iter = static_cast<double>(iter_in_cycle - dynamic_cooling_iter);
            temp = cycle_initial_temp * std::pow(Config4D::COOLING_RATE, normal_iter) * std::pow(slow_cooling_rate, static_cast<double>(dynamic_cooling_iter));
            if (temp < Config4D::MIN_TEMP) temp = Config4D::MIN_TEMP;

            bool in_crit1 = (temp >= 0.015625 * Config4D::CRITICAL_TEMP && temp <= 2.0 * Config4D::CRITICAL_TEMP);
            // bool in_crit2 = (temp >= 0.5 * Config4D::SECOND_CRITICAL_TEMP && temp <= 4.0 * Config4D::SECOND_CRITICAL_TEMP);
            if (in_crit1) dynamic_cooling_iter++;

            apply_mutation();
            update_monitor();
            if (iter_in_segment >= 100) update_weights();

            if (physics_window_iter >= 3000) {
                 accepted_bad_in_physics_window = 0; total_bad_in_physics_window = 0; accepted_total_in_physics_window = 0; physics_window_iter = 0;
                 for(size_t i=0; i<mutation_operators.size(); ++i) { action_total_bad_counts[i]=0; action_accepted_bad_counts[i]=0; action_energy_deltas[i]=0; action_energy_sq_deltas[i]=0; }
            }

            if (current_basis_count >= get_basis_size()) break;
            if (cycle_stagnation_count >= 10000000 || temp < Config4D::MIN_TEMP) break;
            if (g_terminate_all) break;
        }
        
        save_best_board(lineage_id, base_initial_temp, local_best_score, local_best_board);
        if (current_basis_count >= get_basis_size()) break;
        polishing_cycles++;
    }
    
    macro_enabled = true; 
}
