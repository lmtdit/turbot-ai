#include <turbot/core/session/session_store.hpp>
#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_todo.hpp>
#include <turbot/core/common/logger.hpp>
#include <chrono>
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

    // sessions table — aligned with OpenCode session.sql.ts schema.
    // Includes the 6 columns that were previously missing:
    //   workspace_id, share_url, summary_additions, summary_deletions,
    //   summary_files, summary_diffs
    db_->execute(R"SQL(
        CREATE TABLE IF NOT EXISTS sessions (
            id                  TEXT    PRIMARY KEY NOT NULL,
            project_id          TEXT    NOT NULL,
            workspace_id        TEXT,
            parent_id           TEXT,
            slug                TEXT    NOT NULL,
            directory           TEXT    NOT NULL,
            title               TEXT    NOT NULL DEFAULT '',
            version             TEXT    NOT NULL DEFAULT '1.0.0',
            share_url           TEXT,
            summary_additions   INTEGER,
            summary_deletions   INTEGER,
            summary_files       INTEGER,
            summary_diffs       TEXT,
            permission          TEXT,
            state               TEXT    NOT NULL DEFAULT 'created',
            revert              TEXT,
            time_created        INTEGER NOT NULL,
            time_updated        INTEGER NOT NULL,
            time_compacting     INTEGER,
            time_archived       INTEGER
        )
    )SQL");
    db_->execute(R"SQL(
        CREATE INDEX IF NOT EXISTS idx_sessions_project_id ON sessions(project_id)
    )SQL");
    db_->execute(R"SQL(
        CREATE INDEX IF NOT EXISTS idx_sessions_workspace_id ON sessions(workspace_id)
    )SQL");
    db_->execute(R"SQL(
        CREATE INDEX IF NOT EXISTS idx_sessions_parent_id ON sessions(parent_id)
    )SQL");
    // ALTER TABLE for existing DBs: add columns if they don't exist yet.
    // SQLite ignores the command if the column already exists (caught via try/catch).
    static const char* kAlterStmts[] = {
        "ALTER TABLE sessions ADD COLUMN workspace_id TEXT",
        "ALTER TABLE sessions ADD COLUMN share_url TEXT",
        "ALTER TABLE sessions ADD COLUMN summary_additions INTEGER",
        "ALTER TABLE sessions ADD COLUMN summary_deletions INTEGER",
        "ALTER TABLE sessions ADD COLUMN summary_files INTEGER",
        "ALTER TABLE sessions ADD COLUMN summary_diffs TEXT",
        nullptr
    };
    for (int i = 0; kAlterStmts[i]; ++i) {
        try { db_->execute(kAlterStmts[i]); }
        catch (const std::exception&) { /* column already exists — safe to ignore */ }
    }

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

    // ── T3: message table (OpenCode-aligned, TEXT PK with msg_ prefix) ──────────
    // Mirrors OpenCode MessageTable in session.sql.ts:
    //   id TEXT PK (msg_xxx), session_id FK, time_created, time_updated, data JSON
    // Turbot extends this with structured columns (role/agent/model_id etc) for
    // fast server-side queries without JSON extraction.
    // Index: (session_id, time_created, id) for ordered retrieval per session.
    db_->execute(R"SQL(
        CREATE TABLE IF NOT EXISTS messages (
            id          TEXT    PRIMARY KEY NOT NULL,
            session_id  TEXT    NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
            role        TEXT    NOT NULL DEFAULT 'user',
            time_created INTEGER NOT NULL,
            time_updated INTEGER NOT NULL,
            parent_id   TEXT,
            agent       TEXT    NOT NULL DEFAULT '',
            model_id    TEXT    NOT NULL DEFAULT '',
            provider_id TEXT    NOT NULL DEFAULT '',
            system      TEXT,
            tools       TEXT,
            variant     TEXT,
            error       TEXT,
            finish      TEXT,
            cost        REAL    NOT NULL DEFAULT 0.0,
            tokens      TEXT,
            summary     INTEGER,
            structured  TEXT
        )
    )SQL");
    db_->execute(R"SQL(
        CREATE INDEX IF NOT EXISTS message_session_time_created_id_idx
            ON messages(session_id, time_created, id)
    )SQL");
    // ALTER TABLE for existing DBs that may already have a messages table from
    // a previous schema version without some columns.
    {
        static const char* kMsgAlterStmts[] = {
            "ALTER TABLE messages ADD COLUMN parent_id TEXT",
            "ALTER TABLE messages ADD COLUMN agent TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE messages ADD COLUMN model_id TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE messages ADD COLUMN provider_id TEXT NOT NULL DEFAULT ''",
            "ALTER TABLE messages ADD COLUMN system TEXT",
            "ALTER TABLE messages ADD COLUMN tools TEXT",
            "ALTER TABLE messages ADD COLUMN variant TEXT",
            "ALTER TABLE messages ADD COLUMN error TEXT",
            "ALTER TABLE messages ADD COLUMN finish TEXT",
            "ALTER TABLE messages ADD COLUMN cost REAL NOT NULL DEFAULT 0.0",
            "ALTER TABLE messages ADD COLUMN tokens TEXT",
            "ALTER TABLE messages ADD COLUMN summary INTEGER",
            "ALTER TABLE messages ADD COLUMN structured TEXT",
            nullptr
        };
        for (int i = 0; kMsgAlterStmts[i]; ++i) {
            try { db_->execute(kMsgAlterStmts[i]); }
            catch (const std::exception&) { /* column already exists — safe to ignore */ }
        }
    }

    // ── T4: part table (OpenCode-aligned, TEXT PK with prt_ prefix) ─────────────
    // Mirrors OpenCode PartTable in session.sql.ts:
    //   id TEXT PK (prt_xxx), message_id FK, session_id, time_created, time_updated, data JSON
    // Turbot also stores `type` TEXT for fast type-based filtering without JSON extraction.
    // Indexes: (message_id, id) and (session_id) for fast lookups.
    db_->execute(R"SQL(
        CREATE TABLE IF NOT EXISTS parts (
            id          TEXT    PRIMARY KEY NOT NULL,
            message_id  TEXT    NOT NULL REFERENCES messages(id) ON DELETE CASCADE,
            session_id  TEXT    NOT NULL,
            type        TEXT    NOT NULL DEFAULT 'text',
            time_created INTEGER NOT NULL,
            time_updated INTEGER NOT NULL,
            data        TEXT    NOT NULL
        )
    )SQL");
    db_->execute(R"SQL(
        CREATE INDEX IF NOT EXISTS part_message_id_id_idx
            ON parts(message_id, id)
    )SQL");
    db_->execute(R"SQL(
        CREATE INDEX IF NOT EXISTS part_session_idx
            ON parts(session_id)
    )SQL");

    // ── T20: todo table (OpenCode-aligned) ───────────────────────────────────
    // Mirrors OpenCode TodoTable in session.sql.ts:
    //   session_id FK, content, status, priority, position, time_created, time_updated
    //   PK: (session_id, position)
    db_->execute(R"SQL(
        CREATE TABLE IF NOT EXISTS todo (
            session_id   TEXT    NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
            content      TEXT    NOT NULL,
            status       TEXT    NOT NULL,
            priority     TEXT    NOT NULL,
            position     INTEGER NOT NULL,
            time_created INTEGER NOT NULL,
            time_updated INTEGER NOT NULL,
            PRIMARY KEY (session_id, position)
        )
    )SQL");
    db_->execute(R"SQL(
        CREATE INDEX IF NOT EXISTS todo_session_idx
            ON todo(session_id)
    )SQL");

    schema_ready_ = true;
}

