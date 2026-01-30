#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <chrono>
#include <utility>
#include <algorithm>
#include <numeric>
#include <thread>
#include <mutex>
#include <atomic>
#include <set>

#include "sqlite3.h"
#include "common.h"
#include "globals.h"     
#include "db_manager.h"  
#include "logic.h"       

extern sqlite3* db; 

// Corrected: Checks every number from 1 upwards.
// Most permutations will fail in the first 10-20 checks, making this very fast.
int get_score_permuted_strict(const FastBoard& original_fb, const std::array<int, 10>& mapping) {
    FastBoard permuted_fb;
    for(int i=0; i<10; ++i) {
        permuted_fb.bits[mapping[i]] = original_fb.bits[i];
    }

    int num = 1;
    while (num < MAX_PRECOMPUTE) {
        const auto& data = DIGIT_TABLE[num];
        if (!can_make_bitboard(permuted_fb, data.digits, data.len)) return num - 1;
        num++;
    }
    return MAX_PRECOMPUTE - 1;
}

void audit_completion(int lid) {
    std::lock_guard<std::mutex> lock(db_mtx);
    if (!db) return;
    char* errMsg = 0;
    std::string sql = "INSERT OR IGNORE INTO permutation_audit (lineage_id) VALUES (" + std::to_string(lid) + ");";
    sqlite3_exec(db, sql.c_str(), 0, 0, &errMsg);
}

bool is_board_duplicate(const std::string& b_str) {
    std::lock_guard<std::mutex> lock(db_mtx);
    if (!db) return false;
    sqlite3_stmt* stmt;
    std::string sql = "SELECT 1 FROM lineage_results WHERE board_data = ? LIMIT 1;";
    bool exists = false;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, b_str.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) exists = true;
        sqlite3_finalize(stmt);
    }
    return exists;
}

#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <chrono>
#include <utility>
#include <algorithm>
#include <numeric>
#include <thread>
#include <mutex>
#include <atomic>
#include <set>
#include <bitset>

#include "sqlite3.h"
#include "common.h"
#include "globals.h"     
#include "db_manager.h"  
#include "logic.h"       

extern sqlite3* db; 

// Extended constants for the tool
const int TOOL_MAX_SCORE = 100000;

// Optimized Score Check using the Oracle
// (Falling back to bitboard only for very long numbers if score > 100k)
int get_score_with_oracle(const std::array<int8_t, 10>& perm, const class FullWalkOracle& oracle);

void process_board(int lid, std::array<std::array<int, 14>, 8> board);

// --- Sieve Method with Bitset Oracle ---

struct FullWalkOracle {
    // Total bits: 10 + 100 + 1000 + 10000 + 100000 = 111110
    std::bitset<130000> bits;
    
    // Offset for each length
    const int offsets[6] = {0, 0, 10, 110, 1110, 11110};

    void mark(int len, int val) {
        if (len < 1 || len > 5) return;
        bits.set(offsets[len] + val);
    }

    bool check(int len, int val) const {
        if (len < 1 || len > 5) return false;
        return bits.test(offsets[len] + val);
    }
};

void dfs_walks_recursive(
    int r, int c, int depth, int current_val,
    const std::array<std::array<int, 14>, 8>& board, 
    FullWalkOracle& oracle
) {
    int next_val = current_val * 10 + board[r][c];
    oracle.mark(depth, next_val);

    if (depth < 5) {
        const auto& adj = ADJ_TABLE[r][c];
        for(int i = 0; i < adj.count; ++i) {
            dfs_walks_recursive(adj.list[i].y, adj.list[i].x, depth + 1, next_val, board, oracle);
        }
    }
}

FullWalkOracle precompute_all_walks(const std::array<std::array<int, 14>, 8>& board) {
    FullWalkOracle oracle;
    for(int r = 0; r < 8; ++r) {
        for(int c = 0; c < 14; ++c) {
            dfs_walks_recursive(r, c, 1, 0, board, oracle);
        }
    }
    return oracle;
}

int count_valid_walks(const std::array<std::array<int, 14>, 8>& board) {
    auto oracle = precompute_all_walks(board);
    return (int)oracle.bits.count();
}

// Global state for DFS
std::atomic<int> g_max_richness(0);
std::mutex g_dfs_mtx;

