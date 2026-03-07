#pragma once

#include <turbot/storage/export.hpp>

#include <nlohmann/json.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <functional>

namespace turbot::storage {

// Forward declaration
struct QueryResult;

/// Abstract transaction interface
class TURBOT_STORAGE_API Transaction {
public:
    virtual ~Transaction() = default;

    /// Commit the transaction
    virtual void commit() = 0;

    /// Rollback the transaction
    virtual void rollback() = 0;

    /// Execute a SQL query within the transaction
    /// @param sql SQL query string
    /// @param params Parameters to bind
    /// @return Query result
    virtual QueryResult execute(
        const std::string& sql,
        const std::vector<nlohmann::json>& params = {}
    ) = 0;

    /// Execute a SQL query and return a single row
    /// @param sql SQL query string
    /// @param params Parameters to bind
    /// @return Single row result or nullopt if no rows
    virtual std::optional<nlohmann::json> execute_one(
        const std::string& sql,
        const std::vector<nlohmann::json>& params = {}
    ) = 0;

    /// Check if the transaction is still active
    /// @return true if transaction is active
    virtual bool is_active() const = 0;

protected:
    bool active_ = true;
};

/// RAII transaction guard for automatic rollback on scope exit
class TURBOT_STORAGE_API TransactionGuard {
public:
    explicit TransactionGuard(std::shared_ptr<Transaction> tx);
    ~TransactionGuard();

    // Move only, no copy
    TransactionGuard(const TransactionGuard&) = delete;
    TransactionGuard& operator=(const TransactionGuard&) = delete;
    TransactionGuard(TransactionGuard&&) noexcept;
    TransactionGuard& operator=(TransactionGuard&&) noexcept;

    /// Commit the transaction
    void commit();

    /// Rollback the transaction
    void rollback();

    /// Access the underlying transaction
    Transaction* operator->();
    Transaction& operator*();

    /// Check if the guard holds a valid transaction
    explicit operator bool() const;

    /// Release ownership without rollback
    std::shared_ptr<Transaction> release();

private:
    std::shared_ptr<Transaction> tx_;
};

} // namespace turbot::storage
