# Development Guide

This guide is for developers maintaining or extending the SAMultithread optimizer.

## Coding Style
- **Performance First**: Avoid unnecessary allocations in the inner SA loop. Use the `FastBoard` bitboard for all scoring operations.
- **Constexpr Precomputation**: Large tables (digits, adjacency, weights) should be generated at compile-time in `include/common.h`.
- **Thread Safety**: 
  - Each `SAIsland4D` is independent. 
  - Database access is handled via a global lock or independent connections per thread (SQLite handles file-level locking).
  - Monitoring data is written to shared memory without locking (best-effort consistency).

## ⚠️ DO NOT TOUCH (Critical Sections)

- **`include/common.h` - `DIGIT_TABLE` & `ADJ_TABLE`**: Any changes here will break the `FastBoard` logic.
- **`src/logic.cpp` - `can_make_bitboard`**: This is the most performance-critical function in the entire project. Even a minor change can slow down the solver by 20-30%.
- **`src/solver_4d.cpp` - Temperature Scaling**: The relationship between `scale_multiplier` and the scoring function is finely tuned. Changing one without the other will prevent the system from "melting" or "freezing" correctly.

## Known Issues & Future Improvements

### 1. Scoring Logic Refinement
- **The "0-prefix" removal strategy**: Currently, we map the leading digit of a missing 4-digit number to '0' to effectively remove it. This logic is sound but could be better integrated into the `optimize_board_permutation` function.
- **Adaptive Weighting**: The EMA-based weighting for mutations is currently linear. A non-linear or softmax-based selection might be more robust.

### 2. Performance
- **SIMD Bitboards**: The current `FastBoard` uses 16-bit integers. Moving to AVX2/AVX-512 for parallel bitboard checks could provide a massive speedup.
- **DB Bottleneck**: During high-frequency logging, SQLite can become a bottleneck. Using an in-memory buffer that flushes to disk periodically (or a dedicated logging thread) is recommended.

### 3. Algorithm
- **Ratchet Annealing**: Implement decaying reheat cycles where each subsequent cycle has a lower peak temperature.
- **Morphing Strategy**: Start the annealing with a "Ductile" scoring function (Strategy 1) and morph into a "Brittle" one (Strategy 0) as the temperature drops.

### 4. Code Quality
- **Refactor `mutations.cpp`**: The file is becoming very large (900+ lines). It should be split into `mutations_micro.cpp`, `mutations_meso.cpp`, etc.
- **Header Cleanup**: There is some duplication between `solver.h` and `solver_4d.h`. These should be unified using a template or base class.
