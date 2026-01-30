#include "core/basis.h"
#include "utils/config.h"
#include <bitset>
#include <vector>
#include <unordered_set>
#include <string>
#include <sstream>
#include <iomanip>

static std::bitset<1000> BASIS_BITS3;
static std::bitset<10000> BASIS_BITS4;
static std::bitset<100000> BASIS_BITS5;
static std::vector<int> BASIS_LIST;
static int BASIS_SIZE = 0;

std::vector<uint8_t> NODE_FLAGS[6];

// Legacy include needed for MAX_PRECOMPUTE and DIGIT_TABLE if not moved yet
#include "legacy/common.h"

static std::unordered_set<std::string> get_span_cpp(const std::string& start_s, int max_len) {
    std::unordered_set<std::string> results;
    std::vector<std::pair<std::string, int>> q;
    
    int n = (int)start_s.length();
    for(int i=0; i<n; ++i) {
        q.push_back({std::string(1, start_s[i]), i});
    }
    
    int head = 0;
    while(head < (int)q.size()) {
        auto [s, idx] = q[head++];
        results.insert(s);
        
        int neighbors[2] = {idx - 1, idx + 1};
        for (int ni : neighbors) {
            if (ni >= 0 && ni < n) {
                std::string next_s = s + start_s[ni];
                if ((int)next_s.length() <= max_len) {
                    q.push_back({next_s, ni});
                }
            }
        }
    }
    return results;
}

static std::bitset<10000> BASIS_WITH_TWINS_BITS4;
static std::bitset<100000> BASIS_WITH_TWINS_BITS5;
static std::bitset<10000> BASIS_WITH_DOUBLE_TWINS_BITS4;
static std::bitset<100000> BASIS_WITH_DOUBLE_TWINS_BITS5;

void init_basis_set() {
    BASIS_BITS3.reset();
    BASIS_BITS4.reset();
    BASIS_BITS5.reset();
    BASIS_WITH_TWINS_BITS4.reset();
    BASIS_WITH_TWINS_BITS5.reset();
    BASIS_WITH_DOUBLE_TWINS_BITS4.reset();
    BASIS_WITH_DOUBLE_TWINS_BITS5.reset();
    BASIS_LIST.clear();
    
    std::unordered_set<std::string> covered_set;
    std::unordered_set<std::string> basis_set;
    
    int start = Config4D::BASIS_USE_PADDING ? 0 : 1;
    int limit_len = 5;

    for (int i = start; i < Config4D::BASIS_MAX_RANGE; ++i) {
        std::string s;
        if (Config4D::BASIS_USE_PADDING) {
            std::ostringstream oss;
            oss << std::setw(Config4D::BASIS_PADDING_WIDTH) << std::setfill('0') << i;
            s = oss.str();
        } else {
            s = std::to_string(i);
        }
        
        if (covered_set.count(s)) continue;
        
        auto span = get_span_cpp(s, limit_len);
        for (const auto& item : span) covered_set.insert(item);
        
        for (auto it = basis_set.begin(); it != basis_set.end(); ) {
            if (span.count(*it)) it = basis_set.erase(it);
            else ++it;
        }
        basis_set.insert(s);
    }
    
    for (const auto& s : basis_set) {
        try {
            int len = (int)s.length();
            long long val = std::stoll(s);
            
            bool has_twin = false;
            for (int i = 0; i < len - 1; ++i) {
                if (s[i] == s[i+1]) { has_twin = true; break; }
            }

            bool has_double_twin = false;
            // Check xxyy
            for (int i = 0; i < len - 3; ++i) {
                if (s[i] == s[i+1] && s[i+2] == s[i+3] && s[i] != s[i+2]) { has_double_twin = true; break; }
            }
            // Check xxzyy
            if (!has_double_twin) {
                for (int i = 0; i < len - 4; ++i) {
                    if (s[i] == s[i+1] && s[i+3] == s[i+4] && s[i] != s[i+3]) { has_double_twin = true; break; }
                }
            }

            if (len == 3 && val < 1000) {
                BASIS_BITS3.set(val);
                // 3D doesn't typically use twin bonus in this logic but we could add it
            }
            else if (len == 4 && val < 10000) {
                BASIS_BITS4.set(val);
                if (has_twin) BASIS_WITH_TWINS_BITS4.set(val);
                if (has_double_twin) BASIS_WITH_DOUBLE_TWINS_BITS4.set(val);
            }
            else if (len == 5 && val < 100000) {
                BASIS_BITS5.set(val);
                if (has_twin) BASIS_WITH_TWINS_BITS5.set(val);
                if (has_double_twin) BASIS_WITH_DOUBLE_TWINS_BITS5.set(val);
            }

            if (val >= 0 && val < Config4D::BASIS_MAX_RANGE) {
                bool matches_format = true;
                if (Config4D::BASIS_USE_PADDING && len != Config4D::BASIS_PADDING_WIDTH) matches_format = false;
                if (matches_format) {
                    BASIS_LIST.push_back((int)val);
                }
            }
        } catch (...) {}
    }
    BASIS_SIZE = (int)BASIS_LIST.size();

    // Print Twin Statistics
    int twin4 = (int)BASIS_WITH_TWINS_BITS4.count();
    int twin5 = (int)BASIS_WITH_TWINS_BITS5.count();
    int d_twin4 = (int)BASIS_WITH_DOUBLE_TWINS_BITS4.count();
    int d_twin5 = (int)BASIS_WITH_DOUBLE_TWINS_BITS5.count();
    std::printf("[BASIS] Total: %d | Twins: %d (4D: %d, 5D: %d) | D.Twins: %d (4D: %d, 5D: %d)\n", 
        BASIS_SIZE, twin4 + twin5, twin4, twin5, d_twin4 + d_twin5, d_twin4, d_twin5);

    int limits[6] = {0, 10, 100, 1000, 10000, 100000};
    for(int d=1; d<=5; ++d) {
        NODE_FLAGS[d].assign(limits[d], 0);
    }
    
    for(int i=0; i<10000; ++i) {
        if(BASIS_BITS4.test(i)) {
            int val = i;
            for(int d=4; d>=1; --d) {
                NODE_FLAGS[d][val] |= 1;
                val /= 10;
            }
        }
    }
    
    for(int i=0; i<100000; ++i) {
        if(BASIS_BITS5.test(i)) {
            int val = i;
            for(int d=5; d>=1; --d) {
                NODE_FLAGS[d][val] |= 2;
                val /= 10;
            }
        }
    }
}

