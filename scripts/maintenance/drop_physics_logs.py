import sqlite3
import sys
import os

def drop_all_physics_logs(db_path="db/optimizer_4d.db"):
    # Fix DB path relative to script location
    abs_db_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", db_path))
    
    if not os.path.exists(abs_db_path):
        print(f"DB not found: {abs_db_path}")
        return

    conn = sqlite3.connect(abs_db_path)
    cursor = conn.cursor()

    # Find all physics log tables
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'physics_logs_%';")
    tables = [row[0] for row in cursor.fetchall()]

    if not tables:
        print("No physics_logs tables found.")
        conn.close()
        return

    print(f"Found {len(tables)} physics_logs tables.")
    confirm = input("Are you sure you want to drop ALL of them? (y/n): ")
    if confirm.lower() != 'y':
        print("Aborted.")
        conn.close()
        return

    print("Dropping tables...")
    for table in tables:
        cursor.execute(f"DROP TABLE IF EXISTS {table}")
        # print(f"Dropped {table}") # Optional: verify each drop

    conn.commit()
    
    # Optional: VACUUM to reclaim space (can be slow for large DBs)
    # print("Vacuuming database...")
    # cursor.execute("VACUUM;")
    
    conn.close()
    print("All physics_logs tables dropped successfully.")

if __name__ == "__main__":
    drop_all_physics_logs()
