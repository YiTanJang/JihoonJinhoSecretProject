#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <csignal>
#include <atomic>
#include "logic.h"
#include "mutations.h"
#include "dlas_solver.h"
#include "config.h"

std::atomic<bool> g_terminate_dlas{false};

void signal_handler(int signal) {
    if (signal == SIGINT) {
        g_terminate_dlas = true;
    }
}

int main() {
    std::signal(SIGINT, signal_handler);
    
    std::printf("=== DLAS Side Project (Maximizing Basis Score) ===\n");
    
    // Initialize required logic
    init_richness_lookup();
    init_basis_set();
    
    std::printf("Target Basis Range: 1-%d (Padding: %s)\n", 
               Config4D::BASIS_MAX_RANGE - 1, 
               Config4D::BASIS_USE_PADDING ? "Yes" : "No");
    std::printf("Basis Set Size: %d\n", get_basis_size());

    // Initial random board
    std::array<std::array<int, 14>, 8> initial_board;
    std::mt19937 rng(std::random_device{}());
    for(auto& row : initial_board) {
        for(auto& cell : row) {
            cell = std::uniform_int_distribution<int>(0, 9)(rng);
        }
    }

    // Initialize and run DLAS Solver
    // Buffer length can be tuned, 50 is a reasonable default
    DLASSolver<100> solver(initial_board, (int)std::chrono::system_clock::now().time_since_epoch().count());
    
    std::printf("Starting DLAS Search...\n");
    solver.run(100000000, 1000000); // 100M total iters, 1M idle stagnation

    std::printf("Final Best Score: %lld\n", solver.get_best_score());
    
    // Print the best board
    auto best = solver.get_best_board();
    std::printf("Best Board:\n");
    for(int r=0; r<8; ++r) {
        for(int c=0; c<14; ++c) {
            std::printf("%d", best[r][c]);
        }
        std::printf("\n");
    }

    return 0;
}