int get_basis_size() {
    return BASIS_SIZE;
}

void get_basis_list(int* out_list) {
    for (int i = 0; i < BASIS_SIZE; ++i) {
        out_list[i] = BASIS_LIST[i];
    }
}

long long get_basis_score(const RichnessOracle& oracle) {
    return (long long)((oracle.bits4 & BASIS_BITS4).count() + (oracle.bits5 & BASIS_BITS5).count());
}

long long get_basis_score_4d(const RichnessOracle4D& oracle) {
    return (long long)((oracle.bits3 & BASIS_BITS3).count() + (oracle.bits4 & BASIS_BITS4).count());
}

static void dfs_basis_pruned(int r, int c, int depth, int current_val, const BoardArray& board, RichnessOracle& oracle) {
    int val = board[r][c];
    if (val == -1) return;
    int next_val = current_val * 10 + val;

    uint8_t flags = NODE_FLAGS[depth][next_val];
    if (!flags) return;
    
    if (depth == 4) {
        oracle.mark(4, next_val);
        if (!(flags & 2)) return;
    }
    else if (depth == 5) { 
        oracle.mark(5, next_val); 
        return; 
    }

    const auto& adj = ADJ_TABLE[r][c];
    for(int i = 0; i < adj.count; ++i) {
        dfs_basis_pruned(adj.list[i].y, adj.list[i].x, depth + 1, next_val, board, oracle);
    }
}

long long get_basis_score_extended(const BoardArray& board) {
    static thread_local RichnessOracle oracle;
    oracle.bits4.reset();
    oracle.bits5.reset();

    for(int r = 0; r < BOARD_ROWS; ++r) {
        for(int c = 0; c < BOARD_COLS; ++c) {
            dfs_basis_pruned(r, c, 1, 0, board, oracle);
        }
    }

    return get_basis_score(oracle);
}

