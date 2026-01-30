@echo off
echo [LAUNCHER] Starting Monitoring Tools...

:: Start the CLI Monitor in a new window
start "SA Monitor CLI" cmd /c "python scripts/monitoring/monitor.py"

:: Start the Graph Monitor in a new window
start "SA Monitor Graph" cmd /c "python scripts/monitoring/graph_monitor.py"

echo [SUCCESS] Both monitors are now running.
timeout /t 3
