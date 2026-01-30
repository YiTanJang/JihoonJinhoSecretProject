# Technical Debt & Cleanup Roadmap

This document outlines areas of the repository that require "tidying," refactoring, or better organization. These items are categorized by their impact on maintainability and system health.

## 1. Repository & File Organization

### Duplicate Logic (The "4D" Split)
- **Problem**: There is significant code duplication between `solver.cpp`/`solver_4d.cpp`, `db_manager.cpp`/`db_manager_4d.cpp`, and `SAmultithread.cpp`/`SAmultithread_4d.cpp`.
- **Cleanup**: Consolidate these into unified classes/modules. Use template parameters or configuration objects to handle the differences between the 3-digit and 4-digit optimization modes.

### Artifact Pollution
- **Problem**: The root directory contains temporary notes (`메모.txt`), logs (`changes_log.txt`), and screenshots.
- **Cleanup**: 
  - Move `메모.txt` and `changes_log.txt` to a `notes/` or `archive/` folder.
  - Ensure all build artifacts are strictly confined to `bin/` and `obj/`.
  - Remove or move the `legacy/` folder to a separate long-term storage if it's no longer used for active development.

### Tools vs. Source
- **Problem**: `tools/` contains C++ source files that are essentially standalone programs. Some of these overlap with logic in `src/`.
- **Cleanup**: Move stable tools to `src/tools/` and experimental ones to `experimental/`.

## 2. Code Architecture

### Mutation "God File"
- **Problem**: `src/mutations.cpp` is nearly 1,000 lines long and contains 18+ different logic types mixed together.
- **Cleanup**: Split into `src/mutations/micro.cpp`, `src/mutations/meso.cpp`, and `src/mutations/macro.cpp`. Use a registry pattern to load them into the solver.

### Global State Dependency
- **Problem**: `globals.cpp` and `include/globals.h` store many mutable global variables (e.g., `g_monitor_ptr`, `g_experiment_table_name`). This makes unit testing and running multiple different experiments in one process difficult.
- **Cleanup**: Encapsulate these into a `Context` or `Environment` object passed to the solvers.

### Shared Memory Implementation
- **Problem**: Win32 API calls for shared memory (`CreateFileMappingA`, `MapViewOfFile`) are baked directly into `SAmultithread_4d.cpp`.
- **Cleanup**: Move this logic into a proper `SharedMemoryManager` class in `shared_mem.h/cpp` with an RAII-based lifecycle to ensure memory is unmapped correctly on exit.

### Error Handling
- **Problem**: Many critical operations (DB writes, shared memory access, file I/O) use "silent" error handling (e.g., `if (ptr == NULL) return;`).
- **Cleanup**: Implement a unified logging system and use exceptions or `std::expected` (C++23) for critical failure paths.

## 3. Build & Maintenance

### Hardcoded Build Paths
- **Problem**: `scripts/batch/build_4d.bat` searches for Visual Studio in default paths. If a user has a custom installation, the build fails.
- **Cleanup**: Use `vswhere.exe` to dynamically locate the MSVC compiler or transition to a `CMakeLists.txt` based build system for better portability and IDE integration.

### Database Maintenance
- **Problem**: The `scripts/maintenance/` folder is very crowded with scripts that perform similar tasks (e.g., `recalc_db_scores.py` vs `recalc_sum_scores.py`).
- **Cleanup**: Consolidate these into a single `db_tool.py` with subcommands (e.g., `python db_tool.py recalc --mode sum`).

## 4. Documentation & Naming
- **Problem**: Mixed naming conventions (Korean for some files, English for others).
- **Cleanup**: Standardize all filenames and comments to English for broader accessibility, or consistently use one convention.
