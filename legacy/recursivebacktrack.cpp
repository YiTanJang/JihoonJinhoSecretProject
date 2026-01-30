#include <print>
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

#include <cstring>

static std::recursive_mutex tool_console_mtx;

bool backtrack(
    std::array<std::array<int, 14>, 8>& board, 
    FastBoard& fb, 
    int current_score, 
    int depth, 
    int max_depth, 
    int& seed_best, 
    std::array<std::array<int, 14>, 8>& best_board_out,
    int lid,
    int seed_display_idx, 
    long long& nodes 
) {
    nodes++;
    // Safety break
    if (nodes > 2000000000) return false; 
    
    // 1. Find the first failure (Frontier)
    int first_failure = 9001;
    for (int i = current_score + 1; i <= 9000; ++i) {
        if (i >= MAX_PRECOMPUTE) break;
        const auto& data = DIGIT_TABLE[i];
        if (!can_make_bitboard(fb, data.digits, data.len)) {
            first_failure = i;
            break;
        }
    }

    int actual_score = first_failure - 1;
    if (actual_score > seed_best) {
        seed_best = actual_score;
        best_board_out = board; 
        std::lock_guard<std::recursive_mutex> clock(tool_console_mtx);
        std::println("  [Thread {}] New High: {} (Depth: {}/{})", seed_display_idx, seed_best, depth, max_depth);
    }

    if (actual_score >= 9000) return true;
    if (depth >= max_depth) return false;

    // 2. Attempt Repair
    const auto& target_data = DIGIT_TABLE[first_failure];

    struct Candidate {
        int r, c;
        int val;
        bool operator<(const Candidate& other) const {
            if (r != other.r) return r < other.r;
            if (c != other.c) return c < other.c;
            return val < other.val;
        }
        bool operator==(const Candidate& other) const {
            return r == other.r && c == other.c && val == other.val;
        }
    };

    // Helper: Identify all cells used by a specific number's valid path
    auto get_used_cells = [&](const FastBoard& fb, const NumberData& data) -> std::vector<std::pair<int, int>> {
        std::vector<std::pair<int, int>> used;
        int len = data.len;
        if (len == 0) return used;

        // Forward Pass: Record reachable masks
        std::vector<std::array<uint16_t, 8>> steps;
        steps.reserve(len);

        int first_digit = data.digits[len - 1];
        std::array<uint16_t, 8> current_mask = fb.bits[first_digit];
        steps.push_back(current_mask);

        bool possible = true;
        for (int i = len - 2; i >= 0; --i) {
            int next_digit = data.digits[i];
            const auto& next_layer = fb.bits[next_digit];
            std::array<uint16_t, 8> next_mask;
            bool any_bit = false;

            for (int r = 0; r < 8; ++r) {
                uint16_t row_bits = current_mask[r];
                if (row_bits == 0) { // Optimization for empty rows (check neighbors)
                    bool has_neighbor = (r > 0 && current_mask[r-1]) || (r < 7 && current_mask[r+1]);
                    if (!has_neighbor) {
                        next_mask[r] = 0;
                        continue;
                    }
                }
                
                // Dilate
                uint16_t dilated = (row_bits << 1) | (row_bits >> 1);
                if (r > 0) {
                    uint16_t up = current_mask[r-1];
                    dilated |= up | (up << 1) | (up >> 1);
                }
                if (r < 7) {
                    uint16_t down = current_mask[r+1];
                    dilated |= down | (down << 1) | (down >> 1);
                }
                
                uint16_t res = dilated & next_layer[r];
                next_mask[r] = res;
                if (res) any_bit = true;
            }
            if (!any_bit) { possible = false; break; }
            current_mask = next_mask;
            steps.push_back(current_mask);
        }

        if (!possible) return used;

        // Backward Pass
        std::array<uint16_t, 8> valid_mask = steps.back();
        
        auto collect_mask = [&](const std::array<uint16_t, 8>& mask) {
             for(int r=0; r<8; ++r) {
                 uint16_t v = mask[r];
                 if(!v) continue;
                 for(int c=0; c<14; ++c) {
                     if (v & (1<<c)) used.push_back({r, c});
                 }
             }
        };
        collect_mask(valid_mask);

        for (int i = len - 2; i >= 0; --i) {
             std::array<uint16_t, 8> prev_valid;
             for (int r = 0; r < 8; ++r) {
                 uint16_t row_bits = valid_mask[r];
                 uint16_t dilated = (row_bits << 1) | (row_bits >> 1);
                 if (r > 0) {
                     uint16_t up = valid_mask[r-1];
                     dilated |= up | (up << 1) | (up >> 1);
                 }
                 if (r < 7) {
                     uint16_t down = valid_mask[r+1];
                     dilated |= down | (down << 1) | (down >> 1);
                 }
                 prev_valid[r] = steps[i][r] & dilated;
             }
             valid_mask = prev_valid;
             collect_mask(valid_mask);
        }
        return used;
    };

    // Helper to get candidates (Same as before)
    auto get_connection_candidates = [&](const FastBoard& fb, const NumberData& target) {
        std::vector<Candidate> cands;
        cands.reserve(64);
        std::array<uint16_t, 8> temp_mask;
        
        auto add_neighbors = [&](int digit, std::array<uint16_t, 8>& acc_mask) {
             const auto& src = fb.bits[digit];
             for (int r = 0; r < 8; ++r) {
                 uint16_t row_bits = src[r];
                 uint16_t dilated = (row_bits << 1) | (row_bits >> 1);
                 if (r > 0) {
                     uint16_t up = src[r-1];
                     dilated |= up | (up << 1) | (up >> 1);
                 }
                 if (r < 7) {
                     uint16_t down = src[r+1];
                     dilated |= down | (down << 1) | (down >> 1);
                 }
                 acc_mask[r] |= dilated;
             }
        };

        for (int i = 0; i < target.len; ++i) {
            int d_idx = target.len - 1 - i;
            int val = target.digits[d_idx];
            
            std::array<uint16_t, 8> mask_req;
            mask_req.fill(0);
            bool has_req = false;

            if (i > 0) { // Has Prev
                add_neighbors(target.digits[d_idx + 1], mask_req);
                has_req = true;
            }
            if (i < target.len - 1) { // Has Next
                add_neighbors(target.digits[d_idx - 1], mask_req);
                has_req = true;
            }

            if (!has_req) {
                 for(int r=0; r<8; ++r) for(int c=0; c<14; ++c) {
                     if (board[r][c] != val) cands.push_back({r, c, val});
                 }
                 continue;
            }

            for(int r=0; r<8; ++r) {
                if (!mask_req[r]) continue;
                for(int c=0; c<14; ++c) {
                    if (mask_req[r] & (1 << c)) {
                        if (board[r][c] != val) {
                            cands.push_back({r, c, val});
                        }
                    }
                }
            }
        }
        std::sort(cands.begin(), cands.end());
        cands.erase(std::unique(cands.begin(), cands.end()), cands.end());
        return cands;
    };

    // 2.1 Build Connection Map (Cell -> List of Numbers relying on it)
    // This optimization allows checking ONLY affected numbers.
    std::array<std::vector<uint16_t>, 112> connection_map; // 8*14 = 112
    for (int i = 1; i <= actual_score; ++i) {
        const auto& d = DIGIT_TABLE[i];
        // Only map significant numbers? For now map all.
        // Optimization: We could skip very small numbers (len < 3) if they are robust,
        // but correctness requires checking everything.
        auto cells = get_used_cells(fb, d);
        for (const auto& p : cells) {
            connection_map[p.first * 14 + p.second].push_back(static_cast<uint16_t>(i));
        }
    }

    auto candidates = get_connection_candidates(fb, target_data);

    for (const auto& cand : candidates) {
        int r = cand.r;
        int c = cand.c;
        int v = cand.val;
        int old_val = board[r][c];

        board[r][c] = v;
        update_fast_board(fb, r, c, old_val, v);

        // Check 1: Does it fix the target?
        if (can_make_bitboard(fb, target_data.digits, target_data.len)) {
            // Check 2: Did it break anything?
            // OPTIMIZED: Only check numbers that used this cell.
            bool broke = false;
            const auto& affected = connection_map[r * 14 + c];
            
            for (uint16_t num_idx : affected) {
                const auto& d = DIGIT_TABLE[num_idx];
                if (!can_make_bitboard(fb, d.digits, d.len)) {
                    broke = true;
                    break;
                }
            }

            if (!broke) {
                if (backtrack(board, fb, actual_score, depth + 1, max_depth, seed_best, best_board_out, lid, seed_display_idx, nodes)) {
                    return true;
                }
            }
        }

        board[r][c] = old_val;
        update_fast_board(fb, r, c, v, old_val);
    }

    return false;
}

