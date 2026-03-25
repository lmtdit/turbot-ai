#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/session/session.hpp>
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/tool/tool.hpp>
#include <turbot/core/message/message.hpp>
#include <turbot/core/message/token_usage.hpp>
#include <turbot/core/llm/stream_event.hpp>
#include <turbot/core/llm/llm.hpp>
#include <turbot/core/provider/provider.hpp>
#include <turbot/core/permission/permission.hpp>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace turbot::core::session {

/// Result of a loop iteration
enum class LoopResult {
    Continue,   ///< Continue the loop
    Stop,       ///< Stop the loop normally
    Compact,    ///< Need to compact history
    Error       ///< Error occurred
};

/// Convert LoopResult to string
[[nodiscard]] TURBOT_CORE_API std::string loop_result_to_string(LoopResult result);

/// Configuration for the session loop
struct TURBOT_CORE_API SessionLoopConfig {
    int max_iterations = 100;           ///< Maximum iterations before stopping
    int max_tokens = 128000;            ///< Maximum tokens before compaction
    int compact_threshold = 100000;     ///< Token count threshold for compaction
    bool auto_compact = true;           ///< Automatically compact when threshold reached
    int doom_loop_threshold = 3;        ///< Max consecutive identical tool calls
};

/// Extended prompt input — mirrors OpenCode SessionPrompt.PromptInput.
///
/// Aligned with opencode/packages/opencode/src/session/prompt.ts PromptInput:
///   { sessionID, messageID?, model?, agent?, noReply?, format?, system?, variant?, parts? }
///
/// Fields used to control the next run() invocation.
struct TURBOT_CORE_API PromptInput {
    /// The text content of the user message.  At least one of `user_message`
    /// or `parts` must be non-empty.
    std::string user_message;

    /// Optional override model {providerID, modelID}.
    std::optional<std::string> provider_id;
    std::optional<std::string> model_id;

    /// Agent name override (matches OpenCode agent field).
    std::optional<std::string> agent;

    /// If true, only create the user message without running the LLM loop
    /// (mirrors OpenCode noReply: true).
    bool no_reply = false;

    /// Output format override — mirrors OpenCode MessageV2.Format.
    /// Accepted values: "text", "json_schema".
    std::optional<std::string> format;

    /// Additional system prompt injected for this turn only.
    std::optional<std::string> system;

    /// Model variant name (e.g. "thinking") — mirrors OpenCode variant field.
    std::optional<std::string> variant;
};

/// Step information for callbacks
struct TURBOT_CORE_API StepInfo {
    int step_number = 0;
    std::string message_id;
    TokenUsage tokens;
    double cost = 0.0;
    bool has_tool_calls = false;
    std::vector<std::string> tool_names;
};

/// Callback types for the session loop
using MessageCallback = std::function<void(const core::Message&)>;
using ToolCallCallback = std::function<void(const std::string& tool_name, const std::string& call_id, const nlohmann::json& input)>;
using ToolResultCallback = std::function<void(const std::string& tool_name, const std::string& call_id, const tool::ToolResult& result)>;
using ErrorCallback = std::function<void(const std::string& error, const std::string& code)>;
using StreamEventCallback = std::function<void(const StreamEvent& event)>;
using StepCallback = std::function<void(const StepInfo& info)>;

/// Permission request callback — invoked by execute_tool() when a tool needs user permission.
/// Returns the user's PermissionReply (may block until the user responds).
using PermissionCallback = std::function<permission::PermissionReply(const permission::PermissionRequest&)>;

/// Session loop - manages the main interaction loop for a session
class TURBOT_CORE_API SessionLoop {
public:
    explicit SessionLoop(const std::string& session_id);
    explicit SessionLoop(Session session);
    ~SessionLoop() = default;

    // Non-copyable, non-movable
    SessionLoop(const SessionLoop&) = delete;
    SessionLoop& operator=(const SessionLoop&) = delete;
    SessionLoop(SessionLoop&&) = delete;
    SessionLoop& operator=(SessionLoop&&) = delete;

    /// Set the configuration
    void set_config(const SessionLoopConfig& config);

    /// Get the configuration
    [[nodiscard]] const SessionLoopConfig& config() const noexcept { return config_; }

    /// Set the agent to use
    void set_agent(std::shared_ptr<agent::Agent> agent);

    /// Set the provider to use
    void set_provider(provider::Provider* provider);

    /// Set the model ID
    void set_model(const std::string& model_id);

    /// Set callbacks
    void set_on_message(MessageCallback callback);
    void set_on_tool_call(ToolCallCallback callback);
    void set_on_tool_result(ToolResultCallback callback);
    void set_on_error(ErrorCallback callback);
    void set_on_stream_event(StreamEventCallback callback);
    void set_on_step(StepCallback callback);

    /// Set permission request callback.
    /// When set, this callback is invoked whenever a tool requires user permission.
    /// If not set, the SessionLoop falls back to publishing PermissionAskedEvent via
    /// EventBus and waiting for a PermissionRepliedEvent reply (blocking the calling
    /// thread until the reply arrives or a timeout fires).
    void set_on_permission_request(PermissionCallback callback);

    /// Run the loop with a user message
    /// @param user_message The user message to process
    /// @return Final result
    LoopResult run(const std::string& user_message);

    /// Run the loop with extended prompt input (G43 alignment with OpenCode PromptInput).
    ///
    /// Applies agent/model/system/format/variant overrides before dispatching to
    /// the same internal loop as run(user_message).  Mirrors OpenCode's
    /// SessionPrompt.prompt(input) which calls loop({ sessionID }).
    ///
    /// @param input  Extended prompt input
    /// @return Final result
    LoopResult run(const PromptInput& input);

