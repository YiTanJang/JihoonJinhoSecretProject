#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <array>
#include <bitset>
#include <numeric>
#include <iomanip>
#include <cmath>

using namespace std;

// ============================================================================ 
// Mock Logic from src/logic.cpp
// ============================================================================ 

struct Adj { int y, x; };
struct AdjList { Adj list[8]; int count; };
AdjList ADJ_TABLE[8][14];

void init_adj_table() {
    int dr[] = {-1, -1, -1, 0, 0, 1, 1, 1}, dc[] = {-1, 0, 1, -1, 1, -1, 0, 1}; 
    for (int r = 0; r < 8; ++r) for (int c = 0; c < 14; ++c) {
        int cnt = 0;
        for (int i = 0; i < 8; ++i) {
            int nr = r + dr[i], nc = c + dc[i];
            if (nr >= 0 && nr < 8 && nc >= 0 && nc < 14) ADJ_TABLE[r][c].list[cnt++] = {nr, nc};
        }
        ADJ_TABLE[r][c].count = cnt;
    }
}

struct RichnessOracle4D {
    std::bitset<10000> bits4;
    std::bitset<1000> bits3;
    void mark(int len, int val) { if (len == 3) bits3.set(val); else if (len == 4) bits4.set(val); } 
};

void dfs_richness_4d(int r, int c, int depth, int current_val, const std::array<std::array<int, 14>, 8>& board, RichnessOracle4D& oracle) {
    int next_val = current_val * 10 + board[r][c];
    if (depth == 3) oracle.mark(3, next_val); 
    else if (depth == 4) { oracle.mark(4, next_val); return; } 
    const auto& adj = ADJ_TABLE[r][c];
    for(int i = 0; i < adj.count; ++i) dfs_richness_4d(adj.list[i].y, adj.list[i].x, depth + 1, next_val, board, oracle); 
}

// ----------------------------------------------------------------------------
// Optimized Logic (constexpr)
// ----------------------------------------------------------------------------

struct NumberInfo {
    int8_t type; // 0: Trivial, 1: Pal, 2: Cyclic, 3: Standard
    int8_t bucket;
    int16_t partner;
};

const std::array<NumberInfo, 10000> create_lookup_table() {
    std::array<NumberInfo, 10000> table{};
    for (int i = 0; i < 10000; ++i) {
        int d1 = i / 1000, d2 = (i / 100) % 10, d3 = (i / 10) % 10, d4 = i % 10;
        if (d1 == d3 || d2 == d4) { table[i] = {0, 0, -1}; continue; }
        if (d1 == d4 && d2 == d3) { table[i] = {1, (int8_t)d1, -1}; continue; }
        int rev = d4 * 1000 + d3 * 100 + d2 * 10 + d1;
        if (d1 == d4) { table[i] = {2, (int8_t)d1, (int16_t)rev}; continue; }
        table[i] = {3, (int8_t)d1, (int16_t)rev};
    }
    return table;
}
const auto LOOKUP = create_lookup_table();

struct Info3 { int d1, d3; bool is_pal; int rev; };
const std::array<Info3, 1000> create_lookup3_table() {
    std::array<Info3, 1000> table{};
    for (int i = 0; i < 1000; ++i) {
        int d1 = i/100, d3 = i%10;
        if (d1 == d3) table[i] = {d1, d3, true, -1};
        else {
            int d2 = (i/10)%10;
            table[i] = {d1, d3, false, d3*100 + d2*10 + d1};
        }
    }
    return table;
}
const auto LOOKUP3 = create_lookup3_table();

// ----------------------------------------------------------------------------
// Test Scoring Function
// ----------------------------------------------------------------------------

