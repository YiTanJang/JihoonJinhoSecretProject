#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <algorithm>
#include <numeric>
#include <bitset>
#include <sstream>

// Minimal Globals/Constants
const int MAX_SCORE = 20000; // Enough for your 10013 target

// Adjacency Table (Same as logic.h)
struct Point { int y, x; };
struct Neighbors { int count; Point list[8]; };
std::array<std::array<Neighbors, 14>, 8> ADJ;

void init_adj() {
    for(int r=0; r<8; ++r) {
        for(int c=0; c<14; ++c) {
            ADJ[r][c].count = 0;
            for(int dr=-1; dr<=1; ++dr) {
                for(int dc=-1; dc<=1; ++dc) {
                    if(dr==0 && dc==0) continue;
                    int nr = r + dr;
                    int nc = c + dc;
                    if(nr>=0 && nr<8 && nc>=0 && nc<14) {
                        ADJ[r][c].list[ADJ[r][c].count++] = {nr, nc};
                    }
                }
            }
        }
    }
}

// Oracle
struct FullWalkOracle {
    std::bitset<120000> bits;
    const int offsets[6] = {0, 0, 10, 110, 1110, 11110};

    void mark(int len, int val) {
        if (len < 1 || len > 5) return;
        bits.set(offsets[len] + val);
    }

    bool check(int len, int val) const {
        if (len < 1 || len > 5) return false;
        return bits.test(offsets[len] + val);
    }
};

void dfs_walks(int r, int c, int depth, int val, const std::array<std::array<int, 14>, 8>& board, FullWalkOracle& oracle) {
    int next_val = val * 10 + board[r][c];
    oracle.mark(depth, next_val);
    if (depth < 5) {
        const auto& adj = ADJ[r][c];
        for(int i=0; i<adj.count; ++i) {
            dfs_walks(adj.list[i].y, adj.list[i].x, depth+1, next_val, board, oracle);
        }
    }
}

FullWalkOracle precompute(const std::array<std::array<int, 14>, 8>& board) {
    FullWalkOracle oracle;
    for(int r=0; r<8; ++r) for(int c=0; c<14; ++c) dfs_walks(r, c, 1, 0, board, oracle);
    return oracle;
}

// Digit Cache
struct DigitCache {
    struct Entry { uint8_t len; int8_t digits[5]; };
    std::vector<Entry> table;
    DigitCache() {
        table.resize(MAX_SCORE);
        for(int i=1; i<MAX_SCORE; ++i) {
            int t = i;
            std::vector<int8_t> d;
            while(t > 0) { d.push_back(t%10); t/=10; }
            table[i].len = (uint8_t)d.size();
            for(int j=0; j<table[i].len; ++j) table[i].digits[j] = d[table[i].len - 1 - j];
        }
    }
};
static DigitCache G_DIGITS;

inline int map_num(int n, const std::array<int8_t, 10>& perm) {
    const auto& e = G_DIGITS.table[n];
    int res = 0;
    for(int i=0; i<e.len; ++i) res = res * 10 + perm[e.digits[i]];
    return res;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: PermuteMyBoard <112-char-board-string>" << std::endl;
        return 1;
    }

    std::string b_str = argv[1];
    if (b_str.length() != 112) {
        std::cerr << "Error: Board string must be 112 chars. Got " << b_str.length() << std::endl;
        return 1;
    }

    init_adj();

    std::array<std::array<int, 14>, 8> board;
    int idx = 0;
    for(int r=0; r<8; ++r) for(int c=0; c<14; ++c) board[r][c] = b_str[idx++] - '0';

    auto oracle = precompute(board);

    std::vector<std::array<int8_t, 10>> survivors;
    survivors.reserve(3628800);
    std::array<int8_t, 10> p;
    std::iota(p.begin(), p.end(), 0);
    do { survivors.push_back(p); } while (std::next_permutation(p.begin(), p.end()));

    std::cout << "Sieving permutations for board..." << std::endl;

    std::array<int8_t, 10> best_perm = p;
    int current_max = 0;

    for (int n = 1; n < MAX_SCORE; ++n) {
        int len = G_DIGITS.table[n].len;
        
        auto it = std::partition(survivors.begin(), survivors.end(), [&](const std::array<int8_t, 10>& perm) {
            return oracle.check(len, map_num(n, perm));
        });

        if (it == survivors.begin()) {
            std::cout << "Failed at " << n << std::endl;
            break;
        }

        survivors.erase(it, survivors.end());
        current_max = n;
        best_perm = survivors[0];

        if (n % 1000 == 0) std::cout << "Passed " << n << ", candidates: " << survivors.size() << std::endl;

        if (survivors.size() == 1) {
            std::cout << "Converged to single permutation at " << n << ". Fast-forwarding..." << std::endl;
            // Fast forward
            for(int m = n+1; m < MAX_SCORE; ++m) {
                if(oracle.check(G_DIGITS.table[m].len, map_num(m, survivors[0]))) current_max = m;
                else break;
            }
            best_perm = survivors[0];
            break;
        }
    }

    std::cout << "\n=== RESULTS ===" << std::endl;
    std::cout << "Max Richness (X): " << current_max << std::endl;
    
    // best_perm[t] = o means "Target digit t corresponds to Original digit o"
    // We need map[o] = t to transform the board.
    std::array<int, 10> forward_map;
    for(int t=0; t<10; ++t) {
        forward_map[best_perm[t]] = t;
    }

    std::cout << "Forward Mapping (Original -> Target):" << std::endl;
    for(int o=0; o<10; ++o) std::cout << o << "->" << forward_map[o] << " ";
    std::cout << std::endl;

    std::cout << "Permuted Board String:" << std::endl;
    for(int r=0; r<8; ++r) {
        for(int c=0; c<14; ++c) std::cout << forward_map[board[r][c]];
    }
    std::cout << std::endl;

    return 0;
}