// ─── save ─────────────────────────────────────────────────────────────────────

bool SessionStore::save(const SessionInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    try {
        // Serialise optional fields to JSON text or null
        nlohmann::json parent_id_j    = info.parent_id    ? nlohmann::json(*info.parent_id)                 : nlohmann::json(nullptr);
        nlohmann::json workspace_id_j = info.workspace_id ? nlohmann::json(*info.workspace_id)              : nlohmann::json(nullptr);
        nlohmann::json permission_j   = info.permission   ? nlohmann::json(info.permission->dump())         : nlohmann::json(nullptr);
        nlohmann::json revert_j       = info.revert       ? nlohmann::json(info.revert->to_json().dump())   : nlohmann::json(nullptr);
        nlohmann::json time_compact_j = info.time_compacting ? nlohmann::json(*info.time_compacting) : nlohmann::json(nullptr);
        nlohmann::json time_arch_j    = info.time_archived   ? nlohmann::json(*info.time_archived)   : nlohmann::json(nullptr);

        // Share URL
        nlohmann::json share_url_j = (info.share && !info.share->url.empty())
            ? nlohmann::json(info.share->url)
            : nlohmann::json(nullptr);

        // Summary columns
        nlohmann::json sum_add_j  = info.summary ? nlohmann::json(info.summary->additions) : nlohmann::json(nullptr);
        nlohmann::json sum_del_j  = info.summary ? nlohmann::json(info.summary->deletions) : nlohmann::json(nullptr);
        nlohmann::json sum_files_j = info.summary ? nlohmann::json(info.summary->files)    : nlohmann::json(nullptr);
        nlohmann::json sum_diffs_j = (info.summary && info.summary->diffs)
            ? nlohmann::json(info.summary->diffs->dump())
            : nlohmann::json(nullptr);

        db_->execute(R"SQL(
            INSERT INTO sessions
                (id, project_id, workspace_id, parent_id, slug, directory, title, version,
                 share_url, summary_additions, summary_deletions, summary_files, summary_diffs,
                 permission, state, revert, time_created, time_updated,
                 time_compacting, time_archived)
            VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
            ON CONFLICT(id) DO UPDATE SET
                project_id        = excluded.project_id,
                workspace_id      = excluded.workspace_id,
                parent_id         = excluded.parent_id,
                slug              = excluded.slug,
                directory         = excluded.directory,
                title             = excluded.title,
                version           = excluded.version,
                share_url         = excluded.share_url,
                summary_additions = excluded.summary_additions,
                summary_deletions = excluded.summary_deletions,
                summary_files     = excluded.summary_files,
                summary_diffs     = excluded.summary_diffs,
                permission        = excluded.permission,
                state             = excluded.state,
                revert            = excluded.revert,
                time_updated      = excluded.time_updated,
                time_compacting   = excluded.time_compacting,
                time_archived     = excluded.time_archived
        )SQL", {
            info.id,
            info.project_id,
            workspace_id_j,
            parent_id_j,
            info.slug,
            info.directory,
            info.title,
            info.version,
            share_url_j,
            sum_add_j,
            sum_del_j,
            sum_files_j,
            sum_diffs_j,
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

std::vector<nlohmann::json> SessionStore::find_all(const ListParams& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return {};

    try {
        // Build a dynamic WHERE clause from the provided filters.
        std::string sql = "SELECT * FROM sessions WHERE 1=1";
        std::vector<nlohmann::json> bind_vals;

        if (params.project_id) {
            sql += " AND project_id = ?";
            bind_vals.push_back(*params.project_id);
        }
        if (params.directory) {
            sql += " AND directory = ?";
            bind_vals.push_back(*params.directory);
        }
        if (params.workspace_id) {
            sql += " AND workspace_id = ?";
            bind_vals.push_back(*params.workspace_id);
        }
        if (params.roots) {
            sql += " AND parent_id IS NULL";
        }
        if (params.start) {
            sql += " AND time_updated >= ?";
            bind_vals.push_back(*params.start);
        }
        if (params.search) {
            sql += " AND title LIKE ?";
            bind_vals.push_back("%" + *params.search + "%");
        }

        sql += " ORDER BY time_updated DESC";

        const int lim = params.limit > 0 ? params.limit : 100;
        sql += " LIMIT ?";
        bind_vals.push_back(lim);

        auto result = db_->execute(sql, bind_vals);
        return result.rows;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionStore::find_all failed: {}", e.what());
        return {};
    }
}

// ─── find_all_paginated ──────────────────────────────────────────────────────────

// ─── find_all_global — G53 ────────────────────────────────────────────────────
// Cross-project session listing mirrors OpenCode Session.listGlobal(input?).
// Joins with the 'project' table to populate { id, name?, worktree } per session.

std::vector<std::pair<nlohmann::json, nlohmann::json>>
SessionStore::find_all_global(
    const std::optional<std::string>& directory,
    bool roots,
    const std::optional<int64_t>& start,
    const std::optional<int64_t>& cursor,
    const std::optional<std::string>& search,
    int limit,
    bool archived
) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return {};

    try {
        // Build a dynamic WHERE clause.
        std::string sql =
            "SELECT s.*, p.name AS _proj_name, p.worktree AS _proj_worktree "
            "FROM sessions s "
            "LEFT JOIN project p ON s.project_id = p.id "
            "WHERE 1=1";

        std::vector<nlohmann::json> bind_vals;

        if (directory) {
            sql += " AND s.directory = ?";
            bind_vals.push_back(*directory);
        }
        if (roots) {
            sql += " AND s.parent_id IS NULL";
        }
        if (start) {
            sql += " AND s.time_updated >= ?";
            bind_vals.push_back(*start);
        }
        if (cursor) {
            sql += " AND s.time_updated < ?";
            bind_vals.push_back(*cursor);
        }
        if (search) {
            sql += " AND s.title LIKE ?";
            bind_vals.push_back("%" + *search + "%");
        }
        if (!archived) {
            sql += " AND s.time_archived IS NULL";
        }

        const int lim = limit > 0 ? limit : 100;
        sql += " ORDER BY s.time_updated DESC, s.id DESC LIMIT ?";
        bind_vals.push_back(lim);

        auto result = db_->execute(sql, bind_vals);

        std::vector<std::pair<nlohmann::json, nlohmann::json>> out;
        out.reserve(result.rows.size());

        for (auto& row : result.rows) {
            // Extract project info from the joined columns before building session JSON.
            nlohmann::json proj;
            proj["id"] = row.value("project_id", std::string{});

            if (row.contains("_proj_name") && !row["_proj_name"].is_null()) {
                proj["name"] = row["_proj_name"];
            }
            if (row.contains("_proj_worktree") && !row["_proj_worktree"].is_null()) {
                proj["worktree"] = row["_proj_worktree"];
            } else {
                proj["worktree"] = "";
            }

            // Remove joined columns before deserialising as SessionInfo.
            row.erase("_proj_name");
            row.erase("_proj_worktree");

            out.emplace_back(row, std::move(proj));
        }

        return out;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionStore::find_all_global failed: {}", e.what());
        return {};
    }
}


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

// ─── delete_messages_by_ids ──────────────────────────────────────────────────

bool SessionStore::delete_messages_by_ids(
    const std::string& session_id,
    const std::vector<std::string>& ids
) {
    if (ids.empty()) return true;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    try {
        // Build a single DELETE with an IN-list for efficiency:
        //   DELETE FROM session_messages
        //   WHERE session_id = ? AND json_extract(data, '$.id') IN (?,?,…)
        //
        // This issues one statement instead of N, and SQLite only needs to scan
        // the session's rows once rather than once-per-id.
        //
        // json_extract is available in SQLite ≥ 3.9 (json1), shipped by default
        // in all modern builds.
        std::string placeholders;
        placeholders.reserve(ids.size() * 2);  // ",?" per element
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) placeholders += ',';
            placeholders += '?';
        }
        const std::string sql =
            "DELETE FROM session_messages "
            "WHERE session_id = ? AND json_extract(data, '$.id') IN (" +
            placeholders + ")";

        std::vector<nlohmann::json> bind_vals;
        bind_vals.reserve(ids.size() + 1);
        bind_vals.push_back(session_id);
        for (const auto& msg_id : ids) {
            bind_vals.push_back(msg_id);
        }

        auto tx = db_->begin_transaction();
        tx->execute(sql, bind_vals);
        tx->commit();
        return true;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR(
            "SessionStore::delete_messages_by_ids failed for session {}: {}",
            session_id, e.what());
        return false;
    }
}