double get_basis_score_with_twins(const BoardArray& board, double w_twin, double w_double_twin) {
    static thread_local RichnessOracle oracle;
    oracle.bits4.reset();
    oracle.bits5.reset();

    for(int r = 0; r < BOARD_ROWS; ++r) {
        for(int c = 0; c < BOARD_COLS; ++c) {
            dfs_basis_pruned(r, c, 1, 0, board, oracle);
        }
    }

    size_t normal_count = (oracle.bits4 & BASIS_BITS4).count() + (oracle.bits5 & BASIS_BITS5).count();
    size_t twin_count = (oracle.bits4 & BASIS_WITH_TWINS_BITS4).count() + (oracle.bits5 & BASIS_WITH_TWINS_BITS5).count();
    size_t d_twin_count = (oracle.bits4 & BASIS_WITH_DOUBLE_TWINS_BITS4).count() + (oracle.bits5 & BASIS_WITH_DOUBLE_TWINS_BITS5).count();

    return static_cast<double>(normal_count) + (w_twin * static_cast<double>(twin_count)) + (w_double_twin * static_cast<double>(d_twin_count));
}

// New helper for efficient win-checking and scoring
void get_basis_score_combined(const BoardArray& board, double w_twin, double w_double_twin, int& out_basis_count, double& out_weighted_score) {
    static thread_local RichnessOracle oracle;
    oracle.bits4.reset();
    oracle.bits5.reset();

    for(int r = 0; r < BOARD_ROWS; ++r) {
        for(int c = 0; c < BOARD_COLS; ++c) {
            dfs_basis_pruned(r, c, 1, 0, board, oracle);
        }
    }

    out_basis_count = (int)((oracle.bits4 & BASIS_BITS4).count() + (oracle.bits5 & BASIS_BITS5).count());
    size_t twin_count = (oracle.bits4 & BASIS_WITH_TWINS_BITS4).count() + (oracle.bits5 & BASIS_WITH_TWINS_BITS5).count();
    size_t d_twin_count = (oracle.bits4 & BASIS_WITH_DOUBLE_TWINS_BITS4).count() + (oracle.bits5 & BASIS_WITH_DOUBLE_TWINS_BITS5).count();
    out_weighted_score = (double)out_basis_count + (w_twin * (double)twin_count) + (w_double_twin * (double)d_twin_count);
}

// --- Optimized Heatmap Logic (2-Pass Critical Path Mapping) ---

static thread_local std::bitset<Config4D::BASIS_MAX_RANGE + 1> FOUND_BITS;
static thread_local std::bitset<Config4D::BASIS_MAX_RANGE + 1> REDUNDANT_BITS;

// Pass 1: Identify unique and redundant basis numbers
static void dfs_count_global(int r, int c, int depth, int current_val, const BoardArray& board) {
    int val = board[r][c];
    if (val == -1) return;
    int next_val = current_val * 10 + val;

    uint8_t flags = NODE_FLAGS[depth][next_val];
    if (!flags) return;

    if (depth == 4) {
        if (flags & 1) {
            if (FOUND_BITS.test(next_val)) REDUNDANT_BITS.set(next_val);
            else FOUND_BITS.set(next_val);
        }
        if (!(flags & 2)) return;
    } else if (depth == 5) {
        if (flags & 2) {
            if (FOUND_BITS.test(next_val)) REDUNDANT_BITS.set(next_val);
            else FOUND_BITS.set(next_val);
        }
        return;
    }

    const auto& adj = ADJ_TABLE[r][c];
    for (int i = 0; i < adj.count; ++i) {
        dfs_count_global(adj.list[i].y, adj.list[i].x, depth + 1, next_val, board);
    }
}

