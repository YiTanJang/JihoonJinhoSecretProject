#include <iostream>
#include <array>
#include "logic.h"

int main() {
    // Identity board (0-9)
    std::array<std::array<int, 14>, 8> board;
    for(int r=0; r<8; ++r) {
        for(int c=0; c<14; ++c) {
            board[r][c] = (r * 14 + c) % 10;
        }
    }

    int score = get_best_permutation_score(board);
    std::cout << "Best permutation score: " << score << std::endl;

    return 0;
}