// ─── copy_messages ────────────────────────────────────────────────────────────

int SessionStore::copy_messages(
    const std::string& src_session_id,
    const std::string& dst_session_id,
    std::optional<int64_t> max_seq,
    nlohmann::json* id_map_out
) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return -1;

    try {
        if (id_map_out) {
            // Row-by-row path: we need to generate new message IDs and track the mapping.
            // OpenCode fork() generates new MessageID.ascending() for each copied message,
            // remaps parentID references using idMap, and then copies parts per message.
            // Turbot generates new IDs as "msg_" + seq counter for simplicity.

            // 1. Fetch source messages in order.
            std::string fetch_sql =
                "SELECT seq, data FROM session_messages WHERE session_id = ?";
            std::vector<nlohmann::json> fetch_vals = {src_session_id};
            if (max_seq) {
                fetch_sql += " AND seq <= ?";
                fetch_vals.push_back(*max_seq);
            }
            fetch_sql += " ORDER BY seq ASC";

            auto fetch_res = db_->execute(fetch_sql, fetch_vals);

            // 2. Insert each row with a new message ID.
            auto tx = db_->begin_transaction();
            *id_map_out = nlohmann::json::object();

            const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            int count = 0;
            for (const auto& row : fetch_res.rows) {
                const std::string data_str = row.value("data", std::string{"{}"});
                nlohmann::json msg_data;
                try {
                    msg_data = nlohmann::json::parse(data_str);
                } catch (...) {
                    msg_data = nlohmann::json::object();
                }

                // Extract old message ID from JSON.
                const std::string old_id = msg_data.value("id", std::string{});
                // Generate new ascending message ID.
                const std::string new_id = "msg_" + std::to_string(now) + "_" +
                    std::to_string(count);

                if (!old_id.empty()) {
                    (*id_map_out)[old_id] = new_id;
                }

                // Update sessionID and id in the cloned message data.
                msg_data["id"]        = new_id;
                msg_data["sessionID"] = dst_session_id;

                // Remap parentID if present and in the map.
                if (msg_data.contains("parentID") && msg_data["parentID"].is_string()) {
                    const std::string old_parent = msg_data["parentID"].get<std::string>();
                    if (id_map_out->contains(old_parent)) {
                        msg_data["parentID"] = (*id_map_out)[old_parent];
                    }
                }

                tx->execute(
                    R"SQL(
                        INSERT INTO session_messages (session_id, seq, data, created_at)
                        SELECT ?, COALESCE((SELECT MAX(seq) FROM session_messages WHERE session_id = ?), -1) + 1, ?, ?
                    )SQL",
                    {dst_session_id, dst_session_id, msg_data.dump(), now}
                );
                ++count;
            }

            tx->commit();
            return count;
        }

        // Fast bulk-insert path (no id mapping needed).
        std::string insert_sql;
        std::vector<nlohmann::json> bind_vals;
        if (max_seq) {
            insert_sql = R"SQL(
                INSERT INTO session_messages (session_id, seq, data, created_at)
                SELECT ?,
                       COALESCE((SELECT MAX(seq) FROM session_messages WHERE session_id = ?), -1)
                           + ROW_NUMBER() OVER (ORDER BY seq),
                       data, created_at
                FROM session_messages
                WHERE session_id = ? AND seq <= ?
                ORDER BY seq ASC
            )SQL";
            bind_vals = {dst_session_id, dst_session_id, src_session_id, *max_seq};
        } else {
            insert_sql = R"SQL(
                INSERT INTO session_messages (session_id, seq, data, created_at)
                SELECT ?,
                       COALESCE((SELECT MAX(seq) FROM session_messages WHERE session_id = ?), -1)
                           + ROW_NUMBER() OVER (ORDER BY seq),
                       data, created_at
                FROM session_messages
                WHERE session_id = ?
                ORDER BY seq ASC
            )SQL";
            bind_vals = {dst_session_id, dst_session_id, src_session_id};
        }

        auto tx = db_->begin_transaction();
        auto result = tx->execute(insert_sql, bind_vals);
        tx->commit();

        return static_cast<int>(result.affected_rows);
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionStore::copy_messages from {} to {} failed: {}",
                         src_session_id, dst_session_id, e.what());
        return -1;
    }
}

