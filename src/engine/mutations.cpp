#include "engine/mutations.h"
#include "core/board.h"
#include "core/scoring.h"
#include "core/basis.h"
#include "legacy/common.h"
#include <algorithm>
#include <numeric>

// --- Mutation Operators ---

// Elite Patch: Copies a rectangular region from a high-scoring 'donor' board.
std::vector<std::tuple<int, int, int>> apply_single_elite_patch(
    std::array<std::array<int, 14>, 8>& current_board, 
    const std::array<std::array<int, 14>, 8>& donor_board,
    std::mt19937& rng
) {
    std::vector<std::tuple<int, int, int>> backup;
    std::uniform_int_distribution<int> dist_r(0, 6);
    std::uniform_int_distribution<int> dist_c(0, 11);
    std::uniform_int_distribution<int> dist_h(2, 4);
    std::uniform_int_distribution<int> dist_w(2, 4);
    int r_start = dist_r(rng);
    int c_start = dist_c(rng);
    int r_end = std::min(r_start + dist_h(rng), 8);
    int c_end = std::min(c_start + dist_w(rng), 14);
    for (int i = r_start; i < r_end; ++i) {
        for (int j = c_start; j < c_end; ++j) {
            if (current_board[i][j] != donor_board[i][j]) {
                backup.emplace_back(i, j, current_board[i][j]);
                current_board[i][j] = donor_board[i][j];
            }
        }
    }
    return backup;
}

// Sequential Greedy LNS: Optimizes a small 2x3 window by re-solving it cell by cell.
std::vector<std::tuple<int, int, int>> apply_lns_sequential(
    std::array<std::array<int, 14>, 8>& board, 
    FastBoard& fb, 
    int mode, 
    std::mt19937& rng
) {
    std::vector<std::tuple<int, int, int>> backup;
    std::uniform_int_distribution<int> dist_r(0, 6);
    std::uniform_int_distribution<int> dist_c(0, 11);
    int start_r = dist_r(rng);
    int start_c = dist_c(rng);
    struct Point { int r, c; };
    std::vector<Point> targets;
    for(int i=0; i<2; ++i) {
        for(int j=0; j<3; ++j) {
            int r = start_r + i;
            int c = start_c + j;
            targets.push_back({r, c});
            backup.emplace_back(r, c, board[r][c]);
        }
    }
    std::shuffle(targets.begin(), targets.end(), rng);
    for (const auto& p : targets) {
        int r = p.r; int c = p.c;
        int current_val_on_board = board[r][c]; 
        int best_val = current_val_on_board;
        long long best_score = -1;
        if (mode == 0) best_score = get_score_param_bit(fb);
        else if (mode == 1) best_score = get_frequency_score_bit(fb);
        else if (mode == 2) best_score = get_basis_score_extended(board);
        else best_score = get_sum_score(fb);
        for (int v = 0; v <= 9; ++v) {
            if (v == current_val_on_board) continue;
            update_fast_board(fb, r, c, current_val_on_board, v);
            board[r][c] = v;
            long long score = 0;
            if (mode == 0) score = get_score_param_bit(fb);
            else if (mode == 1) score = get_frequency_score_bit(fb);
            else if (mode == 2) score = get_basis_score_extended(board);
            else score = get_sum_score(fb);
            if (score > best_score) { best_score = score; best_val = v; }
            update_fast_board(fb, r, c, v, current_val_on_board);
            board[r][c] = current_val_on_board;
        }
        if (best_val != current_val_on_board) {
            update_fast_board(fb, r, c, current_val_on_board, best_val);
            board[r][c] = best_val;
        }
    }
    for (int i = (int)targets.size() - 1; i >= 0; --i) {
        int r = targets[i].r; int c = targets[i].c;
        int old_val = -1;
        for(const auto& b : backup) if(std::get<0>(b) == r && std::get<1>(b) == c) { old_val = std::get<2>(b); break; }
        update_fast_board(fb, r, c, board[r][c], old_val);
    }
    return backup;
}

// Crossover
std::array<std::array<int, 14>, 8> crossover(const std::array<std::array<int, 14>, 8>& p1, const std::array<std::array<int, 14>, 8>& p2, std::mt19937& rng) {
    std::array<std::array<int, 14>, 8> child = p1; 
    std::uniform_int_distribution<int> dist_r(0, 7), dist_c(0, 13);
    int r1 = dist_r(rng), r2 = dist_r(rng), c1 = dist_c(rng), c2 = dist_c(rng);
    int r_min = std::min(r1, r2), r_max = std::max(r1, r2), c_min = std::min(c1, c2), c_max = std::max(c1, c2);
    for(int i = r_min; i <= r_max; ++i) for(int j = c_min; j <= c_max; ++j) child[i][j] = p2[i][j];
    return child;
}

// Smart Mutation
std::vector<std::tuple<int, int, int>> apply_smart_mutation(std::array<std::array<int, 14>, 8>& board, int current_score, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup; 
    int target_num = current_score + 1;
    if (target_num >= MAX_PRECOMPUTE) return backup;
    const auto& data = DIGIT_TABLE[target_num];
    int first_digit = data.digits[data.len - 1];
    std::vector<std::pair<int, int>> starts;
    for(int r=0; r<8; ++r) for(int c=0; c<14; ++c) if(board[r][c] == first_digit) starts.push_back({r, c});
    if (starts.empty()) {
        int r = std::uniform_int_distribution<int>(0, 7)(rng), c = std::uniform_int_distribution<int>(0, 13)(rng);
        backup.emplace_back(r, c, board[r][c]); board[r][c] = first_digit; return backup;
    }
    std::vector<std::pair<int, int>> endpoints; int found_len = 0;
    for (int len = data.len - 1; len >= 1; --len) {
        endpoints.clear();
        for (auto [sr, sc] : starts) get_endpoints(board, data.digits, data.len, len, 0, sr, sc, endpoints);
        if (!endpoints.empty()) { found_len = len; break; }
    }
    if (endpoints.empty()) return backup;
    auto [ey, ex] = endpoints[std::uniform_int_distribution<size_t>(0, endpoints.size() - 1)(rng)];
    int next_val = data.digits[data.len - 1 - found_len];
    const auto& neighbors = ADJ_TABLE[ey][ex];
    auto [ny, nx] = neighbors.list[std::uniform_int_distribution<int>(0, neighbors.count - 1)(rng)];
    if (board[ny][nx] != next_val) { backup.emplace_back(ny, nx, board[ny][nx]); board[ny][nx] = next_val; }
    return backup; 
}

