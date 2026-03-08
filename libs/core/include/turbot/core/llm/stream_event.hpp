#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/message/token_usage.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>

namespace turbot::core {

/// Stream event type - matches AI SDK v2 stream events
enum class StreamEventType : uint8_t {
    // Stream lifecycle
    Start,       ///< Stream started
    Finish,      ///< Stream finished
    Error,       ///< Error occurred

    // Text events
    TextStart,   ///< Text part started
    TextDelta,   ///< Text content delta
    TextEnd,     ///< Text part ended

    // Reasoning events (for thinking models)
    ReasoningStart, ///< Reasoning part started
    ReasoningDelta, ///< Reasoning content delta
    ReasoningEnd,   ///< Reasoning part ended

    // Tool call events
    ToolInputStart, ///< Tool call started
    ToolInputDelta, ///< Tool input arguments delta
    ToolInputEnd,   ///< Tool call input complete
    ToolCall,       ///< Complete tool call (non-streaming)

    // Step events
    StepStart,   ///< Agent step started
    StepFinish,  ///< Agent step finished

    // Source events (for citations)
    SourceStart, ///< Source reference started
    SourceEnd    ///< Source reference ended
};

/// Convert StreamEventType to string
[[nodiscard]] TURBOT_CORE_API std::string_view stream_event_type_to_string(StreamEventType type) noexcept;

/// Parse StreamEventType from string
[[nodiscard]] TURBOT_CORE_API StreamEventType stream_event_type_from_string(std::string_view str);

/// Finish reason for stream completion
enum class FinishReason : uint8_t {
    Stop,       ///< Normal stop
    Length,     ///< Max tokens reached
    ToolCall,   ///< Tool call requested
    ContentFilter, ///< Content filtered
    Error,      ///< Error occurred
    Other       ///< Other reason
};

/// Convert FinishReason to string
[[nodiscard]] TURBOT_CORE_API std::string_view finish_reason_to_string(FinishReason reason) noexcept;

/// Parse FinishReason from string
[[nodiscard]] TURBOT_CORE_API FinishReason finish_reason_from_string(std::string_view str);

/// Tool call chunk for streaming
struct TURBOT_CORE_API ToolCallChunk {
    std::string id;                      ///< Tool call ID
    std::string name;                    ///< Tool name
    std::string arguments;               ///< Arguments JSON (may be partial)
    bool is_complete = false;            ///< Whether arguments are complete

    [[nodiscard]] nlohmann::json to_json() const;
    static ToolCallChunk from_json(const nlohmann::json& j);
};

/// Reasoning metadata for thinking models
struct TURBOT_CORE_API ReasoningMetadata {
    std::optional<std::string> encrypted_content;  ///< Encrypted reasoning content (Copilot)
    std::optional<int64_t> budget_tokens;          ///< Token budget for reasoning
    std::optional<int64_t> used_tokens;            ///< Tokens used for reasoning

    [[nodiscard]] nlohmann::json to_json() const;
    static ReasoningMetadata from_json(const nlohmann::json& j);
};

/// Source information for citations
struct TURBOT_CORE_API SourceInfo {
    std::string id;                      ///< Source ID
    std::string type;                    ///< Source type (document, url, etc.)
    std::optional<std::string> title;    ///< Source title
    std::optional<std::string> url;      ///< Source URL
    std::optional<std::string> filename; ///< Source filename

    [[nodiscard]] nlohmann::json to_json() const;
    static SourceInfo from_json(const nlohmann::json& j);
};

/// Stream event - unified event type for LLM streaming
struct TURBOT_CORE_API StreamEvent {
    StreamEventType type = StreamEventType::TextDelta;
    std::string id;                      ///< Event/part ID

    // Content deltas
    std::string delta;                   ///< Text/reasoning delta content

    // Tool call data
    std::optional<ToolCallChunk> tool_call;

    // Reasoning metadata
    std::optional<ReasoningMetadata> reasoning;

    // Source info
    std::optional<SourceInfo> source;

    // Finish data
    std::optional<FinishReason> finish_reason;
    std::optional<TokenUsage> usage;

    // Error data
    std::optional<std::string> error_message;
    std::optional<std::string> error_code;

    // Provider-specific metadata
    std::optional<nlohmann::json> provider_metadata;

    // Timestamp
    int64_t timestamp = 0;

    // ===== Factory methods =====

    /// Create a start event
    [[nodiscard]] static StreamEvent create_start();

