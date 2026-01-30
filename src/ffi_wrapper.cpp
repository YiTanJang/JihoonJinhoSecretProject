#include "core/scoring.h"
#include "core/board.h"
#include "core/basis.h"
#include <array>
#include <mutex>

#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT
#endif

static std::once_flag init_flag;

extern "C" {
    // Exported function for Python to call
    EXPORT int calculate_score(const int* board_data) {
        std::array<std::array<int, 14>, 8> board;
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 14; ++j) {
                board[i][j] = board_data[i * 14 + j];
            }
        }

        FastBoard fb;
        rebuild_fast_board(board, fb);
        return get_score_param_bit(fb);
    }

    EXPORT int calculate_freq_score(const int* board_data) {
        std::array<std::array<int, 14>, 8> board;
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 14; ++j) {
                board[i][j] = board_data[i * 14 + j];
            }
        }

        FastBoard fb;
        rebuild_fast_board(board, fb);
        return get_frequency_score_bit(fb);
    }

EXPORT int calculate_sum_score(const int* board_data) {
    FastBoard fb;
    // ... logic to fill fb ...
    std::array<std::array<int, 14>, 8> b;
    for(int i=0; i<8; ++i) {
        for(int j=0; j<14; ++j) {
            b[i][j] = board_data[i*14 + j];
        }
    }
    rebuild_fast_board(b, fb);
    return get_sum_score(fb);
}

EXPORT int calculate_hybrid_score(const int* board_data) {
    FastBoard fb;
    std::array<std::array<int, 14>, 8> b;
    for(int i=0; i<8; ++i) {
        for(int j=0; j<14; ++j) {
            b[i][j] = board_data[i*14 + j];
        }
    }
    rebuild_fast_board(b, fb);
    return get_hybrid_score(fb);
}

EXPORT int calculate_hybrid_sqrt_score(const int* board_data) {
    FastBoard fb;
    std::array<std::array<int, 14>, 8> b;
    for(int i=0; i<8; ++i) {
        for(int j=0; j<14; ++j) {
            b[i][j] = board_data[i*14 + j];
        }
    }
    rebuild_fast_board(b, fb);
    return get_hybrid_sqrt_score(fb);
}

EXPORT int calculate_richness_score(const int* board_data) {
    std::array<std::array<int, 14>, 8> board;
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 14; ++j) {
            board[i][j] = board_data[i * 14 + j];
        }
    }
    return get_richness_score(board);
}

    EXPORT long long calculate_richness_score_4d(const int* board_data) {
        std::call_once(init_flag, init_richness_lookup);
        std::array<std::array<int, 14>, 8> board;
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 14; ++j) {
                board[i][j] = board_data[i * 14 + j];
            }
        }
        return get_richness_score_4d(board);
    }

    EXPORT void analyze_bias(const int* board_data, BiasReport* out_report) {
        std::call_once(init_flag, init_richness_lookup);
        std::array<std::array<int, 14>, 8> board;
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 14; ++j) {
                board[i][j] = board_data[i * 14 + j];
            }
        }
        *out_report = get_bias_report(board);
    }

    EXPORT long long calculate_basis_score_extended(const int* board_data) {
        std::call_once(init_flag, [](){
            init_richness_lookup();
            init_basis_set();
        });
        std::array<std::array<int, 14>, 8> board;
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 14; ++j) {
                board[i][j] = board_data[i * 14 + j];
            }
        }
        return get_basis_score_extended(board);
    }

    EXPORT long long optimize_board_and_score(int* board_data) {
        std::call_once(init_flag, [](){
            init_richness_lookup();
            init_basis_set();
        });

        std::array<std::array<int, 14>, 8> board;
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 14; ++j) {
                board[i][j] = board_data[i * 14 + j];
            }
        }

        optimize_board_permutation(board);

        // Copy back to buffer
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 14; ++j) {
                board_data[i * 14 + j] = board[i][j];
            }
        }

        return get_basis_score_extended(board);
    }

    EXPORT int get_basis_size_ffi() {
        std::call_once(init_flag, [](){
            init_richness_lookup();
            init_basis_set();
        });
        return get_basis_size();
    }

    EXPORT void get_basis_list_ffi(int* out_list) {
        std::call_once(init_flag, [](){
            init_richness_lookup();
            init_basis_set();
        });
        get_basis_list(out_list);
    }

    EXPORT void get_found_basis_flags_ffi(const int* board_data, int8_t* out_flags) {
        std::call_once(init_flag, [](){
            init_richness_lookup();
            init_basis_set();
        });
        std::array<std::array<int, 14>, 8> board;
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 14; ++j) {
                board[i][j] = board_data[i * 14 + j];
            }
        }
        get_found_basis_flags(board, out_flags);
    }
}
