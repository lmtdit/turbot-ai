#include <turbot/storage/migration.hpp>
#include <turbot/storage/database.hpp>
#include <turbot/core/common/logger.hpp>

#include <algorithm>
#include <chrono>

namespace turbot::storage {

MigrationRunner::MigrationRunner(std::shared_ptr<Database> db)
    : db_(std::move(db)) {
    if (!db_) {
        throw std::runtime_error("MigrationRunner requires a valid database");
    }
}

void MigrationRunner::add_migration(std::unique_ptr<Migration> migration) {
    if (!migration) {
        throw std::runtime_error("Cannot add null migration");
    }
    migrations_.push_back(std::move(migration));

    // Sort migrations by version
    std::sort(migrations_.begin(), migrations_.end(),
              [](const std::unique_ptr<Migration>& a,
                 const std::unique_ptr<Migration>& b) {
                  return a->version() < b->version();
              });
}

void MigrationRunner::create_migration_table() {
    // The _migrations table should already exist from database initialization
    // But we ensure it here as well
    db_->execute(R"(
        CREATE TABLE IF NOT EXISTS _migrations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            version INTEGER NOT NULL,
            executed_at INTEGER NOT NULL
        )
    )");
}

void MigrationRunner::record_migration(const Migration& migration) {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    db_->execute(
        "INSERT INTO _migrations (name, version, executed_at) VALUES (?, ?, ?)",
        {nlohmann::json(migration.name()),
         nlohmann::json(migration.version()),
         nlohmann::json(now)});
}

void MigrationRunner::remove_migration(const std::string& name) {
    db_->execute(
        "DELETE FROM _migrations WHERE name = ?",
        {nlohmann::json(name)});
}

void MigrationRunner::run() {
    create_migration_table();

    auto executed = get_executed();
    size_t pending_count = 0;

    for (const auto& migration : migrations_) {
        // Check if already executed
        if (std::find(executed.begin(), executed.end(), migration->name()) != executed.end()) {
            TURBOT_LOG_DEBUG("Migration '{}' already applied", migration->name());
            continue;
        }

        TURBOT_LOG_INFO("Running migration: {} (v{})", migration->name(), migration->version());

        try {
            db_->migrate(migration->name(), migration->up());
            // record_migration is already called inside db_->migrate()
            pending_count++;
        } catch (const std::exception& e) {
            TURBOT_LOG_ERROR("Migration '{}' failed: {}", migration->name(), e.what());
            throw;
        }
    }

    if (pending_count > 0) {
        TURBOT_LOG_INFO("Applied {} migration(s)", pending_count);
    } else {
        TURBOT_LOG_INFO("No pending migrations");
    }
}

void MigrationRunner::rollback(int steps) {
    if (steps <= 0) {
        throw std::runtime_error("Rollback steps must be positive");
    }

    auto executed = get_executed();

    // Reverse order for rollback
    for (int i = 0; i < steps && !executed.empty(); i++) {
        std::string migration_name = executed.back();

        // Find the migration
        auto it = std::find_if(migrations_.begin(), migrations_.end(),
                               [&migration_name](const std::unique_ptr<Migration>& m) {
                                   return m->name() == migration_name;
                               });

        if (it == migrations_.end()) {
            TURBOT_LOG_WARN("Migration '{}' not found in registered migrations", migration_name);
            executed.pop_back();
            continue;
        }

        const auto& migration = *it;
        TURBOT_LOG_INFO("Rolling back migration: {}", migration_name);

        try {
            db_->execute(migration->down());
            remove_migration(migration_name);
            executed.pop_back();
        } catch (const std::exception& e) {
            TURBOT_LOG_ERROR("Rollback of migration '{}' failed: {}", migration_name, e.what());
            throw;
        }
    }
}

std::vector<std::string> MigrationRunner::get_pending() {
    auto executed = get_executed();
    std::vector<std::string> pending;

    for (const auto& migration : migrations_) {
        if (std::find(executed.begin(), executed.end(), migration->name()) == executed.end()) {
            pending.push_back(migration->name());
        }
    }

    return pending;
}

std::vector<std::string> MigrationRunner::get_executed() {
    std::vector<std::string> executed;

    auto result = db_->execute(
        "SELECT name FROM _migrations ORDER BY version ASC");

    for (const auto& row : result.rows) {
        if (row.contains("name")) {
            executed.push_back(row["name"].get<std::string>());
        }
    }

    return executed;
}

bool MigrationRunner::validate() {
    auto executed = get_executed();

    // Check that all executed migrations are registered
    for (const auto& name : executed) {
        auto it = std::find_if(migrations_.begin(), migrations_.end(),
                               [&name](const std::unique_ptr<Migration>& m) {
                                   return m->name() == name;
                               });

        if (it == migrations_.end()) {
            TURBOT_LOG_WARN("Executed migration '{}' not found in registered migrations", name);
            return false;
        }
    }

    // Check that all registered migrations with version <= max_executed are executed
    if (!executed.empty()) {
        int max_version = 0;
        for (const auto& migration : migrations_) {
            if (std::find(executed.begin(), executed.end(), migration->name()) != executed.end()) {
                max_version = std::max(max_version, migration->version());
            }
        }

        for (const auto& migration : migrations_) {
            if (migration->version() <= max_version) {
                if (std::find(executed.begin(), executed.end(), migration->name()) == executed.end()) {
                    TURBOT_LOG_WARN("Migration '{}' (v{}) should have been executed but wasn't",
                                    migration->name(), migration->version());
                    return false;
                }
            }
        }
    }

    TURBOT_LOG_INFO("Migration state is valid");
    return true;
}

} // namespace turbot::storage
