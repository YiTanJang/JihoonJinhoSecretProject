#pragma once
#include "common.h"
#include <vector>
#include <array>
#include <random>

using PositionMap = std::array<std::vector<std::pair<int8_t, int8_t>>, 10>;

struct FastBoard {
    // bits[num][row]
    // num: 0~9
    // row: 0~7
    // value: 14 bits representing columns
    std::array<std::array<uint16_t, 8>, 10> bits;

    FastBoard() {
        for(auto& layer : bits) layer.fill(0);
    }
};


#include <bitset>

struct RichnessOracle {
    std::bitset<100000> bits5;
    std::bitset<10000> bits4;

    void mark(int len, int val) {
        if (len == 4) bits4.set(val);
        else if (len == 5) bits5.set(val);
    }
};

struct RichnessOracle4D {
    std::bitset<10000> bits4;
    std::bitset<1000> bits3;
    void mark(int len, int val) { if (len == 3) bits3.set(val); else if (len == 4) bits4.set(val); }
};

void dfs_richness(int r, int c, int depth, int current_val, const std::array<std::array<int, 14>, 8>& board, RichnessOracle& oracle);
void dfs_richness_4d(int r, int c, int depth, int current_val, const std::array<std::array<int, 14>, 8>& board, RichnessOracle4D& oracle);

// 지도 초기화/업데이트 헬퍼 함수
void rebuild_fast_board(const std::array<std::array<int, 14>, 8>& b, FastBoard& out_fb);
void update_fast_board(FastBoard& fb, int r, int c, int old_val, int new_val);


// [수정] 평가 함수들이 Map을 인자로 받음
bool can_make_bitboard(const FastBoard& fb, const uint8_t* target, int len);
int get_score_param_bit(const FastBoard& fb);
// Bitboard를 사용하는 Frequency Score (Symmetry 최적화 포함)
int get_frequency_score_bit(const FastBoard& fb);

// Calculate sum of all found numbers from 1 to 12999 (Symmetry optimized)
int get_sum_score(const FastBoard& fb);
int get_hybrid_score(const FastBoard& fb);
int get_hybrid_sqrt_score(const FastBoard& fb);
long long get_richness_score(const std::array<std::array<int, 14>, 8>& board);
long long get_richness_score_4d(const std::array<std::array<int, 14>, 8>& board);
long long get_richness_score_4d(const FastBoard& fb);
long long calculate_unbiased_score_4d(const RichnessOracle4D& oracle);

struct AllScores {
    int param;
    int freq;
    int sum;
    int hybrid;
    int hybrid_sqrt;
    long long richness;
};

struct BiasReport {
    int fill_3d;
    int cap_3d;
    int fill_4d;
    int cap_4d;
    
    // Breakdown by unique digit count (1, 2, 3, 4)
    int fill_unique[5]; // Index 0 unused, 1..4 used
    int cap_unique[5];
    
    // Breakdown by Type (0: Trivial, 1: Pal, 2: Cyc, 3: Std)
    int fill_type[4];
    int cap_type[4];

    // Breakdown by Topological Complexity
    // 2: PingPong (abab)
    // 3: Backtrack (abcb, abac)
    // 4: Cycle (abca)
    // 5: Linear (abcd, etc)
    int fill_topo[6]; 
    int cap_topo[6];
};

BiasReport get_bias_report(const std::array<std::array<int, 14>, 8>& board);

AllScores compute_all_scores(const FastBoard& fb);

// [신규] Cell Usage Heatmap 계산
void calculate_cell_usage(const FastBoard& fb, int start_num, int end_num, std::array<std::array<int, 14>, 8>& usage);

// [신규] Permute digits to maximize richness
void optimize_board_permutation(std::array<std::array<int, 14>, 8>& board);

// [신규] Basis Set Initialization & Scoring
void init_basis_set();
int get_basis_size();
long long get_basis_score(const RichnessOracle& oracle);
long long get_basis_score_4d(const RichnessOracle4D& oracle);
long long get_basis_score_extended(const std::array<std::array<int, 14>, 8>& board);

// [Optimization] Pre-compute lookups once at startup
void init_richness_lookup();

void get_endpoints(
    const std::array<std::array<int, 14>, 8>& b, 
    const uint8_t* target, 
    int origin_len, 
    int search_len, 
    int current_idx, 
    int y, int x, 
    std::vector<std::pair<int, int>>& results
);

