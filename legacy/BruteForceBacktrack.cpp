#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <chrono>
#include <utility>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>

#include "common.h"
#include "globals.h"     
#include "db_manager.h"  
#include "logic.h"       

static std::recursive_mutex tool_console_mtx;

bool verify_range(const FastBoard& fb, int target) {
    if (target <= 0) return true;
    for (int i = 1; i <= target; ++i) {
        if (i >= MAX_PRECOMPUTE) break;
        const auto& data = DIGIT_TABLE[i];
        if (!can_make_bitboard(fb, data.digits, data.len)) return false;
    }
    return true;
}

bool backtrack(
    std::array<std::array<int, 14>, 8>& board, 
    FastBoard& fb, 
    int current_score, 
    int depth, 
    int max_depth, 
    int& seed_best, 
    std::array<std::array<int, 14>, 8>& best_board_out,
    int lid
) {
    if (current_score >= 9000) return true;
    if (depth > max_depth) return false;

    if (current_score > seed_best) {
        seed_best = current_score;
        best_board_out = board; 
        
        std::lock_guard<std::recursive_mutex> clock(tool_console_mtx);
        std::cout << "  [TID " << std::this_thread::get_id() << "] Found " << seed_best << " (Seed " << lid << ")" << std::endl;
    }

    int target = current_score + 1;
    if (target >= MAX_PRECOMPUTE) return true;
    const auto& data = DIGIT_TABLE[target];

    if (can_make_bitboard(fb, data.digits, data.len)) {
        return backtrack(board, fb, target, depth, max_depth, seed_best, best_board_out, lid);
    }

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 14; ++c) {
            int old_val = board[r][c];
            for (int v = 0; v <= 9; ++v) {
                if (v == old_val) continue;

                board[r][c] = v;
                update_fast_board(fb, r, c, old_val, v);

                if (can_make_bitboard(fb, data.digits, data.len)) {
                    if (verify_range(fb, current_score)) {
                        if (backtrack(board, fb, target, depth + 1, max_depth, seed_best, best_board_out, lid)) return true;
                    }
                }

                board[r][c] = old_val;
                update_fast_board(fb, r, c, v, old_val);
            }
        }
    }

    return false;
}

void worker_thread(int seed_idx, int lid, std::array<std::array<int, 14>, 8> board, int max_depth) {
    try {
        FastBoard fb;
        rebuild_fast_board(board, fb);
        
        int initial_score = get_score_param_bit(fb);
        int seed_best = initial_score;
        std::array<std::array<int, 14>, 8> best_found_board = board;

        {
            std::lock_guard<std::recursive_mutex> lock(tool_console_mtx);
            std::cout << "[TID " << std::this_thread::get_id() << "] Starting Seed " << seed_idx << std::endl;
        }

        bool hit_goal = backtrack(board, fb, initial_score, 1, max_depth, seed_best, best_found_board, lid);

        // --- THE THREAD HAS REACHED ITS END ---
        if (seed_best > initial_score) {
            // Re-calculate frequency for the best board found
            FastBoard final_fb;
            rebuild_fast_board(best_found_board, final_fb);
            int final_freq = get_frequency_score_bit(final_fb);
            int final_sum = get_sum_score(final_fb);
            
            // SAVE ONLY NOW (at the end of the thread)
            save_lineage_result(lid | 0x70000000, 0, seed_best, final_freq, final_sum, best_found_board);

            std::lock_guard<std::recursive_mutex> lock(tool_console_mtx);
            std::cout << "[TID " << std::this_thread::get_id() << "] Thread Complete. Saved best board (" << seed_best << ") to DB." << std::endl;
        } else {
            std::lock_guard<std::recursive_mutex> lock(tool_console_mtx);
            std::cout << "[TID " << std::this_thread::get_id() << "] Thread Complete. No improvement found." << std::endl;
        }
    } catch (...) {}
}

int main(int argc, char* argv[]) {
    init_db();
    std::cout << "=== Flexible Multi-Seed Backtracker ===" << std::endl;

    int max_depth = 12; 
    int num_seeds = std::thread::hardware_concurrency();
    int num_threads = std::thread::hardware_concurrency();
    
    if (argc > 1) { try { max_depth = std::stoi(argv[1]); } catch (...) {} }
    if (argc > 2) { try { num_seeds = std::stoi(argv[2]); } catch (...) {} }
    if (argc > 3) { try { num_threads = std::stoi(argv[3]); } catch (...) {} }

    auto seeds = load_top_freq_elites(num_seeds);
    if (seeds.empty()) {
        std::cout << "No seeds found in DB." << std::endl;
        return 1;
    }

    int actual_seeds = (int)seeds.size();
    std::cout << "Loading " << actual_seeds << " seeds into " << num_threads << " threads (Depth: " << max_depth << ")..." << std::endl;

    std::atomic<int> next_seed_idx(0);
    std::vector<std::thread> workers;

    for (int t = 0; t < num_threads; ++t) {
        workers.emplace_back([&]() {
            while (true) {
                int i = next_seed_idx.fetch_add(1);
                if (i >= actual_seeds) break;
                
                worker_thread(i + 1, seeds[i].first, seeds[i].second, max_depth);
            }
        });
    }

    for (auto& t : workers) if (t.joinable()) t.join();

    std::cout << "All seeds processed." << std::endl;
    return 0;
}