#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/llm/stream_event.hpp>
#include <turbot/core/message/message.hpp>
#include <turbot/core/message/token_usage.hpp>
#include <turbot/core/tool/tool.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace turbot::core::session {

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

} // namespace turbot::core::session
