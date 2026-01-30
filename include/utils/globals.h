#pragma once
#include <mutex>
#include <atomic>
#include <array>
#include <vector>
#include <string>
#include "data/shared_mem.h"
#include "data/db_manager.h"


extern MonitorData* g_monitor_ptr; // 전역 포인터 추가
// 전역 뮤텍스 (extern으로 선언만)
extern std::mutex console_mtx;
extern std::mutex pool_mtx;
extern std::mutex db_mtx;

// 전역 상태
extern std::array<std::array<int, 14>, 8> global_best_board;
extern std::atomic<long long> global_best_score;
extern int global_best_lineage_id;
extern double global_best_initial_temp;
extern std::chrono::steady_clock::time_point last_global_save_time;

// Elite Gene Pool
extern std::vector<std::pair<int, std::array<std::array<int, 14>, 8>>> gene_pool;
extern std::vector<EliteBoard4D> g_loaded_elites;

extern int g_solver_mode;
extern std::string g_experiment_log_table;
extern const std::string SOLVER_VERSION;

// 공통 유틸리티
std::string serialize_board(const std::array<std::array<int, 14>, 8>& b);
void print_board(int score, const std::array<std::array<int,14>, 8>& b);
