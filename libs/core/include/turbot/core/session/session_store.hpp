#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/storage/database.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace turbot::core::session {

// Forward declarations
struct ListParams;
struct SessionInfo;

/// SessionStore — singleton that owns the database connection used by Session
/// static methods (get / list / remove / messages / save).
///
/// Call SessionStore::instance().init(db) once at application startup (before any
/// Session operations) to wire up SQLite persistence.  Until init() is called the
/// static Session methods fall back to the original placeholder behaviour.
class TURBOT_CORE_API SessionStore {
public:
    /// Get the singleton instance.
    static SessionStore& instance();

    SessionStore(const SessionStore&) = delete;
    SessionStore& operator=(const SessionStore&) = delete;

    /// Wire up a database backend.  Thread-safe; may be called multiple times
    /// (only the first call takes effect).
    void init(std::shared_ptr<storage::Database> db);

    /// @return true if the store has been initialised with a database.
    [[nodiscard]] bool is_initialized() const noexcept;

    /// Reset the store (for testing purposes).
    /// Closes the database connection and resets schema state.
    void reset();

    /// Persist a new or updated Session row.
    /// @param info   SessionInfo to upsert.
    /// @return true on success.
    bool save(const struct SessionInfo& info);

    /// Retrieve a SessionInfo row by ID.
    [[nodiscard]] std::optional<nlohmann::json> find_by_id(const std::string& id);

    /// Retrieve SessionInfo rows matching the given filter params.
    /// Rows are ordered by time_updated DESC; limit defaults to 100.
    [[nodiscard]] std::vector<nlohmann::json> find_all(const ListParams& params);

    /// Retrieve SessionInfo rows for a project with cursor-based pagination.
    /// @param project_id The project ID to filter by
    /// @param limit Maximum number of rows to return
    /// @param cursor Optional cursor (Unix timestamp in milliseconds) for pagination.
    ///               Returns rows with time_updated < cursor.
    /// @return Pair of (rows, next_cursor). next_cursor is empty if no more rows.
    [[nodiscard]] std::pair<std::vector<nlohmann::json>, std::optional<std::string>>
    find_all_paginated(
        const std::string& project_id,
        int limit = 100,
        const std::optional<std::string>& cursor = std::nullopt
    );

    /// Delete a Session row by ID.
    /// @return true if a row was actually deleted.
    bool remove(const std::string& id);

    /// Persist a message JSON for a session.
    bool save_message(const std::string& session_id, const nlohmann::json& message);

    /// Retrieve ordered message rows for a session (offset-based, legacy).
    [[nodiscard]] std::vector<nlohmann::json> list_messages(
        const std::string& session_id,
        int limit = 50,
        int offset = 0
    );

    /// Retrieve ordered message rows for a session with cursor-based pagination.
    /// @param session_id The session ID to filter by
    /// @param limit Maximum number of rows to return
    /// @param cursor Optional cursor (message seq) for pagination.
    ///               Returns rows with seq > cursor.
    /// @return Pair of (messages, next_cursor). next_cursor is empty if no more rows.
    [[nodiscard]] std::pair<std::vector<nlohmann::json>, std::optional<int64_t>>
    list_messages_paginated(
        const std::string& session_id,
        int limit = 50,
        std::optional<int64_t> cursor = std::nullopt
    );

    /// Delete specific messages from a session by their JSON-embedded "id" field.
    ///
    /// Iterates over @p ids and removes the corresponding rows from
    /// session_messages where json_extract(data, '$.id') matches.
    /// All deletions are wrapped in a single transaction.
    ///
    /// @param session_id  Session to delete from.
    /// @param ids         Message id values to remove (embedded in the data JSON).
    /// @return true on success (also true when ids is empty — no-op).
    bool delete_messages_by_ids(
        const std::string& session_id,
        const std::vector<std::string>& ids
    );

    /// Copy messages from @p src_session_id into @p dst_session_id.
    ///
    /// Copies all messages up to and including the one with seq == @p max_seq
    /// (or all messages when max_seq is nullopt).  Each copied message gets a
    /// fresh seq in the destination session, preserving relative order.
    ///
    /// Aligned with OpenCode's Session.fork() message-copy semantics.
    ///
    /// @return Number of messages copied, or -1 on error.
    int copy_messages(
        const std::string& src_session_id,
        const std::string& dst_session_id,
        std::optional<int64_t> max_seq = std::nullopt
    );

    // ── T20: Todo persistence ────────────────────────────────────────────────

    /// Replace the full todo list for a session (all-or-nothing transaction).
    /// Mirrors OpenCode Todo.update() persistence.
    /// @return true on success.
    bool save_todos(const std::string& session_id,
                    const std::vector<struct TodoInfo>& todos);

    /// Retrieve the current todo list for a session, ordered by position.
    [[nodiscard]] std::vector<struct TodoInfo> get_todos(const std::string& session_id);

    // ── T41 (G41): Part deletion — used by SessionRevert::cleanup() ─────────

    /// Delete specific parts from the parts table by their ID.
    ///
    /// Mirrors OpenCode db.delete(PartTable).where(eq(PartTable.id, part.id))
    /// called inside SessionRevert.cleanup() for each removed part.
    ///
    /// @param part_ids   Part id values to remove.
    /// @return true on success (also true when part_ids is empty — no-op).
    bool delete_parts_by_ids(const std::vector<std::string>& part_ids);

    // ── T40: Part upsert ────────────────────────────────────────────────────

    /// Upsert a message part into the parts table.
    ///
    /// Mirrors OpenCode insert(PartTable).onConflictDoUpdate({ target: PartTable.id }).
    ///
    /// @param session_id  Session that owns the part.
    /// @param message_id  Message that owns the part.
    /// @param part_id     Part primary key (prt_xxx).
    /// @param part_json   Full part payload JSON.
    /// @param now         Current epoch-ms timestamp.
    /// @return true on success.
    bool upsert_part(const std::string& session_id,
                     const std::string& message_id,
                     const std::string& part_id,
                     const nlohmann::json& part_json,
                     int64_t now);

private:
    SessionStore() = default;

    /// Ensure the sessions and messages tables exist (idempotent).
    void ensure_schema();

    mutable std::mutex mutex_;
    std::shared_ptr<storage::Database> db_;
    bool schema_ready_ = false;
};

} // namespace turbot::core::session
