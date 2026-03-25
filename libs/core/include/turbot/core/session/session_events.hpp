#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/llm/stream_event.hpp>
#include <turbot/core/message/message.hpp>
#include <turbot/core/message/token_usage.hpp>
#include <turbot/core/tool/tool.hpp>
#include <turbot/core/tool/builtin/question_tool.hpp>
#include <turbot/core/permission/permission.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace turbot::core::session {

// ---------------------------------------------------------------------------
// T1: Session CRUD events — mirrors OpenCode Session.Event.{Created,Updated,Deleted}
// These are published by Session::create(), Session::update(), Session::remove()
// so that ACP / UI consumers receive real-time push notifications.
// ---------------------------------------------------------------------------

/// Published immediately after a new Session is persisted to the DB.
/// Mirrors OpenCode Bus.publish(Session.Event.Created, { info: result }).
struct TURBOT_CORE_API SessionCreatedEvent {
    static constexpr const char* kEventName = "session.created";
    nlohmann::json info;  ///< Wire-format JSON (same as SessionInfo::to_json())
};

/// Published after any Session field is updated (title, state, permission, etc.).
/// Mirrors OpenCode Bus.publish(Session.Event.Updated, { info: result }).
struct TURBOT_CORE_API SessionInfoUpdatedEvent {
    static constexpr const char* kEventName = "session.updated";
    nlohmann::json info;  ///< Wire-format JSON (same as SessionInfo::to_json())
};

/// Published immediately before a Session is deleted from the DB.
/// Mirrors OpenCode Bus.publish(Session.Event.Deleted, { info: session }).
struct TURBOT_CORE_API SessionDeletedEvent {
    static constexpr const char* kEventName = "session.deleted";
    nlohmann::json info;  ///< Wire-format JSON (same as SessionInfo::to_json())
};

// ---------------------------------------------------------------------------
// T1: Project CRUD events — mirrors OpenCode project.updated
// ---------------------------------------------------------------------------

/// Published after a Project is created or its metadata is updated.
/// Mirrors OpenCode Bus.publish(Project.Event.Updated, { info: result }).
struct TURBOT_CORE_API ProjectUpdatedEvent {
    static constexpr const char* kEventName = "project.updated";
    nlohmann::json info;  ///< Wire-format JSON (same as ProjectInfo::to_json())
};

// ---------------------------------------------------------------------------
// SessionStatus – lifecycle state of a running SessionLoop
// ---------------------------------------------------------------------------

/**
 * @brief Session lifecycle status (mirrors opencode SessionStatus).
 *
 * Transitions:
 *   Idle  → Busy  (run() begins)
 *   Busy  → Idle  (run() finishes normally or is aborted)
 *   Busy  → Retry (LLM call fails with a retryable error)
 *   Retry → Busy  (retry delay elapsed, resuming)
 */
enum class SessionStatus {
    Idle,   ///< Session is not running
    Busy,   ///< Session is actively processing (LLM call or tool execution)
    Retry,  ///< Session is waiting before retrying a failed LLM call
};

// ---------------------------------------------------------------------------
// Event structs – published via EventBus
// Each struct carries a static kEventName constant used as the Bus topic key.
// ---------------------------------------------------------------------------

/**
 * @brief Broadcast when the session's lifecycle status changes.
 *
 * Consumers can subscribe to SessionStatusEvent::kEventName to track whether
 * a session is idle, busy, or waiting for retry.
 */
struct TURBOT_CORE_API SessionStatusEvent {
    static constexpr const char* kEventName = "session.status";

    std::string   session_id;
    SessionStatus status        = SessionStatus::Idle;
    int           retry_attempt = 0;      ///< 1-based attempt number (Retry only)
    std::string   retry_message;          ///< Human-readable retry reason
    int64_t       retry_next_ms = 0;      ///< Estimated epoch-ms for next attempt
};

/**
 * @brief Broadcast whenever a new message (user or assistant) is appended.
 */
struct TURBOT_CORE_API SessionMessageEvent {
    static constexpr const char* kEventName = "session.message";

    std::string          session_id;
    turbot::core::Message message;
};

/**
 * @brief Broadcast for each raw stream event received from the LLM provider.
 */
struct TURBOT_CORE_API SessionStreamEvent {
    static constexpr const char* kEventName = "session.stream";

    std::string             session_id;
    turbot::core::StreamEvent event;
};

/**
 * @brief Broadcast when a tool call is initiated by the LLM.
 */
struct TURBOT_CORE_API SessionToolCallEvent {
    static constexpr const char* kEventName = "session.tool_call";

    std::string    session_id;
    std::string    tool_name;
    std::string    call_id;
    nlohmann::json input;
};

/**
 * @brief Broadcast when a tool call has finished (success or error).
 */
struct TURBOT_CORE_API SessionToolResultEvent {
    static constexpr const char* kEventName = "session.tool_result";

    std::string                   session_id;
    std::string                   tool_name;
    std::string                   call_id;
    turbot::core::tool::ToolResult result;
};

/**
 * @brief Broadcast once per step (after all tool calls in the step complete).
 *
 * Mirrors opencode's finish-step event which carries token usage and triggers
 * compaction checks.
 */
struct TURBOT_CORE_API SessionStepEvent {
    static constexpr const char* kEventName = "session.step";

