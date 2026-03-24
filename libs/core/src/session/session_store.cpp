#include <turbot/core/session/session_store.hpp>
#include <turbot/core/session/session.hpp>
#include <turbot/core/common/logger.hpp>
#include <stdexcept>

namespace turbot::core::session {

SessionStore& SessionStore::instance() {
    static SessionStore inst;
    return inst;
}

void SessionStore::init(std::shared_ptr<storage::Database> db) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) return;  // Already initialised — first call wins
    db_ = std::move(db);
    ensure_schema();
}

bool SessionStore::is_initialized() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return db_ != nullptr;
}

void SessionStore::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    db_.reset();
    schema_ready_ = false;
}

void SessionStore::ensure_schema() {
    // Called with mutex_ held and db_ valid.
    if (schema_ready_) return;

    // sessions table (matches storage/migrations/migrations.hpp v1+v2)
    db_->execute(R"SQL(
        CREATE TABLE IF NOT EXISTS sessions (
            id              TEXT    PRIMARY KEY NOT NULL,
            project_id      TEXT    NOT NULL,
            parent_id       TEXT,
            slug            TEXT    NOT NULL,
            directory       TEXT    NOT NULL,
            title           TEXT    NOT NULL DEFAULT '',
            version         TEXT    NOT NULL DEFAULT '1.0.0',
            permission      TEXT,
            state           TEXT    NOT NULL DEFAULT 'created',
            revert          TEXT,
            time_created    INTEGER NOT NULL,
            time_updated    INTEGER NOT NULL,
            time_compacting INTEGER,
            time_archived   INTEGER
        )
    )SQL");
    db_->execute(R"SQL(
        CREATE INDEX IF NOT EXISTS idx_sessions_project_id ON sessions(project_id)
    )SQL");

    // messages table
    db_->execute(R"SQL(
        CREATE TABLE IF NOT EXISTS session_messages (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            session_id  TEXT    NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
            seq         INTEGER NOT NULL,
            data        TEXT    NOT NULL,
            created_at  INTEGER NOT NULL
        )
    )SQL");
    db_->execute(R"SQL(
        CREATE INDEX IF NOT EXISTS idx_session_messages_session
            ON session_messages(session_id, seq)
    )SQL");

    schema_ready_ = true;
}

// ─── save ─────────────────────────────────────────────────────────────────────

bool SessionStore::save(const SessionInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    try {
        // Serialise optional fields to JSON text or null
        nlohmann::json parent_id_j  = info.parent_id  ? nlohmann::json(*info.parent_id)                 : nlohmann::json(nullptr);
        nlohmann::json permission_j = info.permission  ? nlohmann::json(info.permission->dump())         : nlohmann::json(nullptr);
        nlohmann::json revert_j     = info.revert      ? nlohmann::json(info.revert->to_json().dump())   : nlohmann::json(nullptr);
        nlohmann::json time_compact_j = info.time_compacting ? nlohmann::json(*info.time_compacting) : nlohmann::json(nullptr);
        nlohmann::json time_arch_j    = info.time_archived   ? nlohmann::json(*info.time_archived)   : nlohmann::json(nullptr);

        db_->execute(R"SQL(
            INSERT INTO sessions
                (id, project_id, parent_id, slug, directory, title, version,
                 permission, state, revert, time_created, time_updated,
                 time_compacting, time_archived)
            VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)
            ON CONFLICT(id) DO UPDATE SET
                project_id      = excluded.project_id,
                parent_id       = excluded.parent_id,
                slug            = excluded.slug,
                directory       = excluded.directory,
                title           = excluded.title,
                version         = excluded.version,
                permission      = excluded.permission,
                state           = excluded.state,
                revert          = excluded.revert,
                time_updated    = excluded.time_updated,
                time_compacting = excluded.time_compacting,
                time_archived   = excluded.time_archived
        )SQL", {
            info.id,
            info.project_id,
            parent_id_j,
            info.slug,
            info.directory,
            info.title,
            info.version,
            permission_j,
            session_state_to_string(info.state),
            revert_j,
            info.time_created,
            info.time_updated,
            time_compact_j,
            time_arch_j
        });
        return true;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionStore::save failed for session {}: {}", info.id, e.what());
        return false;
    }
}

