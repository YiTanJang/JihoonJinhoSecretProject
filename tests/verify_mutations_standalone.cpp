#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <tuple>
#include <algorithm>
#include <map>
#include <iomanip>
#include <functional>
#include <functional>

// --- Mocking/Copying relevant parts for standalone testing ---

// Global RNG for the test
std::mt19937 g_rng(12345);

// The board type
using Board = std::array<std::array<int, 14>, 8>;

// --- COPIED MUTATION FUNCTIONS (Meso & Macro) ---

// Meso: 2x2 X-Wing Swap
std::vector<std::tuple<int, int, int>> apply_2x2_xwing_swap(Board& board, std::mt19937& rng) {
    int r = std::uniform_int_distribution<int>(0, 6)(rng);
    int c = std::uniform_int_distribution<int>(0, 12)(rng);
    std::vector<std::tuple<int, int, int>> backup = {{r, c, board[r][c]}, {r, c+1, board[r][c+1]}, {r+1, c, board[r+1][c]}, {r+1, c+1, board[r+1][c+1]}};
    std::swap(board[r][c], board[r+1][c+1]); 
    std::swap(board[r][c+1], board[r+1][c]); 
    return backup;
}

// Meso: Triangle Rotate
std::vector<std::tuple<int, int, int>> apply_triangle_rotate(Board& board, std::mt19937& rng) {
    int r = std::uniform_int_distribution<int>(0, 6)(rng);
    int c = std::uniform_int_distribution<int>(0, 12)(rng);
    int type = std::uniform_int_distribution<int>(0, 3)(rng);
    std::vector<std::pair<int, int>> coords = (type == 0) ? std::vector<std::pair<int,int>>{{r,c},{r+1,c},{r,c+1}} : (type == 1) ? std::vector<std::pair<int,int>>{{r,c},{r,c+1},{r+1,c+1}} : (type == 2) ? std::vector<std::pair<int,int>>{{r,c},{r+1,c},{r+1,c+1}} : std::vector<std::pair<int,int>>{{r+1,c},{r,c+1},{r+1,c+1}};
    std::vector<std::tuple<int, int, int>> backup; for (auto p : coords) backup.emplace_back(p.first, p.second, board[p.first][p.second]);
    int temp = board[coords[0].first][coords[0].second]; 
    board[coords[0].first][coords[0].second] = board[coords[1].first][coords[1].second]; 
    board[coords[1].first][coords[1].second] = board[coords[2].first][coords[2].second]; 
    board[coords[2].first][coords[2].second] = temp; 
    return backup;
}

