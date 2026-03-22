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

    /// Retrieve all SessionInfo rows for a project, ordered by time_created DESC.
    [[nodiscard]] std::vector<nlohmann::json> find_all(const std::string& project_id);

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

private:
    SessionStore() = default;

    /// Ensure the sessions and messages tables exist (idempotent).
    void ensure_schema();

    mutable std::mutex mutex_;
    std::shared_ptr<storage::Database> db_;
    bool schema_ready_ = false;
};

} // namespace turbot::core::session