// ─── T20: Todo persistence ────────────────────────────────────────────────────

// ─── copy_parts — G55 ─────────────────────────────────────────────────────────
// Mirrors OpenCode Session.fork(): for each msg in msgs, iterate msg.parts and
// call updatePart({ ...part, id: PartID.ascending(), messageID: cloned.id, sessionID })
// Turbot does a bulk copy: for each (old_msg_id → new_msg_id) mapping, copy
// all parts from that old message into the new session with new IDs.

int SessionStore::copy_parts(
    const std::string& src_session_id,
    const std::string& dst_session_id,
    const nlohmann::json& id_map_json
) {
    if (!id_map_json.is_object() || id_map_json.empty()) return 0;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return -1;

    try {
        int total_copied = 0;
        const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        auto tx = db_->begin_transaction();

        for (auto it = id_map_json.begin(); it != id_map_json.end(); ++it) {
            const std::string old_msg_id = it.key();
            const std::string new_msg_id = it.value().get<std::string>();

            // Fetch all parts for this source message.
            auto res = tx->execute(
                "SELECT type, data FROM parts WHERE message_id = ? AND session_id = ?",
                {nlohmann::json(old_msg_id), nlohmann::json(src_session_id)}
            );

            for (const auto& row : res.rows) {
                const std::string type = row.value("type", std::string{"text"});
                const std::string data_str = row.value("data", std::string{"{}"});

                // Parse the part JSON and update sessionID/messageID fields.
                nlohmann::json part_data;
                try {
                    part_data = nlohmann::json::parse(data_str);
                } catch (...) {
                    part_data = nlohmann::json::object();
                }
                part_data["sessionID"]  = dst_session_id;
                part_data["messageID"]  = new_msg_id;

                // Generate a new part ID (prt_ prefix, ascending order).
                // Use a simple timestamp-counter scheme similar to opencode's PartID.ascending().
                const std::string new_part_id = "prt_" + std::to_string(now) + "_" +
                    std::to_string(total_copied);
                part_data["id"] = new_part_id;

                tx->execute(
                    R"SQL(
                        INSERT INTO parts (id, message_id, session_id, type, time_created, time_updated, data)
                        VALUES (?, ?, ?, ?, ?, ?, ?)
                    )SQL",
                    {nlohmann::json(new_part_id),
                     nlohmann::json(new_msg_id),
                     nlohmann::json(dst_session_id),
                     nlohmann::json(type),
                     nlohmann::json(now),
                     nlohmann::json(now),
                     nlohmann::json(part_data.dump())}
                );
                ++total_copied;
            }
        }

        tx->commit();
        return total_copied;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionStore::copy_parts from {} to {} failed: {}",
                         src_session_id, dst_session_id, e.what());
        return -1;
    }
}