// Single Cell Greedy
std::vector<std::tuple<int, int, int>> apply_greedy_optimize(std::array<std::array<int, 14>, 8>& board, FastBoard& fb, int mode, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup;
    int r = std::uniform_int_distribution<int>(0, 7)(rng), c = std::uniform_int_distribution<int>(0, 13)(rng), original_val = board[r][c];
    backup.emplace_back(r, c, original_val);
    int best_val = original_val; long long best_local_score = -1;
    for (int v = 0; v <= 9; ++v) {
        update_fast_board(fb, r, c, original_val, v);
        long long score = 0;
        if (mode == 0) score = get_score_param_bit(fb);
        else if (mode == 1) score = get_frequency_score_bit(fb);
        else if (mode == 2) score = get_basis_score_extended(board);
        else score = get_sum_score(fb);
        if (score > best_local_score) { best_local_score = score; best_val = v; }
        update_fast_board(fb, r, c, v, original_val);
    }
    board[r][c] = best_val;
    return backup;
}

// Line Swap
std::vector<std::tuple<int, int, int>> apply_line_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup;
    if (std::uniform_real_distribution<double>(0.0, 1.0)(rng) < 0.5) {
        int r1 = std::uniform_int_distribution<int>(0, 7)(rng), r2 = std::uniform_int_distribution<int>(0, 7)(rng);
        while (r1 == r2) r2 = std::uniform_int_distribution<int>(0, 7)(rng);
        for (int c = 0; c < 14; ++c) { backup.emplace_back(r1, c, board[r1][c]); backup.emplace_back(r2, c, board[r2][c]); std::swap(board[r1][c], board[r2][c]); }
    } else {
        int c1 = std::uniform_int_distribution<int>(0, 13)(rng), c2 = std::uniform_int_distribution<int>(0, 13)(rng);
        while (c1 == c2) c2 = std::uniform_int_distribution<int>(0, 13)(rng);
        for (int r = 0; r < 8; ++r) { backup.emplace_back(r, c1, board[r][c1]); backup.emplace_back(r, c2, board[r][c2]); std::swap(board[r][c1], board[r][c2]); }
    }
    return backup;
}

// Permutation
std::vector<std::tuple<int, int, int>> apply_permutation(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup;
    std::array<int, 10> mapping; std::iota(mapping.begin(), mapping.end(), 0); std::shuffle(mapping.begin(), mapping.end(), rng);
    for (int r = 0; r < 8; ++r) for (int c = 0; c < 14; ++c) {
        int old_val = board[r][c], new_val = mapping[old_val];
        if (old_val != new_val) { backup.emplace_back(r, c, old_val); board[r][c] = new_val; }
    }
    return backup;
}

// Adjacent Swap
std::vector<std::tuple<int, int, int>> apply_adjacent_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup;
    int r = std::uniform_int_distribution<int>(0, 7)(rng), c = std::uniform_int_distribution<int>(0, 13)(rng), nr = r, nc = c;
    for(int k=0; k<10; ++k) {
        int dir = std::uniform_int_distribution<int>(0, 3)(rng);
        if (dir == 0) nr = r - 1; else if (dir == 1) nr = r + 1; else if (dir == 2) nc = c - 1; else nc = c + 1;
        if (nr >= 0 && nr < 8 && nc >= 0 && nc < 14) break;
        nr = r; nc = c; 
    }
    if (nr == r && nc == c) return backup;
    if (board[r][c] != board[nr][nc]) { backup.emplace_back(r, c, board[r][c]); backup.emplace_back(nr, nc, board[nr][nc]); std::swap(board[r][c], board[nr][nc]); }
    return backup;
}

// Patch Rotate
std::vector<std::tuple<int, int, int>> apply_patch_rotate(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup;
    int type = std::uniform_int_distribution<int>(0, 3)(rng);
    int h = (type == 0) ? 2 : (type == 1) ? 2 : (type == 2) ? 2 : 3, w = (type == 0) ? 2 : (type == 1) ? 3 : (type == 2) ? 4 : 3;
    int r_start = std::uniform_int_distribution<int>(0, 8 - h)(rng), c_start = std::uniform_int_distribution<int>(0, 14 - w)(rng);
    std::vector<std::pair<int, int>> coords;
    for (int c = 0; c < w - 1; ++c) coords.push_back({r_start, c_start + c});
    for (int r = 0; r < h - 1; ++r) coords.push_back({r_start + r, c_start + w - 1});
    for (int c = 0; c < w - 1; ++c) coords.push_back({r_start + h - 1, c_start + w - 1 - c});
    for (int r = 0; r < h - 1; ++r) coords.push_back({r_start + h - 1 - r, c_start});
    if (coords.empty()) return backup;
    std::vector<int> values; for (auto [r, c] : coords) { values.push_back(board[r][c]); backup.emplace_back(r, c, board[r][c]); }
    bool is_cw = (std::uniform_int_distribution<int>(0, 1)(rng) == 0); int n = values.size();
    for (int i = 0; i < n; ++i) { int src = is_cw ? (i - 1 + n) % n : (i + 1) % n; board[coords[i].first][coords[i].second] = values[src]; }
    return backup;
}