// ─── find_by_id ──────────────────────────────────────────────────────────────

std::optional<nlohmann::json> SessionStore::find_by_id(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return std::nullopt;

    try {
        return db_->execute_one(
            "SELECT * FROM sessions WHERE id = ?",
            {id}
        );
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionStore::find_by_id failed for {}: {}", id, e.what());
        return std::nullopt;
    }
}

// ─── find_all ─────────────────────────────────────────────────────────────────

std::vector<nlohmann::json> SessionStore::find_all(const std::string& project_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return {};

    try {
        auto result = db_->execute(
            "SELECT * FROM sessions WHERE project_id = ? ORDER BY time_created DESC",
            {project_id}
        );
        return result.rows;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionStore::find_all failed for project {}: {}", project_id, e.what());
        return {};
    }
}

// ─── find_all_paginated ──────────────────────────────────────────────────────────

std::pair<std::vector<nlohmann::json>, std::optional<std::string>>
SessionStore::find_all_paginated(
    const std::string& project_id,
    int limit,
    const std::optional<std::string>& cursor
) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return {{}, std::nullopt};

    try {
        std::vector<nlohmann::json> rows;
        
        if (cursor) {
            // Cursor-based: get rows with time_updated < cursor
            auto result = db_->execute(
                "SELECT * FROM sessions WHERE project_id = ? AND time_updated < ? "
                "ORDER BY time_updated DESC LIMIT ?",
                {project_id, std::stoll(*cursor), limit}
            );
            rows = std::move(result.rows);
        } else {
            // First page: get most recent rows
            auto result = db_->execute(
                "SELECT * FROM sessions WHERE project_id = ? "
                "ORDER BY time_updated DESC LIMIT ?",
                {project_id, limit}
            );
            rows = std::move(result.rows);
        }
        
        // Determine next cursor
        std::optional<std::string> next_cursor;
        if (rows.size() == static_cast<size_t>(limit)) {
            // There might be more rows - use the last row's time_updated as cursor
            const auto& last_row = rows.back();
            if (last_row.contains("time_updated") && last_row["time_updated"].is_number()) {
                next_cursor = std::to_string(last_row["time_updated"].get<int64_t>());
            }
        }
        
        return {std::move(rows), next_cursor};
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionStore::find_all_paginated failed for project {}: {}", project_id, e.what());
        return {{}, std::nullopt};
    }
}

// ─── remove ───────────────────────────────────────────────────────────────────

bool SessionStore::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    try {
        auto result = db_->execute(
            "DELETE FROM sessions WHERE id = ?",
            {id}
        );
        return result.affected_rows > 0;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionStore::remove failed for {}: {}", id, e.what());
        return false;
    }
}

// ─── save_message ─────────────────────────────────────────────────────────────

bool SessionStore::save_message(const std::string& session_id, const nlohmann::json& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    try {
        // Determine next seq and insert atomically using a single SQL statement.
        // The subquery prevents TOCTOU between a separate MAX SELECT and INSERT.
        // created_at: use message's time_created if present, else now
        int64_t created_at = 0;
        if (message.contains("time_created") && message["time_created"].is_number()) {
            created_at = message["time_created"].get<int64_t>();
        } else {
            created_at = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        }

        db_->execute(
            R"SQL(
                INSERT INTO session_messages (session_id, seq, data, created_at)
                SELECT ?, COALESCE((SELECT MAX(seq) FROM session_messages WHERE session_id = ?), -1) + 1, ?, ?
            )SQL",
            {session_id, session_id, message.dump(), created_at}
        );
        return true;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionStore::save_message failed for session {}: {}", session_id, e.what());
        return false;
    }
}

// ─── list_messages ────────────────────────────────────────────────────────────