void dfs_optimize_topology(
    std::array<std::array<int, 14>, 8> board, 
    int depth, 
    int lineage_id
) {
    if (depth == 0) return;

    // Try swapping adjacent cells (horizontal and vertical)
    // To keep branching factor manageable, we might sample or try all?
    // 8*13 + 7*14 = 104 + 98 = 202 swaps.
    // Depth 3 -> 8 million nodes. A bit heavy.
    // Let's try Hill Climbing / Beam Search style?
    // The user asked for DFS. Let's do a greedy DFS.
    
    int current_richness = count_valid_walks(board);
    
    // Update Global Best
    {
        int global = g_max_richness.load();
        if (current_richness > global) {
            std::lock_guard<std::mutex> lock(g_dfs_mtx);
            if (current_richness > g_max_richness.load()) {
                g_max_richness = current_richness;
                std::cout << "[DFS " << lineage_id << "] New Max Topology Richness: " << current_richness << std::endl;
                // Found a better topology! Run the expensive permutation sieve.
                process_board(lineage_id, board);
            }
        }
    }

    // Heuristic: Try all 200 swaps, keep top K, recurse.
    struct Move { int r1, c1, r2, c2, score; };
    std::vector<Move> moves;
    moves.reserve(210);

    auto try_swap = [&](int r1, int c1, int r2, int c2) {
        if (board[r1][c1] == board[r2][c2]) return; // No op
        std::swap(board[r1][c1], board[r2][c2]);
        int score = count_valid_walks(board);
        if (score >= current_richness) { // Only consider non-degrading moves
            moves.push_back({r1, c1, r2, c2, score});
        }
        std::swap(board[r1][c1], board[r2][c2]); // Backtrack
    };

    for(int r=0; r<8; ++r) {
        for(int c=0; c<14; ++c) {
            if (c+1 < 14) try_swap(r, c, r, c+1);
            if (r+1 < 8)  try_swap(r, c, r+1, c);
        }
    }

    // Sort by score descending
    std::sort(moves.begin(), moves.end(), [](const Move& a, const Move& b){
        return a.score > b.score;
    });

    // Recurse on top 3 moves
    int branches = 0;
    for(const auto& m : moves) {
        if (branches++ >= 3) break;
        std::swap(board[m.r1][m.c1], board[m.r2][m.c2]);
        dfs_optimize_topology(board, depth - 1, lineage_id);
        std::swap(board[m.r1][m.c1], board[m.r2][m.c2]); // Backtrack
    }
}

// Precompute digit sequences for 1..99999 to avoid repeated division
struct DigitCache {
    struct Entry { uint8_t len; int8_t digits[5]; };
    std::vector<Entry> table;

    DigitCache() {
        table.resize(TOOL_MAX_SCORE);
        for(int i=1; i<TOOL_MAX_SCORE; ++i) {
            int temp = i;
            std::vector<int8_t> d;
            while(temp > 0) { d.push_back(temp % 10); temp /= 10; }
            table[i].len = (uint8_t)d.size();
            for(int j=0; j<table[i].len; ++j) {
                table[i].digits[j] = d[table[i].len - 1 - j]; // Big Endian
            }
        }
    }
};

static DigitCache G_DIGITS;

inline int map_number_fast(int n, const std::array<int8_t, 10>& perm) {
    const auto& e = G_DIGITS.table[n];
    int res = 0;
    for(int i=0; i<e.len; ++i) {
        res = res * 10 + perm[e.digits[i]];
    }
    return res;
}

