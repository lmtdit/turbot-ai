#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/permission/permission.hpp>
#include <turbot/core/snapshot/snapshot.hpp>
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

/// Revert info - tracks a pending revert operation.
///
/// Aligned with opencode Session.Info.revert:
///   { messageID, partID?, snapshot?, diff? }
///
/// JSON key mapping (opencode-compatible):
///   message_id  → "messageID"
///   part_id     → "partID"
///   snapshot_id → "snapshot"
///   diff        → "diff"
///   pre_patch   → "prePatch"  (turbot-specific, stores pre-revert PatchResult for unrevert)
struct TURBOT_CORE_API RevertInfo {
    std::string message_id;                  ///< Message ID to revert from (revert boundary)
    std::optional<std::string> part_id;      ///< Part ID for finer granularity (optional)
    std::optional<std::string> snapshot_id;  ///< Logical snapshot identifier (for UI display)
    std::optional<std::string> diff;         ///< Diff text of reverted range (for UI display)
    /// Pre-revert patch: stores the file states captured just before rolling back,
    /// allowing unrevert() to re-apply them and restore the working directory.
    std::optional<turbot::core::snapshot::PatchResult> pre_patch;

    /// Serialize to JSON (opencode-compatible key names)
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON (accepts both opencode camelCase and snake_case keys)
    static RevertInfo from_json(const nlohmann::json& j);

    /// Equality comparison
    bool operator==(const RevertInfo& other) const noexcept;
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
    std::optional<RevertInfo> revert;        ///< Pending revert operation (nullopt when not reverting)

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static SessionInfo from_json(const nlohmann::json& j);

    /// Equality comparison (includes revert field)
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

/// Parameters for reverting a session to a specific message/part
struct TURBOT_CORE_API RevertParams {
    std::string message_id;             ///< Message ID to revert from (the revert point)
    std::optional<std::string> part_id; ///< Optional part ID for finer granularity
    /// Patches collected from messages after the revert point (to roll back files)
    std::vector<turbot::core::snapshot::PatchResult> patches;
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

    /// Default constructor — initialises the mutex so the object is immediately usable.
    Session() : mutex_(std::make_shared<std::mutex>()) {}

    /// Constructor with session info
    explicit Session(SessionInfo info)
        : info_(std::move(info)), mutex_(std::make_shared<std::mutex>()) {}

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

    /// Revert session to the specified message/part.
    ///
    /// Rolls back all file changes collected in @p params.patches via SnapshotManager,
    /// records the revert info in SessionInfo, and publishes a SessionStatusEvent.
    /// Callers are responsible for truncating the in-memory message list.
    ///
    /// @param params   Revert parameters (target message_id / part_id + patches to roll back)
    /// @return true if the revert was applied successfully
    bool revert(const RevertParams& params);

    /// Undo a previous revert: re-apply file changes from the pre-revert snapshot and
    /// clear the revert info.
    ///
    /// @return true if the unrevert was applied successfully (or there was nothing to unrevert)
    bool unrevert();

    /// Clean up after a committed revert: discard the stored revert info without touching files.
    ///
    /// Call this once the UI/caller has confirmed the revert (messages have been truncated
    /// and the user does not want to unrevert).
    ///
    /// @return true on success
    bool cleanup_revert();

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
