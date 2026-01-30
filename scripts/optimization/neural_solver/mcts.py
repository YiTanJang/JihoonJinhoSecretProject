import numpy as np
import torch
import math

class MCTSNode:
    def __init__(self, state, p=0, parent=None):
        self.state = state # The one-hot encoded board
        self.parent = parent
        self.children = {} # action -> MCTSNode
        self.visit_count = 0
        self.value_sum = 0
        self.prior = p

    @property
    def value(self):
        if self.visit_count == 0:
            return 0
        return self.value_sum / self.visit_count

    def select_child(self, c_puct):
        best_score = -float('inf')
        best_action = -1
        best_child = None

        for action, child in self.children.items():
            # AlphaZero UCB formula
            # Use a large number for unvisited children to ensure they are picked at least once
            if child.visit_count == 0:
                u_score = c_puct * child.prior * math.sqrt(self.visit_count + 1e-8)
                score = u_score + 1000.0 # Bias towards unvisited
            else:
                u_score = c_puct * child.prior * math.sqrt(self.visit_count) / (1 + child.visit_count)
                score = child.value + u_score
            
            if score > best_score:
                best_score = score
                best_action = action
                best_child = child
        
        return best_action, best_child

    def expand(self, action_probs):
        """
        Expands the node with children based on policy probabilities.
        action_probs: (1120,) array
        """
        for action, p in enumerate(action_probs):
            if p > 0:
                # We don't create the node yet to save memory, 
                # just store the prior.
                self.children[action] = MCTSNode(state=None, p=p, parent=self)

class MCTS:
    def __init__(self, model, c_puct=1.4):
        self.model = model
        self.c_puct = c_puct

    def search(self, env, num_simulations):
        state = env.get_state()
        root = MCTSNode(state)
        
        device = next(self.model.parameters()).device

        # Initial expansion of root
        with torch.no_grad():
            state_tensor = torch.FloatTensor(state).unsqueeze(0).to(device)
            policy, value = self.model(state_tensor)
            root.expand(policy[0].cpu().numpy())

        for _ in range(num_simulations):
            node = root
            search_env = BoardEnvCopy(env) 

            # 2. Select: Walk down to a leaf node
            while node.children and all(child.visit_count > 0 for child in node.children.values()):
                action, node = node.select_child(self.c_puct)
                search_env.step(action)
            
            # If we picked a child with visit_count == 0, that's our leaf
            if node.visit_count > 0 and node.children:
                action, node = node.select_child(self.c_puct)
                search_env.step(action)

            # 3. Expand & Evaluate
            if node.visit_count == 0:
                current_state = search_env.get_state()
                with torch.no_grad():
                    state_tensor = torch.FloatTensor(current_state).unsqueeze(0).to(device)
                    policy, value = self.model(state_tensor)
                    
                    node.state = current_state
                    node.expand(policy[0].cpu().numpy())
                    v = value.item()
            else:
                v = node.value

            # 4. Backpropagate
            while node is not None:
                node.visit_count += 1
                node.value_sum += v
                node = node.parent

        # Return action probabilities based on visit counts
        counts = [root.children[a].visit_count if a in root.children else 0 for a in range(1120)]
        total_visits = sum(counts)
        if total_visits == 0:
            # Fallback to policy if no simulations succeeded (shouldn't happen with fixed logic)
            with torch.no_grad():
                state_tensor = torch.FloatTensor(state).unsqueeze(0).to(next(self.model.parameters()).device)
                policy, _ = self.model(state_tensor)
                return policy[0].cpu().numpy()
        
        probs = [c / total_visits for c in counts]
        return probs

class BoardEnvCopy:
    """A lightweight copy of the environment for MCTS simulation."""
    def __init__(self, env):
        self.board = env.board.copy()
        self.rows = env.rows
        self.cols = env.cols
        self.lib = env.lib

    def step(self, action):
        cell_idx = action // 10
        new_val = action % 10
        r, c = cell_idx // self.cols, cell_idx % self.cols
        self.board[r, c] = new_val

    def get_state(self):
        state = np.zeros((10, self.rows, self.cols), dtype=np.float32)
        for r in range(self.rows):
            for c in range(self.cols):
                state[self.board[r, c], r, c] = 1.0
        return state
