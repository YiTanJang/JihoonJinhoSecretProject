# Algorithm & Physics

The core of this project is a specialized Simulated Annealing (SA) engine designed for the combinatorial optimization problem of the BOJ 18789 grid.

## Simulated Annealing Engine

### 1. Temperature Schedule
- **Initial Temperature**: Defined in `Config4D::INITIAL_TEMP` (typically 7000-10000).
- **Cooling Rate**: Ultraslow exponential cooling (`0.999995`).
- **Cosine Annealing (Warm Restarts)**: The solver performs 3 cycles. Cycle 0 is a "hard" start. Cycles 1 and 2 are "warm" restarts that reheat the system to a fraction of the initial temperature to melt defects while preserving high-level structures.

### 2. Phase Transitions
The solver monitors the **Acceptance Ratio (AR)** of "bad" moves:
- **Liquid State**: High AR (> 0.5). Random walk, exploration.
- **Transition State**: AR drops rapidly. Structural formation ("Crystallization").
- **Frozen State**: Low AR (< 0.0033). Only local polishing occurs.

The solver uses a 200,000 iteration monitoring window to detect "True Thermal Equilibrium" before moving to the next cycle.

## Scoring Strategies

We use different "Materials" (Weighting functions) to guide the SA:

| Strategy | Function | Property | Best Use |
| :--- | :--- | :--- | :--- |
| **Strategy 1** | $i^2$ | "Ductile" | Robust exploration, self-repair. |
| **Strategy 3** | $i^4$ | "Stiff" | Locking in deep basins, finishing. |
| **Strategy 0** | $4^i$ | "Brittle" | Massive gradients, forces strict adherence. |

### Anarchy Weighting Logic
To maximize sequential richness, we apply heavy weights to the **Best Buckets** (e.g., numbers near the current gap). We allow the solver to "steal" from the lower-priority buckets to feed the "head" of the sequence.

## Mutation Performance Tracking
Each mutation operator's effectiveness is tracked via:
- **Acceptance Rate**: How often it's accepted.
- **Energy Delta**: Average score change.
- **Juice**: A composite score $( \Delta E \times AR )$.

Weights are updated every 10,000 iterations using an Exponential Moving Average (EMA) to adapt to the current phase of the annealing process.
