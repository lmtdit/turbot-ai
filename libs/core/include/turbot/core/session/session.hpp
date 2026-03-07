#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/permission/permission.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace turbot::core::session {

/// Session state enum
enum class TURBOT_CORE_API SessionState {
    Created,    ///< Session just created, not yet active
    Active,     ///< Session is active and ready for interactions
    Busy,       ///< Session is processing a request
    Compacting, ///< Session is compacting history
    Archived    ///< Session is archived (read-only)
};

/// Convert SessionState to string
[[nodiscard]] TURBOT_CORE_API std::string session_state_to_string(SessionState state);

/// Convert string to SessionState
[[nodiscard]] TURBOT_CORE_API SessionState string_to_session_state(const std::string& str);

/// Session information structure
struct TURBOT_CORE_API SessionInfo {
    std::string id;                          ///< Unique session identifier
    std::string project_id;                  ///< Project this session belongs to
    std::optional<std::string> parent_id;    ///< Parent session ID (for forks)
    std::string slug;                        ///< Human-readable slug
    std::string directory;                   ///< Working directory
    std::string title;                       ///< Session title
    std::string version;                     ///< Session version
    std::optional<nlohmann::json> permission; ///< Permission configuration
    int64_t time_created = 0;                ///< Creation timestamp (epoch seconds)
    int64_t time_updated = 0;                ///< Last update timestamp
    std::optional<int64_t> time_compacting;  ///< Last compacting timestamp
    std::optional<int64_t> time_archived;    ///< Archival timestamp
    SessionState state = SessionState::Created; ///< Current state

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static SessionInfo from_json(const nlohmann::json& j);

    /// Equality comparison
    bool operator==(const SessionInfo& other) const noexcept;
};

/// Parameters for creating a session
struct TURBOT_CORE_API CreateParams {
    std::string project_id;                  ///< Project ID
    std::string slug;                        ///< Human-readable slug
    std::string directory;                   ///< Working directory
    std::string title;                       ///< Session title
    std::optional<nlohmann::json> permission; ///< Initial permission config
};

/// Parameters for updating a session
struct TURBOT_CORE_API UpdateParams {
    std::optional<std::string> title;        ///< New title
    std::optional<nlohmann::json> permission; ///< New permission config
    std::optional<SessionState> state;       ///< New state
};

/// Parameters for forking a session
struct TURBOT_CORE_API ForkParams {
    std::string parent_id;                   ///< Parent session ID
    std::string slug;                        ///< Slug for the fork
    std::string title;                       ///< Title for the fork
};

/// Forward declaration for message
namespace message {
struct Message;
} // namespace message

/// Session class - manages a conversation session
class TURBOT_CORE_API Session {
public:
    /// Create a new session
    /// @param params Creation parameters
    /// @return Created session
    [[nodiscard]] static std::optional<Session> create(const CreateParams& params);

    /// Fork an existing session
    /// @param params Fork parameters
    /// @return Forked session
    [[nodiscard]] static std::optional<Session> fork(const ForkParams& params);

    /// Get a session by ID
    /// @param id Session ID
    /// @return Session or nullopt if not found
    [[nodiscard]] static std::optional<Session> get(const std::string& id);

    /// List sessions for a project
    /// @param project_id Project ID
    /// @return Vector of sessions
    [[nodiscard]] static std::vector<Session> list(const std::string& project_id);

    /// Delete a session by ID
    /// @param id Session ID
    /// @return true if deleted
    static bool remove(const std::string& id);

    /// Default constructor
    Session() = default;

    /// Constructor with session info
    explicit Session(SessionInfo info) : info_(std::move(info)) {}

    /// Update session
    /// @param params Update parameters
    /// @return true if updated
    bool update(const UpdateParams& params);

    /// Set session title
    /// @param title New title
    /// @return true if updated
    bool set_title(const std::string& title);

    /// Set session permission
    /// @param permission New permission config
    /// @return true if updated
    bool set_permission(const nlohmann::json& permission);

    /// Get messages for this session
    /// @param limit Maximum number of messages
    /// @param offset Offset for pagination
    /// @return Vector of message JSONs
    [[nodiscard]] std::vector<nlohmann::json> messages(int limit = 50, int offset = 0) const;

    /// Compact session history
    /// @return true if compacted successfully
    bool compact();

    /// Archive the session
    /// @return true if archived successfully
    bool archive();

    /// Restore from archived state
    /// @return true if restored successfully
    bool restore();

    /// Get session ID
    [[nodiscard]] const std::string& id() const noexcept { return info_.id; }

    /// Get session info
    [[nodiscard]] const SessionInfo& info() const noexcept { return info_; }

    /// Check if session is valid
    [[nodiscard]] bool is_valid() const noexcept { return !info_.id.empty(); }

    /// Get current state
    [[nodiscard]] SessionState state() const noexcept { return info_.state; }

    /// Check if session is active
    [[nodiscard]] bool is_active() const noexcept {
        return info_.state == SessionState::Active;
    }

    /// Check if session is archived
    [[nodiscard]] bool is_archived() const noexcept {
        return info_.state == SessionState::Archived;
    }

    /// Equality comparison
    bool operator==(const Session& other) const noexcept {
        return info_.id == other.info_.id;
    }

private:
    SessionInfo info_;
    mutable std::shared_ptr<std::mutex> mutex_;

    /// Generate a unique session ID
    [[nodiscard]] static std::string generate_id();

    /// Get current timestamp
    [[nodiscard]] static int64_t current_timestamp();
};

} // namespace turbot::core::session