// Meso: Variable Worm Slide (The FIXED Straight Slide Version)
std::vector<std::tuple<int, int, int>> apply_variable_worm_slide(Board& board, std::mt19937& rng) {
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

// Meso: Variable Block Rotate
std::vector<std::tuple<int, int, int>> apply_variable_block_rotate(Board& board, std::mt19937& rng) {
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
    
    int k = std::uniform_int_distribution<int>(1, n-1)(rng);
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

// Macro: Variable Block Swap (Checked Version)
std::vector<std::tuple<int, int, int>> apply_variable_block_swap(Board& board, std::mt19937& rng) {
    auto g1 = [](auto& g) { int v = std::uniform_int_distribution<int>(0, 5)(g); return (v <= 2) ? 1 : (v <= 4) ? 2 : 3; };
    auto g2 = [](auto& g) { return std::uniform_int_distribution<int>(2, 6)(g); };
    int d1 = g1(rng), d2 = g2(rng), h, w; if (std::uniform_int_distribution<int>(0, 1)(rng) == 0) { h = d1; w = d2; } else { h = d2; w = d1; }
    int r1 = std::uniform_int_distribution<int>(0, 8-h)(rng), c1 = std::uniform_int_distribution<int>(0, 14-w)(rng), r2 = std::uniform_int_distribution<int>(0, 8-h)(rng), c2 = std::uniform_int_distribution<int>(0, 14-w)(rng);
    
    // Safety Check: Prevent overlap
    if (! (r1 >= r2+h || r2 >= r1+h || c1 >= c2+w || c2 >= c1+w)) return {};

    std::vector<std::tuple<int, int, int>> backup;
    for (int i = 0; i < h; ++i) for (int j = 0; j < w; ++j) { 
        backup.emplace_back(r1+i, c1+j, board[r1+i][c1+j]); 
        backup.emplace_back(r2+i, c2+j, board[r2+i][c2+j]); 
        std::swap(board[r1+i][c1+j], board[r2+i][c2+j]); 
    }
    return backup;
}

// Macro: Variable Block Flip
std::vector<std::tuple<int, int, int>> apply_variable_block_flip(Board& board, std::mt19937& rng) {
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

// --- TEST INFRASTRUCTURE ---

void init_board(Board& b) {
    for(int r=0; r<8; ++r) {
        for(int c=0; c<14; ++c) {
            b[r][c] = (r + c + 1) % 10;
        }
    }
}

std::map<int, int> count_digits(const Board& b) {
    std::map<int, int> counts;
    for(int r=0; r<8; ++r) {
        for(int c=0; c<14; ++c) {
            counts[b[r][c]]++;
        }
    }
    return counts;
}

bool verify_counts(const std::map<int, int>& initial, const std::map<int, int>& current) {
    if (initial.size() != current.size()) return false;
    for(auto const& [key, val] : initial) {
        if (current.at(key) != val) return false;
    }
    return true;
}

void print_counts(const std::map<int, int>& counts) {
    for(auto const& [k, v] : counts) {
        std::cout << k << ":" << v << " ";
    }
    std::cout << "\n";
}

void print_board(const Board& b, const std::string& label) {
    std::cout << "--- " << label << " ---\n";
    for(int r=0; r<8; ++r) {
        for(int c=0; c<14; ++c) {
            std::cout << b[r][c] << " ";
        }
        std::cout << "\n";
    }
}

void print_backup(const std::vector<std::tuple<int, int, int>>& backup) {
    std::cout << "Backup (changes): ";
    for(const auto& t : backup) {
        std::cout << "(" << std::get<0>(t) << "," << std::get<1>(t) << "):" << std::get<2>(t) << " ";
    }
    std::cout << "\n";
}

int main() {
    Board board;
    init_board(board);
    
    auto initial_counts = count_digits(board);
    std::cout << "Initial Counts: ";
    print_counts(initial_counts);
    
    // REDUCED ITERATIONS FOR VISUALIZATION
    int num_iterations = 2; 
    int errors = 0;

    std::cout << "\nRunning " << num_iterations << " iterations for each operator with VISUALIZATION...\n";

    // Test List
    struct TestOp {
        std::string name;
        std::function<std::vector<std::tuple<int,int,int>>(Board&, std::mt19937&)> func;
    };
    
    std::vector<TestOp> ops = {
        {"2x2 X-Wing Swap", apply_2x2_xwing_swap},
        {"Triangle Rotate", apply_triangle_rotate},
        {"Variable Worm Slide (Straight)", apply_variable_worm_slide},
        {"Variable Block Rotate", apply_variable_block_rotate},
        {"Variable Block Swap", apply_variable_block_swap},
        {"Variable Block Flip", apply_variable_block_flip}
    };

    for (const auto& op : ops) {
        std::cout << "\n========================================\n";
        std::cout << "Testing " << op.name << "\n";
        std::cout << "========================================\n";
        
        init_board(board); // Reset board
        bool op_error = false;
        
        for (int i=0; i<num_iterations; ++i) {
            std::cout << "\n[Iteration " << i << "]\n";
            print_board(board, "Before Mutation");

            auto backup = op.func(board, g_rng);
            
            print_backup(backup);
            print_board(board, "After Mutation");

            // Check Invariant
            auto current_counts = count_digits(board);
            if (!verify_counts(initial_counts, current_counts)) {
                std::cout << "FAILED at iter " << i << "!\n";
                std::cout << "Expected: "; print_counts(initial_counts);
                std::cout << "Actual:   "; print_counts(current_counts);
                op_error = true;
                errors++;
                break;
            }
            
            // ALWAYS revert to check rollback logic
            for (auto it = backup.rbegin(); it != backup.rend(); ++it) {
                const auto& [r, c, old_val] = *it;
                board[r][c] = old_val;
            }
            
            print_board(board, "After Revert");

            // Check invariant again after revert
            current_counts = count_digits(board);
            if (!verify_counts(initial_counts, current_counts)) {
                std::cout << "FAILED REVERT at iter " << i << "!\n";
                op_error = true;
                errors++;
                break;
            }
        }
        if (!op_error) std::cout << ">>> " << op.name << " PASSED.\n";
    }

    if (errors == 0) {
        std::cout << "\nAll Mutation Tests Passed. No duplicates created or destroyed.\n";
        return 0;
    } else {
        std::cout << "\nSome tests FAILED.\n";
        return 1;
    }
}
