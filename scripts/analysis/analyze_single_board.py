import ctypes
import os
import sys

# Add project root to path to find scripts.ffi_config
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
from scripts.ffi_config import load_logic_lib

class BoardAnalyzer:
    def __init__(self):
        self.lib = load_logic_lib()
        
        # Define function signatures
        self.lib.get_basis_size_ffi.restype = ctypes.c_int
        self.lib.get_basis_list_ffi.argtypes = [ctypes.POINTER(ctypes.c_int)]
        self.lib.get_found_basis_flags_ffi.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int8)]

    def analyze(self, board_str):
        size = self.lib.get_basis_size_ffi()
        
        basis_array = (ctypes.c_int * size)()
        self.lib.get_basis_list_ffi(basis_array)
        basis_members = list(basis_array)
        
        board_ints = [int(c) for c in board_str]
        board_ptr = (ctypes.c_int * 112)(*board_ints)
        flags_array = (ctypes.c_int8 * size)()
        self.lib.get_found_basis_flags_ffi(board_ptr, flags_array)
        found_flags = list(flags_array)
        
        missing = [basis_members[i] for i in range(size) if found_flags[i] == 0]
        found_count = size - len(missing)
        
        print(f"Board Analysis Results:")
        print(f"------------------------")
        print(f"Coverage Range: 1 to {max(basis_members) if basis_members else 0}")
        print(f"Total Basis Strings: {size}")
        print(f"Found: {found_count} ({found_count/size*100:.2f}%)")
        print(f"Missing: {len(missing)}")
        
        if missing:
            print("\nMissing Strings (Sorted):")
            missing.sort()
            # Group by length
            len_groups = {}
            for m in missing:
                l = len(str(m))
                if l not in len_groups:
                    len_groups[l] = []
                len_groups[l].append(m)
            
            for l in sorted(len_groups.keys()):
                print(f"Length {l} ({len(len_groups[l])}): {len_groups[l][:50]} {'...' if len(len_groups[l]) > 50 else ''}")

if __name__ == "__main__":
    board_str = "3396862045795854280718523792451231796184097163964016455187096423503912952087357807242642189681458367753149231938"
    if len(sys.argv) > 1:
        board_str = sys.argv[1]
    
    if len(board_str) != 112:
        print(f"Error: Board string must be 112 digits. Got {len(board_str)}")
        sys.exit(1)
        
    analyzer = BoardAnalyzer()
    analyzer.analyze(board_str)