// Replace Redundant
std::vector<std::tuple<int, int, int>> apply_replace_redundant(std::array<std::array<int, 14>, 8>& board, int current_score, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup;
    std::array<std::array<int, 14>, 8> heatmap;
    calculate_fast_heatmap(board, heatmap);
    
    int min_u = 2147483647; 
    std::vector<std::pair<int, int>> candidates;
    for(int r=0; r<8; ++r) {
        for(int c=0; c<14; ++c) {
            if (heatmap[r][c] < min_u) {
                min_u = heatmap[r][c];
                candidates = {{r, c}};
            } else if (heatmap[r][c] == min_u) {
                candidates.push_back({r, c});
            }
        }
    }
    
    if (candidates.empty()) return backup;
    auto [tr, tc] = candidates[std::uniform_int_distribution<size_t>(0, candidates.size() - 1)(rng)];
    int old_v = board[tr][tc], new_v = std::uniform_int_distribution<int>(0, 9)(rng);
    while(new_v == old_v) new_v = std::uniform_int_distribution<int>(0, 9)(rng);
    
    backup.emplace_back(tr, tc, old_v); 
    board[tr][tc] = new_v;
    return backup;
}

// Helper for Rank-Based Selection (Linear Rank)
// Selects a cell with probability inversely proportional to its heatmap rank (lower heatmap val -> higher prob)
std::pair<int, int> select_low_heatmap_cell(const std::array<std::array<int, 14>, 8>& heatmap, std::mt19937& rng) {
    static thread_local std::vector<std::tuple<int, int, int>> candidates;
    candidates.clear();
    candidates.reserve(112);

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 14; ++c) {
            candidates.emplace_back(heatmap[r][c], r, c);
        }
    }

    // Sort by heatmap value (Ascending: Smallest first)
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b){
        return std::get<0>(a) < std::get<0>(b);
    });

    // Linear Rank Selection: P(i) ~ (N - i)
    // Total weight = N*(N+1)/2. 
    // Random integer X in [0, Total-1]. Find k such that sum_{0..k} w_j > X.
    int N = 112;
    long long total_weight = (long long)N * (N + 1) / 2;
    std::uniform_int_distribution<long long> dist(0, total_weight - 1);
    long long choice = dist(rng);

    // Inverse mapping to find index
    // Sum of first k terms (0 to k) of (N-i) series:
    // S_k = (k+1)*N - k*(k+1)/2. 
    // We want smallest k where S_k > choice.
    // Iterating is fast enough for N=112.
    
    long long current_sum = 0;
    int selected_idx = 0;
    for (int i = 0; i < N; ++i) {
        current_sum += (N - i);
        if (current_sum > choice) {
            selected_idx = i;
            break;
        }
    }

    auto& sel = candidates[selected_idx];
    return {std::get<1>(sel), std::get<2>(sel)};
}

std::vector<std::tuple<int, int, int>> apply_heatmap_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup;
    std::array<std::array<int, 14>, 8> heatmap;
    calculate_fast_heatmap(board, heatmap);

    // 1. Select First Source (Useless cell) via Rank-Based Selection
    auto [r1, c1] = select_low_heatmap_cell(heatmap, rng);

    // 2. Select Second Source (Useless cell) via Rank-Based Selection with Retries
    int r2, c2;
    bool found = false;
    for (int retry = 0; retry < 10; ++retry) {
        auto p2 = select_low_heatmap_cell(heatmap, rng);
        r2 = p2.first; c2 = p2.second;
        
        // Ensure distinct coordinates AND distinct values
        if ((r1 != r2 || c1 != c2) && board[r1][c1] != board[r2][c2]) {
            found = true;
            break;
        }
    }

    if (!found) return {};

    backup.emplace_back(r1, c1, board[r1][c1]);
    backup.emplace_back(r2, c2, board[r2][c2]);
    std::swap(board[r1][c1], board[r2][c2]);

    return backup;
}

std::vector<std::tuple<int, int, int>> apply_heatmap_domino_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup;
    std::array<std::array<int, 14>, 8> heatmap;
    calculate_fast_heatmap(board, heatmap);

    // Helper to find a valid neighbor for a domino (r,c) -> (rn, cn)
    auto get_random_neighbor = [&](int r, int c) -> std::pair<int, int> {
        const auto& adj = ADJ_TABLE[r][c];
        if (adj.count == 0) return {-1, -1};
        auto n = adj.list[std::uniform_int_distribution<int>(0, adj.count - 1)(rng)];
        return {n.y, n.x};
    };

    // 1. Select First Source Domino (Rank-Based on Heatmap)
    // We pick the primary cell via heatmap, then pick a random neighbor to form the domino.
    auto [r1, c1] = select_low_heatmap_cell(heatmap, rng);
    auto [r1n, c1n] = get_random_neighbor(r1, c1);
    if (r1n == -1) return {};

    // 2. Select Second Target Domino (Rank-Based on Heatmap)
    int r2, c2, r2n, c2n;
    bool found = false;
    for (int retry = 0; retry < 10; ++retry) {
        auto p2 = select_low_heatmap_cell(heatmap, rng);
        r2 = p2.first; c2 = p2.second;
        
        // Find neighbor
        auto n2 = get_random_neighbor(r2, c2);
        r2n = n2.first; c2n = n2.second;
        if (r2n == -1) continue;

        // Check Overlap: (r1,c1) vs (r2,c2) and neighbors
        if (!((r2 == r1 && c2 == c1) || (r2 == r1n && c2 == c1n) ||
              (r2n == r1 && c2n == c1) || (r2n == r1n && c2n == c1n))) {
            found = true;
            break;
        }
    }

    if (!found) return {};

    // Perform Swap (2 pairs)
    backup.emplace_back(r1, c1, board[r1][c1]);
    backup.emplace_back(r1n, c1n, board[r1n][c1n]);
    backup.emplace_back(r2, c2, board[r2][c2]);
    backup.emplace_back(r2n, c2n, board[r2n][c2n]);

    std::swap(board[r1][c1], board[r2][c2]);
    std::swap(board[r1n][c1n], board[r2n][c2n]);

    return backup;
}

