#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/session/session.hpp>
#include <nlohmann/json.hpp>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace turbot::core::session {

/// A single file's diff statistics (mirrors OpenCode Snapshot.FileDiff).
struct TURBOT_CORE_API FileDiffStat {
    std::string file;          ///< File path (git-quoted paths are unquoted)
    int         additions = 0; ///< Lines added
    int         deletions = 0; ///< Lines removed
    bool        binary    = false; ///< True for binary files (no line counts)

    [[nodiscard]] nlohmann::json to_json() const;
    static FileDiffStat from_json(const nlohmann::json& j);
};

/// Session diff-summary event data (mirrors OpenCode Session.Event.Diff).
struct TURBOT_CORE_API SessionDiffEvent {
    static constexpr const char* kEventName = "session.diff";
    std::string session_id;
    std::vector<FileDiffStat> diff;
};

/**
 * @brief SessionSummary — mirrors OpenCode `session/summary.ts`.
 *
 * Provides three main operations:
 *  1. summarize() — after a session run completes, compute git diff statistics
 *     across all messages and persist them via Session::set_summary().
 *     Also publishes a SessionDiffEvent on the EventBus.
 *
 *  2. diff() — read previously computed diff statistics for a session.
 *
 *  3. compute_diff() — low-level: scan a list of message JSON objects, find the
 *     earliest step-start snapshot and the latest step-finish snapshot, then
 *     compute the git diff between them using SnapshotManager.
 *
 * Aligned with: opencode/packages/opencode/src/session/summary.ts
 */
class TURBOT_CORE_API SessionSummaryService {
public:
    static SessionSummaryService& instance();

    // Non-copyable, non-movable singleton
    SessionSummaryService(const SessionSummaryService&) = delete;
    SessionSummaryService& operator=(const SessionSummaryService&) = delete;
    SessionSummaryService(SessionSummaryService&&) = delete;
    SessionSummaryService& operator=(SessionSummaryService&&) = delete;

    /**
     * @brief Compute and persist session + message diff statistics.
     *
     * Mirrors OpenCode `SessionSummary.summarize({ sessionID, messageID })`.
     * - Loads all messages for the session.
     * - Calls compute_diff() to obtain per-file diff stats.
     * - Calls Session::set_summary() to persist additions/deletions/files.
     * - Publishes SessionDiffEvent on the EventBus.
     *
     * @param session_id   Session to summarize.
     * @param message_id   The triggering message ID (unused internally, kept for API parity).
     */
    void summarize(const std::string& session_id, const std::string& message_id);

    /**
     * @brief Read stored diff statistics for a session.
     *
     * Mirrors OpenCode `SessionSummary.diff({ sessionID })`.
     * Returns the previously computed vector of FileDiffStat, or empty if none.
     *
     * @param session_id   Session to read diff for.
     */
    [[nodiscard]] std::vector<FileDiffStat> diff(const std::string& session_id) const;

    /**
     * @brief Compute diff stats from a list of message JSON objects.
     *
     * Mirrors OpenCode `SessionSummary.computeDiff({ messages })`.
     * Scans messages for "step-start" parts (with snapshot) and "step-finish"
     * parts (with snapshot), then calls SnapshotManager to diff them.
     *
     * @param messages  Vector of message JSON objects (same schema as the DB row data).
     * @returns Per-file diff statistics, empty if no snapshots found.
     */
    [[nodiscard]] std::vector<FileDiffStat> compute_diff(
        const std::vector<nlohmann::json>& messages) const;

private:
    SessionSummaryService() = default;
    ~SessionSummaryService() = default;

    /**
     * @brief Decode a git-quoted path string (C-escape sequences, octal bytes).
     *
     * Git wraps paths containing non-ASCII or special characters in double quotes
     * with C-style escape sequences. This mirrors `unquoteGitPath` in summary.ts.
     *
     * @param input   Possibly-quoted path string.
     * @returns Decoded path string.
     */
    [[nodiscard]] static std::string unquote_git_path(const std::string& input);

    /**
     * @brief Run `git diff --stat` between two snapshot IDs and parse output.
     *
     * @param from_snapshot   Earlier snapshot ID.
     * @param to_snapshot     Later snapshot ID.
     * @returns Per-file diff stats.
     */
    [[nodiscard]] std::vector<FileDiffStat> diff_snapshots(
        const std::string& from_snapshot,
        const std::string& to_snapshot) const;

    // diff cache: session_id → last computed diffs
    mutable std::unordered_map<std::string, std::vector<FileDiffStat>> diff_cache_;
    mutable std::mutex cache_mutex_;
};

} // namespace turbot::core::session
