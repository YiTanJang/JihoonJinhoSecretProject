# Dependency Tree & Impact Map

This document maps the relationships between files in the repository. Use this to determine which files might be affected when you modify a specific component.

## 1. C++ Dependency Tree (Header Inclusion)

The following tree shows which headers are included by which source files. Modifying a file higher in the tree will likely require recompiling all files below it.

```text
common.h (Compile-time constants, Adjacency tables)
├── logic.h (FastBoard struct, bitboard scoring)
│   ├── mutations.h (Mutation operator prototypes)
│   │   ├── mutations.cpp
│   │   ├── solver.h / solver_4d.h
│   │   │   ├── solver.cpp / solver_4d.cpp
│   │   │   └── SAmultithread.cpp / SAmultithread_4d.cpp
│   ├── logic.cpp
│   ├── db_manager.h / db_manager_4d.h
│   │   ├── db_manager.cpp / db_manager_4d.cpp
│   │   └── SAmultithread.cpp / SAmultithread_4d.cpp
│   └── ffi_wrapper.cpp (For Python logic_ffi.dll)
│
├── config.h (Hyperparameters: Temperature, Probabilities)
│   ├── solver.cpp / solver_4d.cpp
│   └── SAmultithread.cpp / SAmultithread_4d.cpp
│
├── shared_mem.h (Monitoring data structure)
│   ├── globals.h
│   │   ├── globals.cpp
│   │   ├── db_manager.cpp / db_manager_4d.cpp
│   │   ├── solver.cpp / solver_4d.cpp
│   │   └── SAmultithread.cpp / SAmultithread_4d.cpp
│   ├── SAmultithread.cpp / SAmultithread_4d.cpp
│   └── check_shm.cpp (Tool)
```

### Critical Impact Nodes
*   **`common.h`**: **High Impact**. Any change here triggers a full recompile of the entire system and potentially changes the fundamental scoring logic.
*   **`logic.h`**: **High Impact**. Affects scoring, FFI, and all solvers.
*   **`shared_mem.h`**: **Medium Impact**. Modifying this requires updating all Python monitoring scripts as their `ctypes` structures will no longer match.

---

## 2. Python & C++ Interop Map

The Python tools interact with the C++ core through two primary channels:

### Channel A: Shared Memory (Real-time Monitoring)
`SAmultithread_4d.exe` $\rightarrow$ `Local\SAMonitor4D` (RAM) $\leftarrow$ `scripts/shared_mem_lib.py` $\leftarrow$ Monitors

**Affected by changes to**: `include/shared_mem.h`

### Channel B: FFI (Direct Scoring)
`logic_ffi.dll` (from `src/ffi_wrapper.cpp`) $\leftarrow$ `scripts/evaluate_board.py` / `ai_optimizer.py`

**Affected by changes to**: `include/logic.h`, `src/logic.cpp`, `src/ffi_wrapper.cpp`

---

## 3. Build System Dependencies

The batch scripts in `scripts/batch/` define how files are linked:

- **`build_4d.bat`**: Links `SAmultithread_4d.cpp`, `solver_4d.cpp`, `db_manager_4d.cpp`, `globals.cpp`, `logic.cpp`, `mutations.cpp`, and `sqlite3.c`.
- **`build_ffi.bat`**: Compiles `logic.cpp` and `ffi_wrapper.cpp` into a DLL.

---

## 4. Database Schema Dependency
Files that interact with `optimizer_4d.db`:
- **C++**: `db_manager_4d.cpp` (Schema definition and writes).
- **Python**: All scripts in `scripts/maintenance/` and `scripts/monitoring/show_best_board.py`.

**Warning**: Changing the table schema in `db_manager_4d.cpp` will break almost all Python maintenance scripts.

```