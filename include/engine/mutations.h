#pragma once
#include <vector>
#include <array>
#include <tuple>
#include <random>
#include "core/board.h"
#include "core/scoring.h"
#include "core/basis.h"

// --- Crossover ---
std::array<std::array<int, 14>, 8> crossover(const std::array<std::array<int, 14>, 8>& p1, const std::array<std::array<int, 14>, 8>& p2, std::mt19937& rng);

// --- Meta-Operators ---
std::vector<std::tuple<int, int, int>> apply_smart_mutation(std::array<std::array<int, 14>, 8>& board, int current_score, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_single_elite_patch(std::array<std::array<int, 14>, 8>& current_board, const std::array<std::array<int, 14>, 8>& donor_board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_lns_sequential(std::array<std::array<int, 14>, 8>& board, FastBoard& fb, int mode, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_greedy_optimize(std::array<std::array<int, 14>, 8>& board, FastBoard& fb, int mode, std::mt19937& rng);

// --- Composite Greedy Basis Operators (Extended) ---
std::vector<std::tuple<int, int, int>> apply_adj_random_mutation_greedy_optimize_basis(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_random_swap_greedy_optimize_basis(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);

// --- Basic Moves ---
std::vector<std::tuple<int, int, int>> apply_line_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_permutation(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_adjacent_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_patch_rotate(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_replace_redundant(std::array<std::array<int, 14>, 8>& board, int current_score, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_region_permutation(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_local_flip(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_heatmap_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_heatmap_domino_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_heatmap_mutate(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_least_used_digit_change(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_double_cell_mutation(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_triple_cycle_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_random_2_cell_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_random_global_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_random_cell_mutation(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_distance_1_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_distance_2_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_local_domino_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_global_domino_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_2x2_rotate(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_2x2_xwing_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_variable_block_rotate(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_variable_block_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_variable_block_flip(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_triangle_rotate(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_straight_slide(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_worm_slide(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_single_cell_mutation(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);

// --- Richness-based / Legacy Greedy (Declared for future use) ---
std::vector<std::tuple<int, int, int>> apply_greedy_optimize_4d(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_adj_random_mutation_greedy_optimize(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);
std::vector<std::tuple<int, int, int>> apply_random_swap_greedy_optimize(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng);