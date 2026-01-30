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

#include "common.h"
#include "logic.h"
#include "mutations.h"
#include "db_manager.h"
#include "globals.h"
#include "shared_mem.h"
#include "config.h"
#include "solver.h"

#ifdef _WIN32
#include <windows.h>
#endif

// Initializes the shared memory mapping for inter-process communication
void init_shared_mem() {
    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 65536, "Local\\SAMonitor");
    if (hMapFile == NULL) return;

    g_monitor_ptr = (MonitorData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 65536);
    if (g_monitor_ptr) {
        memset(g_monitor_ptr, 0, sizeof(MonitorData));
    }
}

int main(int argc, char* argv[]){

    init_db();
    init_shared_mem();

    cleanup_low_scores(Config4D::DB_CLEANUP_THRESHOLD);

    std::println("Arguments Check:");
    for(int i=0; i<argc; ++i) {
        std::println("  argv[{}]: {}", i, argv[i]);
    }

    int mode_arg = 0; // Default to Mode 0: Richness
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--load=0") g_load_mode = 0;
        else if (arg == "--load=1") g_load_mode = 1;
        else if (arg == "--load=2") g_load_mode = 2;

        if (arg == "--mode=0") mode_arg = 0;
        /*
        if (arg == "--mode=1") mode_arg = 1;
        if (arg == "--mode=2") mode_arg = 2;
        if (arg == "--mode=3") mode_arg = 3;
        if (arg == "--mode=4") mode_arg = 4;
        if (arg == "--mode=5") mode_arg = 5;
        if (arg == "--mode=6") mode_arg = 6;
        */
    }

    if(mode_arg == 0) std::println("Currently Evaluating in Richness Mode (Bucket Optimized Penalty)");
    /*
    else if(mode_arg == 1) std::println("Currently Evaluating upon Frequency");
    else if(mode_arg == 2) std::println("Currently Evaluating upon Sum Score");
    else if(mode_arg == 3) std::println("Currently Evaluating in Hybrid Mode (Frequency/Sum Switch)");
    else if(mode_arg == 4) std::println("Currently Evaluating in Direct Hybrid Score (1M/i + 803)");
    else if(mode_arg == 5) std::println("Currently Evaluating in Hybrid Sqrt Score (10k/sqrt(i) + 185)");
    */

    const int num_threads = std::thread::hardware_concurrency();
    std::println("Detected {} CPU threads.", num_threads);

    std::vector<std::pair<int, std::array<std::array<int, 14>, 8>>> seeds;
    if (g_load_mode == 1 || g_load_mode == 2) {
        // For load=2, we still load seeds, but only half of them (implemented below)
        seeds = load_random_elites(num_threads);
        std::println("Loaded {} seeds from database.", seeds.size());
    }

    std::vector<std::thread> workers;
    for (int i = 0; i < num_threads; ++i) {
        std::array<std::array<int, 14>, 8> initial_board;
        int lineage_id;

        bool use_seed = false;
        if (g_load_mode == 1) {
            if (i < (int)seeds.size()) use_seed = true;
        } else if (g_load_mode == 2) {
            // Half the threads use seeds, half generate fresh
            if (i < (int)seeds.size() && (i % 2 == 0)) use_seed = true;
        }

        if (use_seed) {
            lineage_id = seeds[i].first;
            initial_board = seeds[i].second;
        } else {
            std::mt19937 rng(std::random_device{}() ^ (i << 8));
            for (auto& row : initial_board) {
                for (auto& cell : row) cell = rng() % 10;
            }
            lineage_id = (static_cast<int>(rng()) & 0x7FFFFFFF);
        }

        workers.emplace_back([i, initial_board, lineage_id, mode_arg]() {
            SAIsland island(i, initial_board, lineage_id, mode_arg);
            island.run();
        });
    }

    // Periodic Pool Update Thread
    std::thread pool_updater([]() {
        while (true) {
            auto top_results = load_random_elites(Config4D::ELITE_POOL_SIZE);
            {
                std::lock_guard<std::mutex> lock(pool_mtx);
                gene_pool = top_results;
            }
            std::this_thread::sleep_for(std::chrono::seconds(Config4D::POOL_UPDATE_INTERVAL_SEC));
        }
    });

    for (auto& t : workers) t.join();
    pool_updater.join();

    return 0;
}
