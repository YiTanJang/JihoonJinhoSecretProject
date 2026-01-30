#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <array>
#include <chrono>

using namespace std;

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

void dfs_collect_4d(int r, int c, int depth, int val, const array<array<int, 14>, 8>& b, bool found4[10000]) {
    int next_val = val * 10 + b[r][c];
    if (depth == 4) { found4[next_val] = true; return; }
    for (int i = 0; i < ADJ_TABLE[r][c].count; ++i) dfs_collect_4d(ADJ_TABLE[r][c].list[i].y, ADJ_TABLE[r][c].list[i].x, depth + 1, next_val, b, found4);
}

struct SymbolSeq { int d1, d2, d3, d4; };

int main(int argc, char* argv[]) {
    init_adj_table();
    string raw = "";
    for(int i=1; i<argc; ++i) raw += argv[i];
    string clean = "";
    for(char c : raw) if(isdigit(c)) clean += c;
    if(clean.length() != 112) return 1;

    array<array<int, 14>, 8> board;
    for(int i=0; i<112; ++i) board[i/14][i % 14] = clean[i] - '0';

    static bool found4[10000] = {0};
    for(int r = 0; r < 8; ++r) for(int c = 0; c < 14; ++c) dfs_collect_4d(r, c, 1, 0, board, found4);
    
    vector<SymbolSeq> missing;
    for(int i=0; i<10000; ++i) {
        if(!found4[i]) {
            missing.push_back({i/1000, (i/100)%10, (i/10)%10, i%10});
        }
    }

    cout << "Missing Symbol Sequences: " << missing.size() << endl;

    int p[10] = {0,1,2,3,4,5,6,7,8,9}, best_p[10];
    int max_x = -1;

    auto start = chrono::steady_clock::now();
    do {
        int min_val = 10000;
        for(const auto& ms : missing) {
            int d1 = p[ms.d1];
            if (d1 == 0) continue; // Resolved (becomes 3-digit, which are all present)
            
            int val = d1*1000 + p[ms.d2]*100 + p[ms.d3]*10 + p[ms.d4];
            if (val < min_val) min_val = val;
        }
        
        if (min_val > max_x) {
            max_x = min_val;
            for(int i=0; i<10; ++i) best_p[i] = p[i];
        }
    } while (next_permutation(p, p + 10));

    auto end = chrono::steady_clock::now();
    cout << "Optimization Complete in " << chrono::duration<double>(end - start).count() << "s" << endl;
    cout << "Best Sequential Richness (X): " << (max_x == 10000 ? 10000 : max_x) << endl;
    cout << "Mapping: "; for(int i=0; i<10; ++i) cout << i << "->" << best_p[i] << " "; cout << endl;
    for(int i=0; i<112; ++i) { cout << best_p[clean[i]-'0']; if((i+1)%14==0) cout << endl; }
    return 0;
}
