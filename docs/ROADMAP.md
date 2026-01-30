# Future Roadmap: From Experiment to Record-Breaker

The `4d` side-project has served as a high-velocity laboratory for testing Simulated Annealing (SA) physics, mutation balancing, and scoring strategies. The future of this repository lies in **Convergent Evolution**: bringing the "high-tech" experimental features from 4D back into a unified, high-performance 5D engine.

## Phase 1: The Unified Engine (V3)
Currently, logic is split between `SAmultithread` and `SAmultithread_4d`. 
- **Goal**: Create a single `RichnessOptimizer` that is sequence-length agnostic.
- **Key Task**: Port the **Anarchy Lexicographical Score** and **EMA-based Mutation Weighting** from 4D to a template-based C++ core.
- **Benefit**: Any breakthrough in "physics" (like a better cooling schedule) will immediately benefit both 4D and 5D targets.

## Phase 2: Foundational Seeding
Use the high-scoring 4D boards as "seeds" for 5D runs.
- **Theory**: A board that is "perfectly rich" in 4-digit sequences (9999/9999) is a much better starting point for a 5D run than a random board.
- **Implementation**: Create a utility to load the best boards from `optimizer_4d.db` into the 5D solver's starting pool.

## Phase 3: Advanced Physics Implementation
Implement the concepts identified during the 4D experiments:
1.  **Ratchet Annealing**: Instead of simple reheating, use decaying peak temperatures to "lock in" the 4D foundation while exploring 5D extensions.
2.  **Morphing Scoring Functions**:
    *   *Early Phase*: Use Strategy 1 (Ductile) to find a broad 5D basin.
    *   *Late Phase*: Morph into Strategy 0 (Brittle) to force the solver to fix the last remaining holes in the sequence.
3.  **SIMD Bitboards**: Move from 16-bit to 256-bit (AVX2) or 512-bit (AVX-512) bitboards to allow the 5D solver to run at the same speed the 4D solver does today.

## Phase 4: Hybrid AI-Heuristic Solver
Leverage the `scripts/optimization/neural_solver/` work.
- **Goal**: Use the "Physics Logs" from the SA runs to train a lightweight model that can predict which mutation is most likely to "unblock" a specific bottleneck in the current board.
- **Integration**: The SA solver could occasionally "consult" the AI model (perhaps every 1,000 iterations) to perform a highly targeted Macro-move.

## Long-term Vision
Transform this repository from a "BOJ 18789 Solver" into a **General Combinatorial Grid Optimizer** framework that can be applied to any problem involving path-finding and sequence maximization on a 2D topology.