std::vector<std::tuple<int, int, int>> apply_heatmap_mutate(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup;
    std::array<std::array<int, 14>, 8> heatmap;
    std::array<double, 10> missing_weights;
    get_heatmap_and_missing_weights(board, heatmap, missing_weights);

    // 1. Select Target (Useless cell) via Rank-Based Selection
    auto [target_r, target_c] = select_low_heatmap_cell(heatmap, rng);

    // 2. Weighted sampling for new digit
    std::discrete_distribution<int> dist(missing_weights.begin(), missing_weights.end());
    int new_val = dist(rng);
    if (new_val == board[target_r][target_c]) {
        // Fallback to random if weighted sample picked same digit
        new_val = (new_val + 1 + std::uniform_int_distribution<int>(0, 8)(rng)) % 10;
    }

    backup.emplace_back(target_r, target_c, board[target_r][target_c]);
    board[target_r][target_c] = new_val;

    return backup;
}

// Basic Moves
std::vector<std::tuple<int, int, int>> apply_random_global_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    int r1 = std::uniform_int_distribution<int>(0, 7)(rng), c1 = std::uniform_int_distribution<int>(0, 13)(rng), r2 = std::uniform_int_distribution<int>(0, 7)(rng), c2 = std::uniform_int_distribution<int>(0, 13)(rng);
    while (r1 == r2 && c1 == c2) { r2 = std::uniform_int_distribution<int>(0, 7)(rng); c2 = std::uniform_int_distribution<int>(0, 13)(rng); }
    std::vector<std::tuple<int, int, int>> backup = {{r1, c1, board[r1][c1]}, {r2, c2, board[r2][c2]}};
    std::swap(board[r1][c1], board[r2][c2]); return backup;
}
std::vector<std::tuple<int, int, int>> apply_random_cell_mutation(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    int r = std::uniform_int_distribution<int>(0, 7)(rng), c = std::uniform_int_distribution<int>(0, 13)(rng), v = std::uniform_int_distribution<int>(0, 9)(rng);
    std::vector<std::tuple<int, int, int>> backup = {{r, c, board[r][c]}}; board[r][c] = v; return backup;
}
std::vector<std::tuple<int, int, int>> apply_2x2_rotate(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    int r = std::uniform_int_distribution<int>(0, 6)(rng), c = std::uniform_int_distribution<int>(0, 12)(rng);
    std::vector<std::tuple<int, int, int>> backup = {{r, c, board[r][c]}, {r, c+1, board[r][c+1]}, {r+1, c, board[r+1][c]}, {r+1, c+1, board[r+1][c+1]}};
    int temp = board[r][c]; board[r][c] = board[r+1][c]; board[r+1][c] = board[r+1][c+1]; board[r+1][c+1] = board[r][c+1]; board[r][c+1] = temp; return backup;
}
std::vector<std::tuple<int, int, int>> apply_2x2_xwing_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    int r = std::uniform_int_distribution<int>(0, 6)(rng), c = std::uniform_int_distribution<int>(0, 12)(rng);
    std::vector<std::tuple<int, int, int>> backup = {{r, c, board[r][c]}, {r, c+1, board[r][c+1]}, {r+1, c, board[r+1][c]}, {r+1, c+1, board[r+1][c+1]}};
    std::swap(board[r][c], board[r+1][c+1]); std::swap(board[r][c+1], board[r+1][c]); return backup;
}
std::vector<std::tuple<int, int, int>> apply_triangle_rotate(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    int r = std::uniform_int_distribution<int>(0, 6)(rng), c = std::uniform_int_distribution<int>(0, 12)(rng), type = std::uniform_int_distribution<int>(0, 3)(rng);
    std::vector<std::pair<int, int>> coords = (type == 0) ? std::vector<std::pair<int,int>>{{r,c},{r+1,c},{r,c+1}} : (type == 1) ? std::vector<std::pair<int,int>>{{r,c},{r,c+1},{r+1,c+1}} : (type == 2) ? std::vector<std::pair<int,int>>{{r,c},{r+1,c},{r+1,c+1}} : std::vector<std::pair<int,int>>{{r+1,c},{r,c+1},{r+1,c+1}};
    std::vector<std::tuple<int, int, int>> backup; for (auto p : coords) backup.emplace_back(p.first, p.second, board[p.first][p.second]);
    int temp = board[coords[0].first][coords[0].second]; board[coords[0].first][coords[0].second] = board[coords[1].first][coords[1].second]; board[coords[1].first][coords[1].second] = board[coords[2].first][coords[2].second]; board[coords[2].first][coords[2].second] = temp; return backup;
}
std::vector<std::tuple<int, int, int>> apply_straight_slide(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    int lengths[] = {3,3,3,3,4,4,4,4,5,5,5,6,6,7,7,8}; 
    int widths[] = {1,1,1,1,1,2,2,3};
    int len = lengths[std::uniform_int_distribution<int>(0, 15)(rng)];
    int width = widths[std::uniform_int_distribution<int>(0, 7)(rng)];

    std::vector<std::tuple<int, int, int>> backup;
    bool is_horizontal = std::uniform_int_distribution<int>(0, 1)(rng) == 0;
    bool fwd = std::uniform_int_distribution<int>(0, 1)(rng) == 0;

    if (is_horizontal) {
        // Straight Horizontal Slide: W rows, L columns
        if (width > 8) width = 8;
        if (len > 14) len = 14;
        int rs = std::uniform_int_distribution<int>(0, 8 - width)(rng);
        int cs = std::uniform_int_distribution<int>(0, 14 - len)(rng);

        for (int i = 0; i < width; ++i) {
            int r = rs + i;
            std::vector<int> row_vals;
            for (int j = 0; j < len; ++j) {
                int c = cs + j;
                backup.emplace_back(r, c, board[r][c]);
                row_vals.push_back(board[r][c]);
            }
            for (int j = 0; j < len; ++j) {
                int target_c = cs + j;
                int src_idx = fwd ? (j - 1 + len) % len : (j + 1) % len;
                board[r][target_c] = row_vals[src_idx];
            }
        }
    } else {
        // Straight Vertical Slide: L rows, W columns
        if (len > 8) len = 8;
        if (width > 14) width = 14;
        int rs = std::uniform_int_distribution<int>(0, 8 - len)(rng);
        int cs = std::uniform_int_distribution<int>(0, 14 - width)(rng);

        for (int j = 0; j < width; ++j) {
            int c = cs + j;
            std::vector<int> col_vals;
            for (int i = 0; i < len; ++i) {
                int r = rs + i;
                backup.emplace_back(r, c, board[r][c]);
                col_vals.push_back(board[r][c]);
            }
            for (int i = 0; i < len; ++i) {
                int target_r = rs + i;
                int src_idx = fwd ? (i - 1 + len) % len : (i + 1) % len;
                board[target_r][c] = col_vals[src_idx];
            }
        }
    }

    return backup;
}
std::vector<std::tuple<int, int, int>> apply_variable_block_rotate(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    auto g1 = [](auto& g) { int c[] = {2,2,2,3,3,4}; return c[std::uniform_int_distribution<int>(0, 5)(g)]; };
    auto g2 = [](auto& g) { return std::uniform_int_distribution<int>(2, 6)(g); };
    int d1 = g1(rng), d2 = g2(rng), h, w; 
    if (std::uniform_int_distribution<int>(0, 1)(rng) == 0) { h = d1; w = d2; } else { h = d2; w = d1; }
    int rs = std::uniform_int_distribution<int>(0, 8-h)(rng);
    int cs = std::uniform_int_distribution<int>(0, 14-w)(rng);
    
    std::vector<std::pair<int, int>> ring;
    for(int j=0; j<w; ++j) ring.push_back({rs, cs+j});
    for(int i=1; i<h; ++i) ring.push_back({rs+i, cs+w-1});
    for(int j=1; j<w; ++j) ring.push_back({rs+h-1, cs+w-1-j});
    for(int i=1; i<h-1; ++i) ring.push_back({rs+h-1-i, cs});
    
    int n = (int)ring.size();
    if (n < 2) return {};
    
    // Shift restricted to 1 or 2 positions
    int k = std::uniform_int_distribution<int>(1, std::min(2, n - 1))(rng);
    if (std::uniform_int_distribution<int>(0, 1)(rng) == 0) k = n - k; // Flip direction (CCW)

    std::vector<std::tuple<int, int, int>> backup;
    std::vector<int> vals; vals.reserve(n);
    for(auto p : ring) {
        vals.push_back(board[p.first][p.second]);
        backup.emplace_back(p.first, p.second, board[p.first][p.second]);
    }
    
    for(int i=0; i<n; ++i) {
        board[ring[i].first][ring[i].second] = vals[(i - k + n) % n];
    }
    return backup;
}
std::vector<std::tuple<int, int, int>> apply_variable_block_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    auto g1 = [](auto& g) { int v = std::uniform_int_distribution<int>(0, 5)(g); return (v <= 2) ? 1 : (v <= 4) ? 2 : 3; };
    auto g2 = [](auto& g) { return std::uniform_int_distribution<int>(2, 6)(g); };
    int d1 = g1(rng), d2 = g2(rng), h, w; if (std::uniform_int_distribution<int>(0, 1)(rng) == 0) { h = d1; w = d2; } else { h = d2; w = d1; }
    int r1 = std::uniform_int_distribution<int>(0, 8-h)(rng), c1 = std::uniform_int_distribution<int>(0, 14-w)(rng), r2 = std::uniform_int_distribution<int>(0, 8-h)(rng), c2 = std::uniform_int_distribution<int>(0, 14-w)(rng);
    
    // Safety Check: Prevent overlap
    if (! (r1 >= r2+h || r2 >= r1+h || c1 >= c2+w || c2 >= c1+w)) return {};

    std::vector<std::tuple<int, int, int>> backup;
    for (int i = 0; i < h; ++i) for (int j = 0; j < w; ++j) { backup.emplace_back(r1+i, c1+j, board[r1+i][c1+j]); backup.emplace_back(r2+i, c2+j, board[r2+i][c2+j]); std::swap(board[r1+i][c1+j], board[r2+i][c2+j]); }
    return backup;
}