    /// Run a single iteration (for testing or manual control)
    /// @return Loop result
    LoopResult step();

    /// Stop the loop
    void stop();

    /// Check if the loop is running
    [[nodiscard]] bool is_running() const noexcept { return running_.load(std::memory_order_acquire); }

    /// Get the session
    [[nodiscard]] const Session& session() const noexcept { return session_; }

    /// Get the messages (thread-safe copy)
    [[nodiscard]] std::vector<core::Message> messages() const {
        std::lock_guard<std::mutex> lock(messages_mutex_);
        return messages_;
    }

    /// Get token count (estimated)
    [[nodiscard]] int token_count() const noexcept { return token_count_.load(std::memory_order_relaxed); }

    /// Get current step number
    [[nodiscard]] int step_number() const noexcept { return iteration_count_.load(std::memory_order_relaxed); }

    /// Get total token usage
    /// @note Not thread-safe; intended to be read after run() completes
    [[nodiscard]] TokenUsage total_usage() const noexcept { return total_usage_; }

    /// Get total cost
    /// @note Not thread-safe; intended to be read after run() completes
    [[nodiscard]] double total_cost() const noexcept { return total_cost_; }

private:
    Session session_;

    // agent_ is protected by agent_mutex_.
    // Lock ordering rule: agent_mutex_ must NEVER be acquired while
    // messages_mutex_ is already held. Always acquire agent_mutex_ first,
    // then release it before acquiring messages_mutex_.
    mutable std::mutex agent_mutex_;
    std::shared_ptr<agent::Agent> agent_;

    provider::Provider* provider_ = nullptr;
    std::string model_id_;
    SessionLoopConfig config_;

    std::vector<core::Message> messages_;
    mutable std::mutex messages_mutex_;
    std::atomic<int> token_count_{0};
    std::atomic<int> iteration_count_{0};
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};

    mutable std::shared_ptr<std::atomic<bool>> abort_flag_;

    /// Set to true when the user rejects a question (Question::RejectedError).
    /// Causes run() to return LoopResult::Stop at the start of the next step.
    bool blocked_ = false;

    // Token and cost tracking
    TokenUsage total_usage_;
    double total_cost_ = 0.0;

    // Doom loop detection
    std::string last_tool_call_;
    nlohmann::json last_tool_input_{};   ///< Last tool input for doom loop detection
    int same_tool_count_ = 0;

    // Tool definition cache (rebuilt lazily; invalidated when tool registry changes).
    // Since ToolRegistry is a singleton and tools are registered at startup,
    // we build the list once per run() invocation and reuse it throughout.
    //
    // Thread-safety note: tool_defs_dirty_ and cached_tool_defs_ are accessed only
    // from within run() / step() which are single-threaded by design (running_ flag
    // semantically ensures no concurrent run() calls). No mutex required.
    mutable std::vector<turbot::core::llm::LLMToolDefinition> cached_tool_defs_;
    mutable bool tool_defs_dirty_ = true;  ///< true = must rebuild before next LLM call

    MessageCallback on_message_;
    ToolCallCallback on_tool_call_;
    ToolResultCallback on_tool_result_;
    ErrorCallback on_error_;
    StreamEventCallback on_stream_event_;
    StepCallback on_step_;
    PermissionCallback on_permission_request_;  ///< Optional direct callback; if null, uses EventBus

    // ── Permission reply rendezvous ─────────────────────────────────────────
    // When on_permission_request_ is not set, ask_permission publishes a
    // PermissionAskedEvent and then blocks on this condvar waiting for a
    // PermissionRepliedEvent.  The EventBus subscription set up during
    // execute_tool() stores the reply here.
    std::mutex              perm_reply_mutex_;
    std::condition_variable perm_reply_cv_;
    // keyed by request_id → reply
    std::unordered_map<std::string, permission::PermissionReply> perm_reply_map_;
    // Alive flag shared with EventBus lambda to prevent UAF after SessionLoop destructs.
    // Destroyed before the mutex members; lambda checks weak_ptr before accessing *this.
    std::shared_ptr<bool> perm_alive_flag_{std::make_shared<bool>(true)};

    // ── G43: PromptInput overrides (set by run(PromptInput), cleared after run) ──
    std::optional<std::string> system_override_;  ///< Per-turn system prompt injection
    std::optional<std::string> format_override_;  ///< Output format override ("text"|"json_schema")
    std::optional<std::string> variant_override_; ///< Model variant override

    /// Process a user message
    LoopResult process_user_message(const std::string& content);

    /// Process LLM response and handle tool calls
    LoopResult process_llm_response();

    /// Execute a tool call
    tool::ToolResult execute_tool(const std::string& tool_name, const std::string& call_id, const nlohmann::json& input);

    /// Check for doom loop (repeated identical tool calls)
    [[nodiscard]] bool is_doom_loop(const std::string& tool_name, const nlohmann::json& input) const;

    /// Update doom loop tracking
    void update_doom_loop_tracking(const std::string& tool_name, const nlohmann::json& input);

    /// Check if compaction is needed
    [[nodiscard]] bool needs_compaction() const noexcept;

    /// Estimate token count for a string
    [[nodiscard]] static int estimate_tokens(const std::string& text) noexcept;

    /// Build LLM messages from conversation history
    [[nodiscard]] std::vector<turbot::core::llm::LLMMessage> build_llm_messages() const;

    /// Build tool definitions
    [[nodiscard]] std::vector<turbot::core::llm::LLMToolDefinition> build_tool_definitions() const;

    /// Derive and persist a short session title from the first user message.
    /// Called once at the end of run() when the session finishes normally.
    /// Publishes SessionTitleUpdatedEvent via EventBus after setting the title.
    void generate_title_if_needed();
};

} // namespace turbot::core::session