    /// Create a finish event
    [[nodiscard]] static StreamEvent create_finish(
        FinishReason reason,
        const TokenUsage& usage = {}
    );

    /// Create an error event
    [[nodiscard]] static StreamEvent create_error(
        const std::string& message,
        const std::optional<std::string>& code = std::nullopt
    );

    /// Create a text-start event
    [[nodiscard]] static StreamEvent create_text_start(const std::string& id);

    /// Create a text-delta event
    [[nodiscard]] static StreamEvent create_text_delta(const std::string& id, const std::string& delta);

    /// Create a text-end event
    [[nodiscard]] static StreamEvent create_text_end(const std::string& id);

    /// Create a reasoning-start event
    [[nodiscard]] static StreamEvent create_reasoning_start(const std::string& id);

    /// Create a reasoning-delta event
    [[nodiscard]] static StreamEvent create_reasoning_delta(const std::string& id, const std::string& delta);

    /// Create a reasoning-end event
    [[nodiscard]] static StreamEvent create_reasoning_end(const std::string& id);

    /// Create a tool-input-start event
    [[nodiscard]] static StreamEvent create_tool_input_start(
        const std::string& id,
        const std::string& tool_name
    );

    /// Create a tool-input-delta event
    [[nodiscard]] static StreamEvent create_tool_input_delta(
        const std::string& id,
        const std::string& delta
    );

    /// Create a tool-input-end event
    [[nodiscard]] static StreamEvent create_tool_input_end(const std::string& id);

    /// Create a complete tool-call event (for non-streaming)
    [[nodiscard]] static StreamEvent create_tool_call(const ToolCallChunk& tool_call);

    /// Create a step-start event
    [[nodiscard]] static StreamEvent create_step_start(
        const std::string& step_id,
        const std::string& name = ""
    );

    /// Create a step-finish event
    [[nodiscard]] static StreamEvent create_step_finish(
        const std::string& step_id,
        const std::optional<nlohmann::json>& result = std::nullopt
    );

    // ===== Type checking =====

    [[nodiscard]] bool is_start() const noexcept { return type == StreamEventType::Start; }
    [[nodiscard]] bool is_finish() const noexcept { return type == StreamEventType::Finish; }
    [[nodiscard]] bool is_error() const noexcept { return type == StreamEventType::Error; }
    [[nodiscard]] bool is_text_event() const noexcept {
        return type == StreamEventType::TextStart ||
               type == StreamEventType::TextDelta ||
               type == StreamEventType::TextEnd;
    }
    [[nodiscard]] bool is_reasoning_event() const noexcept {
        return type == StreamEventType::ReasoningStart ||
               type == StreamEventType::ReasoningDelta ||
               type == StreamEventType::ReasoningEnd;
    }
    [[nodiscard]] bool is_tool_event() const noexcept {
        return type == StreamEventType::ToolInputStart ||
               type == StreamEventType::ToolInputDelta ||
               type == StreamEventType::ToolInputEnd ||
               type == StreamEventType::ToolCall;
    }
    [[nodiscard]] bool is_step_event() const noexcept {
        return type == StreamEventType::StepStart ||
               type == StreamEventType::StepFinish;
    }

    // ===== Serialization =====

    [[nodiscard]] nlohmann::json to_json() const;
    static StreamEvent from_json(const nlohmann::json& j);

private:
    [[nodiscard]] static int64_t get_current_timestamp() noexcept;
};

/// Stream result - result of a streaming LLM call
struct TURBOT_CORE_API StreamResult {
    std::string id;                      ///< Response ID
    std::string model;                   ///< Model used
    std::vector<StreamEvent> events;     ///< All events from the stream
    TokenUsage usage;                    ///< Final token usage
    FinishReason finish_reason = FinishReason::Stop;
    std::optional<std::string> error;    ///< Error message if failed

    // ===== Text aggregation =====

    /// Get all text content from the stream
    [[nodiscard]] std::string get_text() const;

    /// Get all reasoning content from the stream
    [[nodiscard]] std::string get_reasoning() const;

    /// Get all tool calls from the stream
    [[nodiscard]] std::vector<ToolCallChunk> get_tool_calls() const;

    // ===== Serialization =====

    [[nodiscard]] nlohmann::json to_json() const;
    static StreamResult from_json(const nlohmann::json& j);
};

/// Stream event handler callback type
using StreamEventHandler = std::function<void(const StreamEvent&)>;

} // namespace turbot::core
