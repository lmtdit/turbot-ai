#include <turbot/storage/sqlite_database.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>

namespace turbot::storage::sqlite {

// ============================================================================
// RAII Wrapper for sqlite3_stmt
// ============================================================================

namespace {

struct StmtDeleter {
    void operator()(sqlite3_stmt* stmt) const noexcept {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }
};

using StmtPtr = std::unique_ptr<sqlite3_stmt, StmtDeleter>;

// Shared helper: bind JSON value to SQLite parameter
void bind_json_param(sqlite3_stmt* stmt, int index, const nlohmann::json& value) {
    if (value.is_null()) {
        sqlite3_bind_null(stmt, index);
    } else if (value.is_string()) {
        auto str = value.get<std::string>();
        if (str.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            throw std::runtime_error("String too large for SQLite parameter");
        }
        sqlite3_bind_text(stmt, index, str.c_str(), static_cast<int>(str.size()),
                          SQLITE_TRANSIENT);
    } else if (value.is_number_integer()) {
        sqlite3_bind_int64(stmt, index, value.get<int64_t>());
    } else if (value.is_number_unsigned()) {
        auto uval = value.get<uint64_t>();
        if (uval > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            throw std::runtime_error(
                "Unsigned value " + std::to_string(uval) + " exceeds SQLite int64 range");
        }
        sqlite3_bind_int64(stmt, index, static_cast<int64_t>(uval));
    } else if (value.is_number_float()) {
        sqlite3_bind_double(stmt, index, value.get<double>());
    } else if (value.is_boolean()) {
        sqlite3_bind_int(stmt, index, value.get<bool>() ? 1 : 0);
    } else {
        // Complex types: serialize to JSON string
        std::string json_str = value.dump();
        if (json_str.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            throw std::runtime_error("JSON too large for SQLite parameter");
        }
        sqlite3_bind_text(stmt, index, json_str.c_str(),
                          static_cast<int>(json_str.size()), SQLITE_TRANSIENT);
    }
}

// Shared helper: convert SQLite row to JSON object
nlohmann::json sqlite_row_to_json(sqlite3_stmt* stmt) {
    nlohmann::json row;
    int count = sqlite3_column_count(stmt);

    for (int i = 0; i < count; i++) {
        const char* name = sqlite3_column_name(stmt, i);
        // sqlite3_column_name can return nullptr for unnamed columns
        if (!name) {
            name = "";
        }

        switch (sqlite3_column_type(stmt, i)) {
            case SQLITE_INTEGER:
                row[name] = sqlite3_column_int64(stmt, i);
                break;
            case SQLITE_FLOAT:
                row[name] = sqlite3_column_double(stmt, i);
                break;
            case SQLITE_TEXT: {
                const char* text = reinterpret_cast<const char*>(
                    sqlite3_column_text(stmt, i));
                if (text) {
                    row[name] = std::string(text, static_cast<size_t>(sqlite3_column_bytes(stmt, i)));
                } else {
                    row[name] = "";
                }
                break;
            }
            case SQLITE_BLOB: {
                int size = sqlite3_column_bytes(stmt, i);
                const void* blob = (size > 0) ? sqlite3_column_blob(stmt, i) : nullptr;
                if (size > 0 && blob) {
                    row[name] = nlohmann::json::binary(
                        std::vector<uint8_t>(static_cast<const uint8_t*>(blob),
                                             static_cast<const uint8_t*>(blob) + size));
                } else {
                    row[name] = nlohmann::json::binary({});
                }
                break;
            }
            case SQLITE_NULL:
            default:
                row[name] = nullptr;
                break;
        }
    }

    return row;
}

} // anonymous namespace

// ============================================================================
// SQLiteDatabase Implementation
// ============================================================================

SQLiteDatabase::SQLiteDatabase(const DatabaseConfig& config)
    : config_(config) {
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    if (config.read_only) {
        flags = SQLITE_OPEN_READONLY;
    }

    int result = sqlite3_open_v2(config.path.c_str(), &db_, flags, nullptr);
    if (result != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Failed to open database: " + err);
    }

    // Set busy timeout
    sqlite3_busy_timeout(db_, config.timeout * 1000);

    setup_pragmas();
    ensure_migrations_table();

    spdlog::info("Opened SQLite database: {}", config.path);
}

SQLiteDatabase::~SQLiteDatabase() {
    // Mark destroyed first (atomic store — no lock needed for atomic<bool>).
    // close() will also set it inside the mutex for full synchronisation.
    if (alive_flag_) {
        alive_flag_->store(false);
    }
    close();
}

void SQLiteDatabase::setup_pragmas() {
    // Enable WAL mode for better concurrency
    if (config_.journal_wal) {
        execute("PRAGMA journal_mode=WAL;");
    }

    // Enable foreign key constraints
    if (config_.foreign_keys) {
        execute("PRAGMA foreign_keys=ON;");
    }

    // Set cache size
    execute("PRAGMA cache_size=" + std::to_string(config_.cache_size) + ";");

    // Normal synchronous mode (balance between safety and performance)
    execute("PRAGMA synchronous=NORMAL;");

    // Temp storage in memory
    execute("PRAGMA temp_store=MEMORY;");
}

void SQLiteDatabase::ensure_migrations_table() {
    execute(R"(
        CREATE TABLE IF NOT EXISTS _migrations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            version INTEGER NOT NULL,
            executed_at INTEGER NOT NULL
        )
    )");
}

