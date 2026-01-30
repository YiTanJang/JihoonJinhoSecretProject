import numpy as np
import ctypes
import os

class BoardEnv:
    def __init__(self):
        self.rows = 8
        self.cols = 14
        
        # Load the C++ DLL
        dll_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../bin/logic_ffi.dll"))
        self.lib = ctypes.CDLL(dll_path)
        
        # Define function signatures
        self.lib.calculate_score.argtypes = [ctypes.POINTER(ctypes.c_int)]
        self.lib.calculate_score.restype = ctypes.c_int
        
        self.lib.calculate_freq_score.argtypes = [ctypes.POINTER(ctypes.c_int)]
        self.lib.calculate_freq_score.restype = ctypes.c_int
        
        self.lib.calculate_hybrid_sqrt_score.argtypes = [ctypes.POINTER(ctypes.c_int)]
        self.lib.calculate_hybrid_sqrt_score.restype = ctypes.c_int

        self.lib.calculate_sum_score.argtypes = [ctypes.POINTER(ctypes.c_int)]
        self.lib.calculate_sum_score.restype = ctypes.c_int

                self.lib.calculate_hybrid_score.argtypes = [ctypes.POINTER(ctypes.c_int)]
                self.lib.calculate_hybrid_score.restype = ctypes.c_int
        
                self.lib.calculate_richness_score.argtypes = [ctypes.POINTER(ctypes.c_int)]
                self.lib.calculate_richness_score.restype = ctypes.c_longlong
                
                self.reset()
        
            def reset(self, board=None):
                if board is not None:
                    self.board = np.array(board, dtype=np.int32).copy()
                else:
                    self.board = np.random.randint(0, 10, (self.rows, self.cols), dtype=np.int32)
                return self.get_state()
        
            def get_state(self):
                # One-hot encoding for the network (10, 8, 14)
                state = np.zeros((10, self.rows, self.cols), dtype=np.float32)
                for r in range(self.rows):
                    for c in range(self.cols):
                        state[self.board[r, c], r, c] = 1.0
                return state
        
            def step(self, action):
                """
                Action: integer from 0 to 1119
                cell_idx = action // 10
                new_digit = action % 10
                """
                cell_idx = action // 10
                new_val = action % 10
                r, c = cell_idx // self.cols, cell_idx % self.cols
                
                self.board[r, c] = new_val
                
                score = self.get_score()
                return self.get_state(), score
        
            def get_score(self):
                # Default to Richness Score for Neural Solver training
                board_ptr = self.board.ctypes.data_as(ctypes.POINTER(ctypes.c_int))
                return self.lib.calculate_richness_score(board_ptr)
        
            def get_richness_score(self):
                board_ptr = self.board.ctypes.data_as(ctypes.POINTER(ctypes.c_int))
                return self.lib.calculate_richness_score(board_ptr)
    def get_hybrid_sqrt_score(self):
        board_ptr = self.board.ctypes.data_as(ctypes.POINTER(ctypes.c_int))
        return self.lib.calculate_hybrid_sqrt_score(board_ptr)

    def get_param_score(self):
        board_ptr = self.board.ctypes.data_as(ctypes.POINTER(ctypes.c_int))
        return self.lib.calculate_score(board_ptr)

    def get_freq_score(self):
        board_ptr = self.board.ctypes.data_as(ctypes.POINTER(ctypes.c_int))
        return self.lib.calculate_freq_score(board_ptr)

    def get_sum_score(self):
        board_ptr = self.board.ctypes.data_as(ctypes.POINTER(ctypes.c_int))
        return self.lib.calculate_sum_score(board_ptr)

    def get_hybrid_score(self):
        board_ptr = self.board.ctypes.data_as(ctypes.POINTER(ctypes.c_int))
        return self.lib.calculate_hybrid_score(board_ptr)

if __name__ == "__main__":
    env = BoardEnv()
    print(f"Initial Score (C++ FFI): {env.get_score()}")
    
    # Test a known connection
    test_board = np.zeros((8, 14), dtype=np.int32)
    test_board[0, 0] = 1
    test_board[0, 1] = 2
    test_board[0, 2] = 3
    env.reset(test_board)
    print(f"Test Board Score (C++ FFI): {env.get_score()}")