std::vector<std::tuple<int, int, int>> apply_variable_block_flip(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    auto g1 = [](auto& g) { int c[] = {2,2,2,3,3,4}; return c[std::uniform_int_distribution<int>(0, 5)(g)]; };
    auto g2 = [](auto& g) { return std::uniform_int_distribution<int>(2, 6)(g); };
    int d1 = g1(rng), d2 = g2(rng), h, w; 
    if (std::uniform_int_distribution<int>(0, 1)(rng) == 0) { h = d1; w = d2; } else { h = d2; w = d1; }
    int rs = std::uniform_int_distribution<int>(0, 8-h)(rng), cs = std::uniform_int_distribution<int>(0, 14-w)(rng);
    
    std::vector<std::tuple<int, int, int>> backup;
    int type = std::uniform_int_distribution<int>(0, 1)(rng); // 0: Vert, 1: Horz
    
    if (type == 0) { // Vertical Flip
        for (int i = 0; i < h / 2; ++i) {
            for (int j = 0; j < w; ++j) {
                int r_top = rs + i;
                int r_bot = rs + h - 1 - i;
                int c = cs + j;
                backup.emplace_back(r_top, c, board[r_top][c]);
                backup.emplace_back(r_bot, c, board[r_bot][c]);
                std::swap(board[r_top][c], board[r_bot][c]);
            }
        }
    } else { // Horizontal Flip
        for (int i = 0; i < h; ++i) {
            for (int j = 0; j < w / 2; ++j) {
                int r = rs + i;
                int c_left = cs + j;
                int c_right = cs + w - 1 - j;
                backup.emplace_back(r, c_left, board[r][c_left]);
                backup.emplace_back(r, c_right, board[r][c_right]);
                std::swap(board[r][c_left], board[r][c_right]);
            }
        }
    }
    return backup;
}
std::vector<std::tuple<int, int, int>> apply_local_domino_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup;
    
    // 1. Perform a 4-length non-overlapping random walk
    struct Point { int r, c; };
    std::vector<Point> walk;
    
    int curr_r = std::uniform_int_distribution<int>(0, 7)(rng);
    int curr_c = std::uniform_int_distribution<int>(0, 13)(rng);
    walk.push_back({curr_r, curr_c});
    
    for (int step = 0; step < 3; ++step) {
        const auto& adj = ADJ_TABLE[curr_r][curr_c];
        std::vector<Point> valid_neighbors;
        for (int i = 0; i < adj.count; ++i) {
            bool already_in_walk = false;
            for (const auto& p : walk) {
                if (p.r == adj.list[i].y && p.c == adj.list[i].x) {
                    already_in_walk = true;
                    break;
                }
            }
            if (!already_in_walk) {
                valid_neighbors.push_back({adj.list[i].y, adj.list[i].x});
            }
        }
        
        if (valid_neighbors.empty()) return {}; // Failed to find non-overlapping walk
        
        int next_idx = std::uniform_int_distribution<int>(0, (int)valid_neighbors.size() - 1)(rng);
        curr_r = valid_neighbors[next_idx].r;
        curr_c = valid_neighbors[next_idx].c;
        walk.push_back({curr_r, curr_c});
    }
    
    // 2. Perform Swaps
    for (const auto& p : walk) backup.emplace_back(p.r, p.c, board[p.r][p.c]);
    
    if (std::uniform_int_distribution<int>(0, 1)(rng) == 0) {
        // Half the time: swap (1st, 3rd) and (2nd, 4th)
        std::swap(board[walk[0].r][walk[0].c], board[walk[2].r][walk[2].c]);
        std::swap(board[walk[1].r][walk[1].c], board[walk[3].r][walk[3].c]);
    } else {
        // Half the time: swap (1st, 4th) and (2nd, 3rd)
        std::swap(board[walk[0].r][walk[0].c], board[walk[3].r][walk[3].c]);
        std::swap(board[walk[1].r][walk[1].c], board[walk[2].r][walk[2].c]);
    }
    
    return backup;
}

