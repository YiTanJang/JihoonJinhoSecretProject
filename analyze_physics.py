import sqlite3
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os
import sys
from datetime import datetime

# ACTION_NAMES must match solver_4d.cpp (Updated for 15 operators)
ACTION_NAMES = [
    "Dist 1 Swap", "Dist 2 Swap", "Global Swap", "Rand Cell",
    "Domino Local", "Domino Global", "Tri Rotate", "Straight Slide",
    "Worm Slide", "Block Rotate", "Heatmap Swap", "Heatmap Domino",
    "Heatmap Mutate", "Var Blk Swap", "Var Blk Flip", "N/A",
    "N/A", "N/A", "N/A", "N/A", "N/A", "N/A",
    "N/A", "N/A"
]

# Distinct colors for 15 actions
ACTION_COLORS = [
    '#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd', 
    '#8c564b', '#e377c2', '#7f7f7f', '#bcbd22', '#17becf',
    '#000000', '#4B0082', '#000080', '#00FF00', '#FFD700'
]

def analyze_latest_logs(db_path="db/optimizer_4d.db"):
    print(f"DEBUG: Checking DB at {db_path}")
    if not os.path.exists(db_path):
        print(f"DB not found: {db_path}")
        return

    conn = sqlite3.connect(db_path)
    print("DEBUG: Connected to DB")
    
    # Find latest log tables
    cursor = conn.cursor()
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'physics_logs_%_t%';")
    tables = [row[0] for row in cursor.fetchall()]
    print(f"DEBUG: Found {len(tables)} tables matching pattern")
    
    if not tables:
        print("No log tables found.")
        conn.close()
        return

    # Extract prefixes to identify experiments
    prefixes = sorted(list(set(["_".join(t.split("_")[:-1]) for t in tables])), reverse=True)
    
    # Try to fetch solver version
    solver_version = "Unknown"
    try:
        cursor.execute("SELECT solver_version FROM best_boards ORDER BY id DESC LIMIT 1;")
        row = cursor.fetchone()
        if row and row[0]:
            solver_version = row[0]
    except Exception as e:
        print(f"DEBUG: Could not fetch solver version: {e}")

    print(f"--- Physics Log Stack ---")
    print(f"Solver Version: {solver_version}")
    print(f"Total experiments found: {len(prefixes)}")
    if prefixes:
        print(f"Latest (Target): {prefixes[0]}")
    if len(prefixes) > 1:
        print(f"Next in queue: {prefixes[1]}")
    print(f"-------------------------")

    if not prefixes:
        conn.close()
        return

    latest_prefix = prefixes[0]
    
    # Load all data for this experiment
    all_dfs = []
    target_tables = [t for t in tables if t.startswith(latest_prefix)]
    
    print(f"Loading data from {len(target_tables)} tables for {latest_prefix}...")
    
    for table_name in target_tables:
        try:
            df = pd.read_sql_query(f"SELECT * FROM {table_name}", conn)
            all_dfs.append(df)
        except Exception as e:
            print(f"Error loading table {table_name}: {e}")
            continue
    
    if not all_dfs:
        print("No data loaded. Tables might be empty.")
        conn.close()
        return

    full_df = pd.concat(all_dfs).sort_values(["lineage_id", "iteration"])
    
    # --- Precompute Derived Metrics ---
    full_df["temp_safe"] = full_df["temp"].replace(0, 1e-9)
    full_df["metric_std_temp"] = (full_df["energy_stddev"] / full_df["temp_safe"]) ** 2
    
    # Rolling Score StdDev (per lineage and cycle)
    full_df["score_stddev"] = full_df.groupby(["lineage_id", "cycle_num"])["score"].transform(lambda x: x.rolling(window=20, min_periods=5).std())

    conn.close()

    output_dir = f"scripts/monitoring/logs/analysis_v{solver_version}_{latest_prefix.replace('physics_logs_', '')}_{datetime.now().strftime('%H%M%S')}"
    os.makedirs(output_dir, exist_ok=True)
    
    # Write version info to a file
    with open(os.path.join(output_dir, "version_info.txt"), "w") as f:
        f.write(f"Solver Version: {solver_version}\n")
        f.write(f"Analysis Date: {datetime.now()}\n")
        f.write(f"Log Table Prefix: {latest_prefix}\n")

    analysis_success = False

    try:
        # --- 1. Per Lineage Analysis ---
        for lid, l_df in full_df.groupby("lineage_id"):
            print(f"Generating graphs for Lineage {lid}...")
            lid_dir = os.path.join(output_dir, f"lineage_{lid}")
            os.makedirs(lid_dir, exist_ok=True)

            # A. Per Cycle Graphs
            for cyc, c_df in l_df.groupby("cycle_num"):
                c_df = c_df.sort_values("temp", ascending=False)
                
                # Graph 1: AR vs Temp (Scaled 0-1)
                fig1, ax1 = plt.subplots(figsize=(10, 6))
                ax1.set_xlabel("Temperature (Log Scale)")
                ax1.set_xscale("log")
                ax1.invert_xaxis()
                ax1.set_ylabel("Acceptance Rate")
                ax1.set_ylim(-0.05, 1.05)
                ax1.plot(c_df["temp"], c_df["overall_ar"], color="blue", label="Overall AR")
                ax1.plot(c_df["temp"], c_df["bad_ar"], color="cyan", label="Bad AR", linestyle="--")
                plt.title(f"Lineage {lid} - Cycle {cyc} Acceptance Rates")
                ax1.legend()
                ax1.grid(True, alpha=0.3)
                plt.savefig(os.path.join(lid_dir, f"cycle_{cyc}_ar.png"))
                plt.close()

                # Graph 2: Heat Capacity ((StdDev/T)^2)
                fig2, ax2 = plt.subplots(figsize=(10, 6))
                ax2.set_xlabel("Temperature (Log Scale)")
                ax2.set_xscale("log")
                ax2.invert_xaxis()
                ax2.set_ylabel("Heat Capacity (Fluctuation Metric)")
                ax2.plot(c_df["temp"], c_df["metric_std_temp"], color="purple")
                plt.title(f"Lineage {lid} - Cycle {cyc} Heat Capacity")
                ax2.grid(True, alpha=0.3)
                plt.savefig(os.path.join(lid_dir, f"cycle_{cyc}_heat_capacity.png"))
                plt.close()

                # Graph 3: Score StdDev vs Temp
                fig3, ax3 = plt.subplots(figsize=(10, 6))
                ax3.set_xlabel("Temperature (Log Scale)")
                ax3.set_xscale("log")
                ax3.invert_xaxis()
                ax3.set_ylabel("Score StdDev (Rolling 20)")
                ax3.plot(c_df["temp"], c_df["score_stddev"], color="magenta")
                plt.title(f"Lineage {lid} - Cycle {cyc} Score Volatility vs Temp")
                ax3.grid(True, alpha=0.3)
                plt.savefig(os.path.join(lid_dir, f"cycle_{cyc}_score_volatility.png"))
                plt.close()

                # Graph 4: Action Weights (Smoothed)
                fig4, ax4 = plt.subplots(figsize=(12, 6))
                ax4.set_xlabel("Temperature (Log Scale)")
                ax4.set_xscale("log")
                ax4.invert_xaxis()
                ax4.set_ylabel("Weight")
                for i in range(15): # Updated for 15 ops
                    smoothed = c_df[f"p{i}"].rolling(window=50, min_periods=1).mean()
                    ax4.plot(c_df["temp"], smoothed, label=ACTION_NAMES[i], color=ACTION_COLORS[i])
                plt.title(f"Lineage {lid} - Cycle {cyc} Action Weights (Smoothed)")
                ax4.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize='small')
                plt.tight_layout()
                plt.savefig(os.path.join(lid_dir, f"cycle_{cyc}_actions.png"))
                plt.close()

            # B. Master Timeline
            fig_m, axes_m = plt.subplots(5, 1, figsize=(15, 25), sharex=True)
            fig_m.suptitle(f"Lineage {lid} - Master Timeline")

            # Subplot 1: Iteration vs Temp & Score
            ax_score = axes_m[0].twinx()
            axes_m[0].plot(l_df["iteration"], l_df["temp"], color='blue', alpha=0.5, label="Temp")
            axes_m[0].set_yscale('log')
            axes_m[0].set_ylabel("Temp (Log)")
            ax_score.plot(l_df["iteration"], l_df["score"], color='green', label="Score")
            ax_score.set_ylabel("Score")
            axes_m[0].set_title("Iteration vs Temp & Score")
            
            # Subplot 2: Iteration vs Action Weights
            for i in range(15):
                smoothed = l_df[f"p{i}"].rolling(window=500, min_periods=1).mean()
                axes_m[1].plot(l_df["iteration"], smoothed, label=ACTION_NAMES[i], color=ACTION_COLORS[i])
            axes_m[1].set_title("Iteration vs Action Weights (Smoothed, Window=500)")
            axes_m[1].legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize='small')

            # Subplot 3: Iteration vs Physics Stability (Scaled 0-1)
            ax_phys = axes_m[2].twinx()
            axes_m[2].plot(l_df["iteration"], l_df["overall_ar"], label="Overall AR", color='C0', alpha=0.7)
            axes_m[2].plot(l_df["iteration"], l_df["bad_ar"], label="Bad AR", color='C1', alpha=0.7)
            axes_m[2].set_ylabel("Acceptance Rate")
            axes_m[2].set_ylim(-0.05, 1.05)
            ax_phys.plot(l_df["iteration"], l_df["energy_stddev"], label="Energy StdDev", color='C2', alpha=0.5, linestyle='--')
            ax_phys.set_ylabel("Energy StdDev")
            axes_m[2].set_title("Iteration vs Physics Stability")
            axes_m[2].legend(loc='upper left')
            ax_phys.legend(loc='upper right')

            # Subplot 4: Iteration vs Score StdDev
            axes_m[3].plot(l_df["iteration"], l_df["score_stddev"], color='magenta')
            axes_m[3].set_title("Iteration vs Score StdDev (Rolling Window=20)")
            axes_m[3].set_ylabel("Score StdDev")
            axes_m[3].grid(True, alpha=0.3)

            # Subplot 5: Bad Move Deltas
            if "avg_bad_prop_delta" in l_df.columns and "avg_bad_acc_delta" in l_df.columns:
                axes_m[4].plot(l_df["iteration"], l_df["avg_bad_prop_delta"], label="Avg Prop Bad Delta", color='orange', alpha=0.7)
                axes_m[4].plot(l_df["iteration"], l_df["avg_bad_acc_delta"], label="Avg Acc Bad Delta", color='red', alpha=0.7, linestyle='--')
                axes_m[4].set_title("Iteration vs Bad Move Deltas")
                axes_m[4].set_ylabel("Delta Energy")
                axes_m[4].legend(loc='upper right')
                axes_m[4].grid(True, alpha=0.3)

            plt.tight_layout(rect=[0, 0.03, 0.85, 0.95])
            plt.savefig(os.path.join(lid_dir, "master_timeline.png"))
            plt.close()

        # --- 2. Cycle Aggregate Analysis (All Lineages) ---
        cycle_agg_dir = os.path.join(output_dir, "cycle_aggregate")
        os.makedirs(cycle_agg_dir, exist_ok=True)
        
        print("Generating Cycle Aggregate Analysis...")
        max_cycle = full_df["cycle_num"].max()
        
        for cyc in range(int(max_cycle) + 1):
            cyc_df = full_df[full_df["cycle_num"] == cyc].copy()
            if cyc_df.empty: continue
            
            cyc_folder = os.path.join(cycle_agg_dir, f"cycle_{cyc}")
            os.makedirs(cyc_folder, exist_ok=True)
            
            # Prepare Binned Averages
            min_t = cyc_df["temp"].min()
            max_t = cyc_df["temp"].max()
            if min_t <= 0: min_t = 0.1
            
            bins = np.logspace(np.log10(min_t), np.log10(max_t), 100)
            cyc_df["temp_bin"] = pd.cut(cyc_df["temp"], bins)
            
            # Determine columns to aggregate
            agg_cols = ["overall_ar", "bad_ar", "score_stddev", "temp", "metric_std_temp"] + [f"p{i}" for i in range(15)]
            if "avg_bad_prop_delta" in cyc_df.columns: agg_cols.append("avg_bad_prop_delta")
            if "avg_bad_acc_delta" in cyc_df.columns: agg_cols.append("avg_bad_acc_delta")
            
            avg_stats = cyc_df.groupby("temp_bin", observed=True)[agg_cols].mean()

            # Helper
            def generate_pair(filename_base, plot_func, title_base):
                # 1. Log Scale
                fig_l, ax_l = plt.subplots(figsize=(12, 8))
                plot_func(ax_l)
                ax_l.set_xscale("log")
                ax_l.invert_xaxis()
                ax_l.set_xlabel("Temperature (Log Scale)")
                ax_l.set_title(f"{title_base} (Log Scale)")
                plt.tight_layout()
                plt.savefig(os.path.join(cyc_folder, f"{filename_base}_log.png"))
                plt.close()

                # 2. Linear Scale
                fig_n, ax_n = plt.subplots(figsize=(12, 8))
                plot_func(ax_n)
                ax_n.set_xscale("linear")
                ax_n.invert_xaxis()
                ax_n.set_xlabel("Temperature (Linear Scale)")
                ax_n.set_title(f"{title_base} (Linear Scale)")
                plt.tight_layout()
                plt.savefig(os.path.join(cyc_folder, f"{filename_base}_linear.png"))
                plt.close()

            # Graph 1: Avg ARs (Scaled 0-1)
            def plot_avg_ar(ax):
                ax.set_ylabel("Average Acceptance Rate")
                ax.set_ylim(-0.05, 1.05)
                ax.plot(avg_stats["temp"], avg_stats["overall_ar"], color='blue', linewidth=2, label="Avg Overall AR")
                ax.plot(avg_stats["temp"], avg_stats["bad_ar"], color='cyan', linewidth=2, linestyle='--', label="Avg Bad AR")
                ax.legend()
                ax.grid(True, alpha=0.3)
            
            generate_pair("avg_ar", plot_avg_ar, f"Cycle {cyc} - Average ARs")

            # Graph 2: Avg Heat Capacity
            def plot_avg_heat_cap(ax):
                ax.set_ylabel("Average (StdDev / Temp)^2")
                ax.plot(avg_stats["temp"], avg_stats["metric_std_temp"], color='purple', linewidth=2, label="Avg Heat Cap")
                ax.legend()
                ax.grid(True, alpha=0.3)

            generate_pair("avg_heat_capacity", plot_avg_heat_cap, f"Cycle {cyc} - Average Heat Capacity")

            # Graph 3: Avg Score Volatility
            def plot_avg_volatility(ax):
                ax.set_ylabel("Average Score StdDev")
                ax.plot(avg_stats["temp"], avg_stats["score_stddev"], color='magenta', linewidth=2, label="Avg Score StdDev")
                ax.legend()
                ax.grid(True, alpha=0.3)

            generate_pair("avg_score_volatility", plot_avg_volatility, f"Cycle {cyc} - Average Score Volatility")

            # Graph 3: Aggregate Action Weights
            def plot_agg_actions(ax):
                ax.set_ylabel("Average Weight")
                for i in range(15):
                    ax.plot(avg_stats["temp"], avg_stats[f"p{i}"], label=ACTION_NAMES[i], color=ACTION_COLORS[i])
                ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize='small')
                ax.grid(True, alpha=0.3)
            
            generate_pair("avg_actions", plot_agg_actions, f"Cycle {cyc} - Average Action Weights")

            # Graph 4: Start Temp vs Max Score (Scatter)
            start_temps = []
            max_scores = []
            for lid, group in cyc_df.groupby("lineage_id"):
                s_t = group["temp"].max()
                m_s = group["score"].max()
                start_temps.append(s_t)
                max_scores.append(m_s)
            fig_s, ax_s = plt.subplots(figsize=(10, 6))
            ax_s.scatter(start_temps, max_scores, color='magenta', alpha=0.7)
            ax_s.set_xlabel("Starting Cycle Temperature")
            ax_s.set_ylabel("Highest Score in Cycle")
            ax_s.set_title(f"Cycle {cyc} - Start Temp vs Max Score")
            ax_s.grid(True, alpha=0.3)
            plt.tight_layout()
            plt.savefig(os.path.join(cyc_folder, "start_temp_vs_max_score.png"))
            plt.close()

        # --- 3. Global Master Aggregate ---
        print("Generating Global Master Analysis...")
        global_dir = os.path.join(output_dir, "global_master")
        os.makedirs(global_dir, exist_ok=True)
        
        full_df["iter_bin"] = (full_df["iteration"] // 1000) * 1000
        iter_stats = full_df.groupby("iter_bin").mean(numeric_only=True)
        iter_index = iter_stats.index
        
        # Graph 1: Iteration vs Averaged Action Weights (Smoothed)
        fig_gw, ax_gw = plt.subplots(figsize=(15, 8))
        ax_gw.set_title("Global Average Action Weights vs Iteration (Smoothed)")
        for i in range(15):
            smoothed = iter_stats[f"p{i}"].rolling(window=30, min_periods=1).mean()
            ax_gw.plot(iter_index, smoothed, label=ACTION_NAMES[i], color=ACTION_COLORS[i])
        
        ax_gw.set_xlabel("Iteration")
        ax_gw.set_ylabel("Average Probability")
        ax_gw.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        plt.tight_layout()
        plt.savefig(os.path.join(global_dir, "global_action_weights.png"))
        plt.close()
        
        # Graph 2: Iteration vs Physics Stability (Scaled 0-1)
        fig_gp, ax_gp = plt.subplots(figsize=(15, 8))
        ax_gp.set_title("Global Average Physics Stability vs Iteration")
        ax_gp.plot(iter_index, iter_stats["overall_ar"], label="Avg Overall AR", color='blue')
        ax_gp.plot(iter_index, iter_stats["bad_ar"], label="Avg Bad AR", color='cyan', linestyle="--")
        ax_gp.set_ylabel("Acceptance Rate")
        ax_gp.set_ylim(-0.05, 1.05)
        
        ax_gp_r = ax_gp.twinx()
        ax_gp_r.plot(iter_index, iter_stats["score_stddev"], label="Avg Score StdDev", color='magenta', alpha=0.7)
        ax_gp_r.set_ylabel("Score StdDev")
        
        ax_gp.legend(loc='upper left')
        ax_gp_r.legend(loc='upper right')
        plt.tight_layout()
        plt.savefig(os.path.join(global_dir, "global_physics_stability.png"))
        plt.close()

        # Graph 3: Cycle Score Distribution (Box Plot)
        print("Generating Cycle Score Distribution Box Plot...")
        # 1. Get max score per lineage per cycle
        cycle_max_scores = full_df.groupby(["lineage_id", "cycle_num"])["score"].max().reset_index()
        
        # 2. Prepare data for boxplot
        cycles = sorted(cycle_max_scores["cycle_num"].unique())
        plot_data = []
        labels = []
        for c in cycles:
            scores = cycle_max_scores[cycle_max_scores["cycle_num"] == c]["score"].values
            if len(scores) > 0:
                plot_data.append(scores)
                labels.append(int(c))
        
        # 3. Plot
        if plot_data:
            fig_bx, ax_bx = plt.subplots(figsize=(15, 8))
            ax_bx.boxplot(plot_data, labels=labels, patch_artist=True)
            ax_bx.set_title("Distribution of Final Scores per Cycle (Across all Lineages)")
            ax_bx.set_xlabel("Cycle Number")
            ax_bx.set_ylabel("Max Score Achieved")
            ax_bx.grid(True, axis='y', alpha=0.3)
            
            # Add scatter for individual points (jittered)
            for i, data in enumerate(plot_data):
                y = data
                x = np.random.normal(1 + i, 0.04, size=len(y))
                ax_bx.scatter(x, y, alpha=0.5, color='red', s=10)

            plt.tight_layout()
            plt.savefig(os.path.join(global_dir, "global_cycle_score_dist.png"))
            plt.close()

        analysis_success = True
        print(f"Analysis complete. Results saved to {output_dir}")

    except Exception as e:
        print(f"!!! CRITICAL ERROR DURING ANALYSIS: {e}")
        import traceback
        traceback.print_exc()
        analysis_success = False

    if analysis_success:
        print(f"Dropping {len(target_tables)} analyzed tables...")
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        for table in target_tables:
            cursor.execute(f"DROP TABLE IF EXISTS {table}")
        conn.commit()
        conn.close()
        print("Tables dropped.")

if __name__ == "__main__":
    analyze_latest_logs()
