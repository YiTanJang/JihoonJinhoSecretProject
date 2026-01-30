#include "core/scoring.h"
#include "core/basis.h"
#include "legacy/common.h"
#include "utils/config.h"
#include <numeric>
#include <algorithm>
#include <cmath>

// Lookup tables and bucket logic
struct NumberInfo {
    int8_t type;
    int8_t bucket;
    int16_t partner;
};

static constexpr std::array<NumberInfo, 10000> create_lookup_table() {
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
static constexpr auto LOOKUP = create_lookup_table();

struct Info3 { int d1, d3; bool is_pal; int rev; };
static constexpr std::array<Info3, 1000> create_lookup3_table() {
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
static constexpr auto LOOKUP3 = create_lookup3_table();

static std::array<std::vector<int>, 10> bucket_members;

void init_richness_lookup() {
    for (int i = 0; i < 10000; ++i) {
        if (LOOKUP[i].type != 0) {
            bucket_members[LOOKUP[i].bucket].push_back(i);
        }
    }
}

// Traversal Implementations
void dfs_richness(int r, int c, int depth, int current_val, const BoardArray& board, RichnessOracle& oracle) {
    if (board[r][c] == -1) return;
    int next_val = current_val * 10 + board[r][c];
    if (depth == 4) oracle.mark(4, next_val);
    else if (depth == 5) { oracle.mark(5, next_val); return; }

    const auto& adj = ADJ_TABLE[r][c];
    for(int i = 0; i < adj.count; ++i) {
        dfs_richness(adj.list[i].y, adj.list[i].x, depth + 1, next_val, board, oracle);
    }
}

void dfs_richness_4d(int r, int c, int depth, int current_val, const BoardArray& board, RichnessOracle4D& oracle) {
    if (board[r][c] == -1) return;
    int next_val = current_val * 10 + board[r][c];
    if (depth == 3) oracle.mark(3, next_val);
    else if (depth == 4) { oracle.mark(4, next_val); return; }

    const auto& adj = ADJ_TABLE[r][c];
    for(int i = 0; i < adj.count; ++i) {
        dfs_richness_4d(adj.list[i].y, adj.list[i].x, depth + 1, next_val, board, oracle);
    }
}

long long calculate_unbiased_score_4d(const RichnessOracle4D& oracle) {
    struct Bucket { int capacity = 0, fill = 0, raw_count = 0; };
    std::array<Bucket, 10> buckets;

    for (int i = 0; i < 10000; ++i) {
        if (oracle.bits4.test(i) && LOOKUP[i].type != 0) buckets[LOOKUP[i].bucket].raw_count++;
    }

    std::vector<int> ranked_digits(10);
    std::iota(ranked_digits.begin(), ranked_digits.end(), 0);
    std::sort(ranked_digits.begin(), ranked_digits.end(), [&](int a, int b) {
        return buckets[a].raw_count > buckets[b].raw_count;
    });

    static thread_local std::bitset<10000> consumed;
    consumed.reset();

    for (int d : ranked_digits) {
        for (int i : bucket_members[d]) {
            const auto& info = LOOKUP[i];
            if (consumed.test(i)) continue;
            if (info.type == 1) {
                buckets[d].capacity++;
                if (oracle.bits4.test(i)) buckets[d].fill++;
                consumed.set(i);
            } else if (info.type == 2) {
                buckets[d].capacity++;
                if (oracle.bits4.test(i) || oracle.bits4.test(info.partner)) buckets[d].fill++;
                consumed.set(i); consumed.set(info.partner);
            } else if (info.type == 3) {
                buckets[d].capacity++;
                if (oracle.bits4.test(i)) buckets[d].fill++;
                consumed.set(i); consumed.set(info.partner);
            }
        }
    }

    // 3. Final Scoring (Sliding Window Weighting)
    struct RatedBucket {
        int fill;
        int capacity;
    };
    std::vector<RatedBucket> ranked_all;
    
    // Process Bucket -1 (3D) Debiasing
    int fill3 = 0, cap3 = 0;
    static thread_local std::bitset<1000> consumed3;
    consumed3.reset();
    for (int i = 0; i < 1000; ++i) {
        if (consumed3.test(i)) continue;
        const auto& inf = LOOKUP3[i];
        if (inf.is_pal) { 
            cap3++; if (oracle.bits3.test(i)) fill3++; consumed3.set(i);
        } else {
            cap3++; if (oracle.bits3.test(i) || oracle.bits3.test(inf.rev)) fill3++;
            consumed3.set(i); consumed3.set(inf.rev);
        }
    }
    ranked_all.push_back({fill3, cap3});
    
    for (int d : ranked_digits) {
        ranked_all.push_back({buckets[d].fill, buckets[d].capacity});
    }
    
    int first_fail_rank = -1;
    for (int r = 0; r < 11; ++r) {
        if (ranked_all[r].fill < ranked_all[r].capacity) {
            first_fail_rank = r; break;
        }
    }
    
    long long total_score = 0;
    for (int r = 0; r < 10; ++r) { // Skip 11th bucket (Rank 10)
        int weight_rank = (first_fail_rank != -1 && r > first_fail_rank) ? (r - first_fail_rank) : 0;
        long long i = 11 - weight_rank;
        long long w = i * i * i * i; // Strategy 3 (Quartic)
        total_score += (long long)ranked_all[r].fill * w;
    }
    return total_score;
}

long long get_richness_score_4d(const BoardArray& board) {
    RichnessOracle4D oracle;
    for(int r = 0; r < BOARD_ROWS; ++r) for(int c = 0; c < BOARD_COLS; ++c) dfs_richness_4d(r, c, 1, 0, board, oracle);
    return calculate_unbiased_score_4d(oracle);
}

long long get_richness_score_4d(const FastBoard& fb) {
    BoardArray board;
    for(int r=0; r<BOARD_ROWS; ++r) {
        for(int c=0; c<BOARD_COLS; ++c) {
            board[r][c] = 0;
            for(int v=0; v<10; ++v) if(fb.bits[v][r] & (1<<c)) { board[r][c] = v; break; }
        }
    }
    return get_richness_score_4d(board);
}

long long get_richness_score(const BoardArray& board) {
    RichnessOracle oracle;
    for(int r = 0; r < BOARD_ROWS; ++r) for(int c = 0; c < BOARD_COLS; ++c) dfs_richness(r, c, 1, 0, board, oracle);
    long long total_holes = 0;
    for (int d = 0; d < 10; ++d) {
        int start = d * 10000;
        for (int i = 0; i < 10000; ++i) if (!oracle.bits5.test(start + i)) total_holes++;
    }
    return 100000LL - total_holes;
}

bool can_make_bitboard(const FastBoard& fb, const uint8_t* target, int len) {
    int first_digit = target[len - 1];
    std::array<uint16_t, 8> current_mask = fb.bits[first_digit];
    bool any_init = false;
    for(int r=0; r<BOARD_ROWS; ++r) if(current_mask[r]) { any_init = true; break; }
    if(!any_init) return false;

    for (int i = len - 2; i >= 0; --i) {
        int next_digit = target[i];
        const auto& next_layer = fb.bits[next_digit];
        bool any_bit_set = false;
        std::array<uint16_t, 8> next_mask;
        for (int r = 0; r < BOARD_ROWS; ++r) {
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

BiasReport get_bias_report(const BoardArray& board) {
    RichnessOracle4D oracle;
    for(int r = 0; r < BOARD_ROWS; ++r) for(int c = 0; c < BOARD_COLS; ++c) dfs_richness_4d(r, c, 1, 0, board, oracle);
    
    BiasReport report = {};
    for (int i = 0; i < 1000; ++i) {
        report.cap_3d++;
        if (oracle.bits3.test(i)) report.fill_3d++;
    }
    
    for (int i = 0; i < 10000; ++i) {
        const auto& info = LOOKUP[i];
        int d1 = i/1000, d2 = (i/100)%10, d3 = (i/10)%10, d4 = i%10;
        int unique_mask = (1<<d1) | (1<<d2) | (1<<d3) | (1<<d4);
        int unique_count = 0;
        for(int k=0; k<10; ++k) if((unique_mask >> k) & 1) unique_count++;
        
        report.cap_4d++;
        report.cap_unique[unique_count]++;
        report.cap_type[info.type]++;
        
        int topo = 5; 
        bool eq13 = (d1 == d3), eq24 = (d2 == d4), eq14 = (d1 == d4);
        if (eq13 && eq24) topo = 2; 
        else if (eq13 || eq24) topo = 3; 
        else if (eq14) topo = 4; 
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

AllScores compute_all_scores(const FastBoard& fb) {
    AllScores res = {0, 0, 0, 0, 0, 0};
    BoardArray board;
    for(int r=0; r<BOARD_ROWS; ++r) {
        for(int c=0; c<BOARD_COLS; ++c) {
            board[r][c] = 0;
            for(int v=0; v<10; ++v) if(fb.bits[v][r] & (1<<c)) { board[r][c] = v; break; }
        }
    }
    // res.richness = get_richness_score(board);
    // res.richness_4d = get_richness_score_4d(board);
    res.richness = 0;
    res.richness_4d = 0;
    
    static thread_local std::array<bool, MAX_PRECOMPUTE> found_mask;
    found_mask.fill(false);
    for (const auto& item : SUM_TARGETS) {
        if (can_make_bitboard(fb, DIGIT_TABLE[item.num].digits, DIGIT_TABLE[item.num].len)) {
            res.sum += item.value;
            res.freq += item.value; // Approximate for now
            res.hybrid += item.value;
            res.hybrid_sqrt += item.value;
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

void optimize_board_permutation(BoardArray& board) {
    long long current_score = get_basis_score_extended(board);
    bool improved = true;
    while (improved) {
        improved = false;
        for (int i = 0; i < 9; ++i) {
            for (int j = i + 1; j < 10; ++j) {
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
                } else {
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

void get_endpoints(const BoardArray& b, const uint8_t* target, int origin_len, int search_len, int current_idx, int y, int x, std::vector<std::pair<int, int>>& results) {
    if (results.size() > 1000) return;
    if (current_idx == search_len - 1) { results.push_back({y, x}); return; }
    int next_val = target[origin_len - 2 - current_idx];
    const auto& neighbors = ADJ_TABLE[y][x];
    for (int i = 0; i < neighbors.count; ++i) {
        int ny = neighbors.list[i].y, nx = neighbors.list[i].x;
        if (b[ny][nx] == next_val) get_endpoints(b, target, origin_len, search_len, current_idx + 1, ny, nx, results);
    }
}