std::vector<std::tuple<int, int, int>> apply_global_domino_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup;
    std::uniform_int_distribution<int> dist_r(0, 7);
    std::uniform_int_distribution<int> dist_c(0, 13);

    // 1. Pick first domino (r1,c1) and its neighbor (r1n,c1n)
    int r1 = dist_r(rng);
    int c1 = dist_c(rng);
    const auto& adj1 = ADJ_TABLE[r1][c1];
    if (adj1.count == 0) return {}; 
    auto n1 = adj1.list[std::uniform_int_distribution<int>(0, adj1.count - 1)(rng)];
    int r1n = n1.y;
    int c1n = n1.x;

    // 2. Pick second domino (r2,c2) and its neighbor (r2n,c2n) - Retry on overlap
    int r2, c2, r2n, c2n;
    while (true) {
        r2 = dist_r(rng);
        c2 = dist_c(rng);
        const auto& adj2 = ADJ_TABLE[r2][c2];
        if (adj2.count == 0) continue;
        auto n2 = adj2.list[std::uniform_int_distribution<int>(0, adj2.count - 1)(rng)];
        r2n = n2.y;
        c2n = n2.x;

        // Check Overlap
        if (!((r2 == r1 && c2 == c1) || (r2 == r1n && c2 == c1n) ||
              (r2n == r1 && c2n == c1) || (r2n == r1n && c2n == c1n))) {
            break;
        }
    }

    // 4. Perform Swap
    backup.emplace_back(r1, c1, board[r1][c1]);
    backup.emplace_back(r1n, c1n, board[r1n][c1n]);
    backup.emplace_back(r2, c2, board[r2][c2]);
    backup.emplace_back(r2n, c2n, board[r2n][c2n]);

    std::swap(board[r1][c1], board[r2][c2]);
    std::swap(board[r1n][c1n], board[r2n][c2n]);

    return backup;
}

std::vector<std::tuple<int, int, int>> apply_distance_1_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    int r = std::uniform_int_distribution<int>(0, 7)(rng), c = std::uniform_int_distribution<int>(0, 13)(rng);
    const auto& adj = ADJ_TABLE[r][c]; if (adj.count == 0) return {};
    auto n = adj.list[std::uniform_int_distribution<int>(0, adj.count - 1)(rng)];
    if (board[r][c] == board[n.y][n.x]) return {};
    std::vector<std::tuple<int, int, int>> backup = {{r, c, board[r][c]}, {n.y, n.x, board[n.y][n.x]}};
    std::swap(board[r][c], board[n.y][n.x]); return backup;
}

std::vector<std::tuple<int, int, int>> apply_distance_2_swap(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    int r1 = std::uniform_int_distribution<int>(0, 7)(rng), c1 = std::uniform_int_distribution<int>(0, 13)(rng);
    std::pair<int, int> moves[] = {{0,2},{0,-2},{2,0},{-2,0},{2,2},{2,-2},{-2,2},{-2,-2},{1,2},{1,-2},{-1,2},{-1,-2},{2,1},{2,-1},{-2,1},{-2,-1}};
    std::shuffle(std::begin(moves), std::end(moves), rng);
    for (auto [dr, dc] : moves) {
        int r2 = r1 + dr, c2 = c1 + dc;
        if (r2 >= 0 && r2 < 8 && c2 >= 0 && c2 < 14) {
            if (board[r1][c1] == board[r2][c2]) continue;
            std::vector<std::tuple<int, int, int>> backup = {{r1, c1, board[r1][c1]}, {r2, c2, board[r2][c2]}};
            std::swap(board[r1][c1], board[r2][c2]); return backup;
        }
    }
    return {};
}

