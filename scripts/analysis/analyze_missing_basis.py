import ctypes
import os
import sqlite3
import numpy as np
from collections import Counter

# Configuration
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
DB_PATH = os.path.join(PROJECT_ROOT, "db", "optimizer_4d.db")
TOP_COUNT = 80
DLL_PATH = os.path.join(PROJECT_ROOT, "bin", "logic_ffi.dll")

class MissingBasisAnalyzer:
    def __init__(self):
        if not os.path.exists(DLL_PATH):
            raise FileNotFoundError(f"DLL not found: {DLL_PATH}")
        self.lib = ctypes.CDLL(DLL_PATH)
        
        self.lib.get_basis_size_ffi.restype = ctypes.c_int
        self.lib.get_basis_list_ffi.argtypes = [ctypes.POINTER(ctypes.c_int)]
        self.lib.get_found_basis_flags_ffi.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int8)]

    def get_basis_members(self):
        size = self.lib.get_basis_size_ffi()
        basis_array = (ctypes.c_int * size)()
        self.lib.get_basis_list_ffi(basis_array)
        return list(basis_array)

    def get_found_flags(self, board_str):
        size = self.lib.get_basis_size_ffi()
        board_ints = [int(c) for c in board_str]
        board_ptr = (ctypes.c_int * 112)(*board_ints)
        flags_array = (ctypes.c_int8 * size)()
        self.lib.get_found_basis_flags_ffi(board_ptr, flags_array)
        return list(flags_array)

def main():
    analyzer = MissingBasisAnalyzer()
    basis_members = analyzer.get_basis_members()
    basis_size = len(basis_members)
    
    # Filter for 4-digit and 5-digit members only (per user request)
    filtered_indices = [i for i, val in enumerate(basis_members) if val >= 1000]
    filtered_basis = [basis_members[i] for i in filtered_indices]
    
    print(f"Total Basis Members: {basis_size}")
    print(f"Filtered (>=1000): {len(filtered_basis)}")

    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("SELECT board_data, score FROM best_boards ORDER BY score DESC LIMIT ?", (TOP_COUNT,))
    rows = cursor.fetchall()
    conn.close()

    if not rows:
        print("No boards found in DB.")
        return

    print(f"Analyzing top {len(rows)} boards...")

    # missing_counts[basis_index] = count of boards it's missing from
    missing_counts = Counter()

    for board_str, score in rows:
        flags = analyzer.get_found_flags(board_str)
        for i in filtered_indices:
            if flags[i] == 0:
                missing_counts[i] += 1

    # Report results
    results = []
    for i in filtered_indices:
        count = missing_counts[i]
        if count > 0:
            results.append((basis_members[i], count))

    # Sort by frequency of missing (most common missing first)
    results.sort(key=lambda x: x[1], reverse=True)

    print("\n--- Top Missing Basis Members Statistics ---")
    print(f"{'Value':<10} | {'Missing From (of ' + str(len(rows)) + ')':<20} | {'Type'}")
    print("-" * 45)
    for val, count in results:
        b_type = "4D" if val < 10000 else "5D"
        print(f"{val:<10} | {count:<20} | {b_type}")

    if not results:
        print("No missing members found in top boards!")

if __name__ == "__main__":
    main()
