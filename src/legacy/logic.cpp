#include "logic.h"
#include "config.h"
#include <bitset>
#include <numeric>
#include <algorithm>
#include <vector>
#include <cmath>
#include <unordered_set>
#include <string>
#include <sstream>
#include <iomanip>
#include <print>

// ============================================================================
// 1. DFS & Oracle Implementation
// ============================================================================

void dfs_richness(int r, int c, int depth, int current_val, const std::array<std::array<int, 14>, 8>& board, RichnessOracle& oracle) {
    if (board[r][c] == -1) return;
    int next_val = current_val * 10 + board[r][c];
    if (depth == 4) oracle.mark(4, next_val);
    else if (depth == 5) { oracle.mark(5, next_val); return; }

    const auto& adj = ADJ_TABLE[r][c];
    for(int i = 0; i < adj.count; ++i) {
        dfs_richness(adj.list[i].y, adj.list[i].x, depth + 1, next_val, board, oracle);
    }
}

void dfs_richness_4d(int r, int c, int depth, int current_val, const std::array<std::array<int, 14>, 8>& board, RichnessOracle4D& oracle) {
    if (board[r][c] == -1) return;
    int next_val = current_val * 10 + board[r][c];
    if (depth == 3) oracle.mark(3, next_val);
    else if (depth == 4) { oracle.mark(4, next_val); return; }

    const auto& adj = ADJ_TABLE[r][c];
    for(int i = 0; i < adj.count; ++i) {
        dfs_richness_4d(adj.list[i].y, adj.list[i].x, depth + 1, next_val, board, oracle);
    }
}

// ============================================================================
// 2. 4D Scoring Logic (Unbiased Bucket Analogy)
// ============================================================================

struct NumberInfo {
    int8_t type; // 0: Trivial, 1: Pal, 2: Cyclic, 3: Standard
    int8_t bucket;
    int16_t partner; // For Cyclic or Standard pairs
};

constexpr std::array<NumberInfo, 10000> create_lookup_table() {
    std::array<NumberInfo, 10000> table{};
    for (int i = 0; i < 10000; ++i) {
        int d1 = i / 1000, d2 = (i / 100) % 10, d3 = (i / 10) % 10, d4 = i % 10;
        if (d1 == d3 || d2 == d4) { table[i] = {0, 0, -1}; continue; }
        
        if (d1 == d4 && d2 == d3) { table[i] = {1, (int8_t)d1, -1}; continue; }
        int rev = d4 * 1000 + d3 * 100 + d2 * 10 + d1;
        if (d1 == d4) { table[i] = {2, (int8_t)d1, (int16_t)rev}; continue; }
        table[i] = {3, (int8_t)d1, (int16_t)rev};
    }
    return table;
}

constexpr auto LOOKUP = create_lookup_table();

struct Info3 { int d1, d3; bool is_pal; int rev; };
constexpr std::array<Info3, 1000> create_lookup3_table() {
    std::array<Info3, 1000> table{};
    for (int i = 0; i < 1000; ++i) {
        int d1 = i/100, d3 = i%10;
        if (d1 == d3) table[i] = {d1, d3, true, -1};
        else {
            int d2 = (i/10)%10;
            table[i] = {d1, d3, false, d3*100 + d2*10 + d1};
        }
    }
    return table;
}

constexpr auto LOOKUP3 = create_lookup3_table();

// Pre-grouped bucket members (Computed at runtime)
static std::array<std::vector<int>, 10> bucket_members; 

void init_richness_lookup() {
    for (int i = 0; i < 10000; ++i) {
        if (LOOKUP[i].type != 0) {
            bucket_members[LOOKUP[i].bucket].push_back(i);
        }
    }
}

