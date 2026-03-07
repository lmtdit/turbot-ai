#pragma once

#include <turbot/storage/export.hpp>
#include <turbot/storage/database.hpp>

#include <memory>
#include <mutex>
#include <sqlite3.h>

namespace turbot::storage::sqlite {

class SQLiteDatabase;  // Forward declaration

/// SQLite implementation of the Database interface
class TURBOT_STORAGE_API SQLiteDatabase : public Database {
public:
    /// Construct a database with the given configuration
    /// @param config Database configuration
    explicit SQLiteDatabase(const DatabaseConfig& config);

    /// Destructor - closes the database
    ~SQLiteDatabase() override;

    // Non-copyable
    SQLiteDatabase(const SQLiteDatabase&) = delete;
    SQLiteDatabase& operator=(const SQLiteDatabase&) = delete;

    // Movable
    SQLiteDatabase(SQLiteDatabase&&) noexcept;
    SQLiteDatabase& operator=(SQLiteDatabase&&) noexcept;

    /// @name Database interface implementation
    /// @{

    QueryResult execute(
        const std::string& sql,
        const std::vector<nlohmann::json>& params = {}
    ) override;

    std::optional<nlohmann::json> execute_one(
        const std::string& sql,
        const std::vector<nlohmann::json>& params = {}
    ) override;

    std::shared_ptr<Transaction> begin_transaction() override;

    QueryResult execute_batch(
        const std::string& sql,
        const std::vector<std::vector<nlohmann::json>>& params_list
    ) override;

    void migrate(
        const std::string& name,
        const std::string& sql
    ) override;

    bool health_check() override;

    const DatabaseConfig& config() const override { return config_; }

    void close() override;

    bool is_open() const override;

    /// @}

    /// Get the raw SQLite database handle
    /// @return SQLite database pointer
    sqlite3* handle() const { return db_; }

    /// Check if the database is still valid (not closed/destroyed)
    /// @return true if database is valid
    bool is_valid() const { return db_ != nullptr; }

private:
    /// Bind a parameter to a statement
    void bind_param(sqlite3_stmt* stmt, int index, const nlohmann::json& value);

    /// Convert a result row to JSON
    nlohmann::json row_to_json(sqlite3_stmt* stmt);

    /// Setup database pragmas and optimizations
    void setup_pragmas();

    /// Create migrations table if not exists
    void ensure_migrations_table();

    DatabaseConfig config_;
    sqlite3* db_ = nullptr;
    std::mutex mutex_;
    
    // Track if database is being destroyed
    std::shared_ptr<bool> alive_flag_ = std::make_shared<bool>(true);
};

/// SQLite implementation of the Transaction interface
class TURBOT_STORAGE_API SQLiteTransaction : public Transaction {
public:
    /// Construct a transaction for the given database
    /// @param db SQLite database pointer
    /// @param alive_flag Shared flag to track database lifetime
    explicit SQLiteTransaction(sqlite3* db, std::shared_ptr<bool> alive_flag);
    ~SQLiteTransaction() override;

    // Non-copyable, non-movable
    SQLiteTransaction(const SQLiteTransaction&) = delete;
    SQLiteTransaction& operator=(const SQLiteTransaction&) = delete;
    SQLiteTransaction(SQLiteTransaction&&) = delete;
    SQLiteTransaction& operator=(SQLiteTransaction&&) = delete;

    /// @name Transaction interface implementation
    /// @{

    void commit() override;
    void rollback() override;

    QueryResult execute(
        const std::string& sql,
        const std::vector<nlohmann::json>& params = {}
    ) override;

    std::optional<nlohmann::json> execute_one(
        const std::string& sql,
        const std::vector<nlohmann::json>& params = {}
    ) override;

    bool is_active() const override { return active_ && *alive_flag_; }

    /// @}

private:
    /// Bind a parameter to a statement
    void bind_param(sqlite3_stmt* stmt, int index, const nlohmann::json& value);

    /// Convert a result row to JSON
    nlohmann::json row_to_json(sqlite3_stmt* stmt);

    /// Check if the database is still alive
    bool is_db_alive() const { return alive_flag_ && *alive_flag_; }

    sqlite3* db_;
    std::shared_ptr<bool> alive_flag_;
    std::mutex mutex_;
};

} // namespace turbot::storage::sqlite
