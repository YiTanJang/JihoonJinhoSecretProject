#include "data/db_manager.h"
#include "utils/globals.h" 
#include "core/scoring.h"
#include "sqlite3.h"
#include <sstream>
#include <format>
#include <print>
#include <string>
#include <random>

sqlite3* db_4d = nullptr;

void init_db_4d() {
    std::lock_guard<std::mutex> lock(db_mtx);
    if (db_4d != nullptr) return;

    const char* db_path = "db/optimizer_4d.db";
    int rc = sqlite3_open(db_path, &db_4d);
    
    if (rc != SQLITE_OK) {
        sqlite3_close(db_4d);
        db_path = "../db/optimizer_4d.db";
        rc = sqlite3_open(db_path, &db_4d);
    }

    if (rc != SQLITE_OK) return;

    sqlite3_exec(db_4d, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_4d, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    // Create a permanent best_boards table
    const char* best_boards_sql = 
        "CREATE TABLE IF NOT EXISTS best_boards ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "  lineage_id INTEGER, "
        "  initial_temp REAL, "
        "  score INTEGER, "
        "  board_data TEXT, "
        "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  solver_version TEXT DEFAULT '1.0'"
        ");";
    sqlite3_exec(db_4d, best_boards_sql, 0, 0, nullptr);

    // Thread-specific Log Table (Limited to 12 active threads)
    for (int t = 0; t < 12; ++t) {
        std::string log_cols = "";
        for(int i = 0; i < 24; ++i) log_cols += std::format(", p{} REAL, ar{} REAL, de{} REAL", i, i, i);

        std::string thread_log_table = std::format("{}_t{}", g_experiment_log_table, t);
        std::string logs_sql = std::format(
            "CREATE TABLE IF NOT EXISTS {} ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "  thread_id INTEGER, "
            "  lineage_id INTEGER, "
            "  cycle_num INTEGER, "
            "  iteration INTEGER, "
            "  temp REAL, "
            "  overall_ar REAL, "
            "  bad_ar REAL, "
            "  energy_stddev REAL, "
            "  avg_bad_prop_delta REAL, "
            "  avg_bad_acc_delta REAL, "
            "  score INTEGER {}"
            ",  timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
            ");", thread_log_table, log_cols
        );
        char* errMsg = nullptr;
        sqlite3_exec(db_4d, logs_sql.c_str(), 0, 0, &errMsg);
        if (errMsg) {
            std::println(stderr, "[DB-4D] Error creating logs table {}: {}", thread_log_table, errMsg);
            sqlite3_free(errMsg);
        }
    }
    
    std::println("[DB-4D] Initialized best_boards | Thread-specific Logs prefixed with {}", g_experiment_log_table);
}

void close_db_4d() {
    std::lock_guard<std::mutex> lock(db_mtx);
    if (!db_4d) return;
    
    // Final checkpoint to flush everything from WAL to main DB
    sqlite3_exec(db_4d, "PRAGMA wal_checkpoint(TRUNCATE);", nullptr, nullptr, nullptr);
    sqlite3_close(db_4d);
    db_4d = nullptr;
    std::println("[DB-4D] Database closed cleanly.");
}

void save_best_board(int lineage_id, double init_temp, long long score, const std::array<std::array<int, 14>, 8>& board) {
    std::lock_guard<std::mutex> lock(db_mtx);
    if (!db_4d) return;

    std::stringstream ss;
    for (const auto& row : board) for (const auto& val : row) ss << val;
    std::string b_str = ss.str();

    std::string sql = std::format(
        "INSERT INTO best_boards (lineage_id, initial_temp, score, board_data, solver_version) "
        "VALUES ({}, {}, {}, '{}', '{}');",
        lineage_id, init_temp, score, b_str, SOLVER_VERSION
    );

    sqlite3_exec(db_4d, sql.c_str(), 0, 0, nullptr);
}

void save_richness_result_4d(int lineage_id, long long r_score, const std::array<std::array<int, 14>, 8>& board) {
    std::lock_guard<std::mutex> lock(db_mtx);
    if (!db_4d) return;

    std::stringstream ss;
    for (const auto& row : board) {
        for (const auto& val : row) ss << val;
    }
    std::string b_str = ss.str();

    std::string sql = std::format(
        "INSERT INTO boards (lineage_id, richness_score, board_data) "
        "VALUES ({0}, {1}, '{2}') "
        "ON CONFLICT(lineage_id) DO UPDATE SET "
        "  richness_score = excluded.richness_score, "
        "  board_data = excluded.board_data, "
        "  updated_at = CURRENT_TIMESTAMP "
        "WHERE excluded.richness_score > boards.richness_score;",
        lineage_id, r_score, b_str
    );

    char* errMsg = 0;
    sqlite3_exec(db_4d, sql.c_str(), 0, 0, &errMsg);
    if (errMsg) sqlite3_free(errMsg);
}

// Retrieves 'count' elite boards using a weighted random selection (favoring higher richness).
std::vector<EliteBoard4D> load_random_elites_4d(int count) {
    std::lock_guard<std::mutex> lock(db_mtx);
    std::vector<EliteBoard4D> results;
    if (!db_4d) return results;

    // Fetch top 100 candidates by score
    std::string sql = "SELECT board_data, lineage_id, initial_temp FROM best_boards ORDER BY score DESC LIMIT 100;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_4d, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return results;

    struct Candidate {
        int lineage_id;
        std::array<std::array<int, 14>, 8> board;
        double initial_temp;
    };
    std::vector<Candidate> candidates;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        int lid = sqlite3_column_int(stmt, 1);
        double it = sqlite3_column_double(stmt, 2);
        std::array<std::array<int, 14>, 8> board;
        if (text) {
            std::string board_str(reinterpret_cast<const char*>(text));
            int char_idx = 0;
            for (auto& row : board) {
                for (auto& val : row) {
                    if (char_idx < (int)board_str.length()) val = board_str[char_idx++] - '0';
                    else val = 0;
                }
            }
        } else for(auto& row : board) row.fill(0);
        candidates.push_back({lid, board, it});
    }
    sqlite3_finalize(stmt);

    if (candidates.empty()) return results;

    std::random_device rd;
    std::mt19937 gen(rd());
    
    if (count >= (int)candidates.size()) {
        for(const auto& cand : candidates) results.push_back({cand.lineage_id, cand.board, cand.initial_temp});
        return results;
    }

    std::vector<double> weights;
    for (size_t i = 0; i < candidates.size(); ++i) weights.push_back(1.0 / (i + 1.0));
    std::discrete_distribution<> dist(weights.begin(), weights.end());

    for (int i = 0; i < count; ++i) {
        int selected_idx = dist(gen);
        results.push_back({candidates[selected_idx].lineage_id, candidates[selected_idx].board, candidates[selected_idx].initial_temp});
    }

    return results;
}

