#pragma once
#include <cstdint>

#pragma pack(push, 1)
struct ThreadStatus {
    int32_t thread_id;      // 4
    int64_t current_score;  // 8
    int64_t best_score;     // 8
    double temperature;     // 8
    int64_t total_iter;     // 8
    int32_t mode;           // 4
    int32_t strategy;       // 4 [New]
    int32_t cycle_count;    // [New] 0-3
    int32_t seed_count;     // [New] 0-2
    int32_t trial_id;       // [New] 0-35
    double reheat_factor;   // [New] 0.75, 0.66, 0.50
    double overall_ar;      // [New] Global Acceptance Rate
    double bad_ar;          // [New] Acceptance Rate for Bad Moves
    double energy_stddev;   // [New] Standard Deviation of Energy Deltas
    double action_weights[24]; // ALNS Weights
    double action_ars[24];     // Acceptance Rate per Action
    double action_deltas[24];  // Delta E per Action
    int32_t current_board[8][14]; // [New] Current Board State (448 bytes)
}; 

struct ControlCommand {
    int32_t target_thread;  // -1: 전체, 0~N: 특정 쓰레드
    int32_t command_type;   // 1: Reseed, 2: 파라미터 업데이트
    int32_t processed;      
    int32_t param_idx;      
    double new_value;       
}; 

struct MonitorData {
    int32_t num_threads;
    int64_t global_best_score;
    ControlCommand cmd;
    ThreadStatus status[32];
};
#pragma pack(pop)