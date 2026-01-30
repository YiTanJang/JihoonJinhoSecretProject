#pragma once
#include "core/board.h"
#include <bitset>
#include <vector>

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
    void mark(int len, int val) { 
        if (len == 3) bits3.set(val); 
        else if (len == 4) bits4.set(val); 
    }
};

struct BiasReport {
    int fill_3d;
    int cap_3d;
    int fill_4d;
    int cap_4d;
    int fill_unique[5];
    int cap_unique[5];
    int fill_type[4];
    int cap_type[4];
    int fill_topo[6]; 
    int cap_topo[6];
};

struct AllScores {
    int param;
    int freq;
    int sum;
    int hybrid;
    int hybrid_sqrt;
    long long richness;
    long long richness_4d;
};

// DFS Traversal Functions
void dfs_richness(int r, int c, int depth, int current_val, const BoardArray& board, RichnessOracle& oracle);
void dfs_richness_4d(int r, int c, int depth, int current_val, const BoardArray& board, RichnessOracle4D& oracle);

// Scoring Functions
long long get_richness_score(const BoardArray& board);
long long get_richness_score_4d(const BoardArray& board);
long long get_richness_score_4d(const FastBoard& fb);
long long calculate_unbiased_score_4d(const RichnessOracle4D& oracle);

// Bitboard Scoring
bool can_make_bitboard(const FastBoard& fb, const uint8_t* target, int len);
int get_score_param_bit(const FastBoard& fb);
int get_frequency_score_bit(const FastBoard& fb);
int get_sum_score(const FastBoard& fb);
int get_hybrid_score(const FastBoard& fb);
int get_hybrid_sqrt_score(const FastBoard& fb);

BiasReport get_bias_report(const BoardArray& board);
AllScores compute_all_scores(const FastBoard& fb);

void calculate_cell_usage(const FastBoard& fb, int start_num, int end_num, std::array<std::array<int, 14>, 8>& usage);
void optimize_board_permutation(BoardArray& board);
void get_endpoints(const BoardArray& b, const uint8_t* target, int origin_len, int search_len, int current_idx, int y, int x, std::vector<std::pair<int, int>>& results);

void init_richness_lookup();