bool SessionStore::save_todos(const std::string& session_id,
                              const std::vector<TodoInfo>& todos) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    try {
        auto tx = db_->begin_transaction();
        // Delete existing todos for this session
        tx->execute(
            "DELETE FROM todo WHERE session_id = ?",
            {nlohmann::json(session_id)});

        if (!todos.empty()) {
            const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            for (int pos = 0; pos < static_cast<int>(todos.size()); ++pos) {
                const auto& t = todos[static_cast<std::size_t>(pos)];
                tx->execute(
                    "INSERT INTO todo (session_id, content, status, priority, position,"
                    " time_created, time_updated) VALUES (?, ?, ?, ?, ?, ?, ?)",
                    {nlohmann::json(session_id),
                     nlohmann::json(t.content),
                     nlohmann::json(t.status),
                     nlohmann::json(t.priority),
                     nlohmann::json(pos),
                     nlohmann::json(now),
                     nlohmann::json(now)});
            }
        }
        tx->commit();
        return true;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionStore::save_todos session={} failed: {}", session_id, e.what());
        return false;
    }
}

std::vector<TodoInfo> SessionStore::get_todos(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return {};

    try {
        auto result = db_->execute(
            "SELECT content, status, priority FROM todo"
            " WHERE session_id = ? ORDER BY position ASC",
            {nlohmann::json(session_id)});

        std::vector<TodoInfo> todos;
        todos.reserve(result.rows.size());
        for (const auto& row : result.rows) {
            TodoInfo t;
            t.content  = row.count("content")  ? row.at("content").get<std::string>()  : "";
            t.status   = row.count("status")   ? row.at("status").get<std::string>()   : "pending";
            t.priority = row.count("priority") ? row.at("priority").get<std::string>() : "medium";
            todos.push_back(std::move(t));
        }
        return todos;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionStore::get_todos session={} failed: {}", session_id, e.what());
        return {};
    }
}

