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

/// Git-diff summary for a session (aligned with OpenCode Session.Info.summary)
struct TURBOT_CORE_API SessionSummary {
    int additions = 0;                       ///< Lines added
    int deletions = 0;                       ///< Lines deleted
    int files     = 0;                       ///< Files changed
    std::optional<nlohmann::json> diffs;     ///< Snapshot.FileDiff array (optional)

    bool operator==(const SessionSummary& o) const noexcept {
        return additions == o.additions && deletions == o.deletions &&
               files == o.files && diffs == o.diffs;
    }
};

/// Share info (enterprise — URL to public session share link)
struct TURBOT_CORE_API SessionShare {
    std::string url;
    bool operator==(const SessionShare& o) const noexcept { return url == o.url; }
};

/// Project info embedded in global session listing results (G57).
///
/// Aligned with OpenCode Session.ProjectInfo used in listGlobal():
///   { id: ProjectID, name?: string, worktree: string }
///
/// Returned alongside SessionInfo in list_global() results so callers can
/// display the project context for each session without a separate lookup.
struct TURBOT_CORE_API SessionProjectInfo {
    std::string id;                          ///< Project ID
    std::optional<std::string> name;         ///< Project display name (may be absent)
    std::string worktree;                    ///< Project worktree path

    /// Deserialize from JSON (supports both wire and flat-column forms).
    static SessionProjectInfo from_json(const nlohmann::json& j);

    /// Serialize to JSON.
    [[nodiscard]] nlohmann::json to_json() const;

    bool operator==(const SessionProjectInfo& o) const noexcept {
        return id == o.id && name == o.name && worktree == o.worktree;
    }
};

/// Session information structure
struct TURBOT_CORE_API SessionInfo {
    std::string id;                              ///< Unique session identifier (ses_ prefix)
    std::string project_id;                      ///< Project this session belongs to
    std::optional<std::string> workspace_id;     ///< Workspace ID (enterprise control plane)
    std::optional<std::string> parent_id;        ///< Parent session ID (for forks)
    std::string slug;                            ///< Human-readable slug
    std::string directory;                       ///< Working directory
    std::string title;                           ///< Session title
    std::string version;                         ///< Session version
    std::optional<nlohmann::json> permission;    ///< Permission configuration
    int64_t time_created = 0;                    ///< Creation timestamp (epoch ms)
    int64_t time_updated = 0;                    ///< Last update timestamp (epoch ms)
    std::optional<int64_t> time_compacting;      ///< Last compacting timestamp
    std::optional<int64_t> time_archived;        ///< Archival timestamp

    /// Internal lifecycle state — NOT serialized to JSON wire format.
    /// OpenCode manages this in-memory via SessionStatus; Turbot keeps it for
    /// internal use only (e.g. guard checks in compact()/archive()/restore()).
    SessionState state = SessionState::Created;

    std::optional<RevertInfo> revert;            ///< Pending revert operation
    std::optional<SessionSummary> summary;       ///< Git-diff stats (set after session ends)
    std::optional<SessionShare> share;           ///< Share URL (enterprise)

    /// Serialize to OpenCode-compatible JSON (camelCase keys, nested time object).
    /// NOTE: `state` is intentionally omitted from the output.
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON.
    /// Accepts both camelCase (API/wire) and snake_case (DB column) key variants.
    /// `state` is reconstructed from time_archived/time_compacting if not explicit.
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
    /// Optional cutoff: copy messages up to and including this seq number.
    /// When nullopt, all messages from the parent are copied.
    std::optional<int64_t> message_seq_cutoff;
};

/// Filter parameters for Session::list() — mirrors OpenCode's list() input shape.
///
/// All fields are optional; omitting all fields returns all sessions (up to limit).
/// Aligned with OpenCode packages/opencode/src/session/index.ts list() input.
struct TURBOT_CORE_API ListParams {
    std::optional<std::string> project_id;   ///< Filter by project ID
    std::optional<std::string> directory;    ///< Filter by directory
    std::optional<std::string> workspace_id; ///< Filter by workspace ID
    bool roots = false;                      ///< If true, only root sessions (parent_id IS NULL)
    std::optional<int64_t>  start;           ///< Lower bound on time_updated (epoch ms, inclusive)
    std::optional<std::string> search;       ///< Substring match on title (LIKE %search%)
    int limit = 100;                         ///< Max results (default 100, 0 = unlimited)
};

/// Parameters for reverting a session to a specific message/part
struct TURBOT_CORE_API RevertParams {
    std::string message_id;             ///< Message ID to revert from (the revert point)
    std::optional<std::string> part_id; ///< Optional part ID for finer granularity
    /// Patches collected from messages after the revert point (to roll back files)
    std::vector<turbot::core::snapshot::PatchResult> patches;
};

/// Parameters for Session::list_global() — cross-project global listing.
///
/// Aligned with OpenCode Session.listGlobal(input?) signature.
struct TURBOT_CORE_API ListGlobalParams {
    std::optional<std::string> directory;  ///< Filter by directory
    bool roots = false;                    ///< Only root sessions (parent_id IS NULL)
    std::optional<int64_t> start;          ///< Lower bound on time_updated (epoch ms)
    std::optional<int64_t> cursor;         ///< Upper bound on time_updated for pagination
    std::optional<std::string> search;     ///< Substring match on title
    int limit = 100;                       ///< Max results
    bool archived = false;                 ///< Include archived sessions
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

