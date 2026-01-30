import sqlite3
import os
import sys

def cleanup_physics_logs(db_path="db/optimizer_4d.db", keep_last_n=2):
    # Robust path handling
    if not os.path.exists(db_path):
        # Check if we are in scripts/maintenance
        current_dir = os.path.dirname(os.path.abspath(__file__))
        # Try going up 2 levels
        candidate = os.path.join(current_dir, "..", "..", "db", "optimizer_4d.db")
        if os.path.exists(candidate):
            db_path = candidate
        else:
            # Fallback for simple run from root
            if not os.path.exists(db_path):
                 print(f"Error: Database not found at {db_path}")
                 return

    print(f"Opening database: {db_path}")
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    # 1. List Tables
    try:
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'physics_logs_%';")
        tables = [row[0] for row in cursor.fetchall()]
    except sqlite3.Error as e:
        print(f"Database error: {e}")
        conn.close()
        return

    if not tables:
        print("No physics logs found.")
        conn.close()
        return

    # 2. Group by Experiment (Prefix: physics_logs_YYYYMMDD_HHMMSS)
    experiments = {}
    for t in tables:
        parts = t.split('_')
        # Expect parts: ['physics', 'logs', 'YYYYMMDD', 'HHMMSS', 'tX']
        if len(parts) >= 4:
            prefix = "_".join(parts[:4]) # physics_logs_20260128_001853
            if prefix not in experiments:
                experiments[prefix] = []
            experiments[prefix].append(t)
    
    sorted_prefixes = sorted(experiments.keys(), reverse=True) # Newest first
    
    if len(sorted_prefixes) <= keep_last_n:
        print(f"Only found {len(sorted_prefixes)} experiments. Keeping all (Threshold: Keep Last {keep_last_n}).")
        conn.close()
        return

    to_keep = sorted_prefixes[:keep_last_n]
    to_delete = sorted_prefixes[keep_last_n:]

    print(f"Found {len(sorted_prefixes)} experiments.")
    print(f"Keeping latest {keep_last_n}:")
    for p in to_keep:
        print(f"  - {p} ({len(experiments[p])} tables)")
    
    print(f"\nMarked for DELETION ({len(to_delete)} experiments):")
    total_tables_to_drop = 0
    for p in to_delete:
        count = len(experiments[p])
        print(f"  - {p} ({count} tables)")
        total_tables_to_drop += count

    # Confirmation
    confirm = "y"
    if "--force" not in sys.argv:
         try:
            confirm = input(f"\nProceed to drop {total_tables_to_drop} tables and VACUUM? (y/n): ")
         except EOFError:
            confirm = "n"

    if confirm.lower() == 'y':
        print("\nDropping tables...")
        for p in to_delete:
            for table in experiments[p]:
                cursor.execute(f"DROP TABLE IF EXISTS {table}")
        
        print("Committing changes...")
        conn.commit()
        
        print("Vacuuming database (this may take a moment)...")
        # Ensure auto_vacuum is handled or just force VACUUM
        cursor.execute("VACUUM;")
        
        print("Done. Space reclaimed.")
    else:
        print("Operation cancelled.")

    conn.close()

if __name__ == "__main__":
    # Allow command line arg for Keep N
    keep = 2
    if len(sys.argv) > 1 and sys.argv[1].isdigit():
        keep = int(sys.argv[1])
    
    cleanup_physics_logs(keep_last_n=keep)