long long calculate_unbiased_score_4d(const RichnessOracle4D& oracle) {
    // No locking or checks needed here anymore. 
    // Data is guaranteed to be initialized by main() before threads start.

    struct Bucket {
        int capacity = 0;
        int fill = 0;
        int raw_count = 0;
    };
    std::array<Bucket, 10> buckets;

    // First Pass: Raw Counts for Sorting
    for (int i = 0; i < 10000; ++i) {
        if (oracle.bits4.test(i) && LOOKUP[i].type != 0) {
            buckets[LOOKUP[i].bucket].raw_count++;
        }
    }

    // Determine Bucket Priority (Rank 0 is best)
    std::vector<int> ranked_digits(10);
    std::iota(ranked_digits.begin(), ranked_digits.end(), 0);
    std::sort(ranked_digits.begin(), ranked_digits.end(), [&](int a, int b) {
        return buckets[a].raw_count > buckets[b].raw_count;
    });

    std::array<int, 10> digit_to_rank;
    for(int r=0; r<10; ++r) digit_to_rank[ranked_digits[r]] = r;

    // Second Pass: De-biasing & Capacity Reduction
    static thread_local std::bitset<10000> consumed;
    consumed.reset();

    for (int d : ranked_digits) {
        // Iterate ONLY members of this bucket
        for (int i : bucket_members[d]) {
            const auto& info = LOOKUP[i];
            if (consumed.test(i)) continue;

            if (info.type == 1) { // Palindrome
                buckets[d].capacity++;
                if (oracle.bits4.test(i)) buckets[d].fill++;
                consumed.set(i);
            } else if (info.type == 2) { // Cyclic
                buckets[d].capacity++;
                if (oracle.bits4.test(i) || oracle.bits4.test(info.partner)) buckets[d].fill++;
                consumed.set(i);
                consumed.set(info.partner);
            } else if (info.type == 3) { // Standard
                // Winner (Current Bucket) claims it
                buckets[d].capacity++;
                if (oracle.bits4.test(i)) buckets[d].fill++;
                consumed.set(i);
                // Loser (Partner Bucket) loses it from capacity
                consumed.set(info.partner);
            }
        }
    }

    // 3. Final Scoring (Sliding Window Weighting)
    
    // We have 11 Buckets in total:
    // Rank 0: 3-Digit (Bucket -1) - Debiased
    // Rank 1..10: 4-Digit Buckets (Sorted by fullness)
    
    struct RatedBucket {
        int fill;
        int capacity;
        bool is_3d;
    };
    
    std::vector<RatedBucket> ranked_all;
    
    // Process Bucket -1 (3D) Debiasing
    int fill3 = 0;
    int cap3 = 0;
    static thread_local std::bitset<1000> consumed3;
    consumed3.reset();

    for (int i = 0; i < 1000; ++i) {
        if (consumed3.test(i)) continue;
        const auto& inf = LOOKUP3[i];
        
        if (inf.is_pal) { 
            cap3++;
            if (oracle.bits3.test(i)) fill3++;
            consumed3.set(i);
        } else {
            cap3++;
            if (oracle.bits3.test(i) || oracle.bits3.test(inf.rev)) fill3++;
            consumed3.set(i);
            consumed3.set(inf.rev);
        }
    }
    
    ranked_all.push_back({fill3, cap3, true});
    
    // Add Rank 1..10 (4D)
    for (int d : ranked_digits) {
        ranked_all.push_back({buckets[d].fill, buckets[d].capacity, false});
    }
    
    // Find the first bucket that isn't full
    int first_fail_rank = -1;
    for (int r = 0; r < 11; ++r) {
        if (ranked_all[r].fill < ranked_all[r].capacity) {
            first_fail_rank = r;
            break;
        }
    }
    
    auto get_sliding_weight = [&](int r, int fail_rank) -> long long {
        return 1LL;
    };

    long long total_score = 0;
    
    // "do not apply any weights to the 11th bucket"
    // We iterate 0..9 (Top 10 buckets). Bucket 10 (11th) is ignored.
    for (int r = 0; r < 10; ++r) {
        long long w = get_sliding_weight(r, first_fail_rank);
        total_score += (long long)ranked_all[r].fill * w;
    }

    return total_score;
}

