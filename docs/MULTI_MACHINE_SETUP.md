# Multi-Machine Setup Guide

This guide explains how to run `SAmultithread` on multiple computers to increase your search power and merge the results.

## 1. Preparing the Deployment Package (Computer A)

On your main computer (where you have the code):

1.  Run the packager script:
    ```powershell
    python scripts/create_deployment_pkg.py
    ```
2.  This will create a file named `SAmultithread_Deploy_YYYYMMDD_...zip`.
3.  Transfer this zip file to **Computer B** (via USB, network share, cloud, etc.).

## 2. Setting up Computer B

On the second computer:

1.  **Unzip** the package.
2.  **Prerequisites:**
    *   Ensure you have a C++ Compiler (Visual Studio 2022 / MSVC on Windows).
    *   Ensure you have Python 3.x installed.
3.  **Build:**
    *   Open a terminal (PowerShell/CMD).
    *   Run: `.\build_refactored.bat`
4.  **Run:**
    *   Run: `.\bin\SAmultithread_4d.exe`
    *   The solver will automatically create a new, empty `db/optimizer_4d.db` and start finding solutions.

## 3. Merging Results (Back to Computer A)

When you want to harvest the results from Computer B:

1.  **Stop** the solver on Computer B (Ctrl+C).
2.  **Copy** the database file `db/optimizer_4d.db` from Computer B to Computer A.
    *   *Tip:* Rename it to something like `optimizer_remote.db` so you don't overwrite your main database!
3.  **Merge** on Computer A:
    ```powershell
    python scripts/maintenance/merge_dbs.py optimizer_remote.db
    ```
4.  This script will:
    *   Read all `best_boards` from the remote file.
    *   Insert only the **new, unique** boards into your main `db/optimizer_4d.db`.
    *   Ignore duplicates.

## 4. Troubleshooting

*   **"Database Locked":** Ensure the solver is not actively writing to the DB while you copy it. Copying a live SQLite file can sometimes result in corruption. Always stop the solver or use the `.backup` command in sqlite3 CLI if you need to copy while running.
*   **Version Mismatch:** Ensure both computers are running the same version of the solver logic (check `solver_version` column).