    /// List sessions matching the given filter parameters.
    ///
    /// Aligned with OpenCode Session.list(input?) which accepts directory,
    /// workspaceID, roots, start, search, limit as optional filters.
    ///
    /// @param params  Filter parameters (all fields optional)
    /// @return Vector of matching sessions ordered by time_updated DESC
    [[nodiscard]] static std::vector<Session> list(const ListParams& params = {});

    /// Convenience overload: list all sessions for a project.
    [[nodiscard]] static std::vector<Session> list(const std::string& project_id);

    /// Delete a session by ID
    /// @param id Session ID
    /// @return true if deleted
    static bool remove(const std::string& id);

    /// Touch a session: update time_updated and publish Event.Updated.
    ///
    /// Aligned with OpenCode Session.touch(sessionID).
    ///
    /// @param id Session ID
    /// @return true on success
    static bool touch(const std::string& id);

    /// Check whether a session title is a default auto-generated title.
    ///
    /// Aligned with OpenCode Session.isDefaultTitle(title):
    ///   /^(New session - |Child session - )\d{4}-\d{2}-\d{2}T.../
    ///
    /// @param title Session title to test
    /// @return true if title matches the default pattern
    [[nodiscard]] static bool is_default_title(const std::string& title) noexcept;

    /// List sessions across all projects (global view).
    ///
    /// Aligned with OpenCode Session.listGlobal(input?).
    /// Unlike list(), this is not scoped to a single project; each result
    /// includes a `project` JSON blob with {id, name?, worktree}.
    ///
    /// @param params Filter parameters
    /// @return Vector of {session_json, project_json} pairs
    [[nodiscard]] static std::vector<std::pair<nlohmann::json, nlohmann::json>>
    list_global(const ListGlobalParams& params = {});

    /// Return the path for a session plan file.
    ///
    /// Aligned with OpenCode Session.plan(input):
    ///   vcs project → <worktree>/.opencode/plans/<created>-<slug>.md
    ///   otherwise   → <data_dir>/plans/<created>-<slug>.md
    ///
    /// @param slug         Session slug
    /// @param time_created Session creation timestamp (epoch ms)
    /// @param worktree     Project worktree path (empty → non-vcs mode)
    /// @return Absolute path to the plan file
    [[nodiscard]] static std::string plan(const std::string& slug,
                                          int64_t time_created,
                                          const std::string& worktree = {});

    // =========================================================================
    // T40: Part streaming — mirrors OpenCode Session.updatePart / updatePartDelta
    // =========================================================================

    /**
     * @brief Upsert a message part into the DB and publish PartUpdatedEvent.
     *
     * Mirrors OpenCode Session.updatePart(part): insert/onConflictDoUpdate the
     * part row, then Bus.publish(MessageV2.Event.PartUpdated, { part }).
     *
     * @param session_id  Session that owns the part.
     * @param message_id  Message that owns the part.
     * @param part_json   Full part JSON payload (must contain "id" field).
     * @return true on success.
     */
    static bool update_part(const std::string& session_id,
                            const std::string& message_id,
                            const nlohmann::json& part_json);

    /**
     * @brief Publish a PartDeltaEvent for incremental streaming text.
     *
     * Mirrors OpenCode Session.updatePartDelta(input):
     *   Bus.publish(MessageV2.Event.PartDelta, input)
     *
     * This does NOT write to the DB — it only broadcasts via EventBus so that
     * connected UI/ACP clients can append text incrementally.
     *
     * @param session_id  Session that owns the part.
     * @param message_id  Message that owns the part.
     * @param part_id     Part being streamed.
     * @param field       Field name (typically "text").
     * @param delta       Incremental text fragment.
     */
    static void update_part_delta(const std::string& session_id,
                                  const std::string& message_id,
                                  const std::string& part_id,
                                  const std::string& field,
                                  const std::string& delta);

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

    /// Set the git-diff summary for this session.
    ///
    /// Aligned with OpenCode Session.setSummary(sessionID, summary).
    /// Updates info_.summary and persists to DB.
    ///
    /// @param summary  Git-diff stats to record
    /// @return true on success
    bool set_summary(const SessionSummary& summary);

    /// Set the share URL for this session (enterprise).
    ///
    /// Aligned with OpenCode Session.share(id) which updates share_url and
    /// publishes Session.Event.Updated.  Turbot stores the URL in info_.share
    /// and emits SessionInfoUpdatedEvent so connected ACP/UI clients stay in sync.
    ///
    /// @param share  Share info (must contain a non-empty URL)
    /// @return true on success
    bool set_share(const SessionShare& share);

    /// Clear the share URL for this session (enterprise).
    ///
    /// Aligned with OpenCode Session.unshare(id) which sets share_url to null
    /// and publishes Session.Event.Updated.
    ///
    /// @return true on success
    bool unshare();

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

    /// Set archived time for this session (G56).
    ///
    /// Aligned with OpenCode Session.setArchived({ sessionID, time? }):
    ///   - time provided   → sets time_archived (archives the session)
    ///   - time == nullopt → clears time_archived (unarchives the session)
    ///
    /// Publishes Session.Event.Updated after persisting to DB.
    ///
    /// @param time  Archival timestamp in epoch ms, or nullopt to unarchive
    /// @return true on success
    bool set_archived(std::optional<int64_t> time = std::nullopt);

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
