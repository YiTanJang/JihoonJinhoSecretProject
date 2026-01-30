import sqlite3
import re
import numpy as np
import os
from datetime import datetime

DB_PATH = "db/optimizer_4d.db"
OUTPUT_FILE = "physics_analysis_report.txt"

def get_latest_log_prefix(conn):
    cursor = conn.cursor()
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'physics_logs_%'")
    tables = [row[0] for row in cursor.fetchall()]
    
    # Extract prefixes (physics_logs_YYYYMMDD_HHMMSS)
    prefixes = set()
    for t in tables:
        match = re.match(r"(physics_logs_\d{8}_\d{6})_t\d+", t)
        if match:
            prefixes.add(match.group(1))
    
    if not prefixes:
        return None
    
    # Sort by date string
    return sorted(list(prefixes))[-1]

def analyze_physics():
    if not os.path.exists(DB_PATH):
        print(f"Error: Database {DB_PATH} not found.")
        return

    conn = sqlite3.connect(DB_PATH)
    prefix = get_latest_log_prefix(conn)
    
    if not prefix:
        print("No physics log tables found.")
        conn.close()
        return

    print(f"Analyzing most recent logs: {prefix}...")

    # Aggregating data from all threads
    cursor = conn.cursor()
    # Find all thread tables for this prefix
    cursor.execute(f"SELECT name FROM sqlite_master WHERE type='table' AND name LIKE '{prefix}_t%'")
    tables = [row[0] for row in cursor.fetchall()]

    all_temps = []
    all_bad_ars = []
    all_stddevs = []
    all_p12s = []

    for table in tables:
        # Schema: temp, bad_ar, energy_stddev, p12
        # p12 is the 13th prob column (index 12)
        try:
            cursor.execute(f"SELECT temp, bad_ar, energy_stddev, p12 FROM {table}")
            rows = cursor.fetchall()
            if not rows: continue
            
            data = np.array(rows)
            all_temps.extend(data[:, 0])
            all_bad_ars.extend(data[:, 1])
            all_stddevs.extend(data[:, 2])
            all_p12s.extend(data[:, 3])
        except sqlite3.OperationalError as e:
            print(f"Skipping table {table}: {e}")

    conn.close()

    if not all_temps:
        print("No data extracted.")
        return

    # Convert to numpy arrays
    temps = np.array(all_temps)
    bad_ars = np.array(all_bad_ars)
    stddevs = np.array(all_stddevs)
    p12s = np.array(all_p12s)

    # Binning
    # Sort by temperature descending
    sort_idx = np.argsort(temps)[::-1]
    temps = temps[sort_idx]
    bad_ars = bad_ars[sort_idx]
    stddevs = stddevs[sort_idx]
    p12s = p12s[sort_idx]

    # Create bins (logarithmic or linear). Let's use simple linear bins or rolling average.
    # Given the noise, binning is better.
    # Range: Max T to Min T
    min_t = np.min(temps)
    max_t = np.max(temps)
    
    # Define bin edges (log space is usually better for SA temp)
    # But linear bins of size 1.0 or 0.5 are fine for this range (likely 1000 -> 0)
    # Let's create 200 bins
    bins = np.logspace(np.log10(max(0.001, min_t)), np.log10(max_t), num=200)
    
    bin_centers = []
    bin_bad_ar = []
    bin_cv = []
    bin_p12 = []

    # Digitalize
    indices = np.digitize(temps, bins)

    for i in range(1, len(bins)):
        mask = (indices == i)
        if not np.any(mask):
            continue
        
        mean_t = np.mean(temps[mask])
        mean_bad_ar = np.mean(bad_ars[mask])
        mean_stddev = np.mean(stddevs[mask])
        mean_p12 = np.mean(p12s[mask])

        # Heat Capacity Cv = sigma^2 / T^2
        cv = (mean_stddev ** 2) / (mean_t ** 2)

        bin_centers.append(mean_t)
        bin_bad_ar.append(mean_bad_ar)
        bin_cv.append(cv)
        bin_p12.append(mean_p12)

    # Analysis
    bin_centers = np.array(bin_centers)
    bin_cv = np.array(bin_cv)
    bin_p12 = np.array(bin_p12)

    # 1. Peak Heat Capacity
    peak_cv_idx = np.argmax(bin_cv)
    peak_cv_temp = bin_centers[peak_cv_idx]

    # 2. Peak Heatmap Mutate Probability
    peak_p12_idx = np.argmax(bin_p12)
    peak_p12_temp = bin_centers[peak_p12_idx]

    # Output
    with open(OUTPUT_FILE, "w") as f:
        f.write(f"Analysis of Logs: {prefix}\n")
        f.write("========================================\n")
        f.write(f"Peak Heat Capacity Temperature: {peak_cv_temp:.4f}\n")
        f.write(f"Peak Heatmap Mutate Probability Temp: {peak_p12_temp:.4f}\n")
        f.write("\n")
        f.write("Temperature vs Bad Acceptance Rate Table:\n")
        f.write("Temp\tBad_AR\n")
        
        # Sort output by Temp descending
        sorted_indices = np.argsort(bin_centers)[::-1]
        for idx in sorted_indices:
            f.write(f"{bin_centers[idx]:.4f}\t{bin_bad_ar[idx]:.4f}\n")

    print(f"Analysis complete. Results written to {OUTPUT_FILE}")
    print(f"Peak Cv Temp: {peak_cv_temp:.4f}")
    print(f"Peak P12 Temp: {peak_p12_temp:.4f}")

if __name__ == "__main__":
    analyze_physics()
