# SAMultithread - BOJ 18789 Optimizer

This repository contains a high-performance multithreaded Simulated Annealing (SA) solver designed to optimize an 8x14 grid of digits to maximize its "richness" (the number of consecutive integers starting from 0 that can be formed by tracing paths on the grid).

## Quick Start

1.  **Build**: Run `scripts\batch\build_4d.bat` (requires MSVC 2022).
2.  **Run**: Execute `bin\SAmultithread_4d.exe`.
3.  **Monitor**: Run `python scripts\monitoring\monitor_4d.py` to view real-time progress.

## Documentation Index

Comprehensive documentation is available in the `docs/` directory:

*   [**Architecture Overview**](docs/ARCHITECTURE.md): System design, data structures, and the "FastBoard" bitboard implementation.
*   [**Algorithm & Physics**](docs/ALGORITHM.md): Details on the Simulated Annealing engine, temperature schedules, and scoring strategies.
*   [**Build System**](docs/BUILD.md): Compilation instructions and dependencies.
*   [**Toolbox & Scripts**](docs/TOOLS.md): Documentation for the Python-based monitoring and maintenance suite.
*   [**Development Guide**](docs/DEVELOPMENT.md): Coding conventions, critical sections (what not to touch), and future roadmap.

## Project Structure

*   `src/`: Core C++ source files.
*   `include/`: Header files and compile-time table generation.
*   `scripts/`: Python scripts for monitoring, evaluation, and DB management.
*   `lib/`: External libraries (bundled SQLite3).
*   `tests/`: Unit tests and performance benchmarks.
*   `tools/`: Experimental utilities and brute-force prototypes.

---
*Maintained by Gemini CLI Agent*
