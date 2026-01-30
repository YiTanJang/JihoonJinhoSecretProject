import torch
import torch.optim as optim
import numpy as np
import time
import sqlite3
import os
from model import ConnectionFinderNet
from environment import BoardEnv
from mcts import MCTS

class Trainer:
    def __init__(self, device="cuda"):
        if torch.cuda.is_available():
            self.device = torch.device("cuda")
            print(f"GPU detected: {torch.cuda.get_device_name(0)}")
        else:
            self.device = torch.device("cpu")
            print("No GPU detected, falling back to CPU.")
            
        self.model = ConnectionFinderNet().to(self.device)
        self.optimizer = optim.Adam(self.model.parameters(), lr=0.001)
        self.env = BoardEnv()
        self.mcts = MCTS(self.model)
        
        # DB Connection for persistence
        self.db_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../db/optimizer_results.db"))
        self.checkpoint_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../db/neural_solver_checkpoint.pth"))
        self.load_checkpoint()

    def load_checkpoint(self):
        if os.path.exists(self.checkpoint_path):
            try:
                self.model.load_state_dict(torch.load(self.checkpoint_path, map_location=self.device))
                print(f"Loaded existing checkpoint from {self.checkpoint_path}")
            except Exception as e:
                print(f"Error loading checkpoint: {e}")

    def save_board_to_db(self, board):
        """Saves a high-scoring board to the shared database."""
        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()
            
            temp_env = BoardEnv()
            temp_env.reset(board)
            r_score = temp_env.get_richness_score()
            
            lineage_id = 999000
            board_str = "".join(map(str, board.flatten()))
            
            # Save to boards table
            cursor.execute("""
                INSERT INTO boards (lineage_id, richness_score, board_data)
                VALUES (?, ?, ?)
                ON CONFLICT(lineage_id) DO UPDATE SET
                    richness_score = excluded.richness_score,
                    board_data = excluded.board_data,
                    updated_at = CURRENT_TIMESTAMP
                WHERE excluded.richness_score > boards.richness_score
            """, (lineage_id, r_score, board_str))

            conn.commit()
            conn.close()
        except Exception as e:
            print(f"DB Save Error: {e}")

    def self_play(self, num_games=10, num_sims=50):
        """Generates training data through self-play."""
        batch_data = [] # (state, mcts_probs, final_reward)
        
        for _ in range(num_games):
            self.env.reset()
            game_history = []
            
            # Play a short sequence of moves (e.g., 50 moves)
            for step in range(50):
                state = self.env.get_state()
                probs = self.mcts.search(self.env, num_sims)
                
                # Choose action based on MCTS probabilities
                action = np.random.choice(len(probs), p=probs)
                
                game_history.append((state, probs))
                _, score = self.env.step(action)
            
            # Reward: use richness score (raw negative penalty)
            # Example: -50,000,000 (good) vs -500,000,000 (bad)
            score = self.env.get_richness_score()
            
            # Threshold for saving: if penalty magnitude is relatively small
            if score > -100000000: # Adjust as needed
                self.save_board_to_db(self.env.board)

            # Map negative linear penalties to a normalized [0, 1] range for RL
            # Linear penalty max is roughly 3.5 Million. log(3.5M) ~ 15.
            # (16 - log(abs(score))) / 16 gives higher reward for smaller penalty.
            abs_score = max(1.0, float(abs(score)))
            final_reward = max(0.0, (16.0 - np.log(abs_score)) / 16.0)
            
            for state, probs in game_history:
                batch_data.append((state, probs, final_reward))
                
        return batch_data

    def train_step(self, batch_data):
        self.model.train()
        states, target_probs, target_values = zip(*batch_data)
        
        states = torch.FloatTensor(np.array(states)).to(self.device)
        target_probs = torch.FloatTensor(np.array(target_probs)).to(self.device)
        target_values = torch.FloatTensor(np.array(target_values)).unsqueeze(1).to(self.device)
        
        # Forward pass
        policy, value = self.model(states)
        
        # AlphaZero Loss: (Value Loss) + (Policy Cross-Entropy)
        value_loss = torch.mean((value - target_values) ** 2)
        policy_loss = -torch.mean(torch.sum(target_probs * torch.log(policy + 1e-8), dim=1))
        
        loss = value_loss + policy_loss
        
        self.optimizer.zero_grad()
        loss.backward()
        self.optimizer.step()
        
        return loss.item(), value_loss.item(), policy_loss.item()

    def run(self, iterations=100):
        print(f"Starting Standalone Neural Solver Training on {self.device}...")
        for i in range(iterations):
            start_time = time.time()
            
            # 1. Experience Collection (Self-Play)
            data = self.self_play(num_games=2, num_sims=40)
            
            # 2. Training
            loss, v_loss, p_loss = self.train_step(data)
            
            duration = time.time() - start_time
            print(f"Iter {i}: Loss={loss:.4f} (V:{v_loss:.4f}, P:{p_loss:.4f}) | {duration:.2f}s")
            
            if i % 10 == 0:
                torch.save(self.model.state_dict(), self.checkpoint_path)
                print(f"Checkpoint saved to {self.checkpoint_path}")

if __name__ == "__main__":
    trainer = Trainer()
    trainer.run()
