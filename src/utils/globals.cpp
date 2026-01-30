#include "utils/globals.h"
#include "data/shared_mem.h"
#include <print>

// --- Global State Definitions ---

// Mutexes for thread safety
std::mutex console_mtx;
std::mutex pool_mtx;
std::mutex db_mtx;

MonitorData* g_monitor_ptr = nullptr;

// Global Best Tracking
std::array<std::array<int, 14>, 8> global_best_board;
std::atomic<long long> global_best_score{0};
int global_best_lineage_id = -1;
double global_best_initial_temp = 1000000.0;
std::chrono::steady_clock::time_point last_global_save_time = std::chrono::steady_clock::now();

// Elite Gene Pool
std::vector<std::pair<int, std::array<std::array<int, 14>, 8>>> gene_pool;
std::vector<EliteBoard4D> g_loaded_elites;

int g_solver_mode = 0;
std::string g_experiment_table_name = "anarchy_boards_default";
std::string g_experiment_log_table = "anarchy_logs_default";
const std::string SOLVER_VERSION = "1.4";

std::string serialize_board(const std::array<std::array<int, 14>, 8>& b) {
    std::string s;
    s.reserve(112);
    for (const auto& row : b) for (int val : row) s += std::to_string(val);
    return s;
}

void print_board(int score, const std::array<std::array<int,14>, 8>& b) {
    std::println("========================================");
    std::println(" Current Best Score: {}", score);
    std::println("========================================");
    for(const auto& row : b) {
        for(int val : row) std::print("{} ", val);
        std::println("");
    }
    std::println("");
}