std::vector<std::tuple<int, int, int>> apply_worm_slide(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup;
    
    // 1. Determine walk length (3-8, biased towards 3 and 4)
    int lengths[] = {3, 3, 3, 4, 4, 4, 5, 5, 6, 7, 8};
    int len = lengths[std::uniform_int_distribution<int>(0, 10)(rng)];
    
    // 2. Perform non-overlapping random walk
    struct Point { int r, c; };
    std::vector<Point> walk;
    int curr_r = std::uniform_int_distribution<int>(0, 7)(rng);
    int curr_c = std::uniform_int_distribution<int>(0, 13)(rng);
    walk.push_back({curr_r, curr_c});
    
    for (int step = 1; step < len; ++step) {
        const auto& adj = ADJ_TABLE[curr_r][curr_c];
        std::vector<Point> valid_neighbors;
        for (int i = 0; i < adj.count; ++i) {
            bool already_in_walk = false;
            for (const auto& p : walk) {
                if (p.r == adj.list[i].y && p.c == adj.list[i].x) {
                    already_in_walk = true;
                    break;
                }
            }
            if (!already_in_walk) valid_neighbors.push_back({adj.list[i].y, adj.list[i].x});
        }
        
        if (valid_neighbors.empty()) break; // End walk early if stuck
        
        int next_idx = std::uniform_int_distribution<int>(0, (int)valid_neighbors.size() - 1)(rng);
        curr_r = valid_neighbors[next_idx].r;
        curr_c = valid_neighbors[next_idx].c;
        walk.push_back({curr_r, curr_c});
    }
    
    if (walk.size() < 2) return {};
    int final_len = (int)walk.size();
    
    // 3. Determine slide amount (1 or 2)
    int k = std::uniform_int_distribution<int>(1, std::min(2, final_len - 1))(rng);
    if (std::uniform_int_distribution<int>(0, 1)(rng) == 0) k = final_len - k; // Direction flip
    
    // 4. Apply slide
    std::vector<int> vals; vals.reserve(final_len);
    for (const auto& p : walk) {
        vals.push_back(board[p.r][p.c]);
        backup.emplace_back(p.r, p.c, board[p.r][p.c]);
    }
    
    for (int i = 0; i < final_len; ++i) {
        board[walk[i].r][walk[i].c] = vals[(i - k + final_len) % final_len];
    }
    
    return backup;
}

std::vector<std::tuple<int, int, int>> apply_single_cell_mutation(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    int r = std::uniform_int_distribution<int>(0, 7)(rng), c = std::uniform_int_distribution<int>(0, 13)(rng), v = std::uniform_int_distribution<int>(0, 9)(rng);
    while (v == board[r][c]) v = std::uniform_int_distribution<int>(0, 9)(rng);
    std::vector<std::tuple<int, int, int>> backup = {{r, c, board[r][c]}}; board[r][c] = v; return backup;
}

// --- EXTENDED BASIS GREEDY OPERATORS ---

// Helper: Performs Greedy Optimization using Basis Score (Supports 5D/Extended).
/*
std::vector<std::tuple<int, int, int>> perform_greedy_optimize_basis_cell(
    std::array<std::array<int, 14>, 8>& board,
    int target_r,
    int target_c,
    std::mt19937& rng
) {
    std::vector<std::tuple<int, int, int>> backup;
    int original_val = board[target_r][target_c];
    backup.emplace_back(target_r, target_c, original_val);
    board[target_r][target_c] = -1;
    RichnessOracle base_oracle;
    for(int r = 0; r < 8; ++r) for(int c = 0; c < 14; ++c) if (board[r][c] != -1) dfs_richness(r, c, 1, 0, board, base_oracle);
    int best_val = original_val; long long best_score = -1;
    int r_min = std::max(0, target_r - 5), r_max = std::min(7, target_r + 5), c_min = std::max(0, target_c - 5), c_max = std::min(13, target_c + 4);
    std::array<int, 10> candidates = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}; std::shuffle(candidates.begin(), candidates.end(), rng);
    for (int v : candidates) {
        board[target_r][target_c] = v; RichnessOracle current_oracle = base_oracle;
        for(int r = r_min; r <= r_max; ++r) for(int c = c_min; c <= c_max; ++c) if (board[r][c] != -1) dfs_richness(r, c, 1, 0, board, current_oracle);
        long long score = get_basis_score(current_oracle);
        if (score > best_score) { best_score = score; best_val = v; }
    }
    board[target_r][target_c] = best_val;
    return backup;
}

std::vector<std::tuple<int, int, int>> apply_adj_random_mutation_greedy_optimize_basis(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup;
    int r = std::uniform_int_distribution<int>(0, 7)(rng), c = std::uniform_int_distribution<int>(0, 13)(rng), ov = board[r][c], nv = std::uniform_int_distribution<int>(0, 9)(rng);
    while (nv == ov) nv = std::uniform_int_distribution<int>(0, 9)(rng);
    backup.emplace_back(r, c, ov); board[r][c] = nv;
    const auto& neighbors = ADJ_TABLE[r][c];
    if (neighbors.count > 0) {
        auto neighbor_coord = neighbors.list[std::uniform_int_distribution<int>(0, neighbors.count - 1)(rng)];
        auto g_backup = perform_greedy_optimize_basis_cell(board, neighbor_coord.y, neighbor_coord.x, rng);
        backup.insert(backup.end(), g_backup.begin(), g_backup.end());
    }
    return backup;
}

std::vector<std::tuple<int, int, int>> apply_random_swap_greedy_optimize_basis(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup;
    int r1 = std::uniform_int_distribution<int>(0, 7)(rng), c1 = std::uniform_int_distribution<int>(0, 13)(rng), r2 = std::uniform_int_distribution<int>(0, 7)(rng), c2 = std::uniform_int_distribution<int>(0, 13)(rng);
    while (r1 == r2 && c1 == c2) { r2 = std::uniform_int_distribution<int>(0, 7)(rng); c2 = std::uniform_int_distribution<int>(0, 13)(rng); }
    backup.emplace_back(r1, c1, board[r1][c1]); backup.emplace_back(r2, c2, board[r2][c2]); std::swap(board[r1][c1], board[r2][c2]);
    auto g_backup = perform_greedy_optimize_basis_cell(board, r1, c1, rng);
    backup.insert(backup.end(), g_backup.begin(), g_backup.end());
    return backup;
}
*/