// ---------------------------------------------------------------------------
// ─── delete_parts_by_ids — T41 (G41) ─────────────────────────────────────────
// Mirrors OpenCode db.delete(PartTable).where(eq(PartTable.id, part.id))
// called for each removed part in SessionRevert.cleanup().

bool SessionStore::delete_parts_by_ids(const std::vector<std::string>& part_ids) {
    if (part_ids.empty()) return true;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    try {
        // Build a single DELETE with an IN-list: DELETE FROM parts WHERE id IN (?,…)
        std::string placeholders;
        placeholders.reserve(part_ids.size() * 2);
        for (size_t i = 0; i < part_ids.size(); ++i) {
            if (i > 0) placeholders += ',';
            placeholders += '?';
        }
        const std::string sql = "DELETE FROM parts WHERE id IN (" + placeholders + ")";

        std::vector<nlohmann::json> bind_vals;
        bind_vals.reserve(part_ids.size());
        for (const auto& pid : part_ids) {
            bind_vals.push_back(pid);
        }

        auto tx = db_->begin_transaction();
        tx->execute(sql, bind_vals);
        tx->commit();
        return true;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionStore::delete_parts_by_ids failed: {}", e.what());
        return false;
    }
}

// T40: upsert_part — mirrors OpenCode insert(PartTable).onConflictDoUpdate
// ---------------------------------------------------------------------------

bool SessionStore::upsert_part(
        const std::string&    session_id,
        const std::string&    message_id,
        const std::string&    part_id,
        const nlohmann::json& part_json,
        int64_t               now) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    // Extract type field for the dedicated column (fast filtering without JSON extraction)
    const std::string type = part_json.value("type", std::string{"text"});
    const std::string data = part_json.dump();

    try {
        db_->execute(
            R"SQL(
                INSERT INTO parts (id, message_id, session_id, type, time_created, time_updated, data)
                VALUES (?, ?, ?, ?, ?, ?, ?)
                ON CONFLICT(id) DO UPDATE SET
                    data         = excluded.data,
                    time_updated = excluded.time_updated
            )SQL",
            {nlohmann::json(part_id),
             nlohmann::json(message_id),
             nlohmann::json(session_id),
             nlohmann::json(type),
             nlohmann::json(now),
             nlohmann::json(now),
             nlohmann::json(data)});
        return true;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SessionStore::upsert_part part={} failed: {}", part_id, e.what());
        return false;
    }
}

} // namespace turbot::core::session
