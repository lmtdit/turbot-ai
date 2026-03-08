#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/llm/stream_event.hpp>
#include <turbot/core/llm/message_builder.hpp>
#include <turbot/core/provider/provider.hpp>
#include <turbot/core/message/token_usage.hpp>
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

namespace turbot::core::llm {

/// Tool definition for LLM calls
struct TURBOT_CORE_API LLMToolDefinition {
    std::string name;                   ///< Tool name
    std::string description;            ///< Tool description
    nlohmann::json parameters;          ///< JSON Schema for parameters

    [[nodiscard]] nlohmann::json to_json() const;
    static LLMToolDefinition from_json(const nlohmann::json& j);
};

/// Tool call result from tool execution
struct TURBOT_CORE_API ToolCallResult {
    std::string tool_call_id;           ///< ID of the tool call
    std::string content;                ///< Result content
    bool is_error = false;              ///< Whether execution failed

    [[nodiscard]] nlohmann::json to_json() const;
    static ToolCallResult from_json(const nlohmann::json& j);
};

/// LLM message for conversation
struct TURBOT_CORE_API LLMMessage {
    provider::ChatRole role;            ///< Message role
    std::string content;                ///< Message content
    std::optional<std::string> name;    ///< Name (for tool messages)
    std::optional<std::string> tool_call_id;  ///< Tool call ID (for tool responses)
    std::vector<ToolCallChunk> tool_calls;    ///< Tool calls (for assistant messages)

    [[nodiscard]] nlohmann::json to_json() const;
    static LLMMessage from_json(const nlohmann::json& j);

    // Factory methods
    [[nodiscard]] static LLMMessage system(const std::string& content);
    [[nodiscard]] static LLMMessage user(const std::string& content);
    [[nodiscard]] static LLMMessage assistant(const std::string& content);
    [[nodiscard]] static LLMMessage assistant_with_tools(
        const std::string& content,
        const std::vector<ToolCallChunk>& tool_calls
    );
    [[nodiscard]] static LLMMessage tool_result(
        const std::string& tool_call_id,
        const std::string& content,
        bool is_error = false
    );
};

/// Stream call parameters
struct TURBOT_CORE_API StreamParams {
    std::string session_id;             ///< Session identifier
    std::vector<LLMMessage> messages;   ///< Conversation messages
    std::vector<LLMToolDefinition> tools;  ///< Available tools
    std::optional<std::string> tool_choice;  ///< Tool choice strategy
    double temperature = 1.0;           ///< Sampling temperature
    std::optional<double> top_p;        ///< Top-p sampling
    std::optional<int> max_tokens;      ///< Maximum tokens to generate
    std::vector<std::string> stop;      ///< Stop sequences

    // Abort mechanism
    std::function<bool()> is_aborted;   ///< Callback to check if aborted

    [[nodiscard]] nlohmann::json to_json() const;
    static StreamParams from_json(const nlohmann::json& j);
};

/// Streaming state for incremental result tracking
class TURBOT_CORE_API StreamingState {
public:
    StreamingState() = default;

    /// Add an event to the stream
    void add_event(StreamEvent event);

    /// Get the next event (non-blocking)
    [[nodiscard]] std::optional<StreamEvent> pop_event();

    /// Check if there are pending events
    [[nodiscard]] bool has_events() const;

    /// Check if streaming is complete
    [[nodiscard]] bool is_done() const noexcept { return done_; }

    /// Mark streaming as complete
    void mark_done(FinishReason reason, const TokenUsage& usage = {});

    /// Mark streaming as error
    void mark_error(const std::string& message, const std::string& code = "");

    /// Get all accumulated events
    [[nodiscard]] const std::vector<StreamEvent>& events() const noexcept { return events_; }

    /// Get accumulated text content
    [[nodiscard]] std::string get_text() const;

    /// Get accumulated reasoning content
    [[nodiscard]] std::string get_reasoning() const;

    /// Get accumulated tool calls
    [[nodiscard]] std::vector<ToolCallChunk> get_tool_calls() const;

    /// Get final token usage
    [[nodiscard]] const TokenUsage& usage() const noexcept { return usage_; }