// --- DISABLED / RICHNESS GREEDY OPERATORS ---

/*
std::vector<std::tuple<int, int, int>> perform_greedy_optimize_cell(std::array<std::array<int, 14>, 8>& board, int target_r, int target_c, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup;
    int original_val = board[target_r][target_c];
    backup.emplace_back(target_r, target_c, original_val);
    board[target_r][target_c] = -1;
    RichnessOracle4D base_oracle;
    for(int r = 0; r < 8; ++r) for(int c = 0; c < 14; ++c) if (board[r][c] != -1) dfs_richness_4d(r, c, 1, 0, board, base_oracle);
    int best_val = original_val; long long best_score = -2000000000000LL;
    int r_min = std::max(0, target_r - 4), r_max = std::min(7, target_r + 4), c_min = std::max(0, target_c - 4), c_max = std::min(13, target_c + 4);
    std::array<int, 10> candidates = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}; std::shuffle(candidates.begin(), candidates.end(), rng);
    for (int v : candidates) {
        board[target_r][target_c] = v; RichnessOracle4D current_oracle = base_oracle;
        for(int r = r_min; r <= r_max; ++r) for(int c = c_min; c <= c_max; ++c) if (board[r][c] != -1) dfs_richness_4d(r, c, 1, 0, board, current_oracle);
        long long score = calculate_unbiased_score_4d(current_oracle);
        if (score > best_score) { best_score = score; best_val = v; }
    }
    board[target_r][target_c] = best_val;
    return backup;
}

std::vector<std::tuple<int, int, int>> apply_greedy_optimize_4d(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    int r = std::uniform_int_distribution<int>(0, 7)(rng), c = std::uniform_int_distribution<int>(0, 13)(rng);
    return perform_greedy_optimize_cell(board, r, c, rng);
}

std::vector<std::tuple<int, int, int>> apply_adj_random_mutation_greedy_optimize(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup;
    int r = std::uniform_int_distribution<int>(0, 7)(rng), c = std::uniform_int_distribution<int>(0, 13)(rng), old_val = board[r][c], new_val = std::uniform_int_distribution<int>(0, 9)(rng);
    while (new_val == old_val) new_val = std::uniform_int_distribution<int>(0, 9)(rng);
    backup.emplace_back(r, c, old_val); board[r][c] = new_val;
    const auto& neighbors = ADJ_TABLE[r][c];
    if (neighbors.count > 0) {
        auto n = neighbors.list[std::uniform_int_distribution<int>(0, neighbors.count - 1)(rng)];
        auto greedy_backup = perform_greedy_optimize_cell(board, n.y, n.x, rng);
        backup.insert(backup.end(), greedy_backup.begin(), greedy_backup.end());
    }
    return backup;
}

std::vector<std::tuple<int, int, int>> apply_random_swap_greedy_optimize(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    std::vector<std::tuple<int, int, int>> backup;
    int r1 = std::uniform_int_distribution<int>(0, 7)(rng), c1 = std::uniform_int_distribution<int>(0, 13)(rng), r2 = std::uniform_int_distribution<int>(0, 7)(rng), c2 = std::uniform_int_distribution<int>(0, 13)(rng);
    while (r1 == r2 && c1 == c2) { r2 = std::uniform_int_distribution<int>(0, 7)(rng); c2 = std::uniform_int_distribution<int>(0, 13)(rng); }
    backup.emplace_back(r1, c1, board[r1][c1]); backup.emplace_back(r2, c2, board[r2][c2]); std::swap(board[r1][c1], board[r2][c2]);
    auto greedy_backup = perform_greedy_optimize_cell(board, r1, c1, rng);
    backup.insert(backup.end(), greedy_backup.begin(), greedy_backup.end());
    return backup;
}

std::vector<std::tuple<int, int, int>> apply_greedy_optimize_basis_4d(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    int r = std::uniform_int_distribution<int>(0, 7)(rng), c = std::uniform_int_distribution<int>(0, 13)(rng);
    std::vector<std::tuple<int, int, int>> backup; int original_val = board[r][c];
    backup.emplace_back(r, c, original_val); board[r][c] = -1;
    RichnessOracle4D base_oracle;
    for(int i = 0; i < 8; ++i) for(int j = 0; j < 14; ++j) if (board[i][j] != -1) dfs_richness_4d(i, j, 1, 0, board, base_oracle);
    int best_val = original_val; long long best_score = -1;
    int r_min = std::max(0, r - 4), r_max = std::min(7, r + 4), c_min = std::max(0, c - 4), c_max = std::min(13, c + 4);
    std::array<int, 10> candidates = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}; std::shuffle(candidates.begin(), candidates.end(), rng);
    for (int v : candidates) {
        board[r][c] = v; RichnessOracle4D current_oracle = base_oracle;
        for(int i = r_min; i <= r_max; ++i) for(int j = c_min; j <= c_max; ++j) if (board[i][j] != -1) dfs_richness_4d(i, j, 1, 0, board, current_oracle);
        long long score = get_basis_score_4d(current_oracle);
        if (score > best_score) { best_score = score; best_val = v; }
    }
    board[r][c] = best_val; return backup;
}

std::vector<std::tuple<int, int, int>> apply_greedy_optimize_basis(std::array<std::array<int, 14>, 8>& board, std::mt19937& rng) {
    int r = std::uniform_int_distribution<int>(0, 7)(rng), c = std::uniform_int_distribution<int>(0, 13)(rng);
    return perform_greedy_optimize_basis_cell(board, r, c, rng);
}
*/