#include "core/board.h"

void rebuild_fast_board(const BoardArray& b, FastBoard& out_fb) {
    for(auto& layer : out_fb.bits) layer.fill(0);
    for(int r = 0; r < BOARD_ROWS; ++r) {
        for(int c = 0; c < BOARD_COLS; ++c) {
            out_fb.bits[b[r][c]][r] |= (1 << c);
        }
    }
}

void update_fast_board(FastBoard& fb, int r, int c, int old_val, int new_val) {
    if (old_val == new_val) return;
    fb.bits[old_val][r] &= ~(1 << c);
    fb.bits[new_val][r] |= (1 << c);
}
