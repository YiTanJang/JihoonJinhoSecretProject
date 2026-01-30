#pragma once
#include <vector>
#include <array>
#include <utility>

// DB 초기화
void init_db();

// Richness 결과 저장
void save_richness_result(int lineage_id, long long r_score, const std::array<std::array<int, 14>, 8>& board);

// 엘리트 로드
std::vector<std::pair<int, std::array<std::array<int, 14>, 8>>> load_random_elites(int n);

// Load ALL unique boards (no limit)
std::vector<std::pair<int, std::array<std::array<int, 14>, 8>>> load_all_unique_elites();

void cleanup_low_scores(int threshold);