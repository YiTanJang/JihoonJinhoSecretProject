# Build System

The project is designed for Windows using the Microsoft Visual C++ (MSVC) compiler.

## Prerequisites

- **Visual Studio 2022** (Community or Build Tools) with "Desktop development with C++" workload.
- **Python 3.10+** (for monitoring and maintenance scripts).
- **SQLite3** (Library is included in `lib/sqlite3`, but having the CLI tool is helpful).

## Build Commands

All build scripts are located in `scripts\batch\`.

### 1. Main Solver (4D)
```batch
scripts\batch\build_4d.bat
```
This produces `bin\SAmultithread_4d.exe`. It uses C++20 (`/std:c++latest`) and heavy constexpr precomputation.

### 2. Full Build
```batch
scripts\batch\build.bat
```
Builds all components including legacy solvers and utility tools.

### 3. FFI Wrapper
```batch
scripts\batch\build_ffi.bat
```
Produces `bin\logic_ffi.dll`, which allows the Python evaluation scripts to call the high-performance C++ scoring logic directly.

## Compiler Flags

- `/O2`: Maximum speed optimization.
- `/EHsc`: Standard exception handling.
- `/utf-8`: Force UTF-8 encoding (required for some source comments).
- `/constexpr:steps10000000`: Increased limit for complex compile-time table generation.
- `/I include`: Header search path.

## Dependencies

- **SQLite3**: Bundled in the source. No external installation required.
- **Python Libraries**:
  - `sqlite3`: Standard library.
  - `matplotlib`, `numpy`: Required for `graph_monitor.py`.
  - `pandas`: Used in some analysis scripts.