    std::string              session_id;
    int                      step_number    = 0;
    turbot::core::TokenUsage tokens;
    double                   cost           = 0.0;
    bool                     has_tool_calls = false;
    std::vector<std::string> tool_names;
};

/**
 * @brief Broadcast when a non-retryable error terminates the session loop,
 *        or when a retry notification is issued during the retry wait.
 */
struct TURBOT_CORE_API SessionErrorEvent {
    static constexpr const char* kEventName = "session.error";

    std::string session_id;
    std::string message;
    std::string code;   ///< e.g. "llm_error", "retry", "doom_loop"
};

/**
 * @brief Broadcast when context-window pruning (prune old tool outputs) completes.
 *
 * Allows UI/consumers to show "Context was automatically summarized" indicators
 * and to update token-count displays without a round-trip.
 */
struct TURBOT_CORE_API SessionCompactionEvent {
    static constexpr const char* kEventName = "session.compaction";

    std::string session_id;
    int  pruned_parts   = 0;    ///< Number of tool-result parts that were pruned
    int64_t freed_tokens = 0;   ///< Estimated token count freed by pruning
    bool did_prune      = false;///< false = not enough tokens to warrant pruning
};

// ============================================================================
// Question events — published by QuestionTool ask/reply/reject
// ============================================================================

/**
 * @brief Broadcast when the LLM agent asks the user one or more questions.
 *
 * UI clients subscribe to this event to display the question UI.
 * After displaying, they call Question::reply() or Question::reject().
 */
struct TURBOT_CORE_API QuestionAskedEvent {
    static constexpr const char* kEventName = "question.asked";

    turbot::core::tool::builtin::QuestionRequest request;
};

/**
 * @brief Broadcast when the user provides answers to a pending question.
 */
struct TURBOT_CORE_API QuestionRepliedEvent {
    static constexpr const char* kEventName = "question.replied";

    std::string session_id;
    std::string request_id;
    std::vector<std::vector<std::string>> answers; ///< answers[i] = selected labels for question[i]
};

/**
 * @brief Broadcast when the user dismisses (rejects) a pending question.
 */
struct TURBOT_CORE_API QuestionRejectedEvent {
    static constexpr const char* kEventName = "question.rejected";

    std::string session_id;
    std::string request_id;
};

/**
 * @brief Broadcast after the title Agent has derived a short title for the session.
 *
 * Consumers (e.g. UI layers) can subscribe to this event to update the
 * session-title display without polling the Session object.
 */
struct TURBOT_CORE_API SessionTitleUpdatedEvent {
    static constexpr const char* kEventName = "session.title_updated";

    std::string session_id;
    std::string title;   ///< The newly assigned title
};

// ============================================================================
// Permission events — published by SessionLoop when a tool requests permission
// ============================================================================

/**
 * @brief Broadcast when a tool needs user permission during execution.
 *
 * ACP / UI subscribers display a permission prompt and then publish
 * PermissionRepliedEvent with the user's decision.
 */
struct TURBOT_CORE_API PermissionAskedEvent {
    static constexpr const char* kEventName = "permission.asked";

    std::string session_id;                         ///< Session that raised the request
    permission::PermissionRequest request;           ///< Full permission request payload
};

/**
 * @brief Broadcast by ACP / UI when the user has decided on a permission request.
 *
 * SessionLoop subscribes internally to unblock the ask_permission callback.
 */
struct TURBOT_CORE_API PermissionRepliedEvent {
    static constexpr const char* kEventName = "permission.replied";

    std::string session_id;                         ///< Session that originated the request
    std::string request_id;                         ///< Matches PermissionRequest::id
    permission::PermissionReply reply;              ///< User's decision
};

// ============================================================================
// T40: Part streaming delta events — mirrors OpenCode MessageV2.Event.PartDelta /
// MessageV2.Event.PartUpdated published by Session.updatePartDelta / updatePart
// ============================================================================

/**
 * @brief Broadcast for each incremental text delta of a streaming part.
 *
 * Mirrors OpenCode Bus.publish(MessageV2.Event.PartDelta, input) from
 * Session.updatePartDelta().  Subscribers (UI / ACP) append the delta to the
 * current displayed text without a full re-render.
 */
struct TURBOT_CORE_API PartDeltaEvent {
    static constexpr const char* kEventName = "session.part_delta";

    std::string session_id;   ///< Session that owns the part
    std::string message_id;   ///< Message that owns the part
    std::string part_id;      ///< Part being updated
    std::string field;        ///< Field receiving the delta (e.g. "text")
    std::string delta;        ///< Incremental text delta
};

/**
 * @brief Broadcast whenever a message part is fully written / updated.
 *
 * Mirrors OpenCode Bus.publish(MessageV2.Event.PartUpdated, { part }) from
 * Session.updatePart().  Contains the complete part JSON so subscribers can
 * replace their local copy atomically.
 */
struct TURBOT_CORE_API PartUpdatedEvent {
    static constexpr const char* kEventName = "session.part_updated";

    std::string    session_id;  ///< Session that owns the part
    std::string    message_id;  ///< Message that owns the part
    std::string    part_id;     ///< Part that was updated
    nlohmann::json part;        ///< Full part JSON payload
};

} // namespace turbot::core::session