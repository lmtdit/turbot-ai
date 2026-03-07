#pragma once

#include <turbot/storage/export.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace turbot::storage {

// Forward declarations
class Database;

/// Migration interface for database schema migrations
class TURBOT_STORAGE_API Migration {
public:
    virtual ~Migration() = default;

    /// Get the migration name
    /// @return Migration name
    virtual std::string name() const = 0;

    /// Get the SQL to apply the migration
    /// @return SQL string
    virtual std::string up() const = 0;

    /// Get the SQL to rollback the migration
    /// @return SQL string
    virtual std::string down() const = 0;

    /// Get the migration version number
    /// @return Version number
    virtual int version() const = 0;
};

/// Migration runner for managing database migrations
class TURBOT_STORAGE_API MigrationRunner {
public:
    /// Construct a migration runner for the given database
    /// @param db Database to run migrations on
    explicit MigrationRunner(std::shared_ptr<Database> db);

    /// Add a migration to be managed
    /// @param migration Migration to add
    void add_migration(std::unique_ptr<Migration> migration);

    /// Run all pending migrations
    void run();

    /// Rollback the last N migrations
    /// @param steps Number of migrations to rollback
    void rollback(int steps = 1);

    /// Get list of pending migrations
    /// @return List of pending migration names
    std::vector<std::string> get_pending();

    /// Get list of already executed migrations
    /// @return List of executed migration names
    std::vector<std::string> get_executed();

    /// Validate migration state
    /// @return true if all migrations are in sync
    bool validate();

private:
    /// Create the migrations tracking table
    void create_migration_table();

    /// Record a migration as executed
    /// @param migration Migration to record
    void record_migration(const Migration& migration);

    /// Remove a migration record
    /// @param name Migration name to remove
    void remove_migration(const std::string& name);

    /// Ensure migrations are sorted by version (lazy sort)
    void ensure_sorted();

    std::shared_ptr<Database> db_;
    std::vector<std::unique_ptr<Migration>> migrations_;
    bool migrations_sorted_ = false;  ///< Track if migrations_ is sorted
};

/// Helper macro for creating simple migrations
#define TURBOT_MIGRATION(Name, Version, UpSQL, DownSQL) \
    class Name##Migration : public turbot::storage::Migration { \
    public: \
        std::string name() const override { return #Name; } \
        std::string up() const override { return UpSQL; } \
        std::string down() const override { return DownSQL; } \
        int version() const override { return Version; } \
    }

} // namespace turbot::storage
