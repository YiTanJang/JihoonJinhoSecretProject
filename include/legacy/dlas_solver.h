#ifndef DLAS_SOLVER_H
#define DLAS_SOLVER_H

#include <array>
#include <algorithm>
#include <functional>
#include <vector>
#include <random>
#include "logic.h"
#include "mutations.h"

#include <atomic>

extern std::atomic<bool> g_terminate_dlas;

/**
 * Diversified Late Acceptance Search (DLAS) Solver for BOJ 18789
 * Adapted for Maximization from https://gist.github.com/cgiosy/ed16f4988eeb7e989a97644fe61e1561
 */
template<size_t BUFFER_LEN = 50>
class DLASSolver {
public:
    using Board = std::array<std::array<int, 14>, 8>;

    DLASSolver(Board initial_board, int seed) 
        : current_board(initial_board), best_board(initial_board) 
    {
        rng.seed(seed);
        // We maintain 3 solutions as in the original gist
        solutions[0] = initial_board;
        solutions[1] = initial_board;
        solutions[2] = initial_board;
        
        current_score = get_basis_score_extended(solutions[0]);
        best_score = current_score;
        
        // Initialize fitness buffer with initial score
        fitness_buffer.fill(current_score);
    }

    void run(long long max_iters, long long max_idle_iters = 1000000) {
        size_t cur_pos = 0;
        size_t best_pos = 0;
        size_t k = 0;
        long long idle_iters = 0;

        for (long long iter = 0; iter < max_iters; ++iter) {
            if (g_terminate_dlas) {
                std::printf("\n[DLAS] Termination signal received. Stopping gracefully.\n");
                break;
            }

            long long prev_score = current_score;
            
            // Pick next position in the 3-solution array
            size_t next_pos = (cur_pos + 1) % 3;
            if (next_pos == best_pos) next_pos = (next_pos + 1) % 3;

            // Copy current to next and mutate
            solutions[next_pos] = solutions[cur_pos];
            
            // Use existing mutation logic from mutations.h
            // We'll pick a mutation randomly for now
            apply_mutation(solutions[next_pos]);
            
            long long next_score = get_basis_score_extended(solutions[next_pos]);

            // If better than global best
            if (next_score > best_score) {
                idle_iters = 0;
                best_pos = next_pos;
                best_score = next_score;
                best_board = solutions[next_pos];
                std::printf("[DLAS] New Best: %lld at iter %lld\n", best_score, iter);
            } else {
                idle_iters++;
            }

            // Acceptance Criteria (Maximization)
            // Accept if equal to current OR better than the minimum in the buffer
            long long min_in_buffer = *std::min_element(fitness_buffer.begin(), fitness_buffer.end());
            if (next_score == current_score || next_score > min_in_buffer) {
                cur_pos = next_pos;
                current_score = next_score;
            }

            // Update fitness buffer
            long long& fit_val = fitness_buffer[k];
            // If current is worse than buffer val OR (current is better than buffer AND better than previous)
            if (current_score < fit_val || (current_score > fit_val && current_score > prev_score)) {
                fit_val = current_score;
            }
            k = (k + 1) % BUFFER_LEN;

            if (idle_iters >= max_idle_iters) {
                std::printf("[DLAS] Stagnated for %lld iters. Stopping.\n", idle_iters);
                break;
            }
            
            if (iter % 100000 == 0) {
                std::printf("[DLAS] Iter %lld | Current: %lld | Best: %lld | MinBuf: %lld\n", 
                           iter, current_score, best_score, min_in_buffer);
            }
        }
    }

    Board get_best_board() const { return best_board; }
    long long get_best_score() const { return best_score; }

private:
    void apply_mutation(Board& board) {
        // Updated Action Probabilities:
        // Micro: dist_1_swap (20%), dist_2_swap (15%), random_global_swap (15%), random_cell_mutation (10%)
        // Meso: 2x2_rotate (7%), 2x2_xwing_swap (7%), triangle_rotate (7%), variable_worm_slide (7%)
        // Macro: variable_block_rotate (5%), variable_block_swap (5%)
        
        static const std::vector<double> weights = {
            20.0, 15.0, 15.0, 10.0, // Micro
            7.0, 7.0, 7.0, 7.0,     // Meso
            5.0, 5.0                // Macro
        };
        std::discrete_distribution<int> dist(weights.begin(), weights.end());
        int choice = dist(rng);
        
        switch(choice) {
            case 0: apply_distance_1_swap(board, rng); break;
            case 1: apply_distance_2_swap(board, rng); break;
            case 2: apply_random_global_swap(board, rng); break;
            case 3: apply_random_cell_mutation(board, rng); break;
            case 4: apply_2x2_rotate(board, rng); break;
            case 5: apply_2x2_xwing_swap(board, rng); break;
            case 6: apply_triangle_rotate(board, rng); break;
            case 7: apply_variable_worm_slide(board, rng); break;
            case 8: apply_variable_block_rotate(board, rng); break;
            case 9: apply_variable_block_swap(board, rng); break;
        }
    }

    std::array<Board, 3> solutions;
    std::array<long long, BUFFER_LEN> fitness_buffer;
    Board current_board;
    Board best_board;
    long long current_score;
    long long best_score;
    std::mt19937 rng;
};

#endif
