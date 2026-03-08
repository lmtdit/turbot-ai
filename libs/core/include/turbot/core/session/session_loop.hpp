#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/session/session.hpp>
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/tool/tool.hpp>
#include <turbot/core/message/message.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace turbot::core::session {

/// Result of a loop iteration
enum class LoopResult {
    Continue,   ///< Continue the loop
    Stop,       ///< Stop the loop normally
    Compact     ///< Need to compact history
};

/// Convert LoopResult to string
[[nodiscard]] TURBOT_CORE_API std::string loop_result_to_string(LoopResult result);

/// Configuration for the session loop
struct TURBOT_CORE_API SessionLoopConfig {
    int max_iterations = 100;           ///< Maximum iterations before stopping
    int max_tokens = 128000;            ///< Maximum tokens before compaction
    int compact_threshold = 100000;     ///< Token count threshold for compaction
    bool auto_compact = true;           ///< Automatically compact when threshold reached
};

/// Callback types for the session loop
using MessageCallback = std::function<void(const core::Message&)>;
using ToolCallCallback = std::function<void(const std::string& tool_name, const nlohmann::json& input)>;
using ToolResultCallback = std::function<void(const std::string& tool_name, const tool::ToolResult& result)>;
using ErrorCallback = std::function<void(const std::string& error)>;

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

    /// Set callbacks
    void set_on_message(MessageCallback callback);
    void set_on_tool_call(ToolCallCallback callback);
    void set_on_tool_result(ToolResultCallback callback);
    void set_on_error(ErrorCallback callback);

    /// Run the loop with a user message
    /// @param user_message The user message to process
    /// @return Final result
    LoopResult run(const std::string& user_message);

    /// Run a single iteration (for testing or manual control)
    /// @return Loop result
    LoopResult step();

    /// Stop the loop
    void stop();

    /// Check if the loop is running
    [[nodiscard]] bool is_running() const noexcept { return running_.load(std::memory_order_relaxed); }

    /// Get the session
    [[nodiscard]] const Session& session() const noexcept { return session_; }

    /// Get the messages
    [[nodiscard]] const std::vector<core::Message>& messages() const noexcept { return messages_; }

    /// Get token count (estimated)
    [[nodiscard]] int token_count() const noexcept { return token_count_.load(std::memory_order_relaxed); }

private:
    Session session_;
    std::shared_ptr<agent::Agent> agent_;
    SessionLoopConfig config_;
    
    std::vector<core::Message> messages_;
    std::atomic<int>  token_count_{0};
    std::atomic<int>  iteration_count_{0};
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    
    mutable std::shared_ptr<std::atomic<bool>> abort_flag_;
    
    MessageCallback on_message_;
    ToolCallCallback on_tool_call_;
    ToolResultCallback on_tool_result_;
    ErrorCallback on_error_;

    /// Process a user message
    LoopResult process_user_message(const std::string& content);

    /// Process a tool call
    LoopResult process_tool_call(const std::string& tool_name, const nlohmann::json& input);

    /// Check if compaction is needed
    [[nodiscard]] bool needs_compaction() const noexcept;

    /// Estimate token count for a string
    [[nodiscard]] static int estimate_tokens(const std::string& text) noexcept;
};

} // namespace turbot::core::session