std::vector<EliteBoard4D> load_all_unique_elites_4d() {
    std::lock_guard<std::mutex> lock(db_mtx);
    std::vector<EliteBoard4D> results;
    if (!db_4d) return results;

    std::string sql = "SELECT board_data, lineage_id, initial_temp FROM best_boards ORDER BY score DESC;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_4d, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return results;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        int lid = sqlite3_column_int(stmt, 1);
        double it = sqlite3_column_double(stmt, 2);
        std::array<std::array<int, 14>, 8> board;
        if (text) {
            std::string board_str(reinterpret_cast<const char*>(text));
            int char_idx = 0;
            for (auto& row : board) {
                for (auto& val : row) {
                    if (char_idx < (int)board_str.length()) val = board_str[char_idx++] - '0';
                    else val = 0;
                }
            }
        } else for(auto& row : board) row.fill(0);
        results.push_back({lid, board, it});
    }
    sqlite3_finalize(stmt);
    return results;
}

void cleanup_low_scores_4d(int threshold) {
    std::lock_guard<std::mutex> lock(db_mtx);
    if (!db_4d) return;

    // Routine maintenance: Checkpoint WAL to prevent file bloat
    sqlite3_exec(db_4d, "PRAGMA wal_checkpoint(TRUNCATE);", nullptr, nullptr, nullptr);
}

void save_physics_log_batch(const std::vector<PhysicsLogRecord>& records) {
    if (records.empty()) return;
    
    std::lock_guard<std::mutex> lock(db_mtx);
    if (!db_4d) return;

    // Precompute column string once
    static const std::string cols_str = [](){
        std::string s = "";
        for(int i=0; i<24; ++i) s += std::format(", p{}, ar{}, de{}", i, i, i);
        return s;
    }();

    sqlite3_exec(db_4d, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    for (const auto& rec : records) {
        std::string val_cols = "";
        for(int i=0; i<24; ++i) {
            val_cols += std::format(", {}, {}, {}", rec.probs[i], rec.ars[i], rec.deltas[i]);
        }

        std::string thread_log_table = std::format("{}_t{}", g_experiment_log_table, rec.thread_id);

        std::string sql = std::format(
            "INSERT INTO {} (thread_id, lineage_id, cycle_num, iteration, temp, overall_ar, bad_ar, energy_stddev, avg_bad_prop_delta, avg_bad_acc_delta, score "
            "  {} ) "
            "VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {} {});",
            thread_log_table, 
            cols_str,
            rec.thread_id, rec.lineage_id, rec.cycle, rec.iteration, rec.temp, rec.overall_ar, rec.bad_ar, rec.energy_stddev, 
            rec.avg_bad_prop_delta, rec.avg_bad_acc_delta,
            rec.score, val_cols
        );

        sqlite3_exec(db_4d, sql.c_str(), nullptr, nullptr, nullptr);
    }

    sqlite3_exec(db_4d, "COMMIT;", nullptr, nullptr, nullptr);
}