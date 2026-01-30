#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <bitset>
#include <algorithm>

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

void dfs(int r, int c, int depth, int val, const array<array<int, 14>, 8>& b, bitset<10000>& bits) {
    int next_val = val * 10 + b[r][c];
    if (next_val < 10000) bits.set(next_val);
    if (depth == 4) return;
    for (int i = 0; i < ADJ_TABLE[r][c].count; ++i) dfs(ADJ_TABLE[r][c].list[i].y, ADJ_TABLE[r][c].list[i].x, depth + 1, next_val, b, bits);
}

int main(int argc, char* argv[]) {
    init_adj_table();
    string s = "5062076649580938417057458790728694253014511725691869234289513781428637049213043560786386792079015699347152682713";
    array<array<int, 14>, 8> board;
    for (int i = 0; i < 112; ++i) board[i / 14][i % 14] = s[i] - '0';

    bitset<10000> bits;
    for (int r = 0; r < 8; ++r) for (int c = 0; c < 14; ++c) dfs(r, c, 1, 0, board, bits);

    bool all_3_present = true;
    for(int i=1; i<1000; ++i) if(!bits.test(i)) { all_3_present = false; break; }
    cout << "All numbers 1-999 present: " << (all_3_present ? "YES" : "NO") << endl;

    int x = 1;
    while(x < 10000 && bits.test(x)) x++;
    cout << "Sequential Richness (X): " << x << endl;

    cout << "Missing numbers up to 9999:" << endl;
    int count = 0;
    for (int i = 1; i < 10000; ++i) {
        if (!bits.test(i)) {
            if (count < 50) cout << i << " ";
            count++;
        }
    }
    cout << "\nTotal Missing: " << count << endl;
    return 0;
}
