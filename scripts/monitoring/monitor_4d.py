import time
import collections
import sys
import os
import struct

try:
    import msvcrt
    WINDOWS = True
except ImportError:
    import select
    import tty
    import termios
    WINDOWS = False

# Add parent directory to path to allow importing shared_mem_lib
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from rich.live import Live
from rich.table import Table
from rich.layout import Layout
from rich.panel import Panel
from rich.console import Group
from rich.text import Text
from shared_mem_lib import SharedMemoryInterface, MODE_NAMES, get_normalized_score

# [Specialized for 4D]
class SharedMemoryInterface4D(SharedMemoryInterface):
    def __init__(self):
        # Pass 4D SHM Name directly to base constructor
        super().__init__(shm_name=r"Local\SAMonitor4D")

shm_4d = SharedMemoryInterface4D()

PARAM_NAMES = [
    "Adj Swap", "Double", "TripleSwap", "2CellSwap", 
    "Single", "P5", "P6", "P7",
    "P8", "P9", "P10", "P11", "P12", "P13", "P14", "P15", "Temperature"
]

MAX_HISTORY = 500
history = collections.defaultdict(lambda: {"score": collections.deque(maxlen=MAX_HISTORY)})

# Thread Data Storage
thread_data = collections.defaultdict(lambda: {
    "overall_ar": 0.0,
    "weights": [0.0]*24,
    "ars": [0.0]*24,
    "deltas": [0.0]*24,
    "temp": 0.0
})

last_iters = {}
last_time = time.time()
ema_ips = {}
selected_thread = 0
selected_action_idx = 0
current_boards = {}

ACTION_NAMES = [
    "Dist 1 Swap", "Dist 2 Swap", "Global Swap", "Rand Cell",
    "Domino Local", "Domino Global", "Tri Rotate", "Straight Slide",
    "Worm Slide", "Block Rotate", "Heatmap Swap", "Heatmap Domino",
    "Heatmap Mutate", "Var Blk Swap", "Var Blk Flip", "N/A",
    "N/A", "N/A", "N/A", "N/A", "N/A", "N/A",
    "N/A", "N/A"
]

def get_color(val):
    colors = ["grey30", "bright_red", "bright_green", "bright_yellow", "bright_blue", "bright_magenta", "bright_cyan", "white", "orange1", "plum1"]
    try:
        idx = int(val) % len(colors)
        return colors[idx]
    except:
        return "white"

def make_bar(val, width=20, color="green"):
    blocks = int(val * width)
    bar = "█" * blocks
    remainder = val * width - blocks
    if remainder > 0.5: bar += "▌"
    return f"[{color}]{bar.ljust(width)}[/{color}]"

def generate_board_panel(tid):
    if tid not in current_boards:
        return Panel("No Data")
    
    board = current_boards[tid]
    
    grid = Table.grid(padding=0)
    for _ in range(14): grid.add_column(justify="center", width=3)
    
    for r in range(8):
        row_cells = []
        for c in range(14):
            val = board[r][c]
            style = f"bold {get_color(val)}"
            row_cells.append(Text(str(val), style=style))
        grid.add_row(*row_cells)
        
    return Panel(grid, title=f"Thread {tid} Board View", border_style="white")

