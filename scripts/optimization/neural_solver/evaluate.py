import torch
import numpy as np
import sys
import os
import time
from rich.console import Console
from rich.table import Table

# Add current directory to path to allow imports
sys.path.append(os.path.dirname(__file__))

try:
    from trainer import Trainer
except ImportError:
    # If running from root, python path might need adjustment
    sys.path.append(os.path.join(os.getcwd(), 'scripts', 'optimization', 'neural_solver'))
    from trainer import Trainer

def evaluate():
    console = Console()
    console.print("[bold green]Starting Neural Solver Evaluation...[/]")

    try:
        trainer = Trainer()
        trainer.model.eval() # Set to evaluation mode
    except Exception as e:
        console.print(f"[red]Failed to initialize Trainer: {e}[/]")
        return

    def load_best_board_from_db(db_path):
        import sqlite3
        if not os.path.exists(db_path):
            return None
        try:
            conn = sqlite3.connect(db_path)
            cursor = conn.cursor()
            cursor.execute("SELECT board_data, richness_score FROM boards ORDER BY richness_score DESC LIMIT 1")
            row = cursor.fetchone()
            conn.close()
            if row:
                board_str = row[0]
                score = row[1]
                if len(board_str) == 8 * 14:
                    arr = np.array([int(c) for c in board_str], dtype=np.int32).reshape(8, 14)
                    return arr, score
        except Exception as e:
            console.print(f"[yellow]DB Read Error: {e}[/]")
        return None

    best_board_info = load_best_board_from_db(trainer.db_path)
    if best_board_info:
        console.print(f"[bold cyan]Loaded Best Board from DB (Richness: {best_board_info[1]})[/]")
        initial_board = best_board_info[0]
    else:
        console.print("[yellow]No best board found in DB, using random initialization.[/]")
        initial_board = None

    num_episodes = 5
    num_sims = 100 # Higher simulations for evaluation
    max_steps = 30 # Number of moves to attempt

    results = []

    for episode in range(num_episodes):
        console.print(f"[cyan]Episode {episode + 1}/{num_episodes}[/] playing...")
        
        if initial_board is not None:
             trainer.env.reset(initial_board)
        else:
             trainer.env.reset()
        
        start_score = trainer.env.get_score()
        best_in_episode = start_score
        
        start_time = time.time()
        
        for step in range(max_steps):
            probs = trainer.mcts.search(trainer.env, num_sims)
            action = np.argmax(probs)
            _, score = trainer.env.step(action)
            best_in_episode = max(best_in_episode, score)

        duration = time.time() - start_time
        
        results.append({
            "episode": episode + 1,
            "start": start_score,
            "best": best_in_episode,
            "improvement": best_in_episode - start_score, # For richness, (-100) - (-500) = +400 (Good)
            "duration": duration
        })

    # Summary Table
    table = Table(title=f"Evaluation Results (Sims={num_sims}, Steps={max_steps})")
    table.add_column("Episode", justify="right")
    table.add_column("Start Score", justify="right")
    table.add_column("Best Score", justify="right")
    table.add_column("Improvement", justify="right")
    table.add_column("Time (s)", justify="right")

    total_imp = 0
    for res in results:
        table.add_row(
            str(res["episode"]), 
            str(res["start"]), 
            str(res["best"]), 
            str(res["improvement"]),
            f"{res['duration']:.2f}"
        )
        total_imp += res["improvement"]
    
    avg_imp = total_imp / num_episodes
    console.print(table)
    console.print(f"[bold green]Average Improvement: {avg_imp:.2f}[/]")

if __name__ == "__main__":
    evaluate()
