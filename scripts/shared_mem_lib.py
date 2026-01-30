import mmap
import struct
import time
import os
import re

# Shared Memory Configuration
# Must match include/data/shared_mem.h with #pragma pack(push, 1)

# MonitorData Header: int32_t num_threads; int64_t global_best_score;
FMT_MAIN = "<iq"
SIZE_MAIN = struct.calcsize(FMT_MAIN)

# ControlCommand: int32_t target_thread; int32_t command_type; int32_t processed; int32_t param_idx; double new_value;
FMT_CMD = "<iiiid"
SIZE_CMD = struct.calcsize(FMT_CMD)

# ThreadStatus Header:
# tid(i), curr(q), best(q), temp(d), iter(q) -> 5 fields
# mode(i), strat(i), cycle(i), seed(i), trial(i) -> 5 fields
# reheat(d), overall_ar(d), bad_ar(d), stddev(d) -> 4 fields
# weights(24d), ars(24d), deltas(24d), board(112i)
FMT_STATUS = "<iqqdqiiiiidddd24d24d24d112i"
SIZE_STATUS = struct.calcsize(FMT_STATUS)

MAP_SIZE = 65536
SHM_NAME = "SAMonitor4D"

MODE_NAMES = {
    0: "RICHNESS",
    1: "FREQ",
    2: "SUM",
    3: "HYBRID (LEGACY)",
    4: "HYBRID",
    5: "HYBRID_SQRT"
}

class SharedMemoryInterface:
    def __init__(self, shm_name=None):
        self.shm_name = shm_name if shm_name else SHM_NAME
        self._shm_obj = None

    def _open_shm(self, access):
        if self._shm_obj:
            try:
                self._shm_obj[:4]
                return self._shm_obj
            except Exception:
                self._shm_obj.close()
                self._shm_obj = None

        if os.name == 'nt':
            # Try exact name, then with Local\ prefix
            names = [self.shm_name, f"Local\\{self.shm_name}"]
            for name in names:
                try:
                    shm = mmap.mmap(-1, MAP_SIZE, name, access=access)
                    # num_threads should be initialized by solver to 12
                    num_threads = struct.unpack("<i", shm[:4])[0]
                    if 0 < num_threads <= 32:
                        self._shm_obj = shm
                        return shm
                    shm.close()
                except Exception:
                    continue
            return None
        else:
            name = self.shm_name.replace("\\", "/").split('/')[-1]
            path = f"/dev/shm/{name}"
            try:
                if not os.path.exists(path): return None
                f = open(path, 'r+b')
                prot = mmap.PROT_READ
                if access == mmap.ACCESS_WRITE: prot |= mmap.PROT_WRITE
                self._shm_obj = mmap.mmap(f.fileno(), MAP_SIZE, flags=mmap.MAP_SHARED, prot=prot)
                return self._shm_obj
            except Exception:
                return None

    def send_cmd(self, target, c_type, p_idx=0, p_val=0.0):
        try:
            shm = self._open_shm(mmap.ACCESS_WRITE)
            if not shm: return False
            shm.seek(SIZE_MAIN)
            cmd_data = struct.pack(FMT_CMD, target, c_type, 0, p_idx, p_val)
            shm.write(cmd_data)
            return True
        except Exception:
            return False
            
    def send_cmd_sync(self, target, c_type, p_idx=0, p_val=0.0, timeout=0.2):
        if not self.send_cmd(target, c_type, p_idx, p_val): return False
        start_wait = time.time()
        while time.time() - start_wait < timeout:
            shm = self._open_shm(mmap.ACCESS_READ)
            if not shm: break
            chk = shm[SIZE_MAIN : SIZE_MAIN + SIZE_CMD]
            _, _, processed, _, _ = struct.unpack(FMT_CMD, chk)
            if processed == 1: return True
            time.sleep(0.005)
        return False

    def read_monitor(self):
        try:
            shm = self._open_shm(mmap.ACCESS_READ)
            if not shm: return 0, 0, []
            
            header_raw = shm[:SIZE_MAIN]
            num_threads, global_best = struct.unpack(FMT_MAIN, header_raw)
            
            if num_threads <= 0 or num_threads > 32:
                return 0, 0, []

            statuses = []
            for i in range(num_threads):
                offset = SIZE_MAIN + SIZE_CMD + (i * SIZE_STATUS)
                s_data = shm[offset : offset + SIZE_STATUS]
                if len(s_data) == SIZE_STATUS:
                    statuses.append(struct.unpack(FMT_STATUS, s_data))
            
            return num_threads, global_best, statuses
        except Exception:
            return 0, 0, []

# --- Score Scaling Constants ---
def integer_sqrt(n):
    if n < 0: return -1
    if n == 0: return 0
    return int(n**0.5)

def get_basis_max_range():
    # Try multiple possible relative paths to find config.h
    possible_paths = [
        os.path.join(os.path.dirname(__file__), "..", "include", "utils", "config.h"),
        os.path.join(os.path.dirname(__file__), "include", "utils", "config.h"),
        "include/utils/config.h"
    ]
    for path in possible_paths:
        if os.path.exists(path):
            try:
                with open(path, "r") as f:
                    content = f.read()
                    match = re.search(r"BASIS_MAX_RANGE\s*=\s*(\d+)", content)
                    if match:
                        return int(match.group(1))
            except:
                pass
    return 13000 # Fallback

BASIS_MAX_RANGE = get_basis_max_range()
FREQ_LIMIT = BASIS_MAX_RANGE
MAX_FREQ_SCORE = 0.0
MAX_SUM_SCORE = 0.0
MAX_HYBRID_SCORE = 0.0
MAX_HYBRID_SQRT_SCORE = 0.0

for i in range(1, FREQ_LIMIT):
    MAX_FREQ_SCORE += 1.0
    MAX_SUM_SCORE += (1000000.0 / i)
    MAX_HYBRID_SCORE += ((1000000.0 / i) + 803.0)
    sq = integer_sqrt(i)
    denom = sq if sq > 0 else 1
    MAX_HYBRID_SQRT_SCORE += ((10000.0 / denom) + 185.0)

NORMALIZATION_FACTOR = float(BASIS_MAX_RANGE - 1)

SCORE_SCALES = {
    0: 1.0,
    1: MAX_FREQ_SCORE / NORMALIZATION_FACTOR,
    2: MAX_SUM_SCORE / NORMALIZATION_FACTOR,
    3: MAX_HYBRID_SCORE / NORMALIZATION_FACTOR,
    4: MAX_HYBRID_SCORE / NORMALIZATION_FACTOR,
    5: MAX_HYBRID_SQRT_SCORE / NORMALIZATION_FACTOR
}

def get_normalized_score(mode, raw_score):
    scale = SCORE_SCALES.get(mode, 1.0)
    if scale == 0: return raw_score
    return raw_score / scale

shm_interface = SharedMemoryInterface()