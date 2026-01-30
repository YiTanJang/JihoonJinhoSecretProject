import sqlite3
import struct
import sys
import os

# -----------------------------------------------------------------------------
# 1. Database & Board Loading
# -----------------------------------------------------------------------------

def load_best_board(db_path="db/optimizer_4d.db"):
    abs_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../", db_path))
    if not os.path.exists(abs_path):
        print(f"Error: DB not found at {abs_path}")
        return None

    conn = sqlite3.connect(abs_path)
    cursor = conn.cursor()
    cursor.execute("SELECT board_data FROM best_boards ORDER BY id DESC LIMIT 1")
    row = cursor.fetchone()
    conn.close()

    if not row:
        print("Error: No boards found in DB.")
        return None

    board_str = row[0]
    board = []
    for r in range(8):
        row_data = [int(c) for c in board_str[r*14 : (r+1)*14]]
        board.append(row_data)
    return board

# -----------------------------------------------------------------------------
# 2. Cycle Analysis Logic
# -----------------------------------------------------------------------------

def analyze_cycles(board):
    print("Analyzing Cycle (abca) patterns on the latest board...")
    
    # Adjacency List (8-way King's moves)
    R, C = 8, 14
    adj = [[[] for _ in range(C)] for _ in range(R)]
    for r in range(R):
        for c in range(C):
            for dr in [-1, 0, 1]:
                for dc in [-1, 0, 1]:
                    if dr == 0 and dc == 0: continue
                    nr, nc = r+dr, c+dc
                    if 0 <= nr < R and 0 <= nc < C:
                        adj[r][c].append((nr, nc))

    # Identify all 'abca' patterns
    # defined as: 4-digit number where d1 == d4, but d1 != d3 (not Backtrack) and d2 != d4 (not Backtrack)
    
    # Collect all 4-digit numbers present on board
    found_cycles = set()
    
    for r in range(R):
        for c in range(C):
            # Depth 1: Start
            d1 = board[r][c]
            # Depth 2
            for nr2, nc2 in adj[r][c]:
                d2 = board[nr2][nc2]
                # Depth 3
                for nr3, nc3 in adj[nr2][nc2]:
                    d3 = board[nr3][nc3]
                    # Depth 4
                    for nr4, nc4 in adj[nr3][nc3]:
                        d4 = board[nr4][nc4]
                        val = d1*1000 + d2*100 + d3*10 + d4
                        found_cycles.add(val)

    # Now iterate all theoretical 'abca' numbers
    total_cycles = 0
    missing_count = 0
    missing_examples = []

    for i in range(10000):
        d1 = i // 1000
        d2 = (i // 100) % 10
        d3 = (i // 10) % 10
        d4 = i % 10
        
        # Logic.cpp Priority:
        # 1. PingPong (abab): d1==d3 AND d2==d4
        # 2. Backtrack (abcb/abac): d1==d3 OR d2==d4
        # 3. Cycle (abca): d1==d4
        
        is_pingpong = (d1 == d3) and (d2 == d4)
        is_backtrack = (d1 == d3) or (d2 == d4)
        is_cycle = (d1 == d4)
        
        if is_pingpong or is_backtrack:
            continue # Not counted as a Cycle topology
            
        if is_cycle:
            total_cycles += 1
            if i not in found_cycles:
                missing_count += 1
                missing_examples.append(i)

    print(f"\nTotal Theoretical Cycle Patterns: {total_cycles}")
    print(f"Missing Cycle Patterns: {missing_count}")
    print(f"Coverage: {(1 - missing_count/total_cycles)*100:.2f}%")
    
    if missing_examples:
        print("\nFirst 20 Missing Examples:")
        print(missing_examples[:20])
        
        # Analyze missing digits
        from collections import Counter
        starts = Counter(x // 1000 for x in missing_examples)
        print("\nMissing Cycles by Start Digit:")
        for d in range(10):
            print(f"Digit {d}: {starts[d]} missing")

if __name__ == "__main__":
    board = load_best_board()
    if board:
        analyze_cycles(board)
