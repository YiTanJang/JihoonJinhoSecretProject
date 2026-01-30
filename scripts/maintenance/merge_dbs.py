import sqlite3
import argparse
import os
import shutil
from datetime import datetime

def merge_databases(main_db_path, remote_db_path):
    if not os.path.exists(remote_db_path):
        print(f"Error: Remote DB file not found: {remote_db_path}")
        return

    print(f"--- Merging '{remote_db_path}' into '{main_db_path}' ---")
    
    # Connect to Main DB
    conn_main = sqlite3.connect(main_db_path)
    cursor_main = conn_main.cursor()
    
    # Attach Remote DB
    # We use a raw SQL attach command. Note: paths must be escaped properly or simple strings.
    # It's safest to use absolute paths.
    abs_remote = os.path.abspath(remote_db_path)
    
    try:
        cursor_main.execute(f"ATTACH DATABASE ? AS remote_db", (abs_remote,))
    except sqlite3.OperationalError as e:
        print(f"Error attaching database: {e}")
        return

    # 1. Merge Best Boards
    print("Merging 'best_boards'...")
    
    # We want to insert boards from remote that do NOT exist in main (based on board_data)
    # OR simpler: Just insert them and let a separate dedup process handle it?
    # Better: Ignore exact duplicates (same board_data).
    
    query = """
    INSERT INTO main.best_boards (lineage_id, initial_temp, score, board_data, updated_at, solver_version)
    SELECT 
        r.lineage_id, 
        r.initial_temp, 
        r.score, 
        r.board_data, 
        r.updated_at,
        COALESCE(r.solver_version, '1.0')
    FROM remote_db.best_boards r
    WHERE NOT EXISTS (
        SELECT 1 FROM main.best_boards m WHERE m.board_data = r.board_data
    );
    """
    
    try:
        cursor_main.execute(query)
        rows_added = cursor_main.rowcount
        print(f"Successfully imported {rows_added} new unique boards from remote DB.")
    except sqlite3.OperationalError as e:
        print(f"Error executing merge: {e}")
        print("Tip: Ensure both databases have the latest schema (run the solver once on both machines to update schema).")

    # Detach
    conn_main.commit()
    cursor_main.execute("DETACH DATABASE remote_db")
    conn_main.close()
    print("Merge complete.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Merge 'best_boards' from a remote/backup DB into the main DB.")
    parser.add_argument("--main", default="db/optimizer_4d.db", help="Path to your main database")
    parser.add_argument("remote", help="Path to the database file from the second computer")
    
    args = parser.parse_args()
    
    merge_databases(args.main, args.remote)
