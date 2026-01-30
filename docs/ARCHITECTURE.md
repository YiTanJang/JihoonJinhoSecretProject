# System Architecture

The SAMultithread optimizer is built for extreme performance, leveraging C++20 features, bitboard optimizations, and an asynchronous database architecture.

## Core Data Structures

### 1. FastBoard (Bitboard)
Defined in `include/logic.h`. Instead of a raw 2D array for scoring, we use a bitboard representation:
- Each digit (0-9) has its own layer.
- Each layer is an array of 8 `uint16_t` values (representing 14 columns).
- **Benefit**: Number search is implemented using bitwise operations, significantly faster than recursive DFS on a 2D grid.

### 2. SAIsland4D
Defined in `include/solver_4d.h`. Represents an independent optimization thread ("Island").
- Each island maintains its own board state, RNG, and temperature schedule.
- Uses **Adaptive Mutation Weights**: An EMA-based system that favors mutation operators that have historically produced better results in the current thermal state.

### 3. Shared Memory Monitoring
Defined in `include/shared_mem.h`. 
- High-frequency telemetry (scores, AR, temperature) is written to a named shared memory segment (`Local\SAMonitor4D`).
- This allows external Python scripts (monitoring tools) to read real-time data without impacting the solver's performance or locking the database.

## Components

### 1. Scoring Logic (`src/logic.cpp`)
Contains multiple scoring modes:
- **Frequency Score**: Number of unique targets found.
- **Sum Score**: Inverse-weighted sum of found numbers (emphasizing smaller missing numbers).
- **Richness Score**: The primary metric (sequential numbers from 0 up to X).
- **Anarchy Lexicographical Score**: A custom billion-point scale that prioritizes filling buckets (0-999, 1000-1999, etc.) with large bonuses for full buckets.

### 2. Mutation Engine (`src/mutations.cpp`)
Classified into three tiers:
- **Micro**: Small swaps (Adjacent, Diagonal, Knight's move).
- **Meso**: Structural changes (Rotations, Patch swaps).
- **Macro**: Global changes (Block swaps, random mutations).
- **Greedy LNS**: A background 5% process that performs local optimization on small windows.

### 3. Database Manager (`src/db_manager_4d.cpp`)
Uses SQLite3 for persistence.
- **WAL Mode**: Enabled for high-concurrency writes.
- **Timestamped Tables**: Every run creates its own tables to prevent data contamination between experiments.
- **Physics Logs**: Detailed per-action statistics are logged to the database for post-run analysis.

## Hardware Utilization
- The system is configured by default for 12 threads (standard for 6-core/12-thread CPUs).
- Memory usage is extremely low (mostly stack-allocated tables and a small heap for the DB buffer).
