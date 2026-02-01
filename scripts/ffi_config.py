import sys
import os
import ctypes

def get_dll_path():
    """
    Returns the path to the compiled FFI library based on the operating system.
    """
    # Determine directory of this script to resolve relative paths if needed
    # But usually we run from project root. Assuming bin/ is in root.
    
    if sys.platform == "win32":
        return "bin/logic_ffi.dll"
    elif sys.platform.startswith("linux"):
        return "bin/liblogic_ffi.so"
    elif sys.platform == "darwin": # MacOS
        return "bin/liblogic_ffi.dylib"
    else:
        raise RuntimeError(f"Unsupported platform: {sys.platform}")

def load_logic_lib():
    """
    Loads and returns the logic library via ctypes.
    """
    dll_path = get_dll_path()
    if not os.path.exists(dll_path):
        raise FileNotFoundError(f"FFI Library not found at: {os.path.abspath(dll_path)}\nPlease build the project first (make on Linux, build.bat on Windows).")
    
    return ctypes.CDLL(dll_path)