    /// Get finish reason
    [[nodiscard]] FinishReason finish_reason() const noexcept { return finish_reason_; }

    /// Get error message if any
    [[nodiscard]] const std::optional<std::string>& error() const noexcept { return error_; }

    /// Get response ID
    [[nodiscard]] const std::string& response_id() const noexcept { return response_id_; }

    /// Set response ID
    void set_response_id(const std::string& id) { response_id_ = id; }

    /// Get model name
    [[nodiscard]] const std::string& model() const noexcept { return model_; }

    /// Set model name
    void set_model(const std::string& model) { model_ = model; }

    /// Convert to StreamResult
    [[nodiscard]] StreamResult to_result() const;

private:
    std::vector<StreamEvent> events_;
    std::queue<StreamEvent> pending_;
    mutable std::mutex mutex_;
    bool done_ = false;
    FinishReason finish_reason_ = FinishReason::Stop;
    TokenUsage usage_;
    std::optional<std::string> error_;
    std::string response_id_;
    std::string model_;
};

/// LLM streaming result - wrapper around StreamingState for ergonomic API
class TURBOT_CORE_API LLMStreamResult {
public:
    LLMStreamResult() = default;
    explicit LLMStreamResult(std::shared_ptr<StreamingState> state);

    /// Get the next event (blocking until available or done)
    [[nodiscard]] std::optional<StreamEvent> next();

    /// Get all events (blocking until complete)
    [[nodiscard]] std::vector<StreamEvent> collect();

    /// Check if streaming is complete
    [[nodiscard]] bool is_done() const;

    /// Get the final text content
    [[nodiscard]] std::string final_text() const;

    /// Get the final reasoning content
    [[nodiscard]] std::string final_reasoning() const;

    /// Get all tool calls
    [[nodiscard]] std::vector<ToolCallChunk> tool_calls() const;

    /// Get token usage
    [[nodiscard]] TokenUsage usage() const;

    /// Get finish reason
    [[nodiscard]] FinishReason finish_reason() const;

    /// Check if there was an error
    [[nodiscard]] bool has_error() const;

    /// Get error message
    [[nodiscard]] std::optional<std::string> error() const;

    /// Get the underlying state
    [[nodiscard]] std::shared_ptr<StreamingState> state() const noexcept { return state_; }

private:
    std::shared_ptr<StreamingState> state_;
};

/// LLM call interface - provides high-level LLM operations
class TURBOT_CORE_API LLM {
public:
    /// Streaming chat completion
    /// @param provider The AI provider to use
    /// @param model_id Model identifier
    /// @param params Stream parameters
    /// @param handler Optional event handler (called for each event)
    /// @return Stream result
    [[nodiscard]] static LLMStreamResult stream(
        provider::Provider& provider,
        const std::string& model_id,
        const StreamParams& params,
        StreamEventHandler handler = nullptr
    );

    /// Non-streaming chat completion
    /// @param provider The AI provider to use
    /// @param model_id Model identifier
    /// @param params Stream parameters
    /// @return Complete text response
    [[nodiscard]] static std::string complete(
        provider::Provider& provider,
        const std::string& model_id,
        const StreamParams& params
    );

    /// Streaming with provider message format
    /// @param provider The AI provider to use
    /// @param model_id Model identifier
    /// @param messages Provider-format messages
    /// @param options Chat options
    /// @param handler Event handler
    /// @return Stream result
    [[nodiscard]] static LLMStreamResult stream_with_provider_format(
        provider::Provider& provider,
        const std::string& model_id,
        const std::vector<provider::ChatMessage>& messages,
        const provider::ChatOptions& options,
        StreamEventHandler handler = nullptr
    );

    /// Convert LLMMessage to provider ChatMessage
    [[nodiscard]] static provider::ChatMessage to_provider_message(const LLMMessage& msg);

    /// Convert LLMToolDefinition to provider ToolDefinition
    [[nodiscard]] static provider::ToolDefinition to_provider_tool(const LLMToolDefinition& tool);

    /// Convert StreamParams to ChatOptions
    [[nodiscard]] static provider::ChatOptions to_chat_options(const StreamParams& params);
};

} // namespace turbot::core::llm
