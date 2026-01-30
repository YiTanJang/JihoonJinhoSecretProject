import sqlite3
import os
import sys

DB_PATH = r"db/optimizer_4d.db"

def deduplicate_best_boards():
    # Fix DB path relative to script location
    abs_db_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", DB_PATH))
    
    if not os.path.exists(abs_db_path):
        print(f"Database not found at: {abs_db_path}")
        return

    print(f"Opening database: {abs_db_path}")
    conn = sqlite3.connect(abs_db_path)
    cursor = conn.cursor()

    # Get all boards
    print("Fetching all boards from 'best_boards'...")
    try:
        cursor.execute("SELECT id, board_data, score, lineage_id FROM best_boards ORDER BY score DESC, id ASC")
        rows = cursor.fetchall()
    except sqlite3.Error as e:
        print(f"Error accessing best_boards table: {e}")
        conn.close()
        return

    total_rows = len(rows)
    print(f"Total rows found: {total_rows}")

    seen_boards = {} # board_data -> (id, score, lineage_id)
    ids_to_delete = []

    for row in rows:
        bid, board_data, score, lineage_id = row
        
        if board_data in seen_boards:
            # Duplicate found!
            ids_to_delete.append(bid)
        else:
            seen_boards[board_data] = (bid, score, lineage_id)

    num_duplicates = len(ids_to_delete)
    
    if num_duplicates == 0:
        print("No duplicate boards found.")
    else:
        print(f"Found {num_duplicates} duplicate entries to delete.")
        
        # Batch delete
        # Chunking to avoid "too many SQL variables" error if thousands of duplicates
        chunk_size = 500
        for i in range(0, num_duplicates, chunk_size):
            chunk = ids_to_delete[i:i + chunk_size]
            placeholders = ','.join('?' for _ in chunk)
            sql = f"DELETE FROM best_boards WHERE id IN ({placeholders})"
            cursor.execute(sql, chunk)
            print(f"Deleted chunk {i//chunk_size + 1}...")

        conn.commit()
        print("Deletion committed.")
        
        print("Vacuuming database to reclaim space...")
        cursor.execute("VACUUM")
        print("Vacuum complete.")

    conn.close()

if __name__ == "__main__":
    deduplicate_best_boards()