void process_board(int lid, std::array<std::array<int, 14>, 8> board) {
    auto oracle = precompute_all_walks(board);

    std::vector<std::array<int8_t, 10>> survivors;
    survivors.reserve(3628800);
    std::array<int8_t, 10> p;
    std::iota(p.begin(), p.end(), 0);
    do { survivors.push_back(p); } while (std::next_permutation(p.begin(), p.end()));

    std::cout << "[BOARD " << lid << "] Oracle built. Sieving for 1.." << TOOL_MAX_SCORE-1 << "..." << std::endl;

    std::array<int8_t, 10> best_perm = p; // Identity
    int current_max = 0;

    for (int n = 1; n < TOOL_MAX_SCORE; ++n) {
        int len = G_DIGITS.table[n].len;
        
        auto end_it = survivors.end();
        auto split_it = std::partition(survivors.begin(), end_it, [&](const std::array<int8_t, 10>& perm) {
            int mapped = map_number_fast(n, perm);
            return oracle.check(len, mapped);
        });

        if (split_it == survivors.begin()) {
            break; // No one survives n
        }

        survivors.erase(split_it, end_it);
        current_max = n;
        best_perm = survivors[0]; // Representative of the winner group

        // Optimization: If only one survivor left, we can just finish it with get_score
        if (survivors.size() == 1) {
            // Check how much further this single perm can go
            for (int m = n + 1; m < TOOL_MAX_SCORE; ++m) {
                int m_len = G_DIGITS.table[m].len;
                int m_mapped = map_number_fast(m, survivors[0]);
                if (oracle.check(m_len, m_mapped)) current_max = m;
                else break;
            }
            best_perm = survivors[0];
            survivors.clear();
            break;
        }
    }

    std::cout << "  [RESULT] Max Score: " << current_max << std::endl;

    // Check if improved over DB
    FastBoard identity_fb;
    rebuild_fast_board(board, identity_fb);
    int initial_score = get_score_param_bit(identity_fb);

    if (current_max > initial_score) {
        std::array<std::array<int, 14>, 8> best_board = board;
        for(auto& row : best_board) {
            for(auto& val : row) val = best_perm[val];
        }
        
        std::stringstream ss;
        for (const auto& row : best_board) for (const auto& val : row) ss << val;
        
        if (!is_board_duplicate(ss.str())) {
            FastBoard final_fb;
            rebuild_fast_board(best_board, final_fb);
            int f = get_frequency_score_bit(final_fb);
            int s = get_sum_score(final_fb);
            int hyb = get_hybrid_score(final_fb);
            int hsq = get_hybrid_sqrt_score(final_fb);
            save_lineage_result(lid | 0x60000000, 0, current_max, f, s, hyb, hsq, best_board);
            std::cout << "  [SAVED] Improvement: " << initial_score << " -> " << current_max << std::endl;
        }
    }
    
    audit_completion(lid);
}

int main(int argc, char* argv[]) {
    init_db();
    {
        std::lock_guard<std::mutex> lock(db_mtx);
        if (db) sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS permutation_audit (lineage_id INTEGER PRIMARY KEY);", 0, 0, 0);
    }

    std::cout << "=== Flexible Permutation Brute-Forcer ===" << std::endl;

    // Load ALL unique boards from DB
    auto candidates = load_all_unique_elites();
    std::vector<std::pair<int, std::array<std::array<int, 14>, 8>>> to_process;

    {
        std::lock_guard<std::mutex> lock(db_mtx);
        if (db) {
            for(auto& cand : candidates) {
                int lid = cand.first;
                // Skip if this board is already a result of permutation (tagged with 0x60000000)
                if ((lid & 0x60000000) != 0) continue;

                // FORCE RE-EVALUATION: Ignore previous audit status
                // We want to re-run everything with the new Sieve Method
                /*
                sqlite3_stmt* stmt;
                std::string sql = "SELECT 1 FROM permutation_audit WHERE lineage_id = " + std::to_string(lid);
                if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
                    if (sqlite3_step(stmt) != SQLITE_ROW) to_process.push_back(cand);
                    sqlite3_finalize(stmt);
                }
                */
                to_process.push_back(cand);
            }
        }
    }

    if (to_process.empty()) {
        std::cout << "All available boards already audited." << std::endl;
        return 0;
    }

    std::cout << "Processing " << to_process.size() << " boards with Topology DFS..." << std::endl;
    
    // Reset global max for this run (or per board? Global makes sense if comparing across all)
    // Actually per-board local max is better to avoid getting stuck on one super-board.
    // Let's reset per board inside the loop?
    // No, g_max_richness is global.
    // We should initialize it with the seed's richness.

    for (auto& item : to_process) {
        int lid = item.first;
        auto& board = item.second;
        
        g_max_richness = count_valid_walks(board);
        std::cout << "[START " << lid << "] Initial Richness: " << g_max_richness.load() << std::endl;
        
        // Initial check
        process_board(lid, board);
        
        // Start DFS
        // Depth 10 is deep but branching factor is small (3). 3^10 = 59k. Feasible.
        dfs_optimize_topology(board, 10, lid);
    }

    std::cout << "All tasks finished." << std::endl;
    return 0;
}