std::vector<nlohmann::json> SessionStore::list_messages(
    const std::string& session_id,
    int limit,
    int offset
) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return {};

    try {
        auto result = db_->execute(
            "SELECT data FROM session_messages WHERE session_id = ? ORDER BY seq ASC LIMIT ? OFFSET ?",
            {session_id, limit, offset}
        );

        std::vector<nlohmann::json> messages;
        messages.reserve(result.rows.size());
        for (const auto& row : result.rows) {
            if (row.contains("data") && row["data"].is_string()) {
                try {
                    messages.push_back(nlohmann::json::parse(row["data"].get<std::string>()));
                } catch (const nlohmann::json::parse_error& e) {
                    TURBOT_LOG_WARN("SessionStore: skipping corrupt message row for session {}: parse error: {}", session_id, e.what());
                } catch (const std::exception& e) {
                    TURBOT_LOG_WARN("SessionStore: skipping corrupt message row for session {}: {}", session_id, e.what());
                } catch (...) {
                    TURBOT_LOG_WARN("SessionStore: skipping corrupt message row for session {}: unknown error", session_id);
                }
            }
        }
        return messages;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionStore::list_messages failed for session {}: {}", session_id, e.what());
        return {};
    }
}

// ─── list_messages_paginated ────────────────────────────────────────────────────

std::pair<std::vector<nlohmann::json>, std::optional<int64_t>>
SessionStore::list_messages_paginated(
    const std::string& session_id,
    int limit,
    std::optional<int64_t> cursor
) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return {{}, std::nullopt};

    try {
        std::vector<nlohmann::json> messages;
        int64_t last_seq = -1;
        
        if (cursor) {
            // Cursor-based: get rows with seq > cursor
            auto result = db_->execute(
                "SELECT seq, data FROM session_messages WHERE session_id = ? AND seq > ? "
                "ORDER BY seq ASC LIMIT ?",
                {session_id, *cursor, limit}
            );
            
            messages.reserve(result.rows.size());
            for (const auto& row : result.rows) {
                if (row.contains("data") && row["data"].is_string() &&
                    row.contains("seq") && row["seq"].is_number()) {
                    try {
                        messages.push_back(nlohmann::json::parse(row["data"].get<std::string>()));
                        last_seq = row["seq"].get<int64_t>();
                    } catch (const nlohmann::json::parse_error& e) {
                        TURBOT_LOG_WARN("SessionStore: skipping corrupt message row for session {}: parse error: {}", session_id, e.what());
                    } catch (const std::exception& e) {
                        TURBOT_LOG_WARN("SessionStore: skipping corrupt message row for session {}: {}", session_id, e.what());
                    } catch (...) {
                        TURBOT_LOG_WARN("SessionStore: skipping corrupt message row for session {}: unknown error", session_id);
                    }
                }
            }
        } else {
            // First page: get earliest rows
            auto result = db_->execute(
                "SELECT seq, data FROM session_messages WHERE session_id = ? "
                "ORDER BY seq ASC LIMIT ?",
                {session_id, limit}
            );
            
            messages.reserve(result.rows.size());
            for (const auto& row : result.rows) {
                if (row.contains("data") && row["data"].is_string() &&
                    row.contains("seq") && row["seq"].is_number()) {
                    try {
                        messages.push_back(nlohmann::json::parse(row["data"].get<std::string>()));
                        last_seq = row["seq"].get<int64_t>();
                    } catch (const nlohmann::json::parse_error& e) {
                        TURBOT_LOG_WARN("SessionStore: skipping corrupt message row for session {}: parse error: {}", session_id, e.what());
                    } catch (const std::exception& e) {
                        TURBOT_LOG_WARN("SessionStore: skipping corrupt message row for session {}: {}", session_id, e.what());
                    } catch (...) {
                        TURBOT_LOG_WARN("SessionStore: skipping corrupt message row for session {}: unknown error", session_id);
                    }
                }
            }
        }
        
        // Return next cursor if there might be more rows
        std::optional<int64_t> next_cursor;
        if (messages.size() == static_cast<size_t>(limit) && last_seq >= 0) {
            next_cursor = last_seq;
        }
        
        return {std::move(messages), next_cursor};
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionStore::list_messages_paginated failed for session {}: {}", session_id, e.what());
        return {{}, std::nullopt};
    }
}

} // namespace turbot::core::session
