#pragma once
#include <array>
#include <cstdint>
#include <vector>

// Board dimensions
constexpr int BOARD_ROWS = 8;
constexpr int BOARD_COLS = 14;

struct Coord { int8_t y; int8_t x; };
struct AdjList { int8_t count; Coord list[8]; };

/**
 * @brief Precomputed adjacency table for the 8x14 board.
 * Uses 8-way connectivity.
 */
consteval auto generate_adj_table() {
    std::array<std::array<AdjList, BOARD_COLS>, BOARD_ROWS> table{};
    constexpr int dy[] = {-1, -1, -1,  0,  0,  1, 1, 1};
    constexpr int dx[] = {-1,  0,  1, -1,  1, -1, 0, 1};

    for (int y = 0; y < BOARD_ROWS; ++y) {
        for (int x = 0; x < BOARD_COLS; ++x) {
            int cnt = 0;
            for (int i = 0; i < 8; ++i) {
                int ny = y + dy[i];
                int nx = x + dx[i];
                if (ny >= 0 && ny < BOARD_ROWS && nx >= 0 && nx < BOARD_COLS) {
                    table[y][x].list[cnt] = {static_cast<int8_t>(ny), static_cast<int8_t>(nx)};
                    cnt++;
                }
            }
            table[y][x].count = static_cast<int8_t>(cnt);
        }
    }
    return table;
}
constexpr auto ADJ_TABLE = generate_adj_table();

/**
 * @brief Bitboard representation of the board for fast sequence checking.
 * Each digit (0-9) has a 14-bit mask for each of the 8 rows.
 */
struct FastBoard {
    // bits[num][row]
    // num: 0~9, row: 0~7
    std::array<std::array<uint16_t, BOARD_ROWS>, 10> bits;

    FastBoard() {
        for(auto& layer : bits) layer.fill(0);
    }
};

using BoardArray = std::array<std::array<int, BOARD_COLS>, BOARD_ROWS>;

void rebuild_fast_board(const BoardArray& b, FastBoard& out_fb);
void update_fast_board(FastBoard& fb, int r, int c, int old_val, int new_val);
