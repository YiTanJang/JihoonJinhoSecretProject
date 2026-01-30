import torch
import torch.nn as nn
import torch.nn.functional as F

class ConnectionFinderNet(nn.Module):
    def __init__(self, embed_dim=128, num_heads=8, num_layers=4):
        super(ConnectionFinderNet, self).__init__()
        
        # 1. Spatial Feature Extraction (CNN)
        # Input: (B, 10, 8, 14) - One-hot board
        self.conv1 = nn.Conv2d(10, 64, kernel_size=3, padding=1)
        self.conv2 = nn.Conv2d(64, embed_dim, kernel_size=3, padding=1)
        
        # 2. Global Connection Analysis (Transformer)
        # Reshape (B, C, H, W) -> (B, H*W, C) for attention
        encoder_layer = nn.TransformerEncoderLayer(
            d_model=embed_dim, 
            nhead=num_heads, 
            dim_feedforward=embed_dim * 2,
            batch_first=True,
            dropout=0.1
        )
        self.transformer = nn.TransformerEncoder(encoder_layer, num_layers=num_layers)
        
        # 3. Policy Head: (8 * 14 * 10) actions
        # Predicts which cell to change (8*14) and to which digit (10)
        self.policy_fc = nn.Sequential(
            nn.Linear(embed_dim, 64),
            nn.ReLU(),
            nn.Linear(64, 10) # 10 digits per cell
        )
        
        # 4. Value Head: (1) board potential
        self.value_fc = nn.Sequential(
            nn.Linear(embed_dim * 8 * 14, 128),
            nn.ReLU(),
            nn.Linear(128, 1),
            nn.Tanh() # Normalized potential -1 to 1
        )

    def forward(self, x):
        batch_size = x.size(0)
        
        # CNN stage
        x = F.relu(self.conv1(x))
        x = F.relu(self.conv2(x)) # (B, 128, 8, 14)
        
        # Prepare for Transformer: (B, 112, 128)
        x = x.view(batch_size, 128, -1).permute(0, 2, 1)
        
        # Attention stage
        x = self.transformer(x) # (B, 112, 128)
        
        # Policy: (B, 112, 10) -> (B, 1120)
        policy = self.policy_fc(x).view(batch_size, -1)
        
        # Value: (B, 1)
        value_in = x.reshape(batch_size, -1)
        value = self.value_fc(value_in)
        
        return F.softmax(policy, dim=1), value

if __name__ == "__main__":
    # Quick test
    net = ConnectionFinderNet()
    dummy_board = torch.randn(1, 10, 8, 14)
    p, v = net(dummy_board)
    print(f"Policy shape: {p.shape}") # Should be (1, 1120)
    print(f"Value shape: {v.shape}")   # Should be (1, 1)
