#include <iostream>
#include <array>
#include <vector>
#include "logic.h"
#include "common.h"

void test_board_tiered(std::string label, std::array<std::array<int, 14>, 8> board) {
    std::cout << "\n--- " << label << " ---" << std::endl;
    int score = get_best_permutation_score(board);
    std::cout << "Resulting Score: " << score << std::endl;
}

int main() {
    // Test Case 1: Length 1 Bottleneck
    // A board missing the digit '5'. 
    // Score should be determined by Length 1 (0-9).
    std::array<std::array<int, 14>, 8> board_no_5;
    for(auto& row : board_no_5) row.fill(0); 
    // The only digit is 0. 1-9 are missing.
    // Permutation can map 0 to any other digit.
    // But since only 1 physical digit exists, we can only satisfy ONE semantic digit at length 1.
    // Best score should be 0 (if we map 0->1, 2 is missing. if we map 0->0, 1 is missing).
    test_board_tiered("Length 1 Bottleneck (Only 0s on board)", board_no_5);

    // Test Case 2: Length 2 Bottleneck (Leading Zeros)
    // Board has all digits 0-9, so Length 1 is satisfied.
    // But it has no adjacent digits, so Length 2 (00-99) is empty.
    std::array<std::array<int, 14>, 8> board_no_adj;
    for(int r=0; r<8; ++r) {
        for(int c=0; c<14; ++c) {
            // Checkerboard pattern to minimize adjacencies? 
            // Actually walk_board follows neighbors. If we place digits far apart:
            board_no_adj[r][c] = (r+c) % 10;
        }
    }
    // This board HAS adjacencies. Let's make one that has 0-9 but NO "00" "01" etc.
    // A simple way is to check the score of a board that has 0-9.
    test_board_tiered("Length 2 Check (Standard dense board)", board_no_adj);

    // Test Case 3: "05 is 05" logic
    // Create a board where physical sequence "0-5" exists but "5-0" does not.
    // And identity mapping has '50' missing.
    std::array<std::array<int, 14>, 8> board_path;
    for(auto& row : board_path) row.fill(1);
    board_path[0][0] = 0;
    board_path[0][1] = 5; 
    // Board has: 0, 5, 1. Adjacencies: (0,5), (5,0), (5,1), (1,5), (0,1), (1,0) etc.
    // Length 1: 0, 1, 5 present. 2,3,4,6,7,8,9 missing.
    // Permutation will try to fix Length 1 first.
    test_board_tiered("Path Verification (0-5 Path)", board_path);

    return 0;
}
