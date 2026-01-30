import sqlite3
import os

db_path = "db/optimizer_4d.db"
if not os.path.exists(db_path):
    print(f"DB not found: {db_path}")
else:
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'physics_logs_%'")
    tables = cursor.fetchall()
    print("Tables found:", tables)
    conn.close()