void SQLiteDatabase::bind_param(sqlite3_stmt* stmt, int index, const nlohmann::json& value) {
    bind_json_param(stmt, index, value);
}

nlohmann::json SQLiteDatabase::row_to_json(sqlite3_stmt* stmt) {
    return sqlite_row_to_json(stmt);
}

QueryResult SQLiteDatabase::execute(
    const std::string& sql,
    const std::vector<nlohmann::json>& params) {

    std::lock_guard<std::mutex> lock(*mutex_);

    if (!db_) {
        throw std::runtime_error("Database is not open");
    }

    sqlite3_stmt* raw_stmt = nullptr;
    int result = sqlite3_prepare_v2(db_, sql.c_str(), -1, &raw_stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " +
                                 std::string(sqlite3_errmsg(db_)));
    }

    // 使用 RAII wrapper 管理 statement 生命周期
    StmtPtr stmt(raw_stmt);

    // Bind parameters
    for (size_t i = 0; i < params.size(); i++) {
        bind_param(stmt.get(), static_cast<int>(i + 1), params[i]);
    }

    QueryResult query_result;
    query_result.affected_rows = 0;

    // Execute and collect results
    while ((result = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        query_result.rows.push_back(row_to_json(stmt.get()));
    }

    if (result != SQLITE_DONE) {
        std::string err = sqlite3_errmsg(db_);
        // stmt 会通过 RAII 自动释放
        throw std::runtime_error("Query execution failed: " + err);
    }

    query_result.affected_rows = sqlite3_changes(db_);
    // stmt 会通过 RAII 自动释放

    return query_result;
}

std::optional<nlohmann::json> SQLiteDatabase::execute_one(
    const std::string& sql,
    const std::vector<nlohmann::json>& params) {

    auto result = execute(sql, params);
    if (result.rows.empty()) {
        return std::nullopt;
    }
    return result.rows[0];
}

std::shared_ptr<Transaction> SQLiteDatabase::begin_transaction() {
    std::lock_guard<std::mutex> lock(*mutex_);
    if (!db_) {
        throw std::runtime_error("Database is not open");
    }

    // Execute BEGIN IMMEDIATE TRANSACTION while holding the lock to avoid
    // TOCTOU: db_ cannot be closed between the check above and here.
    sqlite3_stmt* raw_stmt = nullptr;
    const char* begin_sql = "BEGIN IMMEDIATE TRANSACTION;";
    int result = sqlite3_prepare_v2(db_, begin_sql, -1, &raw_stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to begin transaction: " +
                                 std::string(sqlite3_errmsg(db_)));
    }
    StmtPtr stmt(raw_stmt);
    result = sqlite3_step(stmt.get());
    if (result != SQLITE_DONE) {
        throw std::runtime_error("Failed to begin transaction: " +
                                 std::string(sqlite3_errmsg(db_)));
    }
    return std::make_shared<SQLiteTransaction>(db_, alive_flag_, mutex_);
}