// Pass 2: Map critical paths to heatmap
static void dfs_heatmap_mapping(int r, int c, int depth, int current_val, const BoardArray& board, 
                               int path_r[], int path_c[], std::array<std::array<int, 14>, 8>& heatmap) {
    int val = board[r][c];
    if (val == -1) return;
    int next_val = current_val * 10 + val;

    uint8_t flags = NODE_FLAGS[depth][next_val];
    if (!flags) return;

    path_r[depth-1] = r;
    path_c[depth-1] = c;

    auto add_score = [&](int num) {
        // Critical if FOUND but NOT REDUNDANT
        if (FOUND_BITS.test(num) && !REDUNDANT_BITS.test(num)) {
            bool is_twin = false;
            bool is_d_twin = false;
            if (num < 10000) { 
                if(BASIS_WITH_TWINS_BITS4.test(num)) is_twin = true; 
                if(BASIS_WITH_DOUBLE_TWINS_BITS4.test(num)) is_d_twin = true;
            }
            else { 
                if(BASIS_WITH_TWINS_BITS5.test(num)) is_twin = true; 
                if(BASIS_WITH_DOUBLE_TWINS_BITS5.test(num)) is_d_twin = true;
            }
            
            // Base: 100, Twin: +75, DoubleTwin: +25
            int score_val = 100;
            if (is_twin) score_val += 75;
            if (is_d_twin) score_val += 25;
            
            for (int i = 0; i < depth; ++i) {
                heatmap[path_r[i]][path_c[i]] += score_val;
            }
        }
    };

    if (depth == 4) {
        if (flags & 1) add_score(next_val);
        if (!(flags & 2)) return;
    } else if (depth == 5) {
        if (flags & 2) add_score(next_val);
        return;
    }

    const auto& adj = ADJ_TABLE[r][c];
    for (int i = 0; i < adj.count; ++i) {
        dfs_heatmap_mapping(adj.list[i].y, adj.list[i].x, depth + 1, next_val, board, path_r, path_c, heatmap);
    }
}

void calculate_fast_heatmap(const BoardArray& board, std::array<std::array<int, 14>, 8>& heatmap) {
    FOUND_BITS.reset();
    REDUNDANT_BITS.reset();
    for(int r = 0; r < BOARD_ROWS; ++r) {
        for(int c = 0; c < BOARD_COLS; ++c) {
            dfs_count_global(r, c, 1, 0, board);
        }
    }

    for(auto& row : heatmap) row.fill(0);
    int path_r[5], path_c[5];
    for(int r = 0; r < BOARD_ROWS; ++r) {
        for(int c = 0; c < BOARD_COLS; ++c) {
            dfs_heatmap_mapping(r, c, 1, 0, board, path_r, path_c, heatmap);
        }
    }
}

void get_heatmap_and_missing_weights(const BoardArray& board, std::array<std::array<int, 14>, 8>& heatmap, std::array<double, 10>& missing_weights) {
    FOUND_BITS.reset();
    REDUNDANT_BITS.reset();
    for(int r = 0; r < BOARD_ROWS; ++r) {
        for(int c = 0; c < BOARD_COLS; ++c) {
            dfs_count_global(r, c, 1, 0, board);
        }
    }

    missing_weights.fill(0.0);
    for (int i = 0; i < 10000; ++i) {
        if (BASIS_BITS4.test(i) && !FOUND_BITS.test(i)) {
            int val = i;
            for(int d=0; d<4; ++d) { missing_weights[val % 10] += 1.0; val /= 10; }
        }
    }
    for (int i = 0; i < 100000; ++i) {
        if (BASIS_BITS5.test(i) && !FOUND_BITS.test(i)) {
            int val = i;
            for(int d=0; d<5; ++d) { missing_weights[val % 10] += 1.0; val /= 10; }
        }
    }

    for(auto& row : heatmap) row.fill(0);
    int path_r[5], path_c[5];
    for(int r = 0; r < BOARD_ROWS; ++r) {
        for(int c = 0; c < BOARD_COLS; ++c) {
            dfs_heatmap_mapping(r, c, 1, 0, board, path_r, path_c, heatmap);
        }
    }
}

void get_found_basis_flags(const BoardArray& board, int8_t* out_flags) {
    static thread_local RichnessOracle oracle;
    oracle.bits4.reset();
    oracle.bits5.reset();

    for(int r = 0; r < BOARD_ROWS; ++r) {
        for(int c = 0; c < BOARD_COLS; ++c) {
            dfs_basis_pruned(r, c, 1, 0, board, oracle);
        }
    }

    for (int i = 0; i < BASIS_SIZE; ++i) {
        int val = BASIS_LIST[i];
        bool found = false;
        
        if (val >= 1000 && val < 10000 && BASIS_BITS4.test(val)) {
            if (oracle.bits4.test(val)) found = true;
        } else if (val >= 10000 && val < 100000 && BASIS_BITS5.test(val)) {
            if (oracle.bits5.test(val)) found = true;
        }
        
        out_flags[i] = found ? 1 : 0;
    }
}
