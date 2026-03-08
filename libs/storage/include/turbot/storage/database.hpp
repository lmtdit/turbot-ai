#pragma once

#include <turbot/storage/export.hpp>
#include <turbot/storage/transaction.hpp>

#include <nlohmann/json.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace turbot::storage {

/// Result of a database query
struct TURBOT_STORAGE_API QueryResult {
    std::vector<nlohmann::json> rows;
    int64_t affected_rows = 0;
};

/// Database configuration options
struct TURBOT_STORAGE_API DatabaseConfig {
    std::string path;              ///< Database file path (":memory:" for in-memory)
    bool read_only = false;        ///< Open in read-only mode
    int timeout = 30;              ///< Query timeout in seconds
    int cache_size = -2000;        ///< Cache size in KB (negative = N*1024)
    bool journal_wal = true;       ///< Use WAL journal mode
    bool foreign_keys = true;      ///< Enable foreign key constraints
};

/// Abstract database interface
class TURBOT_STORAGE_API Database {
public:
    virtual ~Database() = default;

    /// Execute a SQL query with parameters
    /// @param sql SQL query string
    /// @param params Parameters to bind
    /// @return Query result with rows and affected row count
    [[nodiscard]] virtual QueryResult execute(
        const std::string& sql,
        const std::vector<nlohmann::json>& params = {}
    ) = 0;

    /// Execute a SQL query and return a single row
    /// @param sql SQL query string
    /// @param params Parameters to bind
    /// @return Single row result or nullopt if no rows
    [[nodiscard]] virtual std::optional<nlohmann::json> execute_one(
        const std::string& sql,
        const std::vector<nlohmann::json>& params = {}
    ) = 0;

    /// Execute a SQL query and return a scalar value
    /// @tparam T Type of the scalar value
    /// @param sql SQL query string
    /// @param params Parameters to bind
    /// @return Scalar value or nullopt if no result
    template<typename T>
    [[nodiscard]] std::optional<T> execute_scalar(
        const std::string& sql,
        const std::vector<nlohmann::json>& params = {}
    ) {
        auto result = execute_one(sql, params);
        if (!result || result->empty()) {
            return std::nullopt;
        }
        return result->begin().value().template get<T>();
    }

    /// Begin a new transaction
    /// @return Transaction object
    [[nodiscard]] virtual std::shared_ptr<Transaction> begin_transaction() = 0;

    /// Execute a batch of statements
    /// @param sql SQL statement
    /// @param params_list List of parameter sets
    /// @return Combined result
    [[nodiscard]] virtual QueryResult execute_batch(
        const std::string& sql,
        const std::vector<std::vector<nlohmann::json>>& params_list
    ) = 0;

    /// Run a migration
    /// @param name Migration name
    /// @param sql SQL to execute
    /// @param version Migration version number (used for ordering and recording)
    virtual void migrate(
        const std::string& name,
        const std::string& sql,
        int version = 1
    ) = 0;

    /// Check database health
    /// @return true if database is healthy
    [[nodiscard]] virtual bool health_check() = 0;

    /// Get the database configuration
    /// @return Configuration reference
    [[nodiscard]] virtual const DatabaseConfig& config() const = 0;

    /// Close the database connection
    virtual void close() = 0;

    /// Check if database is open
    [[nodiscard]] virtual bool is_open() const = 0;
};

} // namespace turbot::storage
