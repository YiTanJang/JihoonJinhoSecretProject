import sys
import collections

def get_neighbors(r, c):
    neighs = []
    for dr in [-1, 0, 1]:
        for dc in [-1, 0, 1]:
            if dr == 0 and dc == 0: continue
            nr, nc = r + dr, c + dc
            if 0 <= nr < 8 and 0 <= nc < 14:
                neighs.append((nr, nc))
    return neighs

ADJ = [[get_neighbors(r, c) for c in range(14)] for r in range(8)]

def analyze_board(board_str):
    if len(board_str) != 112:
        print(f"Error: Board string must be 112 characters. Got {len(board_str)}.")
        return
    
    board = []
    idx = 0
    for r in range(8):
        row = []
        for c in range(14):
            row.append(int(board_str[idx]))
            idx += 1
        board.append(row)

    found_4 = set()
    found_3 = set()
    
    def dfs(r, c, depth, current_val):
        val = current_val * 10 + board[r][c]
        if depth == 3:
            found_3.add(val)
        if depth == 4:
            found_4.add(val)
            return
        
        for nr, nc in ADJ[r][c]:
            dfs(nr, nc, depth + 1, val)

    for r in range(8):
        for c in range(14):
            dfs(r, c, 1, 0)

    # Bucket analysis
    buckets = [0] * 10
    # Weighted calculation (Bucket Analogy)
    # Normals: 1, Palindromes (ABCBA): 2. 
    # For 4D, palindromes are (d x x d)
    weighted_buckets = [0] * 10
    capacities = [1000] * 10
    
    for num in found_4:
        d = num // 1000
        x = (num // 100) % 10
        y = (num // 10) % 10
        z = num % 10
        is_pal = (z == d and x == y)
        
        buckets[d] += 1
        weighted_buckets[d] += (2 if is_pal else 1)

    # Adjust capacities for palindromes
    # There are 10 palindromes per 1000 numbers (d 0 0 d, d 1 1 d ... d 9 9 d)
    # Total weighted capacity = 990 + (10 * 2) = 1010
    
    print("\n=== BUCKET ANALYSIS ===")
    print(f"{'Digit':<6} | {'Count':<10} | {'Weighted':<10} | {'Holes'}")
    print("-" * 40)
    
    for d in range(10):
        holes = 1010 - weighted_buckets[d]
        print(f"{d:<6} | {buckets[d]:<10} | {weighted_buckets[d]:<10} | {holes}")

    sorted_weighted = sorted(weighted_buckets, reverse=True)
    print("\nSorted Distribution (Rank 0 -> 9):")
    print(" ".join(f"{x:4d}" for x in sorted_weighted))
    
    print(f"\n3-digit coverage: {len(found_3)} / 1000")
    
    # Calculate Sequential Richness (X)
    x = 0
    while x < 10000 and x in found_4:
        x += 1
    print(f"Sequential Richness (X): {x}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python bucket_analysis.py <112_char_board_string>")
    else:
        # Combine all arguments in case of spaces/newlines
        board_input = "".join(sys.argv[1:]).strip()
        # Remove non-digits
        board_clean = "".join(c for c in board_input if c.isdigit())
        analyze_board(board_clean)