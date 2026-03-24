#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/storage/database.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace turbot::core::project {

struct ProjectInfo;

/// Singleton store for project persistence (SQLite backend).
///
/// Aligned with OpenCode's project SQLite schema (project table).
/// Shared the same Database instance as SessionStore so cross-table
/// operations (e.g. session migration) work within a single SQLite file.
class TURBOT_CORE_API ProjectStore {
public:
    static ProjectStore& instance();

    /// Initialise the store with a database.  First call wins.
    void init(std::shared_ptr<turbot::storage::Database> db);

    /// Returns true if a database has been provided.
    [[nodiscard]] bool is_initialized() const noexcept;

    /// Reset the store (for testing).
    void reset();

    // ── CRUD ──────────────────────────────────────────────────────────────

    /// Persist a project (INSERT OR REPLACE / UPSERT).
    [[nodiscard]] bool save(const ProjectInfo& info);

    /// Find a project by ID.  Returns nullopt if not found or not initialised.
    [[nodiscard]] std::optional<nlohmann::json> find_by_id(const std::string& id);

    /// List all projects ordered by time_updated DESC.
    [[nodiscard]] std::vector<nlohmann::json> find_all();

    /// Delete a project by ID.
    bool remove(const std::string& id);

    // ── Migration ─────────────────────────────────────────────────────────

    /// Re-assign orphaned sessions from 'global' project to a concrete project.
    ///
    /// Executes:
    ///   UPDATE sessions SET project_id = ?
    ///   WHERE project_id = 'global' AND directory = ?
    ///
    /// Returns the number of sessions migrated, or -1 on error.
    int migrate_sessions(const std::string& project_id, const std::string& worktree);

private:
    ProjectStore() = default;

    void ensure_schema();   // Called with mutex_ held and db_ valid.

    mutable std::mutex mutex_;
    std::shared_ptr<turbot::storage::Database> db_;
    bool schema_ready_ = false;
};

} // namespace turbot::core::project
