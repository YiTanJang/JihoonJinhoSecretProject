#include <iostream>
#include <array>
#include <vector>
#include "logic.h"
#include "common.h"

void test_board_v3(std::string label, std::array<std::array<int, 14>, 8> board) {
    std::cout << "\n--- " << label << " ---" << std::endl;
    int score = get_best_permutation_score(board);
    std::cout << "Resulting Score: " << score << std::endl;
}

int main() {
    // Test Case 1: Standard board with 0-9.
    // Score should be at least 9.
    std::array<std::array<int, 14>, 8> board_dense;
    for(int r=0; r<8; ++r) {
        for(int c=0; c<14; ++c) {
            board_dense[r][c] = (r*14 + c) % 10;
        }
    }
    test_board_v3("Dense Board (0-9 present)", board_dense);

    // Test Case 2: Only zeros.
    // 0 is present, but 1 is missing. Score should be 0 (if 1 is missing).
    // Wait, the score is "X if 1..X are reachable". 
    // If 1 is not reachable, score is 0.
    std::array<std::array<int, 14>, 8> board_zeros;
    for(auto& row : board_zeros) row.fill(0);
    test_board_v3("Only Zeros", board_zeros);

    return 0;
}
