import sqlite3
import os

db_path = "db/optimizer_4d.db"
if os.path.exists(db_path):
    print(f"Vacuuming {db_path}...")
    conn = sqlite3.connect(db_path)
    conn.execute("VACUUM;")
    conn.close()
    print("Vacuum complete.")
else:
    print("DB not found.")
