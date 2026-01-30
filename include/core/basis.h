#pragma once
#include "core/board.h"
#include "core/scoring.h"

void init_basis_set();
int get_basis_size();
void get_basis_list(int* out_list);
long long get_basis_score(const RichnessOracle& oracle);
long long get_basis_score_4d(const RichnessOracle4D& oracle);
long long get_basis_score_extended(const BoardArray& board);
double get_basis_score_with_twins(const BoardArray& board, double w_twin = 0.5, double w_double_twin = 0.25);
void get_basis_score_combined(const BoardArray& board, double w_twin, double w_double_twin, int& out_basis_count, double& out_weighted_score);
void calculate_fast_heatmap(const BoardArray& board, std::array<std::array<int, 14>, 8>& heatmap);
void get_heatmap_and_missing_weights(const BoardArray& board, std::array<std::array<int, 14>, 8>& heatmap, std::array<double, 10>& missing_weights);
void get_found_basis_flags(const BoardArray& board, int8_t* out_flags);

// Optimization: Precomputed validity flags for DFS pruning
//NODE_FLAGS[depth][value]
extern std::vector<uint8_t> NODE_FLAGS[6];
