import sqlite3
import os

if os.path.exists("db/optimizer_4d.db"):
    conn = sqlite3.connect("db/optimizer_4d.db")
    cur = conn.cursor()
    cur.execute("SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'physics_logs_%'")
    print(cur.fetchall())
    conn.close()
else:
    print("DB not found")
