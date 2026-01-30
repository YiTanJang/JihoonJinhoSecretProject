# Simulated Annealing Implementation Summary (BOJ 18789)

## 1. Architecture & Parallelism
*   **Multi-threading:** The solver utilizes `std::thread` to run multiple independent SA instances (`SAIsland4D`).
*   **Shared State:** Threads share a "Gene Pool" of elite boards and a global best-score database.
*   **Lineage System:** Each thread maintains a persistent "lineage" of its solution, periodically checkpointing to a SQLite database (`optimizer_results.db`) to allow recovery and long-term optimization.

## 2. Cooling Schedule & Thermal Physics
*   **Schedule Type:** Cyclic Cooling with Dynamic Reheating.
*   **Cooling Function:** Geometric cooling with a rate of `0.9999994`.
    *   **Phase Transition Zone:** A dynamic slowdown factor is applied when temperature $T$ is within $[0.3 \times T_{crit}, 2.0 \times T_{crit}]$. In this zone, the cooling rate is effectively raised to the power of 0.2, significantly extending the time spent in the critical "crystallization" phase.
*   **Reheat Logic:**
    *   **Calibration:** At the start of a new cycle, $T_{init}$ is calibrated by sampling random moves to determine the average energy delta of "bad" moves. The temperature is set to achieve a specific target acceptance rate (starting at 40% and decaying exponentially with each cycle).
    *   **Stagnation:** If a thread fails to improve its local best score for 3 consecutive cycles, a "Hard Reset" (Reseed) is triggered.

## 3. Objective Function: Basis-Extended-Twin Scoring
The solver uses a specialized scoring function designed to efficiently approximate the "Richness" (number of unique 4-digit strings) of the board.
*   **The Basis Set:** A precomputed minimal set of strings (lengths 4 and 5) that cover the target integer range.
    *   **Construction:** Created via a greedy span-covering algorithm (`get_span_cpp`). A minimal set of "seed strings" is selected such that if these seeds exist on the board, all numbers in the range `[0, 12999]` are mathematically guaranteed to be formed. (Range controlled by `BASIS_MAX_RANGE` in `config.h`)
    *   **Extended Range:** The set includes 5-digit strings to robustly cover the required 4-digit range `0000-9999`.
*   **Twin Bonus:** A weighted bonus is applied for finding Basis strings that contain "twins" (consecutive identical digits, e.g., `11`, `22`).
    *   **Formula:** $Score = Count_{basis} + 0.5 \times Count_{twins} + 1.0 \times Count_{double\_twins}$.
    *   **Double Twins:** Strings with two distinct pairs of twins (e.g., `1122`, `3300`) receive an additional bonus to encourage complex structure formation.
*   **Optimization:** Scoring is performed via a pruned DFS (`dfs_basis_pruned`) using a lookup table (`NODE_FLAGS`).

## 4. Adaptive Operator Selection (ALNS)
The solver uses Adaptive Large Neighborhood Search (ALNS) to dynamically adjust mutation probabilities every 100 iterations based on performance.

### Probability Constraints
| Operator Group | Specific Operators | Floor (Min %) | Cap (Max %) | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Micro** | `Dist 1 Swap`, `Dist 2 Swap` | **0.1%** | **40%** | High-mobility local search. |
| **Destructive** | `Global Swap`, `Rand Cell Mut` | **1.0%** | **3.0%** | Strictly limited to prevent structure breaking. |
| **Struct. Meso** | `Local Domino` | **4.0%** | **35%** | Key structural shifters. |
| **Global Meso** | `Global Domino Swap` | **4.0%** | **30%** | Long-range structural transfer. |
| **Other Meso** | `Tri Rotate`, `Slide`, `Worm`, `Block Rotate` | **2.0%** | - | Geometry-specific moves. `Block Rotate` replaced `Ring Shift`. |
| **Heuristic** | `Heatmap Swap` | **2.0%** | **50%** | Dynamically capped at **5%** during low temp. |
| **Heuristic** | `Heatmap Domino` | **2.0%** | **50%** | Like Heatmap Swap but moves a domino pair. |
| **Heuristic** | `Heatmap Mutate` | **4.0%** | **50%** | Dynamically capped at **5%** during low temp. |
| **Macro** | `Block Swap`, `Block Flip` | **4.0%** | - | Large-scale reconfiguration. |

## 5. Mutation Operators (The Move Set)
The solver employs 14 operators classified by scope:

### Micro & Meso Moves
*   Includes adjacent swaps, Knight's move swaps, random cell mutations, and global swaps.
*   **Domino Swap:** Swaps two non-overlapping dominoes (Horizontal/Vertical/Diagonal) with retry logic.
*   **Slides/Rotates:** `straight_slide`, `worm_slide`, `triangle_rotate`, `variable_block_rotate`.

### Heuristic Moves (Score Contribution Heatmap)
Heuristic operators (`heatmap_swap`, `heatmap_mutate`) utilize an optimized **2-Pass Critical Path Mapping** algorithm to calculate a "Score Drop Heatmap" for every cell.
*   **Selection Strategy (Rank-Based):** Cells are sorted by their heatmap value (score contribution). A **Linear Rank Selection** is used where the probability of selecting a cell is inversely proportional to its rank ($P \propto N - Rank$). This strongly biases selection towards "useless" or low-contribution cells while maintaining some diversity.
*   **`heatmap_swap`**:
    1.  Selects a low-contribution source cell using Rank-Based Selection.
    2.  Selects a **second low-contribution target cell** using Rank-Based Selection.
    3.  Swaps them to aggressively relocate redundant digits to new positions.
*   **`heatmap_mutate`**:
    1.  Selects a low-contribution target cell using Rank-Based Selection.
    2.  Mutates its value to a digit that is statistically most needed to form currently missing Basis strings (weighted probability).

### Macro-Moves
*   `variable_block_swap` and `variable_block_flip` for large-scale structural rearrangement.
