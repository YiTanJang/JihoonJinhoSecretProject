# Toolbox & Scripts

The repository includes a comprehensive suite of Python scripts for monitoring, analyzing, and maintaining the optimization environment.

## Monitoring Tools (`scripts/monitoring/`)

- **`monitor_4d.py`**: The primary CLI monitor. Displays real-time stats from shared memory (Thread ID, Temp, AR, Score, Best Score).
- **`graph_monitor.py`**: Visualizes the annealing process using Matplotlib. Shows temperature decay and score improvement curves.
- **`visualizer.py`**: A GUI tool to view the current best board and its "heat map" (which numbers are covered).
- **`show_best_board.py`**: Quickly prints the top-scoring boards currently in the database.

## Maintenance Scripts (`scripts/maintenance/`)

- **`full_db_refresh.py`**: Re-evaluates all boards in the database using the latest scoring logic.
- **`prune_db.py`**: Removes low-scoring boards to keep the database size manageable.
- **`validate_db.py`**: Checks for data corruption or duplicate entries.
- **`analyze_transitions.py`**: Analyzes the "Physics Logs" to find the optimal temperature ranges for different scoring strategies.

## Optimization & Evaluation (`scripts/`)

- **`evaluate_board.py`**: A standalone script to score a board provided as a string or file.
- **`ai_optimizer.py`**: (Experimental) Uses a neural network or MCTS approach to suggest mutations (located in `scripts/optimization/`).

## Shared Memory Library (`scripts/shared_mem_lib.py`)
A utility module used by the monitoring scripts to interface with the C++ shared memory segment using `ctypes`.
