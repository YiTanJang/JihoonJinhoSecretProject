import sqlite3
import os

def prune_database(db_path="db/optimizer_4d.db"):
    # Fix DB path relative to script location
    abs_db_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", db_path))
    
    if not os.path.exists(abs_db_path):
        print(f"Database not found at: {abs_db_path}")
        return

    conn = sqlite3.connect(abs_db_path)
    cursor = conn.cursor()

    # Find tables to delete
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table';")
    all_tables = [row[0] for row in cursor.fetchall()]

    to_delete = []
    for t in all_tables:
        if t.startswith("anarchy_lex_boards_"):
            to_delete.append(t)
        elif t.startswith("anarchy_lex_logs_"):
            to_delete.append(t)
        elif t == "anarchy_boards":
            to_delete.append(t)
        elif t == "boards":
            to_delete.append(t)

    if not to_delete:
        print("No tables found to prune.")
        conn.close()
        return

    print(f"Found {len(to_delete)} tables to delete:")
    for t in to_delete:
        print(f" - {t}")

    # confirm = input("\nType 'yes' to confirm deletion: ")
    confirm = 'yes'
    if confirm.lower() == 'yes':
        for t in to_delete:
            print(f"Dropping {t}...")
            cursor.execute(f"DROP TABLE IF EXISTS {t}")
        
        # Optimize storage
        print("Vacuuming database...")
        cursor.execute("VACUUM;")
        
        conn.commit()
        print("Pruning complete.")
    else:
        print("Operation cancelled.")

    conn.close()

if __name__ == "__main__":
    prune_database()