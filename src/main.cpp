#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <random>
#include <chrono>
#include <string>
#include <print>
#include <fstream>
#include <csignal>
#include <iomanip>
#include <sstream>

#include "legacy/common.h"
#include "core/scoring.h"
#include "core/basis.h"
#include "engine/mutations.h"
#include "data/db_manager.h"
#include "utils/globals.h"
#include "data/shared_mem.h"
#include "utils/config.h"
#include "engine/solver.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#endif

void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::println("\n[SYSTEM] Interrupt received. Shaking hands with threads for graceful exit...");
        g_terminate_all = true;
    }
}

void init_shared_mem_4d(int thread_count) {
#ifdef _WIN32
    // Use "SAMonitor4D" directly for broader compatibility between processes
    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 65536, "SAMonitor4D");
    if (hMapFile == NULL) {
        std::println("[ERROR] CreateFileMappingA failed ({})", GetLastError());
        return;
    }

    g_monitor_ptr = (MonitorData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 65536);
    if (g_monitor_ptr) {
        memset(g_monitor_ptr, 0, sizeof(MonitorData));
        g_monitor_ptr->num_threads = thread_count; 
        std::println("[INIT] Shared Memory Mapped at {:p}", (void*)g_monitor_ptr);
    } else {
        std::println("[ERROR] MapViewOfFile failed ({})", GetLastError());
    }
#else
    int shm_fd = shm_open("/SAMonitor4D", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) return;

    if (ftruncate(shm_fd, 65536) == -1) {
        close(shm_fd);
        return;
    }

    g_monitor_ptr = (MonitorData*)mmap(0, 65536, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (g_monitor_ptr == MAP_FAILED) {
        g_monitor_ptr = nullptr;
        close(shm_fd);
        return;
    }
    close(shm_fd);
    memset(g_monitor_ptr, 0, sizeof(MonitorData));
    g_monitor_ptr->num_threads = thread_count;
#endif
}

int main(int argc, char* argv[]){
    try {
        // Generate Timestamped Log Table Name
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss_l;
        auto lt = std::localtime(&in_time_t);
        ss_l << "physics_logs_" << std::put_time(lt, "%Y%m%d_%H%M%S");
        g_experiment_log_table = ss_l.str();

        std::signal(SIGINT, signal_handler);

        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg.starts_with("--mode=")) {
                g_solver_mode = std::stoi(std::string(arg.substr(7)));
            }
        }

        std::println("[INIT] Initializing DB...");
        init_db_4d();
        
        unsigned int hw_threads = std::thread::hardware_concurrency();
        const int num_threads = (hw_threads > 0) ? static_cast<int>(hw_threads) : 12;

        std::println("[INIT] Initializing Shared Memory...");
        init_shared_mem_4d(num_threads);
        
        std::println("[INIT] Initializing Richness Lookup...");
        init_richness_lookup();
        
        std::println("[INIT] Initializing Basis Set (This may take a moment)...");
        init_basis_set(); 
        
        std::println("=== 4-Digit Optimizer Side Project ===");
        std::println("Target Log Table Prefix: {}", g_experiment_log_table);
        std::println("Configuration: Solver Mode={}", g_solver_mode);

        if (g_solver_mode == 1 || g_solver_mode == 2) {
            std::println("Loading top boards from DB for Mode {}...", g_solver_mode);
            g_loaded_elites = load_random_elites_4d(120);
            std::println("Loaded {} boards.", g_loaded_elites.size());
        }

        std::vector<std::thread> workers;

        std::println("[INIT] Spawning {} threads...", num_threads);

        for (int i = 0; i < num_threads; ++i) {
            workers.emplace_back([i]() {
                try {
                    std::println("[Thread {}] Worker started.", i);
                    SAIsland4D island(i, g_solver_mode);
                    island.run();
                } catch (const std::exception& e) {
                    std::println(stderr, "[Thread {} CRASH] Exception: {}", i, e.what());
                } catch (...) {
                    std::println(stderr, "[Thread {} CRASH] Unknown Exception", i);
                }
            });
        }

        for (auto& t : workers) t.join();

        // Save Global Best after all cycles
        if (global_best_lineage_id != -1) {
            std::println("Saving Global Best Board: {} (Lineage {})", (long long)global_best_score, (int)global_best_lineage_id);
            save_best_board(global_best_lineage_id, global_best_initial_temp, global_best_score, global_best_board);
        }

        std::println("All threads completed. Exiting.");
        close_db_4d();
        return 0;
    } catch (const std::exception& e) {
        std::println(stderr, "[MAIN CRASH] Uncaught Exception: {}", e.what());
        return 1;
    } catch (...) {
        std::println(stderr, "[MAIN CRASH] Unknown Exception.");
        return 1;
    }
}
