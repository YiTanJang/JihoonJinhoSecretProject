# Diversified Late Acceptance Search (DLAS) Solver

The DLAS Solver is a metaheuristic optimization tool designed to maximize the presence of a "Basis Set" of numbers on the board. Unlike Simulated Annealing (SA), which relies on a cooling temperature, DLAS uses a fitness buffer to allow the search to traverse plateaus and escape local optima.

## 1. How DLAS Works

DLAS maintains two key structures:
1.  **A pool of solutions**: It tracks the *Current* solution, the *Best* solution, and a *Previous* solution.
2.  **A Fitness Buffer**: An array of size `L` that stores the best scores from recent iterations.

### Acceptance Criteria
A new mutation is accepted if:
*   The new score is **better than or equal** to the current score.
*   The new score is **better than the minimum** value in the fitness buffer (Late Acceptance).

This allows the solver to accept "worse" moves temporarily, provided they are still better than what the solver found `L` iterations ago. This "memory" effect helps the search navigate flat regions of the fitness landscape.

---

## 2. Tuning Parameters

### A. Basis Range and Format (in `include/config.h`)
The basis set used for scoring is defined globally:
*   `BASIS_MAX_RANGE`: Set this to `13000` for the 1-12999 range (Managed in `include/utils/config.h`).
*   `BASIS_USE_PADDING`: Set to `false` if you don't want leading zeros (e.g., `123` instead of `00123`).

### B. Search Intensity (in `tools/DLASRunner.cpp`)
Inside the `main` function of `DLASRunner.cpp`, find the line:
`solver.run(100000000, 5000000);`

*   **Total Iterations (`100,000,000`)**: The maximum number of moves to try.
*   **Stagnation Limit (`5,000,000`)**: If no new *Global Best* is found for this many moves, the solver stops.

### C. Exploration Depth (in `include/dlas_solver.h`)
The "Memory" of the algorithm is controlled by the template parameter:
`DLASSolver<100> solver(...)`

*   **Buffer Length (`100`)**: 
    *   **Smaller (e.g., 50)**: Makes the search "stiffer" and more greedy. It converges faster but can get stuck easily.
    *   **Larger (e.g., 500)**: Makes the search more "relaxed" and exploratory. It can wander through low-scoring regions to find better peaks.

### D. Mutation Distribution (in `include/dlas_solver.h`)
Inside `DLASSolver::apply_mutation`, you can adjust the probabilities of different moves:
*   Increase **Micro-Moves** (swaps) for fine-tuning.
*   Increase **Meso-Moves** (rotations/worm slides) for structural diversification.
*   The current mix includes **Greedy Basis Optimization**, which is very powerful but slower.

---

## 3. Usage Instructions

### Build
Run the dedicated batch script to compile the runner:
```powershell
scripts\batch\build_dlas.bat
```

### Run
Execute the compiled binary:
```powershell
bin\DLASRunner.exe
```

### Monitoring
The solver prints status updates to the console every **100,000 iterations**:
*   `Current`: The score of the board currently being mutated.
*   `Best`: The highest score found since the start.
*   `MinBuf`: The lowest score in the fitness buffer (the "bar" to beat for acceptance).

---

## 4. When to use DLAS vs SA
*   **SA (Simulated Annealing)**: Best for finding the general "shape" of a good board from a completely random start.
*   **DLAS**: Excellent for "ridge-following" and breaking through plateaus once you have a decent board. It is less sensitive to initial temperature tuning than SA.
