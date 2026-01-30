import ctypes
import os
import sys
import numpy as np
import matplotlib.pyplot as plt
import argparse

class HeatmapGenerator:
    def __init__(self):
        # Resolve absolute path to the DLL
        base_dir = os.path.dirname(os.path.abspath(__file__))
        dll_path = os.path.abspath(os.path.join(base_dir, "../../bin/logic_ffi.dll"))
        
        if not os.path.exists(dll_path):
            dll_path = os.path.abspath(os.path.join(os.getcwd(), "bin/logic_ffi.dll"))
            
        if not os.path.exists(dll_path):
            print(f"Error: {dll_path} not found. Please build the project first.")
            sys.exit(1)
            
        try:
            self.lib = ctypes.CDLL(dll_path)
        except Exception as e:
            print(f"Error loading DLL: {e}")
            sys.exit(1)
        
        # Define function signatures
        self.lib.calculate_basis_score_extended.argtypes = [ctypes.POINTER(ctypes.c_int)]
        self.lib.calculate_basis_score_extended.restype = ctypes.c_longlong

    def calculate_score(self, board_data):
        board_ptr = board_data.ctypes.data_as(ctypes.POINTER(ctypes.c_int))
        return self.lib.calculate_basis_score_extended(board_ptr)

    def generate_heatmap(self, board_str):
        if len(board_str) != 112:
            raise ValueError(f"Board string must be 112 chars, got {len(board_str)}")
            
        try:
            original_board = np.array([int(c) for c in board_str], dtype=np.int32)
        except ValueError:
            raise ValueError("Board string must contain only digits 0-9")
            
        base_score = self.calculate_score(original_board)
        heatmap = np.zeros(112, dtype=np.int64)
        
        print(f"Base Extended Basis Score: {base_score}")
        print("Calculating heatmap (punching holes with -1)...")
        
        for i in range(112):
            original_val = original_board[i]
            
            # Punch hole (set to -1)
            original_board[i] = -1
            new_score = self.calculate_score(original_board)
            
            # Decrease in score is the temperature
            diff = base_score - new_score
            heatmap[i] = diff
            
            # Restore
            original_board[i] = original_val
            
        return heatmap.reshape((8, 14))

def visualize_heatmap(heatmap, output_file="heatmap.png"):
    plt.figure(figsize=(14, 8))
    plt.imshow(heatmap, cmap='hot', interpolation='nearest')
    plt.colorbar(label='Temperature (Basis Score Decrease)')
    plt.title('Board Heatmap (Extended Basis Score Sensitivity)')
    
    # Add text annotations
    for i in range(8):
        for j in range(14):
            # Choose text color based on background intensity
            text_color = 'white' if heatmap[i, j] < heatmap.max() / 2 else 'black'
            # Or simplified: Blue for everything might be hard to read on dark red
            # Let's stick to blue or cyan for now, or simple adaptive
            plt.text(j, i, str(heatmap[i, j]), ha='center', va='center', color='cyan', fontsize=8, fontweight='bold')
            
    plt.tight_layout()
    plt.savefig(output_file)
    print(f"Heatmap image saved to {output_file}")

def print_grid(heatmap):
    print("\nHeatmap Grid:")
    print("-" * 75)
    for row in heatmap:
        print(" ".join(f"{val:4}" for val in row))
    print("-" * 75)

def main():
    parser = argparse.ArgumentParser(description="Generate a heatmap of board sensitivity.")
    parser.add_argument("board", help="112-character board string")
    parser.add_argument("--out", default="heatmap.png", help="Output image filename")
    parser.add_argument("--raw", action="store_true", help="Print raw values only, no image")
    
    args = parser.parse_args()
    
    try:
        generator = HeatmapGenerator()
        heatmap = generator.generate_heatmap(args.board)
        
        print_grid(heatmap)
        
        if not args.raw:
            visualize_heatmap(heatmap, args.out)
            
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()