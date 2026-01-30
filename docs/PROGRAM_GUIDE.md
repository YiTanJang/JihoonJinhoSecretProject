# SAMultithread Program Guide

This guide provides a comprehensive overview of the various executables and scripts in the SAMultithread project.

## Core Solvers (`bin/`)

These are the primary engines for optimizing the board.

*   **`SAmultithread_4d.exe`**: **[MAIN]** The flagship solver.
    *   **Function**: Runs a highly optimized, multithreaded Simulated Annealing (SA) algorithm.
    *   **Features**: Uses 12 concurrent threads ("islands"), shared memory monitoring, SQLite logging, and advanced mutation strategies (Micro/Meso/Macro moves).
    *   **Usage**: `bin\SAmultithread_4d.exe` (run via `run_4d.bat`).

*   **`DLASRunner.exe`**:
    *   **Function**: Runs the Diversified Late Acceptance Search (DLAS) algorithm.
    *   **Purpose**: An alternative meta-heuristic to SA. Useful for escaping local optima that SA might get trapped in.
    *   **Usage**: `bin\DLASRunner.exe`.

*   **`SAmultithread.exe`**:
    *   **Function**: Legacy or specific 3D-focused version of the SA solver.
    *   **Status**: Mostly superseded by `_4d` version but kept for reference or specific non-4D tasks.

## Analysis & Validation Tools (`bin/`)

Tools for verifying results and debugging the system.

*   **`BoardValidator.exe`**:
    *   **Function**: validating board integrity and scoring.
    *   **Usage**: Checks if a given board string meets the problem constraints and calculates its score.

*   **`check_shm.exe`**:
    *   **Function**: Shared Memory Debugger.
    *   **Purpose**: Prints the memory layout (offsets and sizes) of the C++ structs used for shared memory. Essential for ensuring the Python monitor (`monitor_4d.py`) reads data correctly.

*   **`TestUnbiasedScore.exe`**:
    *   **Function**: Scoring Logic Benchmark.
    *   **Purpose**: Tests the "Unbiased" scoring algorithm (and potentially others) for correctness and performance.

## Brute Force & Experimental Tools (`bin/`)

Prototypes and specialized solvers for sub-problems.

*   **`BruteForceBacktrack.exe`**:
    *   **Function**: Depth-First Search (DFS) solver.
    *   **Purpose**: Attempts to solve the board (or small sections) using exhaustive backtracking. computationally infeasible for the full 8x14 board but useful for small patches.

*   **`RecursiveBacktrack.exe`**:
    *   **Function**: Alternative recursive backtracking implementation.

*   **`BruteForcePermutation.exe`**:
    *   **Function**: Permutation Solver.
    *   **Purpose**: Optimizes a board by permuting the digit mapping (e.g., swapping all 1s and 2s) to see if the score improves.

*   **`PermuteMyBoard.exe`**:
    *   **Function**: Targeted Permutation Tool.
    *   **Purpose**: Likely a focused tool for applying specific permutations to a specific board input.

*   **`RichnessOptimizer.exe`**:
    *   **Function**: Simple Optimizer.
    *   **Purpose**: A lighter-weight or earlier prototype of the richness optimization logic.

## Python Scripts

### Root Directory
*   **`run_4d.bat`**: Shortcut to run `SAmultithread_4d.exe`.
*   **`calculate_basis_12000.py`**: Pre-computation script. Generates or verifies the "basis set" (all paths) for numbers up to 12,999 (Current range: `BASIS_MAX_RANGE` in `config.h`).
*   **`check_10012.py`**: Specific check script for the 10,012 milestone.
*   **`check_path.py`**: Path verification utility.

### Monitoring (`scripts/monitoring/`)
*   **`monitor_4d.py`**: **[MAIN]** The real-time dashboard. Connects to the running C++ solver via shared memory to show temperature, score, and acceptance rates.

### Maintenance (`scripts/maintenance/`)
*   **`analyze_physics.py`**: Analyzes the log data (SQLite) produced by the solver to tune parameters (temperature schedules, mutation weights).
*   **`prune_db.py`**: Cleans up the SQLite database by removing old or low-scoring entries.
*   **`recalc_scores.py`**: Re-calculates scores for boards in the DB (useful if the scoring function changes).
