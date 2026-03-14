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

    /// Persist a new or updated Session row.
    /// @param info   SessionInfo to upsert.
    /// @return true on success.
    bool save(const struct SessionInfo& info);

    /// Retrieve a SessionInfo row by ID.
    [[nodiscard]] std::optional<nlohmann::json> find_by_id(const std::string& id);

    /// Retrieve all SessionInfo rows for a project, ordered by time_created DESC.
    [[nodiscard]] std::vector<nlohmann::json> find_all(const std::string& project_id);

    /// Delete a Session row by ID.
    /// @return true if a row was actually deleted.
    bool remove(const std::string& id);

    /// Persist a message JSON for a session.
    bool save_message(const std::string& session_id, const nlohmann::json& message);

    /// Retrieve ordered message rows for a session.
    [[nodiscard]] std::vector<nlohmann::json> list_messages(
        const std::string& session_id,
        int limit = 50,
        int offset = 0
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
