#include <iostream>
#include <array>
#include <vector>
#include <numeric>
#include <algorithm>
#include <random>
#include "logic.h"
#include "common.h"

// Helper to print board
void print_board_simple(const std::array<std::array<int, 14>, 8>& b) {
    for(int r=0; r<8; ++r) {
        for(int c=0; c<14; ++c) {
            std::cout << b[r][c];
        }
        std::cout << "\n";
    }
}

void test_board(std::string label, std::array<std::array<int, 14>, 8> board) {
    std::cout << "\n--- Testing " << label << " ---" << std::endl;
    
    // 1. Calculate Identity Score (using the standard function)
    FastBoard fb;
    rebuild_fast_board(board, fb);
    int identity_score = get_score_param_bit(fb);
    std::cout << "Identity Score: " << identity_score << std::endl;

    // 2. Calculate Best Permutation Score
    int perm_score = get_best_permutation_score(board);
    std::cout << "Optimized Score: " << perm_score << std::endl;

    if (perm_score >= identity_score) {
        std::cout << "[PASS] Optimized score is >= Identity score." << std::endl;
    } else {
        std::cout << "[FAIL] Optimized score is LOWER than Identity score!" << std::endl;
    }
}

int main() {
    // Case 1: Ordered Board (01234...)
    // Likely has good identity score.
    std::array<std::array<int, 14>, 8> board_ordered;
    for(int r=0; r<8; ++r) {
        for(int c=0; c<14; ++c) {
            board_ordered[r][c] = (r * 14 + c) % 10;
        }
    }
    test_board("Ordered Board", board_ordered);

    // Case 2: Sparse/Bad Board (Repeated 0s and 1s)
    // Identity score should be low. Permutation should improve it significantly
    // by mapping 0/1 to other numbers or utilizing the 0-prefix trick.
    std::array<std::array<int, 14>, 8> board_sparse;
    for(auto& row : board_sparse) row.fill(1); // All 1s
    // Add a few 0s to allow 0-resolution if mapped correctly
    board_sparse[0][0] = 0; 
    test_board("Sparse Board (All 1s, one 0)", board_sparse);

    // Case 3: Random Board
    std::array<std::array<int, 14>, 8> board_random;
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(0, 9);
    for(auto& row : board_random) for(auto& v : row) v = dist(rng);
    test_board("Random Board", board_random);

    return 0;
}
