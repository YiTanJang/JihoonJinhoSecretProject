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
logic_lib.optimize_board_and_score.argtypes = [ctypes.POINTER(ctypes.c_int)]
logic_lib.optimize_board_and_score.restype = ctypes.c_longlong

def permute_and_update(db_path=r"db/optimizer_4d.db"):
    abs_db_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", db_path))
    
    if not os.path.exists(abs_db_path):
        print(f"DB not found: {abs_db_path}")
        return

    conn = sqlite3.connect(abs_db_path)
    cursor = conn.cursor()
    
    print("Fetching boards from 'best_boards'...")
    cursor.execute("SELECT id, board_data, score FROM best_boards ORDER BY score DESC")
    rows = cursor.fetchall()
    
    print(f"{'ID':<5} | {'Old Score':<10} | {'New Score':<10} | {'Change':<8} | {'Board Changed'}")
    print("-" * 60)
    
    updated_count = 0
    
    for row in rows:
        bid, board_str, old_score = row
        
        if len(board_str) != 112:
            print(f"Invalid board length for ID {bid}")
            continue
            
        board_ints = [int(c) for c in board_str]
        
        # Create a ctypes array (mutable)
        c_board = (ctypes.c_int * 112)(*board_ints)
        
        # Call the optimizer (modifies c_board in place)
        new_score = logic_lib.optimize_board_and_score(c_board)
        
        # Convert back to string
        new_board_str = "".join(str(c_board[i]) for i in range(112))
        
        board_changed = (new_board_str != board_str)
        score_changed = (new_score != old_score)
        
        status = "No"
        # Only update if the new score is better or equal to the old score
        if new_score >= old_score and (board_changed or score_changed):
            cursor.execute(
                "UPDATE best_boards SET score = ?, board_data = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?", 
                (int(new_score), new_board_str, bid)
            )
            updated_count += 1
            status = "YES"
        elif new_score < old_score:
            status = "LOWER!" # Should not happen with correct permutation logic, but safety first
            
        print(f"{bid:<5} | {old_score:<10} | {new_score:<10} | {new_score - old_score:<8} | {status}")

    conn.commit()
    conn.close()
    print(f"\nProcessing complete. Updated {updated_count} boards.")

if __name__ == "__main__":
    permute_and_update()