BiasReport get_bias_report(const std::array<std::array<int, 14>, 8>& board) {
    RichnessOracle4D oracle;
    for(int r = 0; r < 8; ++r) for(int c = 0; c < 14; ++c) dfs_richness_4d(r, c, 1, 0, board, oracle);
    
    BiasReport report = {};
    
    // 3D Stats
    for (int i = 0; i < 1000; ++i) {
        report.cap_3d++;
        if (oracle.bits3.test(i)) report.fill_3d++;
    }
    
    // 4D Stats
    for (int i = 0; i < 10000; ++i) {
        const auto& info = LOOKUP[i];
        
        int d1 = i/1000, d2 = (i/100)%10, d3 = (i/10)%10, d4 = i%10;
        int unique_mask = (1<<d1) | (1<<d2) | (1<<d3) | (1<<d4);
        int unique_count = 0;
        for(int k=0; k<10; ++k) if((unique_mask >> k) & 1) unique_count++;
        
        report.cap_4d++;
        report.cap_unique[unique_count]++;
        report.cap_type[info.type]++;
        
        // Topological Complexity
        int topo = 5; // Default: Linear (4 distinct cells needed)
        bool eq13 = (d1 == d3);
        bool eq24 = (d2 == d4);
        bool eq14 = (d1 == d4);
        
        if (eq13 && eq24) {
            topo = 2; // PingPong (abab)
        } else if (eq13 || eq24) {
            topo = 3; // Backtrack (abcb, abac)
        } else if (eq14) {
            topo = 4; // Cycle (abca)
        }
        
        report.cap_topo[topo]++;
        
        if (oracle.bits4.test(i)) {
            report.fill_4d++;
            report.fill_unique[unique_count]++;
            report.fill_type[info.type]++;
            report.fill_topo[topo]++;
        }
    }
    
    return report;
}

long long get_richness_score_4d(const std::array<std::array<int, 14>, 8>& board) {
    RichnessOracle4D oracle;
    for(int r = 0; r < 8; ++r) for(int c = 0; c < 14; ++c) dfs_richness_4d(r, c, 1, 0, board, oracle);
    return calculate_unbiased_score_4d(oracle);
}

long long get_richness_score_4d(const FastBoard& fb) {
    std::array<std::array<int, 14>, 8> board;
    for(int r=0; r<8; ++r) {
        for(int c=0; c<14; ++c) {
            board[r][c] = 0;
            for(int v=0; v<10; ++v) if(fb.bits[v][r] & (1<<c)) { board[r][c] = v; break; }
        }
    }
    return get_richness_score_4d(board);
}

// ============================================================================
// 3. 5D Scoring Logic (Soft/Double Bucket Analogy)
// ============================================================================

double get_richness_score_5d_double(const std::array<std::array<int, 14>, 8>& board) {
    RichnessOracle oracle;
    for(int r = 0; r < 8; ++r) for(int c = 0; c < 14; ++c) dfs_richness(r, c, 1, 0, board, oracle);

    std::vector<int> fills;
    std::vector<int> capacities;
    
    // Bucket 0: 4D sequences
    fills.push_back((int)oracle.bits4.count());
    capacities.push_back(10000);

    // Buckets 1-10: 5D prefixes 0-9
    for (int d = 0; d < 10; ++d) {
        int filled_weighted = 0, capacity = 10000, start = d * 10000;
        for (int i = 0; i < 10000; ++i) {
            int val = start + i;
            bool is_pal = (d == (val % 10)) && (((val / 1000) % 10) == ((val / 10) % 10));
            if (is_pal) capacity += 1;
            if (oracle.bits5.test(val)) filled_weighted += (is_pal ? 2 : 1);
        }
        fills.push_back(filled_weighted);
        capacities.push_back(capacity);
    }

    int priority_idx = -1;
    for (int i = 0; i < 11; ++i) {
        if (fills[i] < capacities[i]) { priority_idx = i; break; }
    }

    double total_score = 0.0;
    for (int i = 0; i < 11; ++i) {
        int weight_rank = (priority_idx != -1 && i > priority_idx) ? (i - priority_idx) : 0;
        double w = (weight_rank >= 11) ? 0.0 : std::pow(11.0 - weight_rank, 4.0);
        total_score += (double)fills[i] * w;
    }
    return total_score;
}

