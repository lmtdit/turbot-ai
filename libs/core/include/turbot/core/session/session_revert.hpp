#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_summary.hpp>
#include <string>
#include <optional>

namespace turbot::core::session {

/**
 * @brief SessionRevert — mirrors OpenCode `session/revert.ts`.
 *
 * Provides three high-level operations that coordinate snapshot rollback,
 * diff computation, DB persistence and event publishing:
 *
 *  1. revert()  — roll back file changes, record revert boundary and summary.
 *  2. unrevert() — undo a previously applied revert, restoring working-dir state.
 *  3. cleanup() — commit a revert: delete messages after the boundary from the DB
 *                 and fire MessageV2 Removed/PartRemoved events.
 *
 * These functions wrap the low-level Session::revert() / unrevert() /
 * cleanup_revert() methods, adding the orchestration that OpenCode performs
 * inside `revert.ts`.
 *
 * Aligned with: opencode/packages/opencode/src/session/revert.ts
 */
namespace SessionRevert {

/**
 * @brief Input for the revert() operation.
 *
 * Mirrors OpenCode SessionRevert.RevertInput:
 *   { sessionID, messageID, partID? }
 */
struct TURBOT_CORE_API RevertInput {
    std::string session_id;                  ///< Session to revert
    std::string message_id;                  ///< Revert boundary message ID
    std::optional<std::string> part_id;      ///< Optional part granularity
};

/**
 * @brief Revert a session to the specified message/part boundary.
 *
 * Mirrors OpenCode `SessionRevert.revert(input)`:
 *  1. Walks all session messages collecting patch parts after the revert point.
 *  2. Calls SnapshotManager::rollback_patch() to undo file changes.
 *  3. Computes diff stats via SessionSummaryService::compute_diff().
 *  4. Persists session summary + revert info via Session::revert().
 *  5. Publishes SessionDiffEvent on the EventBus.
 *
 * @param input   Revert parameters.
 * @return Updated SessionInfo on success, nullopt on failure.
 */
[[nodiscard]] TURBOT_CORE_API std::optional<SessionInfo>
revert(const RevertInput& input);

/**
 * @brief Undo a previous revert, restoring the working directory.
 *
 * Mirrors OpenCode `SessionRevert.unrevert({ sessionID })`:
 *  1. Loads the session, checks if a revert is pending.
 *  2. Calls SnapshotManager to re-apply the pre-revert patch.
 *  3. Calls Session::unrevert() to clear the revert info.
 *
 * @param session_id   Session to unrevert.
 * @return Updated SessionInfo on success, nullopt on failure.
 */
[[nodiscard]] TURBOT_CORE_API std::optional<SessionInfo>
unrevert(const std::string& session_id);

/**
 * @brief Commit a revert: delete messages after the revert boundary from the DB.
 *
 * Mirrors OpenCode `SessionRevert.cleanup(session)`:
 *  1. Determines which messages/parts to preserve vs remove based on
 *     session.revert.messageID and optional partID.
 *  2. Deletes removed messages from the DB.
 *  3. Fires MessageV2.Event.Removed / PartRemoved events.
 *  4. Calls Session::cleanup_revert() to clear the revert info.
 *
 * @param session   The session (must have a pending revert, otherwise no-op).
 */
TURBOT_CORE_API void cleanup(const SessionInfo& session);

} // namespace SessionRevert
} // namespace turbot::core::session