QueryResult SQLiteDatabase::execute_batch(
    const std::string& sql,
    const std::vector<std::vector<nlohmann::json>>& params_list) {

    // Wrap all executions in a single transaction for atomicity:
    // if any statement fails, all previous changes are rolled back.
    auto tx = begin_transaction();
    try {
        QueryResult total_result;
        total_result.affected_rows = 0;

        for (const auto& params : params_list) {
            auto result = tx->execute(sql, params);
            total_result.affected_rows += result.affected_rows;
            total_result.rows.insert(total_result.rows.end(),
                                      result.rows.begin(), result.rows.end());
        }

        tx->commit();
        return total_result;
    } catch (...) {
        // Transaction auto-rollback in destructor
        throw;
    }
}

void SQLiteDatabase::migrate(const std::string& name, const std::string& sql, int version) {
    // Use transaction for atomic migration - ensures both SQL execution and
    // migration record are committed together or rolled back together
    auto tx = begin_transaction();

    try {
        // Check if migration already applied (within transaction)
        auto existing = tx->execute_one(
            "SELECT id FROM _migrations WHERE name = ?",
            {nlohmann::json(name)});

        if (existing.has_value()) {
            spdlog::debug("Migration '{}' already applied", name);
            tx->rollback();  // Release transaction
            return;
        }

        // Execute migration SQL - supports multiple statements (e.g. CREATE TABLE; CREATE INDEX;)
        // by iterating through all statements using sqlite3_prepare_v2 pzTail
        {
            std::lock_guard<std::mutex> lock(*mutex_);
            if (!db_) {
                throw std::runtime_error("Database was closed during migration execution");
            }
            const char* remaining = sql.c_str();
            while (remaining && *remaining) {
                sqlite3_stmt* raw_stmt = nullptr;
                const char* tail = nullptr;
                int result = sqlite3_prepare_v2(db_, remaining, -1, &raw_stmt, &tail);
                if (result != SQLITE_OK) {
                    throw std::runtime_error("Failed to prepare migration statement: " +
                                             std::string(sqlite3_errmsg(db_)));
                }
                if (!raw_stmt) {
                    // Only whitespace or comments, skip ahead
                    remaining = tail;
                    continue;
                }
                StmtPtr stmt(raw_stmt);
                while ((result = sqlite3_step(stmt.get())) == SQLITE_ROW) {
                    // DDL statements return no rows; ignore any rows for other stmts
                }
                if (result != SQLITE_DONE) {
                    throw std::runtime_error("Migration statement failed: " +
                                             std::string(sqlite3_errmsg(db_)));
                }
                remaining = tail;
            }
        }

        // Record migration with the actual version number
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        tx->execute(
            "INSERT INTO _migrations (name, version, executed_at) VALUES (?, ?, ?)",
            {nlohmann::json(name), nlohmann::json(version), nlohmann::json(now)});

        // Commit transaction - atomic commit of both operations
        tx->commit();
        spdlog::info("Applied migration: {} (v{})", name, version);
    } catch (...) {
        // Transaction will auto-rollback in destructor if not committed
        throw;
    }
}

bool SQLiteDatabase::health_check() {
    try {
        auto result = execute_one("SELECT 1 as ok");
        return result.has_value() && (*result)["ok"] == 1;
    } catch (...) {
        return false;
    }
}

void SQLiteDatabase::close() {
    std::lock_guard<std::mutex> lock(*mutex_);
    if (db_) {
        // Invalidate outstanding transactions before closing the handle.
        // This must be done inside the lock so that any transaction that
        // passes the is_db_alive() check sees db_ == nullptr atomically.
        if (alive_flag_) alive_flag_->store(false);
        sqlite3_close_v2(db_);
        db_ = nullptr;
        spdlog::info("Closed SQLite database: {}", config_.path);
    }
}

bool SQLiteDatabase::is_open() const {
    std::lock_guard<std::mutex> lock(*mutex_);
    return db_ != nullptr;
}

// ============================================================================
// SQLiteTransaction Implementation
// ============================================================================