void worker_thread(int seed_idx, int lid, std::array<std::array<int, 14>, 8> board, int max_depth) {
    try {
        for (const auto& row : board) {
            for (int val : row) {
                if (val < 0 || val > 9) throw std::runtime_error("Invalid board data");
            }
        }

        FastBoard fb;
        rebuild_fast_board(board, fb);
        
        int initial_score = get_score_param_bit(fb);
        int seed_best = initial_score;
        std::array<std::array<int, 14>, 8> best_found_board = board;
        long long nodes = 0;

        {
            std::lock_guard<std::recursive_mutex> lock(tool_console_mtx);
            std::println("[Thread {}] Seed {} Start: {}", seed_idx, seed_idx, initial_score);
        }

        // Start recursion
        backtrack(board, fb, initial_score, 0, max_depth, seed_best, best_found_board, lid, seed_idx, nodes);


        if (seed_best > initial_score) {
            FastBoard final_fb;
            rebuild_fast_board(best_found_board, final_fb);
            int final_freq = get_frequency_score_bit(final_fb);
            int final_sum = get_sum_score(final_fb);
            save_lineage_result(lid | 0x70000000, 0, seed_best, final_freq, final_sum, best_found_board);

            std::lock_guard<std::recursive_mutex> lock(tool_console_mtx);
            std::println("[Thread {}] SAVED Seed {} | {} -> {} (Nodes: {})", seed_idx, seed_idx, initial_score, seed_best, nodes);
        } else {
            std::lock_guard<std::recursive_mutex> lock(tool_console_mtx);
            std::println("[Thread {}] Seed {} No improvement (Nodes: {})", seed_idx, seed_idx, nodes);
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::recursive_mutex> lock(tool_console_mtx);
        std::println("[Thread {}] Error: {}", seed_idx, e.what());
    } catch (...) {}
}

int main(int argc, char* argv[]) {
    init_db();
    
    int max_depth = 8; 
    int num_seeds = std::thread::hardware_concurrency();
    int num_threads = std::thread::hardware_concurrency();
    
    if (argc > 1) { try { max_depth = std::stoi(argv[1]); } catch (...) {} }
    if (argc > 2) { try { num_seeds = std::stoi(argv[2]); } catch (...) {} }
    if (argc > 3) { try { num_threads = std::stoi(argv[3]); } catch (...) {} }

    std::println("=== Deep Repair Backtracker (Node Capped) ===");
    std::println("  [Config] Max Depth: {}", max_depth);
    std::println("  [Config] Seeds: {}, Threads: {}", num_seeds, num_threads);

    auto seeds = load_top_freq_elites(num_seeds);
    if (seeds.empty()) {
        std::println("No seeds found in database.");
        return 1;
    }

    std::atomic<int> next_seed_idx(0);
    int total_seeds = (int)seeds.size();
    std::vector<std::thread> workers;

    for (int t = 0; t < num_threads; ++t) {
        workers.emplace_back([&]() {
            while (true) {
                int i = next_seed_idx.fetch_add(1);
                if (i >= total_seeds) break;
                
                {
                    std::lock_guard<std::recursive_mutex> lock(tool_console_mtx);
                    std::println("[Progress] Starting Seed {}/{}...", i + 1, total_seeds);
                }
                
                worker_thread(i + 1, seeds[i].first, seeds[i].second, max_depth);
            }
        });
    }

    for (auto& t : workers) if (t.joinable()) t.join();
    return 0;
}