long long test_unbiased_score(const RichnessOracle4D& oracle) {
    static std::array<std::vector<int>, 10> bucket_members; 
    static bool members_init = false;
    if (!members_init) {
        for (int i = 0; i < 10000; ++i) if (LOOKUP[i].type != 0) bucket_members[LOOKUP[i].bucket].push_back(i);
        members_init = true;
    }

    struct Bucket { int capacity = 0; int fill = 0; int raw_count = 0; };
    std::array<Bucket, 10> buckets;

    for (int i = 0; i < 10000; ++i) {
        if (oracle.bits4.test(i) && LOOKUP[i].type != 0) buckets[LOOKUP[i].bucket].raw_count++;
    }

    // Sort by Raw Counts
    std::vector<int> ranked_digits(10);
    std::iota(ranked_digits.begin(), ranked_digits.end(), 0);
    std::sort(ranked_digits.begin(), ranked_digits.end(), [&](int a, int b) {
        return buckets[a].raw_count > buckets[b].raw_count;
    });

    cout << "\n=== BUCKET ANALYSIS ===" << endl;
    cout << "Priority Order: ";
    for(int d : ranked_digits) cout << d << "(" << buckets[d].raw_count << ") ";
    cout << endl;

    static std::bitset<10000> consumed;
    consumed.reset();

    for (int d : ranked_digits) {
        for (int i : bucket_members[d]) {
            const auto& info = LOOKUP[i];
            if (consumed.test(i)) continue;

            if (info.type == 1) { // Palindrome
                buckets[d].capacity++;
                if (oracle.bits4.test(i)) buckets[d].fill++;
                consumed.set(i);
            } else if (info.type == 2) { // Cyclic
                buckets[d].capacity++;
                if (oracle.bits4.test(i) || oracle.bits4.test(info.partner)) buckets[d].fill++;
                consumed.set(i); consumed.set(info.partner);
            } else if (info.type == 3) { // Standard
                buckets[d].capacity++;
                if (oracle.bits4.test(i)) buckets[d].fill++;
                consumed.set(i); consumed.set(info.partner);
            }
        }
    }

    // 3-Digit Debiasing
    int fill3 = 0, cap3 = 0;
    static std::bitset<1000> consumed3;
    consumed3.reset();

    for (int i = 0; i < 1000; ++i) {
        if (consumed3.test(i)) continue;
        const auto& inf = LOOKUP3[i];
        cap3++;
        if (inf.is_pal) { 
            if (oracle.bits3.test(i)) fill3++;
            consumed3.set(i);
        } else {
            if (oracle.bits3.test(i) || oracle.bits3.test(inf.rev)) fill3++;
            consumed3.set(i); consumed3.set(inf.rev);
        }
    }

    cout << "\n=== 3. FINAL RANKING & WEIGHTS ===" << endl;
    cout << setw(5) << "Rank" << setw(5) << "Dig" << setw(8) << "Fill" << setw(8) << "Cap" << setw(8) << "Pct" << setw(12) << "Weight" << setw(15) << "Score" << endl;
    cout << "----------------------------------------------------------------" << endl;

    struct RatedBucket { 
        int fill;
        int capacity;
        int digit; // -1 for 3D
    };
    
    std::vector<RatedBucket> ranked_all;
    ranked_all.push_back({fill3, cap3, -1});
    for (int d : ranked_digits) {
        ranked_all.push_back({buckets[d].fill, buckets[d].capacity, d});
    }
    
    int first_fail_rank = -1;
    for (int r = 0; r < 11; ++r) {
        if (ranked_all[r].fill < ranked_all[r].capacity) {
            first_fail_rank = r;
            break;
        }
    }
    
    long long total_score = 0;
    
    for (int r = 0; r < 11; ++r) {
        long long w = 0;
        if (r < 10) { // Ignore 11th bucket
            if (first_fail_rank == -1 || r <= first_fail_rank) w = 10000LL;
            else { 
                int dist = r - first_fail_rank;
                int val = 10 - dist;
                if (val < 0) val = 0;
                w = (long long)val * val * val * val;
            }
        }

        long long s = (long long)ranked_all[r].fill * w;
        total_score += s;

        cout << setw(5) << r << setw(5) << ranked_all[r].digit << setw(8) << ranked_all[r].fill 
             << setw(8) << ranked_all[r].capacity 
             << setw(8) << fixed << setprecision(1) << (ranked_all[r].capacity > 0 ? (double)ranked_all[r].fill/ranked_all[r].capacity*100 : 0)
             << setw(10) << w << setw(12) << s << endl;
    }

    cout << "--------------------------------------------------------" << endl;
    cout << "TOTAL UNBIASED SCORE: " << total_score << endl;
    return total_score;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: TestUnbiasedScore <112_char_board>" << endl;
        return 1;
    }

    string raw = "";
    for(int i=1; i<argc; ++i) raw += argv[i];
    string clean = "";
    for(char c : raw) if(isdigit(c)) clean += c;
    
    if (clean.length() != 112) {
        cout << "Error: Board must be 112 digits." << endl;
        return 1;
    }

    init_adj_table();
    array<array<int, 14>, 8> board;
    for (int i = 0; i < 112; ++i) board[i / 14][i % 14] = clean[i] - '0';

    RichnessOracle4D oracle;
    for (int r = 0; r < 8; ++r) for (int c = 0; c < 14; ++c) dfs_richness_4d(r, c, 1, 0, board, oracle);

    test_unbiased_score(oracle);
    return 0;
}
