# Palindrome Bottleneck Analysis
**Date:** 2026-01-12

## Observation
After optimizing the board to >99% completion for 4-digit numbers, the remaining missing numbers ("holes") are disproportionately concentrated in the **Cycle Topology (`abca`)**, specifically **Palindromes (`abba`)**.

**Missing Patterns (as of Jan 12):**
1.  `2112`
2.  `3003`
3.  `5115`
4.  `5335`
5.  `8008`
6.  `8228`
(Plus `5675` and `5765` which are standard triangles).

## The Structural Constraint
A 4-digit number of the form `abba` implies a path:
$$ Cell_1(A) \to Cell_2(B) \to Cell_3(B) \to Cell_4(A) $$

### The "Doublet Edge" Requirement
The critical transition is **Step 2 ($B \to B$)**.
Since the grid does not allow self-loops (a cell cannot be its own neighbor), $Cell_2$ and $Cell_3$ must be **two distinct physical cells** that:
1.  Are adjacent to each other.
2.  Both contain the value $B$.

This forms a **"Doublet Edge"** (a connection between two identical numbers, e.g., `1-1` or `0-0`).

### Why This is Hard
1.  **Entropy vs. Order:** A randomly filled board tends to maximize local entropy, making adjacent identical numbers rare ("anti-clumping").
2.  **Topology Dependency:**
    *   `abcd` (Linear): Needs 4 distinct values. Easy to find in high-entropy regions.
    *   `abab` (PingPong): Needs $A \to B \to A \to B$. Needs alternating neighbors ($A-B$). Extremely common.
    *   `abba` (Palindrome): **Strictly requires** a doublet edge ($B-B$). If the board has no `1-1` edges, `X11Y` is physically impossible.

## Symmetry Note
The solver guarantees symmetry: if path $A \to B \to C \to D$ exists, the reverse path $D \to C \to B \to A$ also exists because the grid graph is undirected.
*   Therefore, checking `test(forward) || test(reverse)` is logically equivalent to `test(forward)`.
*   The scarcity of palindromes is not due to directionality, but due to the physical absence of the required $B-B$ edge subgraph.

## Conclusion
To close the final <0.1% gap, the board *must* contain specific adjacent pairs (Doublets) for the digits involved in the missing palindromes (e.g., `1-1` for `2112`, `0-0` for `3003`). The optimization pressure must eventually force these "clumps" to appear, even if they locally reduce entropy.