def generate_layout():
    global last_time, selected_thread, selected_action_idx, thread_data, current_boards
    
    try:
        n_threads, g_best, statuses = shm_4d.read_monitor()
    except:
        return Panel("Read Error")

    if n_threads <= 0:
        return Panel(Text("4D 솔버가 실행 중이지 않습니다 (SHM: SAMonitor4D)", style="bold yellow"))

    # [Keyboard Interaction]
    if WINDOWS:
        if msvcrt.kbhit():
            ch = msvcrt.getch()
            if ch in [b'\x00', b'\xe0']:
                ch = msvcrt.getch()
                if ch == b'H': selected_thread = (selected_thread - 1) % n_threads # Up
                elif ch == b'P': selected_thread = (selected_thread + 1) % n_threads # Down
            else:
                try:
                    k = ch.decode('utf-8').lower()
                    if k == 'r':
                        shm_4d.send_cmd_sync(selected_thread, 1)
                    elif k == 'k':
                        shm_4d.send_cmd_sync(selected_thread, 2) # Kill Cycle & Reseed
                except:
                    pass
    else:
        # Linux non-blocking read
        if select.select([sys.stdin], [], [], 0) == ([sys.stdin], [], []):
            k = sys.stdin.read(1).lower()
            if k == 'w': selected_thread = (selected_thread - 1) % n_threads
            elif k == 's': selected_thread = (selected_thread + 1) % n_threads
            elif k == 'r': shm_4d.send_cmd_sync(selected_thread, 1)
            elif k == 'k': shm_4d.send_cmd_sync(selected_thread, 2)

    dt = time.time() - last_time
    last_time = time.time()
    
    table = Table(expand=True, title="[bold cyan]BOJ 18789 Simulator Monitor[/]")
    table.caption = "[bold yellow]Arrows: Nav | R: Soft Reseed | K: Kill Cycle & Reseed[/]"
    for col in ["ID", "Cyc", "IPS", "Current", "Best", "Temp", "O.AR%", "B.AR%", "StdDev"]: table.add_column(col, justify="center")

    for s in statuses:
        try:
            tid = s[0]
            curr = s[1]
            best = s[2]
            temp = s[3]
            iters = s[4]
            cycle = s[7]
            trial = s[9]
            reheat = s[10]
            
            # New Fields
            overall_ar = s[11]
            bad_ar = s[12]
            stddev = s[13]
            weights = s[14:38]
            ars = s[38:62]
            deltas = s[62:86]
            
            # Board starts at index 86
            board_flat = s[86:]
            
            prev_i = last_iters.get(tid, iters)
            ips = (iters - prev_i) / dt if dt > 0 else 0
            last_iters[tid] = iters
            ema = (ema_ips.get(tid, ips) * 0.8) + (ips * 0.2); ema_ips[tid] = ema
            
            row_style = "on blue" if tid == selected_thread else ""
            
            # Color Logic
            c_temp = "[bold red]" if temp > 100 else "[bold yellow]" if temp > 1 else "[bold cyan]"
            c_oar = "[bold green]" if overall_ar > 0.1 else "[bold yellow]" if overall_ar > 0.01 else "[red]"
            
            table.add_row(
                str(tid), f"C{cycle}", f"{ema:,.0f}", 
                f"[bold white]{curr:,.0f}[/]", f"[bold green]{best:,.0f}[/]", f"{c_temp}{temp:.2f}[/]", 
                f"{c_oar}{overall_ar*100:.1f}%[/]", f"{bad_ar*100:.1f}%", f"{stddev:.1f}", 
                style=row_style
            )

            b_matrix = [board_flat[r*14 : (r+1)*14] for r in range(8)]
            current_boards[tid] = b_matrix
            
            thread_data[tid] = {
                "overall_ar": overall_ar,
                "bad_ar": bad_ar,
                "stddev": stddev,
                "weights": weights,
                "ars": ars,
                "deltas": deltas,
                "temp": temp
            }
        except Exception:
            continue

    # Generate Action Stats Grid
    action_grid = Table.grid(expand=True, padding=(0, 2))
    action_grid.add_column(justify="right", width=15) # Name
    action_grid.add_column(justify="left", width=25)  # Bar
    action_grid.add_column(justify="right", width=10) # Prob
    action_grid.add_column(justify="right", width=10) # AR
    action_grid.add_column(justify="right", width=10) # Delta
    
    if selected_thread in thread_data:
        td = thread_data[selected_thread]
        weights = td["weights"]
        ars = td["ars"]
        deltas = td["deltas"]
        
        # Determine max weight for scaling
        max_w = max(weights[:15]) if max(weights[:15]) > 0 else 1.0

        for i in range(15): # All 15 actions
            name = ACTION_NAMES[i]
            w = weights[i]
            ar = ars[i]
            delta = deltas[i]
            
            # Category Colors
            cat_color = "bright_cyan" # Micro (0-3)
            if i >= 4 and i <= 12: cat_color = "bright_green" # Meso (4-12)
            elif i >= 13: cat_color = "bright_red" # Macro (13-14)
            
            scaled_w = w / max_w
            bar = make_bar(scaled_w, width=20, color=cat_color)
            
            action_grid.add_row(
                f"[{cat_color}]{name}[/]",
                bar,
                f"{w*100:.1f}%",
                f"{ar*100:.1f}%",
                f"{delta:.2f}"
            )
    
    main_body = Layout()
    main_body.split_row(Layout(table, ratio=2), Layout(generate_board_panel(selected_thread), ratio=1))
    
    layout = Layout()
    layout.split_column(Layout(main_body, size=n_threads + 4), Layout(Panel(action_grid, title=f"Thread {selected_thread} Probabilities & Physics"), size=22))
    return layout

if __name__ == "__main__":
    with Live(generate_layout(), refresh_per_second=4, screen=True) as live:
        while True:
            try:
                live.update(generate_layout())
            except:
                pass
            time.sleep(0.2)
