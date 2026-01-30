import sqlite3
import pandas as pd
import numpy as np
import os

def extract_data(db_path="db/optimizer_4d.db"):
    if not os.path.exists(db_path):
        print(f"DB not found: {db_path}")
        return

    conn = sqlite3.connect(db_path)
    
    # Find latest log tables
    cursor = conn.cursor()
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'physics_logs_%_t%'")
    tables = [row[0] for row in cursor.fetchall()]
    
    if not tables:
        print("No log tables found.")
        conn.close()
        return

    # Extract prefixes to identify experiments
    prefixes = sorted(list(set(["_".join(t.split("_")[:-1]) for t in tables])), reverse=True)
    latest_prefix = prefixes[0]
    print(f"Analyzing experiment: {latest_prefix}")
    
    target_tables = [t for t in tables if t.startswith(latest_prefix)]
    all_dfs = []
    
    for table_name in target_tables:
        try:
            df = pd.read_sql_query(f"SELECT * FROM {table_name}", conn)
            all_dfs.append(df)
        except Exception as e:
            print(f"Error loading table {table_name}: {e}")
            continue
            
    conn.close()
    
    if not all_dfs:
        print("No data loaded.")
        return

    full_df = pd.concat(all_dfs).sort_values(["lineage_id", "iteration"])
    
    max_cycle = full_df["cycle_num"].max()
    print(f"Found {int(max_cycle) + 1} cycles.")

    for cyc in range(int(max_cycle) + 1):
        cyc_df = full_df[full_df["cycle_num"] == cyc].copy()
        if cyc_df.empty: continue
        
        print(f"\n--- Cycle {cyc} Aggregate Data (Temp vs Bad AR) ---")
        
        min_t = cyc_df["temp"].min()
        max_t = cyc_df["temp"].max()
        if min_t <= 0: min_t = 0.1
        
        # Binning logic matching analyze_physics.py
        bins = np.logspace(np.log10(min_t), np.log10(max_t), 100)
        cyc_df["temp_bin"] = pd.cut(cyc_df["temp"], bins)
        
        agg_cols = ["temp", "bad_ar"]
        avg_stats = cyc_df.groupby("temp_bin", observed=True)[agg_cols].mean()
        
        # Drop NaN rows (bins with no data)
        avg_stats = avg_stats.dropna()
        
        # Sort by Temperature descending (Hot -> Cold) which is typical for SA
        avg_stats = avg_stats.sort_values("temp", ascending=False)
        
        # Print Table
        print(f"{ 'Temperature':<15} | { 'Bad AR':<15}")
        print("-" * 33)
        for index, row in avg_stats.iterrows():
            print(f"{row['temp']:<15.4f} | {row['bad_ar']:<15.4f}")

if __name__ == "__main__":
    extract_data()
