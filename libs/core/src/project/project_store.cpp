#include <turbot/core/project/project_store.hpp>
#include <turbot/core/project/project.hpp>
#include <turbot/core/common/logger.hpp>
#include <stdexcept>

namespace turbot::core::project {

ProjectStore& ProjectStore::instance() {
    static ProjectStore inst;
    return inst;
}

void ProjectStore::init(std::shared_ptr<turbot::storage::Database> db) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) return;  // First call wins.
    db_ = std::move(db);
    ensure_schema();
}

bool ProjectStore::is_initialized() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return db_ != nullptr;
}

void ProjectStore::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    db_.reset();
    schema_ready_ = false;
}

// ─── ensure_schema ────────────────────────────────────────────────────────────

void ProjectStore::ensure_schema() {
    // Called with mutex_ held and db_ valid.
    if (schema_ready_) return;

    // project table — aligned with OpenCode project SQLite schema.
    db_->execute(R"SQL(
        CREATE TABLE IF NOT EXISTS project (
            id               TEXT    PRIMARY KEY NOT NULL,
            worktree         TEXT    NOT NULL,
            vcs              TEXT    NOT NULL DEFAULT 'none',
            name             TEXT,
            icon_url         TEXT,
            icon_color       TEXT,
            time_created     INTEGER NOT NULL,
            time_updated     INTEGER NOT NULL,
            time_initialized INTEGER,
            sandboxes        TEXT,   -- JSON array
            commands         TEXT    -- JSON object
        )
    )SQL");

    db_->execute(R"SQL(
        CREATE INDEX IF NOT EXISTS idx_project_worktree ON project(worktree)
    )SQL");

    // ALTER TABLE for existing DBs: add columns if they don't exist yet.
    static const char* kAlterStmts[] = {
        "ALTER TABLE project ADD COLUMN icon_url TEXT",
        "ALTER TABLE project ADD COLUMN icon_color TEXT",
        "ALTER TABLE project ADD COLUMN time_initialized INTEGER",
        "ALTER TABLE project ADD COLUMN sandboxes TEXT",
        "ALTER TABLE project ADD COLUMN commands TEXT",
        nullptr
    };
    for (int i = 0; kAlterStmts[i]; ++i) {
        try { db_->execute(kAlterStmts[i]); }
        catch (const std::exception&) { /* column already exists — safe to ignore */ }
    }

    schema_ready_ = true;
}

// ─── save ─────────────────────────────────────────────────────────────────────

bool ProjectStore::save(const ProjectInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    try {
        nlohmann::json name_j     = info.name ? nlohmann::json(*info.name) : nlohmann::json(nullptr);
        nlohmann::json icon_url_j = (info.icon && info.icon->url)
            ? nlohmann::json(*info.icon->url) : nlohmann::json(nullptr);
        nlohmann::json icon_col_j = (info.icon && info.icon->color)
            ? nlohmann::json(*info.icon->color) : nlohmann::json(nullptr);
        nlohmann::json time_init_j = info.time.initialized
            ? nlohmann::json(*info.time.initialized) : nlohmann::json(nullptr);
        // sandboxes: JSON array string
        nlohmann::json sandboxes_j = info.sandboxes.empty()
            ? nlohmann::json(nullptr)
            : nlohmann::json(nlohmann::json(info.sandboxes).dump());
        // commands: JSON object string
        nlohmann::json commands_j = info.commands
            ? nlohmann::json(info.commands->to_json().dump())
            : nlohmann::json(nullptr);

        db_->execute(R"SQL(
            INSERT INTO project
                (id, worktree, vcs, name, icon_url, icon_color,
                 time_created, time_updated, time_initialized,
                 sandboxes, commands)
            VALUES (?,?,?,?,?,?,?,?,?,?,?)
            ON CONFLICT(id) DO UPDATE SET
                worktree         = excluded.worktree,
                vcs              = excluded.vcs,
                name             = excluded.name,
                icon_url         = excluded.icon_url,
                icon_color       = excluded.icon_color,
                time_updated     = excluded.time_updated,
                time_initialized = excluded.time_initialized,
                sandboxes        = excluded.sandboxes,
                commands         = excluded.commands
        )SQL", {
            info.id,
            info.worktree,
            vcs_type_to_string(info.vcs),
            name_j,
            icon_url_j,
            icon_col_j,
            info.time.created,
            info.time.updated,
            time_init_j,
            sandboxes_j,
            commands_j
        });
        return true;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("ProjectStore::save failed for project {}: {}", info.id, e.what());
        return false;
    }
}

// ─── find_by_id ───────────────────────────────────────────────────────────────

std::optional<nlohmann::json> ProjectStore::find_by_id(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return std::nullopt;

    try {
        return db_->execute_one("SELECT * FROM project WHERE id = ?", {id});
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("ProjectStore::find_by_id failed for {}: {}", id, e.what());
        return std::nullopt;
    }
}

// ─── find_all ─────────────────────────────────────────────────────────────────

std::vector<nlohmann::json> ProjectStore::find_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return {};

    try {
        auto result = db_->execute(
            "SELECT * FROM project ORDER BY time_updated DESC");
        return result.rows;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("ProjectStore::find_all failed: {}", e.what());
        return {};
    }
}

// ─── remove ───────────────────────────────────────────────────────────────────

bool ProjectStore::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    try {
        auto result = db_->execute(
            "DELETE FROM project WHERE id = ?", {id});
        return result.affected_rows > 0;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("ProjectStore::remove failed for {}: {}", id, e.what());
        return false;
    }
}

// ─── migrate_sessions ─────────────────────────────────────────────────────────

int ProjectStore::migrate_sessions(const std::string& project_id,
                                   const std::string& worktree) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return -1;

    // Re-assign sessions that were created under the 'global' project
    // but belong to this worktree directory.  Aligned with OpenCode
    // Project.fromDirectory() which migrates legacy global sessions.
    try {
        auto result = db_->execute(
            "UPDATE sessions SET project_id = ? "
            "WHERE project_id = 'global' AND directory = ?",
            {project_id, worktree});
        return static_cast<int>(result.affected_rows);
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("ProjectStore::migrate_sessions failed "
                         "(project={} worktree={}): {}",
                         project_id, worktree, e.what());
        return -1;
    }
}

} // namespace turbot::core::project