SQLiteTransaction::SQLiteTransaction(sqlite3* db,
                                     std::shared_ptr<std::atomic<bool>> alive_flag,
                                     std::shared_ptr<std::mutex> shared_mutex)
    : db_(db)
    , alive_flag_(std::move(alive_flag))
    , mutex_(std::move(shared_mutex)) {
    if (!db_) {
        throw std::runtime_error("Cannot create transaction without database");
    }
    if (!mutex_) {
        throw std::runtime_error("Cannot create transaction without a valid mutex");
    }
}

SQLiteTransaction::~SQLiteTransaction() {
    if (active_.load() && is_db_alive() && db_) {
        try {
            rollback();
        } catch (...) {
            // Ignore errors in destructor
        }
    }
}

void SQLiteTransaction::commit() {
    if (!active_.load()) {
        throw std::runtime_error("Transaction is not active");
    }

    std::lock_guard<std::mutex> lock(*mutex_);
    // Re-check db_ inside the lock to close the TOCTOU window between
    // is_db_alive() and the sqlite3_exec call.
    if (!db_) {
        active_.store(false);
        throw std::runtime_error("Database has been destroyed");
    }

    char* err_msg = nullptr;
    int result = sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &err_msg);

    if (result != SQLITE_OK) {
        std::string err = err_msg ? err_msg : "unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("Failed to commit transaction: " + err);
    }

    active_.store(false);
    spdlog::debug("Transaction committed");
}

void SQLiteTransaction::rollback() {
    if (!active_.load()) {
        return;  // Already rolled back
    }

    std::lock_guard<std::mutex> lock(*mutex_);
    // Re-check db_ inside the lock; if the database was closed while we waited
    // for the lock, there is nothing to roll back.
    if (!db_) {
        active_.store(false);
        return;
    }

    char* err_msg = nullptr;
    int result = sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, &err_msg);

    if (result != SQLITE_OK) {
        std::string err = err_msg ? err_msg : "unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("Failed to rollback transaction: " + err);
    }

    active_.store(false);
    spdlog::debug("Transaction rolled back");
}

void SQLiteTransaction::bind_param(sqlite3_stmt* stmt, int index, const nlohmann::json& value) {
    bind_json_param(stmt, index, value);
}

nlohmann::json SQLiteTransaction::row_to_json(sqlite3_stmt* stmt) {
    return sqlite_row_to_json(stmt);
}

QueryResult SQLiteTransaction::execute(
    const std::string& sql,
    const std::vector<nlohmann::json>& params) {

    if (!active_.load()) {
        throw std::runtime_error("Transaction is not active");
    }

    std::lock_guard<std::mutex> lock(*mutex_);
    // Re-check db_ inside the lock to close the TOCTOU window.
    if (!db_) {
        active_.store(false);
        throw std::runtime_error("Database has been destroyed");
    }

    sqlite3_stmt* raw_stmt = nullptr;
    int result = sqlite3_prepare_v2(db_, sql.c_str(), -1, &raw_stmt, nullptr);
    if (result != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " +
                                 std::string(sqlite3_errmsg(db_)));
    }

    // 使用 RAII wrapper 管理 statement 生命周期
    StmtPtr stmt(raw_stmt);

    for (size_t i = 0; i < params.size(); i++) {
        bind_param(stmt.get(), static_cast<int>(i + 1), params[i]);
    }

    QueryResult query_result;
    query_result.affected_rows = 0;

    while ((result = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        query_result.rows.push_back(row_to_json(stmt.get()));
    }

    if (result != SQLITE_DONE) {
        std::string err = sqlite3_errmsg(db_);
        // stmt 会通过 RAII 自动释放
        throw std::runtime_error("Query execution failed: " + err);
    }

    query_result.affected_rows = sqlite3_changes(db_);
    // stmt 会通过 RAII 自动释放

    return query_result;
}

std::optional<nlohmann::json> SQLiteTransaction::execute_one(
    const std::string& sql,
    const std::vector<nlohmann::json>& params) {

    auto result = execute(sql, params);
    if (result.rows.empty()) {
        return std::nullopt;
    }
    return result.rows[0];
}

} // namespace turbot::storage::sqlite
