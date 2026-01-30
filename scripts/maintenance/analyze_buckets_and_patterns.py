import sqlite3
import sys
import os
import ctypes
from collections import defaultdict

# -----------------------------------------------------------------------------
# 1. Setup & C++ FFI (for Logic Consistency)
# -----------------------------------------------------------------------------
# We'll use Python for the logic to keep it flexible, but we will replicate 
# the C++ logic strictly (8-way adjacency, same type definitions).

# Adjacency (8-way King's moves)
R, C = 8, 14
ADJ = [[[] for _ in range(C)] for _ in range(R)]
for r in range(R):
    for c in range(C):
        for dr in [-1, 0, 1]:
            for dc in [-1, 0, 1]:
                if dr == 0 and dc == 0: continue
                nr, nc = r+dr, c+dc
                if 0 <= nr < R and 0 <= nc < C:
                    ADJ[r][c].append((nr, nc))

def load_best_board(db_path="db/optimizer_4d.db"):
    abs_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../", db_path))
    if not os.path.exists(abs_path):
        print(f"Error: DB not found at {abs_path}")
        return None, None

    conn = sqlite3.connect(abs_path)
    cursor = conn.cursor()
    cursor.execute("SELECT id, board_data FROM best_boards ORDER BY id DESC LIMIT 1")
    row = cursor.fetchone()
    conn.close()

    if not row:
        print("Error: No boards found in DB.")
        return None, None

    bid = row[0]
    board_str = row[1]
    board = []
    for r in range(8):
        row_data = [int(c) for c in board_str[r*14 : (r+1)*14]]
        board.append(row_data)
    return bid, board

# -----------------------------------------------------------------------------
# 2. Pattern Logic (Replicating logic.cpp)
# -----------------------------------------------------------------------------

def get_pattern_info(val):
    d1 = val // 1000
    d2 = (val // 100) % 10
    d3 = (val // 10) % 10
    d4 = val % 10
    
    # Priority matches logic.cpp:
    # 1. PingPong (abab): d1==d3 AND d2==d4
    # 2. Backtrack (abcb/abac): d1==d3 OR d2==d4
    # 3. Cycle (abca): d1==d4
    # 4. Linear (abcd): Everything else
    
    is_pingpong = (d1 == d3) and (d2 == d4)
    if is_pingpong: return "PingPong"
    
    is_backtrack = (d1 == d3) or (d2 == d4)
    if is_backtrack: return "Backtrack"
    
    is_cycle = (d1 == d4)
    if is_cycle:
        if d2 == d3: return "Palindrome" # Subtype of Cycle
        return "Cycle"
        
    return "Linear"

def get_bucket_and_type(val):
    # This logic must match create_lookup_table() in logic.cpp exactly
    d1 = val // 1000
    d2 = (val // 100) % 10
    d3 = (val // 10) % 10
    d4 = val % 10

    if d1 == d3 or d2 == d4:
        return -1, "Trivial" # Not part of bucket scoring (PingPong/Backtrack)
    
    # Bucket is determined by Leading Digit (d1)
    bucket = d1
    
    if d1 == d4 and d2 == d3:
        return bucket, "Palindrome"
    
    rev = d4*1000 + d3*100 + d2*10 + d1
    if d1 == d4:
        return bucket, "Cyclic" # Cyclic Pair
        
    return bucket, "Standard" # Standard Pair

# -----------------------------------------------------------------------------
# 3. Main Analysis
# -----------------------------------------------------------------------------

def analyze_board():
    bid, board = load_best_board()
    if not board: return

    print(f"Analyzing Board ID: {bid}")
    print("-" * 60)

    # 1. Find all present 4-digit numbers
    found_numbers = set()
    for r in range(R):
        for c in range(C):
            d1 = board[r][c]
            for nr2, nc2 in ADJ[r][c]:
                d2 = board[nr2][nc2]
                for nr3, nc3 in ADJ[nr2][nc2]:
                    d3 = board[nr3][nc3]
                    for nr4, nc4 in ADJ[nr3][nc3]:
                        d4 = board[nr4][nc4]
                        found_numbers.add(d1*1000 + d2*100 + d3*10 + d4)
    
    print(f"Total Unique Numbers Found: {len(found_numbers)}")
    print("-" * 60)

    # 2. Bucket Analysis
    # Buckets 0-9 store 4-digit numbers starting with that digit.
    # But ONLY non-trivial ones (Cycle, Palindrome, Standard).
    
    buckets = defaultdict(lambda: {"cap": 0, "fill": 0, "missing": []})
    
    # We iterate 0000-9999
    for i in range(10000):
        bucket_idx, p_type = get_bucket_and_type(i)
        
        if p_type == "Trivial": 
            continue # Skip trivial numbers for bucket analysis
            
        # Check logic.cpp scoring rules:
        # Palindrome: capacity +1, fill +1 if found
        # Cyclic: capacity +1, fill +1 if (found OR partner_found)
        # Standard: capacity +1, fill +1 if (found OR partner_found)
        
        # NOTE: logic.cpp calculates SCORE based on pairs.
        # But here we want to see missing PATTERNS.
        # So we will list a pattern as missing if IT ITSELF is missing.
        # (Even if its partner is present, physically missing the pattern is interesting).
        
        # Actually, for the "Score" view, we should follow the logic.
        # For "Missing Pattern" view, we list physical absence.
        
        buckets[bucket_idx]["cap"] += 1
        
        # Scoring Logic Check (Presence)
        is_present = (i in found_numbers)
        
        # Partner check for scoring
        d1 = i // 1000
        d2 = (i // 100) % 10
        d3 = (i // 10) % 10
        d4 = i % 10
        rev = d4*1000 + d3*100 + d2*10 + d1
        is_partner_present = (rev in found_numbers)
        
        score_point = 0
        if p_type == "Palindrome":
            if is_present: score_point = 1
        else: # Cyclic or Standard
            if is_present or is_partner_present: score_point = 1
            
        buckets[bucket_idx]["fill"] += score_point
        
        if not is_present:
            topo = get_pattern_info(i)
            buckets[bucket_idx]["missing"].append((i, p_type, topo))

    # 3. Print Report
    print(f"{'Bkt':<3} | {'Score %':<7} | {'Missing (Physical)':<20} | {'Details'}")
    print("-" * 100)
    
    sorted_buckets = sorted(buckets.items(), key=lambda x: x[1]["fill"], reverse=True)
    
    for b_idx, data in sorted_buckets:
        pct = (data["fill"] / data["cap"]) * 100 if data["cap"] > 0 else 0
        missing_list = data["missing"]
        missing_count = len(missing_list)
        
        # Summarize missing types
        missing_types = defaultdict(int)
        examples = []
        for val, ptype, topo in missing_list:
            missing_types[topo] += 1
            if len(examples) < 5: examples.append(str(val))
            
        details = ", ".join([f"{k}:{v}" for k,v in missing_types.items()])
        ex_str = ", ".join(examples)
        if missing_count > 5: ex_str += "..."
        
        print(f"{b_idx:<3} | {pct:<7.2f} | {missing_count:<3} ({ex_str:<15}) | {details}")

    print("-" * 100)
    print("Note: 'Score %' follows scoring logic (Pairs count if either exists).")
    print("      'Missing (Physical)' counts actual missing strings on board.")

if __name__ == "__main__":
    analyze_board()
