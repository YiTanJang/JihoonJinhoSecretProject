#include "db_manager.h"
#include "globals.h" 
#include "logic.h"
#include "sqlite3.h"
#include <sstream>
#include <format>
#include <print>
#include <string>
#include <random>

sqlite3* db = nullptr;

void init_db() {
    std::lock_guard<std::mutex> lock(db_mtx);
    if (db != nullptr) return;

    const char* db_path = "db/optimizer_results.db";
    int rc = sqlite3_open(db_path, &db);
    
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        db_path = "../db/optimizer_results.db";
        rc = sqlite3_open(db_path, &db);
    }

    if (rc != SQLITE_OK) return;

    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    // Create new unified table if it doesn't exist
    const char* create_sql = 
        "CREATE TABLE IF NOT EXISTS boards ("
        "  lineage_id INTEGER PRIMARY KEY, "
        "  richness_score INTEGER, "
        "  board_data TEXT, "
        "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_richness ON boards(richness_score);";
    sqlite3_exec(db, create_sql, 0, 0, nullptr);

    // Migration Logic: Check if old tables exist and move data
    auto migrate = [](const char* source_table, const char* score_col) {
        std::string check_sql = std::format("SELECT name FROM sqlite_master WHERE type='table' AND name='{}';", source_table);
        sqlite3_stmt* check_stmt;
        if (sqlite3_prepare_v2(db, check_sql.c_str(), -1, &check_stmt, 0) == SQLITE_OK) {
            if (sqlite3_step(check_stmt) == SQLITE_ROW) {
                // Table exists, migrate unique boards
                std::string select_sql = (std::string(source_table) == "lineage_results") 
                    ? "SELECT MAX(lineage_id), board_data FROM lineage_results GROUP BY board_data;"
                    : "SELECT lineage_id, board_data FROM richness_results;";
                
                sqlite3_stmt* sel_stmt;
                if (sqlite3_prepare_v2(db, select_sql.c_str(), -1, &sel_stmt, 0) == SQLITE_OK) {
                    while (sqlite3_step(sel_stmt) == SQLITE_ROW) {
                        int lid = sqlite3_column_int(sel_stmt, 0);
                        const char* b_data = (const char*)sqlite3_column_text(sel_stmt, 1);
                        if (!b_data) continue;

                        // Calculate richness for the board
                        std::array<std::array<int, 14>, 8> board;
                        std::string b_str(b_data);
                        for(int i=0; i<112; ++i) board[i/14][i%14] = b_str[i] - '0';
                        long long r_score = get_richness_score(board);

                        std::string ins_sql = std::format(
                            "INSERT OR IGNORE INTO boards (lineage_id, richness_score, board_data) VALUES ({}, {}, '{}');",
                            lid, r_score, b_data
                        );
                        sqlite3_exec(db, ins_sql.c_str(), 0, 0, nullptr);
                    }
                    sqlite3_finalize(sel_stmt);
                }
            }
            sqlite3_finalize(check_stmt);
        }
    };

    migrate("lineage_results", "hybrid_sqrt_score");
    migrate("richness_results", "richness_score");

    // Drop old tables after migration
    sqlite3_exec(db, "DROP TABLE IF EXISTS lineage_results;", nullptr, nullptr, nullptr);
    sqlite3_exec(db, "DROP TABLE IF EXISTS richness_results;", nullptr, nullptr, nullptr);

    // --- FULL RECALCULATION STEP ---
    // Ensure all boards in the new table reflect the latest scoring logic
    sqlite3_stmt* update_stmt;
    if (sqlite3_prepare_v2(db, "SELECT lineage_id, board_data FROM boards;", -1, &update_stmt, 0) == SQLITE_OK) {
        std::println("[DB] Recalculating all board scores with latest logic...");
        while (sqlite3_step(update_stmt) == SQLITE_ROW) {
            int lid = sqlite3_column_int(update_stmt, 0);
            const char* b_data = (const char*)sqlite3_column_text(update_stmt, 1);
            if (!b_data) continue;

            std::array<std::array<int, 14>, 8> board;
            std::string b_str(b_data);
            for(int i=0; i<112; ++i) board[i/14][i%14] = b_str[i] - '0';
            long long new_r_score = get_richness_score(board);

            std::string sql = std::format(
                "UPDATE boards SET richness_score = {} WHERE lineage_id = {};",
                new_r_score, lid
            );
            sqlite3_exec(db, sql.c_str(), 0, 0, nullptr);
        }
        sqlite3_finalize(update_stmt);
        std::println("[DB] All scores updated successfully.");
    }
}

void save_richness_result(int lineage_id, long long r_score, const std::array<std::array<int, 14>, 8>& board) {
    std::lock_guard<std::mutex> lock(db_mtx);
    if (!db) return;

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
    sqlite3_exec(db, sql.c_str(), 0, 0, &errMsg);
    if (errMsg) sqlite3_free(errMsg);
}

// Retrieves 'count' elite boards using a weighted random selection (favoring higher richness).
std::vector<std::pair<int, std::array<std::array<int, 14>, 8>>> load_random_elites(int count) {
    std::vector<std::pair<int, std::array<std::array<int, 14>, 8>>> results;
    if (!db) return results;

    // Fetch top 100 candidates by richness
    std::string sql = "SELECT board_data, lineage_id FROM boards ORDER BY richness_score DESC LIMIT 100;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return results;

    struct Candidate {
        int lineage_id;
        std::array<std::array<int, 14>, 8> board;
    };
    std::vector<Candidate> candidates;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        int lid = sqlite3_column_int(stmt, 1);
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
        candidates.push_back({lid, board});
    }
    sqlite3_finalize(stmt);

    if (candidates.empty()) return results;

    std::random_device rd;
    std::mt19937 gen(rd());
    
    if (count >= (int)candidates.size()) {
        for(const auto& cand : candidates) results.push_back({cand.lineage_id, cand.board});
        return results;
    }

    std::vector<double> weights;
    for (size_t i = 0; i < candidates.size(); ++i) weights.push_back(1.0 / (i + 1.0));

    for (int i = 0; i < count; ++i) {
        if (weights.empty()) break;
        std::discrete_distribution<> dist(weights.begin(), weights.end());
        int selected_idx = dist(gen);
        results.push_back({candidates[selected_idx].lineage_id, candidates[selected_idx].board});
        candidates.erase(candidates.begin() + selected_idx);
        weights.erase(weights.begin() + selected_idx);
    }
    return results;
}

std::vector<std::pair<int, std::array<std::array<int, 14>, 8>>> load_all_unique_elites() {
    std::vector<std::pair<int, std::array<std::array<int, 14>, 8>>> results;
    if (!db) return results;

    const char* sql = "SELECT board_data, lineage_id FROM boards;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return results;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        int lid = sqlite3_column_int(stmt, 1);
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
        results.push_back({lid, board});
    }
    sqlite3_finalize(stmt);
    return results;
}

void cleanup_low_scores(int threshold) {
    std::lock_guard<std::mutex> lock(db_mtx);
    if (!db) return;

    // Routine maintenance: Checkpoint WAL to prevent file bloat
    sqlite3_exec(db, "PRAGMA wal_checkpoint(TRUNCATE);", nullptr, nullptr, nullptr);
}