// Legacy 5D Integer Score
long long get_richness_score(const std::array<std::array<int, 14>, 8>& board) {
    RichnessOracle oracle;
    for(int r = 0; r < 8; ++r) for(int c = 0; c < 14; ++c) dfs_richness(r, c, 1, 0, board, oracle);

    long long total_holes = 0;
    for (int d = 0; d < 10; ++d) {
        int start = d * 10000;
        for (int i = 0; i < 10000; ++i) {
            if (!oracle.bits5.test(start + i)) total_holes++;
        }
    }
    return 100000LL - total_holes;
}

// ============================================================================
// 7. Basis Set Logic (Implemented in C++)
// ============================================================================

static std::bitset<1000> BASIS_BITS3;
static std::bitset<10000> BASIS_BITS4;
static std::bitset<100000> BASIS_BITS5;
static std::vector<int> BASIS_LIST;
static int BASIS_SIZE = 0;

// Optimization: Precomputed validity flags for DFS pruning
// Index: [depth][value]
// Flags: 1 = Valid 4D prefix/value, 2 = Valid 5D prefix/value
static std::vector<uint8_t> NODE_FLAGS[6];

static std::unordered_set<std::string> get_span_cpp(const std::string& start_s, int max_len) {
    std::unordered_set<std::string> results;
    // queue of (string, index)
    std::vector<std::pair<std::string, int>> q;
    
    int n = (int)start_s.length();
    for(int i=0; i<n; ++i) {
        q.push_back({std::string(1, start_s[i]), i});
    }
    
    int head = 0;
    while(head < (int)q.size()) {
        auto [s, idx] = q[head++];
        results.insert(s); // Add current substring to results
        
        // Explore neighbors
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

void init_basis_set() {
    BASIS_BITS3.reset();
    BASIS_BITS4.reset();
    BASIS_BITS5.reset();
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
            
            // Mark bitsets based on string length (how they appear on board)
            if (len == 3 && val < 1000) BASIS_BITS3.set(val);
            else if (len == 4 && val < 10000) BASIS_BITS4.set(val);
            else if (len == 5 && val < 100000) BASIS_BITS5.set(val);

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

    // Initialize Optimization Tables
    int limits[6] = {0, 10, 100, 1000, 10000, 100000};
    for(int d=1; d<=5; ++d) {
        NODE_FLAGS[d].assign(limits[d], 0);
    }
    
    // Propagate 4D flags (Bit 0 -> Value 1)
    for(int i=0; i<10000; ++i) {
        if(BASIS_BITS4.test(i)) {
            int val = i;
            for(int d=4; d>=1; --d) {
                NODE_FLAGS[d][val] |= 1;
                val /= 10;
            }
        }
    }
    
    // Propagate 5D flags (Bit 1 -> Value 2)
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

long long get_basis_score(const RichnessOracle& oracle) {
    // Intersection of Oracle (found numbers) and Basis
    return (long long)((oracle.bits4 & BASIS_BITS4).count() + (oracle.bits5 & BASIS_BITS5).count());
}

long long get_basis_score_4d(const RichnessOracle4D& oracle) {
    // Intersection of Oracle4D (found numbers) and Basis
    return (long long)((oracle.bits3 & BASIS_BITS3).count() + (oracle.bits4 & BASIS_BITS4).count());
}

// ============================================================================
// 8. Extended Basis Scoring (Variable Range/Depth)
// ============================================================================

static void dfs_extended(int r, int c, int depth, int max_depth, int current_val, const std::array<std::array<int, 14>, 8>& board, std::vector<bool>& found) {
    if (board[r][c] == -1) return;
    int next_val = current_val * 10 + board[r][c];
    
    // Determine if we should mark this number
    bool check_this = false;
    if (Config4D::BASIS_USE_PADDING) {
        // Only mark if length matches padding width
        if (depth == Config4D::BASIS_PADDING_WIDTH) check_this = true;
    } else {
        // No padding: check all lengths (e.g. "1", "12", "123"...)
        check_this = true; 
    }

    if (check_this) {
        if (next_val >= 0 && next_val < Config4D::BASIS_MAX_RANGE) {
            found[next_val] = true;
        }
    }

    if (depth >= max_depth) return;

    const auto& adj = ADJ_TABLE[r][c];
    for(int i = 0; i < adj.count; ++i) {
        dfs_extended(adj.list[i].y, adj.list[i].x, depth + 1, max_depth, next_val, board, found);
    }
}

// Optimized DFS for Basis Scoring (Prunes invalid Depth 5 branches)
void dfs_basis_pruned(int r, int c, int depth, int current_val, const std::array<std::array<int, 14>, 8>& board, RichnessOracle& oracle) {
    int val = board[r][c];
    if (val == -1) return;
    if (val < 0 || val > 9) {
        // std::println(stderr, "[ERROR] Invalid board value at ({}, {}): {}", r, c, val);
        return; 
    }

    int next_val = current_val * 10 + val;

    // Optimization: Prune if this prefix cannot lead to any basis number
    uint8_t flags = NODE_FLAGS[depth][next_val];
    if (!flags) return;
    
    // Mark bitsets
    if (depth == 4) {
        oracle.mark(4, next_val);
        // Optimization: If this path cannot extend to a 5D basis number, stop recursion
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

long long get_basis_score_extended(const std::array<std::array<int, 14>, 8>& board) {
    static thread_local RichnessOracle oracle;
    oracle.bits4.reset();
    oracle.bits5.reset();

    // Use optimized DFS
    for(int r = 0; r < 8; ++r) {
        for(int c = 0; c < 14; ++c) {
            dfs_basis_pruned(r, c, 1, 0, board, oracle);
        }
    }

    // Use the optimized bitset intersection logic
    return get_basis_score(oracle);
}
// ============================================================================

bool can_make_bitboard(const FastBoard& fb, const uint8_t* target, int len) {
    int first_digit = target[len - 1];
    std::array<uint16_t, 8> current_mask = fb.bits[first_digit];
    bool any_init = false;
    for(int r=0; r<8; ++r) if(current_mask[r]) { any_init = true; break; }
    if(!any_init) return false;

    for (int i = len - 2; i >= 0; --i) {
        int next_digit = target[i];
        const auto& next_layer = fb.bits[next_digit];
        bool any_bit_set = false;
        std::array<uint16_t, 8> next_mask;
        for (int r = 0; r < 8; ++r) {
            uint16_t row_bits = current_mask[r];
            if (row_bits == 0 && (r == 0 || current_mask[r-1] == 0) && (r == 7 || current_mask[r+1] == 0)) {
                next_mask[r] = 0; continue;
            }
            uint16_t dilated = (row_bits << 1) | (row_bits >> 1);
            if (r > 0) { uint16_t up = current_mask[r-1]; dilated |= up | (up << 1) | (up >> 1); }
            if (r < 7) { uint16_t down = current_mask[r+1]; dilated |= down | (down << 1) | (down >> 1); }
            uint16_t res = dilated & next_layer[r];
            next_mask[r] = res;
            if (res) any_bit_set = true;
        }
        if (!any_bit_set) return false;
        current_mask = next_mask;
    }
    return true;
}

void rebuild_fast_board(const std::array<std::array<int, 14>, 8>& b, FastBoard& out_fb) {
    for(auto& layer : out_fb.bits) layer.fill(0);
    for(int r=0; r<8; ++r) for(int c=0; c<14; ++c) out_fb.bits[b[r][c]][r] |= (1 << c);
}

void update_fast_board(FastBoard& fb, int r, int c, int old_val, int new_val) {
    if (old_val == new_val) return;
    fb.bits[old_val][r] &= ~(1 << c);
    fb.bits[new_val][r] |= (1 << c);
}

// ============================================================================
// 5. High-Level Scoring Wrappers
// ============================================================================

int get_score_param_bit(const FastBoard& fb) {
    for (int num = 1; num < MAX_PRECOMPUTE; ++num) {
        const auto& data = DIGIT_TABLE[num];
        if (!can_make_bitboard(fb, data.digits, data.len)) return num - 1;
    }
    return MAX_PRECOMPUTE - 1;
}

int get_frequency_score_bit(const FastBoard& fb) {
    int count = 0;
    for (const auto& item : FREQ_TARGETS) {
        if (can_make_bitboard(fb, DIGIT_TABLE[item.num].digits, DIGIT_TABLE[item.num].len)) count += item.weight;
    }
    return count;
}

int get_sum_score(const FastBoard& fb) {
    int total = 0;
    for (const auto& item : SUM_TARGETS) {
        if (can_make_bitboard(fb, DIGIT_TABLE[item.num].digits, DIGIT_TABLE[item.num].len)) total += item.value;
    }
    return total;
}

int get_hybrid_score(const FastBoard& fb) {
    int total = 0;
    for (const auto& item : HYBRID_TARGETS) {
        if (can_make_bitboard(fb, DIGIT_TABLE[item.num].digits, DIGIT_TABLE[item.num].len)) total += item.value;
    }
    return total;
}

int get_hybrid_sqrt_score(const FastBoard& fb) {
    int total = 0;
    for (const auto& item : HYBRID_SQRT_TARGETS) {
        if (can_make_bitboard(fb, DIGIT_TABLE[item.num].digits, DIGIT_TABLE[item.num].len)) total += item.value;
    }
    return total;
}

AllScores compute_all_scores(const FastBoard& fb) {
    AllScores res = {0, 0, 0, 0, 0, 0};
    std::array<std::array<int, 14>, 8> board;
    for(int r=0; r<8; ++r) {
        for(int c=0; c<14; ++c) {
            board[r][c] = 0;
            for(int v=0; v<10; ++v) if(fb.bits[v][r] & (1<<c)) { board[r][c] = v; break; }
        }
    }
    res.richness = get_richness_score(board);
    
    // Legacy scoring logic for parameters
    static thread_local std::array<bool, MAX_PRECOMPUTE> found_mask;
    found_mask.fill(false);
    for (int i = 0; i < (int)SUM_TARGETS.size(); ++i) {
        const auto& item = SUM_TARGETS[i];
        if (can_make_bitboard(fb, DIGIT_TABLE[item.num].digits, DIGIT_TABLE[item.num].len)) {
            res.sum += item.value;
            res.freq += FREQ_TARGETS[i].weight;
            res.hybrid += HYBRID_TARGETS[i].value;
            res.hybrid_sqrt += HYBRID_SQRT_TARGETS[i].value;
            found_mask[item.num] = true;
            int rev = reverse_int(item.num);
            if (item.num < rev && rev < MAX_PRECOMPUTE) found_mask[rev] = true;
        }
    }
    int p = 1;
    while (p < MAX_PRECOMPUTE) { if (!found_mask[p]) { res.param = p - 1; break; } p++; }
    if (p == MAX_PRECOMPUTE) res.param = MAX_PRECOMPUTE - 1;
    return res;
}

// ============================================================================
// 6. Utility Functions
// ============================================================================

void calculate_cell_usage(const FastBoard& fb, int start_num, int end_num, std::array<std::array<int, 14>, 8>& usage) {
    for(auto& row : usage) row.fill(0);
    for (int num = start_num; num <= end_num; ++num) {
        if (num >= MAX_PRECOMPUTE) break;
        const auto& data = DIGIT_TABLE[num];
        int len = data.len;
        std::vector<std::array<uint16_t, 8>> steps;
        steps.reserve(len);
        std::array<uint16_t, 8> current_mask = fb.bits[data.digits[len - 1]];
        steps.push_back(current_mask);
        bool possible = true;
        for (int i = len - 2; i >= 0; --i) {
            const auto& next_layer = fb.bits[data.digits[i]];
            std::array<uint16_t, 8> next_mask;
            bool any_bit = false;
            for (int r = 0; r < 8; ++r) {
                uint16_t dilated = (current_mask[r] << 1) | (current_mask[r] >> 1);
                if (r > 0) dilated |= current_mask[r-1] | (current_mask[r-1] << 1) | (current_mask[r-1] >> 1);
                if (r < 7) dilated |= current_mask[r+1] | (current_mask[r+1] << 1) | (current_mask[r+1] >> 1);
                uint16_t res = dilated & next_layer[r];
                next_mask[r] = res;
                if (res) any_bit = true;
            }
            if (!any_bit) { possible = false; break; }
            current_mask = next_mask;
            steps.push_back(current_mask);
        }
        if (!possible) continue;
        std::array<uint16_t, 8> valid_mask = steps.back();
        auto add_usage = [&](const std::array<uint16_t, 8>& mask) {
             for(int r=0; r<8; ++r) for(int c=0; c<14; ++c) if (mask[r] & (1<<c)) usage[r][c]++;
        };
        add_usage(valid_mask);
        for (int i = len - 2; i >= 0; --i) {
             std::array<uint16_t, 8> prev_valid;
             for (int r = 0; r < 8; ++r) {
                 uint16_t dilated = (valid_mask[r] << 1) | (valid_mask[r] >> 1);
                 if (r > 0) dilated |= valid_mask[r-1] | (valid_mask[r-1] << 1) | (valid_mask[r-1] >> 1);
                 if (r < 7) dilated |= valid_mask[r+1] | (valid_mask[r+1] << 1) | (valid_mask[r+1] >> 1);
                 prev_valid[r] = steps[i][r] & dilated;
             }
             valid_mask = prev_valid;
             add_usage(valid_mask);
        }
    }
}

void optimize_board_permutation(std::array<std::array<int, 14>, 8>& board) {
    long long current_score = get_basis_score_extended(board);
    bool improved = true;
    
    // Iterative First-Improvement Hill Climbing
    while (improved) {
        improved = false;
        for (int i = 0; i < 9; ++i) {
            for (int j = i + 1; j < 10; ++j) {
                // Swap digits i and j on the board
                for(auto& row : board) {
                    for(auto& val : row) {
                        if (val == i) val = j;
                        else if (val == j) val = i;
                    }
                }

                long long new_score = get_basis_score_extended(board);

                if (new_score > current_score) {
                    current_score = new_score;
                    improved = true;
                    // Keep the swap and continue (First Improvement)
                    // We could break here to restart the loops, but continuing is also fine 
                    // (effectively checking other pairs against the new baseline)
                } else {
                    // Revert swap
                    for(auto& row : board) {
                        for(auto& val : row) {
                            if (val == i) val = j;
                            else if (val == j) val = i;
                        }
                    }
                }
            }
        }
    }
}

void get_endpoints(const std::array<std::array<int, 14>, 8>& b, const uint8_t* target, int origin_len, int search_len, int current_idx, int y, int x, std::vector<std::pair<int, int>>& results) {
    if (results.size() > 1000) return;
    if (current_idx == search_len - 1) { results.push_back({y, x}); return; }
    int next_val = target[origin_len - 2 - current_idx];
    const auto& neighbors = ADJ_TABLE[y][x];
    for (int i = 0; i < neighbors.count; ++i) {
        int ny = neighbors.list[i].y, nx = neighbors.list[i].x;
        if (b[ny][nx] == next_val) get_endpoints(b, target, origin_len, search_len, current_idx + 1, ny, nx, results);
    }
}
