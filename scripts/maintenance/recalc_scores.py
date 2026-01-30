import sqlite3
import ctypes
import os
import sys

# Load DLL
dll_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "bin", "logic_ffi.dll"))

try:
    logic_lib = ctypes.CDLL(dll_path)
except Exception as e:
    print(f"Failed to load DLL at {dll_path}: {e}")
    sys.exit(1)

# Define function signature
logic_lib.calculate_basis_score_extended.argtypes = [ctypes.POINTER(ctypes.c_int)]
logic_lib.calculate_basis_score_extended.restype = ctypes.c_longlong

def recalc_scores(db_path=r"db/optimizer_4d.db"):
    # Fix DB path relative to script if needed, or use absolute
    abs_db_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", db_path))
    
    if not os.path.exists(abs_db_path):
        print(f"DB not found: {abs_db_path}")
        return

    conn = sqlite3.connect(abs_db_path)
    cursor = conn.cursor()
    
    cursor.execute("SELECT id, board_data, score FROM best_boards ORDER BY score DESC")
    rows = cursor.fetchall()
    
    print(f"{'ID':<5} | {'Old Score':<15} | {'Basis Score':<15} | {'Diff':<10} | {'Status'}")
    print("-" * 70)
    
    for row in rows:
        bid, board_str, old_score = row
        
        # Parse board string to int array
        if len(board_str) != 112:
            print(f"Invalid board length for ID {bid}: {len(board_str)}")
            continue
            
        board_ints = [int(c) for c in board_str]
        c_board = (ctypes.c_int * 112)(*board_ints)
        
        new_score = logic_lib.calculate_basis_score_extended(c_board)
        
        diff = new_score - old_score
        
        status = "Skipped"
        if diff != 0:
            cursor.execute("UPDATE best_boards SET score = ? WHERE id = ?", (int(new_score), bid))
            status = "Updated"
        
        print(f"{bid:<5} | {old_score:<15} | {new_score:<15} | {diff:<+10} | {status}")

    conn.commit()
    conn.close()
    print("\nDatabase scores updated and committed.")

if __name__ == "__main__":
    recalc_scores()
