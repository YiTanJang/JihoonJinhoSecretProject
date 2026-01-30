import sqlite3
import ctypes
import os
import sys

# Structure definition must match logic.h (Size 6 for topo arrays)
class BiasReport(ctypes.Structure):
    _fields_ = [
        ("fill_3d", ctypes.c_int),
        ("cap_3d", ctypes.c_int),
        ("fill_4d", ctypes.c_int),
        ("cap_4d", ctypes.c_int),
        ("fill_unique", ctypes.c_int * 5),
        ("cap_unique", ctypes.c_int * 5),
        ("fill_type", ctypes.c_int * 4),
        ("cap_type", ctypes.c_int * 4),
        ("fill_topo", ctypes.c_int * 6),
        ("cap_topo", ctypes.c_int * 6),
    ]

# Load DLL
dll_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "bin", "logic_ffi.dll"))
logic_lib = None

try:
    logic_lib = ctypes.CDLL(dll_path)
    logic_lib.analyze_bias.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.POINTER(BiasReport)]
    logic_lib.analyze_bias.restype = None
except Exception as e:
    print(f"Failed to load DLL at {dll_path}: {e}")
    sys.exit(1)

def analyze_bias(db_path=r"db/optimizer_4d.db"):
    abs_db_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", db_path))
    if not os.path.exists(abs_db_path):
        print(f"DB not found: {abs_db_path}")
        return

    conn = sqlite3.connect(abs_db_path)
    cursor = conn.cursor()
    
    # Get latest 28 boards by ID
    cursor.execute("SELECT id, board_data FROM best_boards ORDER BY id DESC LIMIT 28")
    rows = cursor.fetchall()
    
    # Topo 2: PingPong, Topo 3: Backtrack, Topo 4: Cycle, Topo 5: Linear
    print(f"{'ID':<5} | {'3D %':<6} | {'4D %':<6} | {'Pp (2)':<7} | {'Bt (3)':<7} | {'Cyc (4)':<7} | {'Lin (5)':<7}")
    print("-" * 75)
    
    avg_3d = 0
    avg_4d = 0
    avg_t2 = 0
    avg_t3 = 0
    avg_t4 = 0
    avg_t5 = 0
    count = 0

    for row in rows:
        bid, board_str = row
        if len(board_str) != 112: continue
        
        board_ints = [int(c) for c in board_str]
        c_board = (ctypes.c_int * 112)(*board_ints)
        report = BiasReport()
        
        logic_lib.analyze_bias(c_board, ctypes.byref(report))
        
        p_3d = (report.fill_3d / report.cap_3d * 100) if report.cap_3d else 0
        p_4d = (report.fill_4d / report.cap_4d * 100) if report.cap_4d else 0
        
        p_t2 = (report.fill_topo[2] / report.cap_topo[2] * 100) if report.cap_topo[2] else 0
        p_t3 = (report.fill_topo[3] / report.cap_topo[3] * 100) if report.cap_topo[3] else 0
        p_t4 = (report.fill_topo[4] / report.cap_topo[4] * 100) if report.cap_topo[4] else 0
        p_t5 = (report.fill_topo[5] / report.cap_topo[5] * 100) if report.cap_topo[5] else 0
        
        print(f"{bid:<5} | {p_3d:<6.1f} | {p_4d:<6.1f} | {p_t2:<7.1f} | {p_t3:<7.1f} | {p_t4:<7.1f} | {p_t5:<7.1f}")
        
        avg_3d += p_3d
        avg_4d += p_4d
        avg_t2 += p_t2
        avg_t3 += p_t3
        avg_t4 += p_t4
        avg_t5 += p_t5
        count += 1
        
    print("-" * 75)
    if count > 0:
        print(f"AVG   | {avg_3d/count:<6.1f} | {avg_4d/count:<6.1f} | {avg_t2/count:<7.1f} | {avg_t3/count:<7.1f} | {avg_t4/count:<7.1f} | {avg_t5/count:<7.1f}")

    conn.close()

if __name__ == "__main__":
    analyze_bias()
