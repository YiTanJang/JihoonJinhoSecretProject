#pragma once
#include <vector>
#include <array>
#include <utility>

// DB 초기화 및 종료
void init_db_4d();
void close_db_4d();

// Richness 결과 저장
void save_richness_result_4d(int lineage_id, long long r_score, const std::array<std::array<int, 14>, 8>& board);
void update_lineage_state_4d(int lineage_id, long long r_score, const std::array<std::array<int, 14>, 8>& board);
void save_best_board(int lineage_id, double init_temp, long long score, const std::array<std::array<int, 14>, 8>& board);
struct PhysicsLogRecord {
    int thread_id;
    int lineage_id;
    int cycle;
    long long iteration;
    double temp;
    double overall_ar;
    double bad_ar;
    double energy_stddev;
    double avg_bad_prop_delta; // New: Avg DeltaE of all proposed bad moves
    double avg_bad_acc_delta;  // New: Avg DeltaE of accepted bad moves
    long long score;
    double probs[24];
    double ars[24];
    double deltas[24];
};

void save_physics_log_batch(const std::vector<PhysicsLogRecord>& records);

struct EliteBoard4D {
    int lineage_id;
    std::array<std::array<int, 14>, 8> board;
    double initial_temp;
};

// 엘리트 로드
std::vector<EliteBoard4D> load_random_elites_4d(int n);

// Load ALL unique boards (no limit)
std::vector<EliteBoard4D> load_all_unique_elites_4d();

void cleanup_low_scores_4d(int threshold);