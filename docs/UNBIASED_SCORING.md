# Unbiased 4D Scoring Algorithm

This document details the mechanics of the `calculate_unbiased_score_4d` function, designed to evaluate board states for the BOJ 18789 4D Richness problem. The algorithm aims to eliminate topological biases (palindromes, cycles, shared paths) and enforce a strict lexicographical priority for filling number buckets.

## Core Concepts

### 1. Topological Bias Types
The board is an undirected graph. Paths on this graph generate number sequences.
*   **Type 1: Palindrome ($P == P_{rev}$)**
    *   Example: `1221` ($1 \to 2 \to 2 \to 1$).
    *   Yield: Generates **1** unique number.
    *   Bias: Less efficient than standard paths (which yield 2).
*   **Type 2: Cyclic Non-Palindrome ($Start == End, P \neq P_{rev}$)**
    *   Example: `1231` vs `1321`.
    *   Yield: Generates **2** unique numbers, but both belong to the **same bucket** (Start digit 1).
    *   Bias: Inflates the count of a single bucket compared to standard paths that distribute across two.
*   **Type 3: Standard Non-Palindrome ($Start \neq End$)**
    *   Example: `1234` vs `4321`.
    *   Yield: Generates **2** unique numbers belonging to **different buckets** (Bucket 1 and Bucket 4).
    *   Bias: Creates a dependency between buckets. Filling Bucket 1 might "consume" a path needed for Bucket 4.
*   **Type 0: Trivial Bounce**
    *   Example: `1219` ($1 \to 2 \to 1 \to 9$) or `9121`.
    *   Yield: These are effectively 3-digit paths extended artificially.
    *   Action: **Ignored**. They do not count toward 4-digit richness.

### 2. Dynamic Capacity Reduction
To normalize the score, we treat the "Capacity" of a bucket not as a fixed 10,000, but as a dynamic value based on the available topology.

*   **Initialization**: Every bucket starts with 0 Capacity.
*   **Assignment Logic**:
    *   **Palindrome**: Adds **1** to Capacity. If present, adds **1** to Fill.
    *   **Cyclic Pair**: The pair (`A...A`, `A...A'`) adds **1** to Capacity. If *either* is present, adds **1** to Fill.
    *   **Standard Pair**: (`A...B`, `B...A`).
        *   We prioritize buckets by their **Raw Fill Count** (Rank 0 = Fullest, Rank 9 = Emptiest).
        *   If Bucket A is higher rank than Bucket B:
            *   Bucket A **claims** the path. Adds **1** to A's Capacity. If `A...B` is present, adds **1** to A's Fill.
            *   Bucket B **loses** the path. It adds **0** to Capacity. (Effectively reducing its max potential score).

### 3. Bucket Prioritization (Sliding Window)
We have 11 functional buckets:
*   **Rank 0**: 3-Digit Numbers (Bucket -1).
*   **Ranks 1-10**: The 10 4-Digit Buckets (0xxx - 9xxx), sorted by their *unbiased fullness*.

**Scoring Weight**:
*   Iterate through ranks $r=0 \dots 10$.
*   Identify $R_{fail}$: The first rank where `Fill < Capacity`.
*   **Weighting**:
    *   If $r \le R_{fail}$: Weight = $10^4$ (Max priority).
    *   If $r > R_{fail}$: Weight = $(10 - (r - R_{fail}))^4$ (Decaying priority).
*   **The 11th Bucket**: Rank 10 (the worst 4D bucket) is explicitly **ignored** (Weight 0) to allow the solver to "dump" entropy there.

### 4. 3-Digit Debiasing
The 3-digit bucket (000-999) undergoes similar debiasing:
*   **Palindrome (ABA)**: Cap 1, Fill 1.
*   **Standard (ABC/CBA)**: Pair counts as Cap 1. If either exists, Fill 1.
*   **Result**: Max Capacity $\approx 550$.

## Algorithm Steps
1.  **Static Analysis**: Pre-compute types/partners for all 10,000 numbers (0000-9999) and 1,000 numbers (000-999).
2.  **DFS Traversal**: Mark all present numbers in `RichnessOracle`.
3.  **Raw Ranking**: Count raw 4D numbers per bucket to determine priority order.
4.  **Debiasing Pass**: Iterate through numbers, apply Dynamic Capacity logic based on priority.
5.  **Final Sum**: Calculate weighted sum of `Fill` counts.
