#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/message/token_usage.hpp>
#include <nlohmann/json.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace turbot::core::provider {

/// Input/output modality capabilities (mirrors opencode capabilities.input / capabilities.output)
struct TURBOT_CORE_API ModelModalities {
    bool text  = true;   ///< Supports text
    bool image = false;  ///< Supports image (was: vision)
    bool audio = false;  ///< Supports audio
    bool video = false;  ///< Supports video (new)
    bool pdf   = false;  ///< Supports PDF (new)

    [[nodiscard]] nlohmann::json to_json() const;
    static ModelModalities from_json(const nlohmann::json& j);
};

/// Model capabilities (mirrors opencode Provider.Model.capabilities)
struct TURBOT_CORE_API ModelCapabilities {
    bool temperature = true;     ///< Supports temperature parameter
    bool reasoning = false;      ///< Supports reasoning (o1-style)
    bool attachment = false;     ///< Supports file attachment (new — mirrors opencode capabilities.attachment)
    bool tool_call = true;       ///< Supports function calling
    bool streaming = true;       ///< Supports streaming responses

    /// Input modalities (replaces top-level vision/audio fields)
    ModelModalities input;
    /// Output modalities
    ModelModalities output;

    /// Interleaved reasoning: when non-empty, the model expects reasoning content
    /// to be extracted from assistant messages and placed in
    /// providerOptions.openaiCompatible[interleaved_field].
    ///
    /// Aligned with OpenCode capabilities.interleaved.field:
    ///   if (typeof model.capabilities.interleaved === "object" && interleaved.field) { … }
    std::string interleaved_field;  ///< e.g. "reasoning_content" for DeepSeek-R1

    // Backwards-compatibility accessors (deprecated — use input.image / input.audio)
    [[nodiscard]] bool vision() const noexcept { return input.image; }
    [[nodiscard]] bool audio() const noexcept  { return input.audio; }

    [[nodiscard]] nlohmann::json to_json() const;
    static ModelCapabilities from_json(const nlohmann::json& j);
};

/// API endpoint info for a model (mirrors opencode Provider.Model.api)
struct TURBOT_CORE_API ModelApiInfo {
    std::string id;   ///< API model ID (may differ from turbot Model id)
    std::string url;  ///< API base URL
    std::string npm;  ///< npm package name (e.g. "@ai-sdk/anthropic")

    [[nodiscard]] nlohmann::json to_json() const;
    static ModelApiInfo from_json(const nlohmann::json& j);
};

/// Model token limits (mirrors opencode Provider.Model.limit)
struct TURBOT_CORE_API ModelLimit {
    int context = 4096;                 ///< Context window size
    int output  = 32000;                ///< Max output tokens
    std::optional<int> input;           ///< Max input tokens (optional)

    [[nodiscard]] nlohmann::json to_json() const;
    static ModelLimit from_json(const nlohmann::json& j);
};

/// Model information (mirrors opencode Provider.Model)
struct TURBOT_CORE_API ModelInfo {
    std::string id;                        ///< Model identifier (e.g., "gpt-4")
    std::string provider_id;               ///< Provider identifier (e.g., "openai")
    std::string name;                      ///< Display name
    std::string description;               ///< Model description
    ModelCapabilities capabilities;        ///< Model capabilities
    ModelApiInfo api;                      ///< API endpoint info (new — mirrors model.api)
    ModelLimit limit;                      ///< Token limits (new — mirrors model.limit)
    std::string release_date;              ///< Release date string (new — mirrors model.release_date)
    nlohmann::json pricing;                ///< Pricing info {"input": 0.01, "output": 0.03, "cache": {...}}
    nlohmann::json limits;                 ///< Legacy: rate limits JSON (kept for compat)
    int64_t context_window = 4096;         ///< Legacy: context window (kept for compat, use limit.context)

    [[nodiscard]] nlohmann::json to_json() const;
    static ModelInfo from_json(const nlohmann::json& j);
};

/// Chat message role
enum class ChatRole {
    System,    ///< System message
    User,      ///< User message
    Assistant, ///< AI assistant message
    Tool       ///< Tool response message
};

/// Convert ChatRole to string
[[nodiscard]] TURBOT_CORE_API std::string_view chat_role_to_string(ChatRole role) noexcept;

/// Parse ChatRole from string
[[nodiscard]] TURBOT_CORE_API ChatRole chat_role_from_string(std::string_view str);

/// Tool call information
struct TURBOT_CORE_API ToolCall {
    std::string id;              ///< Tool call ID
    std::string type = "function";
    std::string name;            ///< Function name
    nlohmann::json arguments;    ///< Function arguments as JSON

    [[nodiscard]] nlohmann::json to_json() const;
    static ToolCall from_json(const nlohmann::json& j);
};

/// Chat message for API requests
struct TURBOT_CORE_API ChatMessage {
    ChatRole role = ChatRole::User;
    std::string content;
    std::optional<std::string> name;          ///< Name for tool messages
    std::optional<std::string> tool_call_id;  ///< Tool call ID for tool responses
    std::optional<std::vector<ToolCall>> tool_calls;  ///< Tool calls from assistant

    [[nodiscard]] nlohmann::json to_json() const;
    static ChatMessage from_json(const nlohmann::json& j);

    // Factory methods
    [[nodiscard]] static ChatMessage system(const std::string& content);
    [[nodiscard]] static ChatMessage user(const std::string& content);
    [[nodiscard]] static ChatMessage assistant(const std::string& content);
    [[nodiscard]] static ChatMessage assistant_with_tools(
        const std::string& content,
        const std::vector<ToolCall>& tool_calls
    );
    [[nodiscard]] static ChatMessage tool_result(
        const std::string& tool_call_id,
        const std::string& content
    );
};

/// Tool definition for function calling
struct TURBOT_CORE_API ToolDefinition {
    std::string type = "function";
    std::string name;
    std::string description;
    nlohmann::json parameters;  ///< JSON Schema for parameters

    [[nodiscard]] nlohmann::json to_json() const;
    static ToolDefinition from_json(const nlohmann::json& j);
};

/// Stream event type
enum class StreamEventType {
    TextDelta,   ///< Text content delta
    ToolCall,    ///< Tool call chunk
    Reasoning,   ///< Reasoning content
    Finish,      ///< Stream finished
    Error        ///< Error occurred
};

/// Convert StreamEventType to string
[[nodiscard]] TURBOT_CORE_API std::string_view stream_event_type_to_string(StreamEventType type) noexcept;

/// Parse StreamEventType from string
[[nodiscard]] TURBOT_CORE_API StreamEventType stream_event_type_from_string(std::string_view str);

/// Chat stream event
struct TURBOT_CORE_API ChatStreamEvent {
    StreamEventType type = StreamEventType::TextDelta;
    std::string content;
    std::optional<ToolCall> tool_call;  ///< For tool call events
    std::optional<std::string> finish_reason;  ///< For finish events
    std::optional<nlohmann::json> error;  ///< For error events
    TokenUsage usage;  ///< Token usage (available on finish)

    [[nodiscard]] nlohmann::json to_json() const;
    static ChatStreamEvent from_json(const nlohmann::json& j);
};

/// Chat request options
struct TURBOT_CORE_API ChatOptions {
    double temperature = 1.0;           ///< Sampling temperature
    double top_p = 1.0;                 ///< Top-p sampling
    int max_tokens = 4096;              ///< Maximum tokens to generate
    std::vector<std::string> stop;      ///< Stop sequences
    std::vector<ToolDefinition> tools;  ///< Available tools
    bool stream = false;                ///< Enable streaming
    std::optional<std::string> user;    ///< User identifier for tracking
    nlohmann::json extra;               ///< Provider-specific options

    [[nodiscard]] nlohmann::json to_json() const;
    static ChatOptions from_json(const nlohmann::json& j);
};

/// Chat response
struct TURBOT_CORE_API ChatResponse {
    std::string id;                     ///< Response ID
    std::string model;                  ///< Model used
    std::vector<ChatMessage> choices;   ///< Generated messages
    TokenUsage usage;                   ///< Token usage
    std::string finish_reason;          ///< Finish reason
    std::optional<nlohmann::json> error;  ///< Error if any

    [[nodiscard]] nlohmann::json to_json() const;
    static ChatResponse from_json(const nlohmann::json& j);

    /// Get the main text content
    [[nodiscard]] std::string get_text() const;

    /// Check if response is an error
    [[nodiscard]] bool is_error() const noexcept { return error.has_value(); }

    /// Check if response has tool calls
    [[nodiscard]] bool has_tool_calls() const;
};

/// Provider configuration
struct TURBOT_CORE_API ProviderConfig {
    std::string api_key;
    std::string base_url;               ///< API base URL (optional override)
    std::string organization;           ///< Organization ID (provider-specific)
    int timeout_seconds = 60;           ///< Request timeout
    int max_retries = 3;                ///< Maximum retry attempts
    bool verify_ssl = true;             ///< Verify SSL certificates
    std::string proxy;                  ///< Proxy URL
    nlohmann::json extra;               ///< Provider-specific configuration

    [[nodiscard]] nlohmann::json to_json() const;
    static ProviderConfig from_json(const nlohmann::json& j);
};

/// Stream callback type
using StreamCallback = std::function<bool(const ChatStreamEvent& event)>;

/// Provider interface - abstract base class for AI providers
class TURBOT_CORE_API Provider {
public:
    virtual ~Provider() = default;

    /// Get provider identifier
    [[nodiscard]] virtual std::string id() const = 0;

    /// Get provider display name
    [[nodiscard]] virtual std::string name() const = 0;

    /// Check if provider is configured and ready
    [[nodiscard]] virtual bool is_ready() const = 0;

    /// Get available models
    [[nodiscard]] virtual std::vector<ModelInfo> list_models() const = 0;

    /// Get specific model info
    [[nodiscard]] virtual std::optional<ModelInfo> get_model(const std::string& model_id) const = 0;

    /// Check if a model is supported
    [[nodiscard]] virtual bool supports_model(const std::string& model_id) const = 0;

    /// Perform a chat completion (non-streaming)
    /// @param messages Conversation messages
    /// @param model_id Model to use
    /// @param options Chat options
    /// @return Chat response
    [[nodiscard]] virtual ChatResponse chat(
        const std::vector<ChatMessage>& messages,
        const std::string& model_id,
        const ChatOptions& options = {}
    ) = 0;

    /// Perform a streaming chat completion
    /// @param messages Conversation messages
    /// @param model_id Model to use
    /// @param options Chat options
    /// @param callback Called for each stream event, return false to abort
    /// @return Final response (may be partial if aborted)
    [[nodiscard]] virtual ChatResponse chat_stream(
        const std::vector<ChatMessage>& messages,
        const std::string& model_id,
        const ChatOptions& options,
        StreamCallback callback
    ) = 0;

    /// Count tokens for messages (approximate)
    [[nodiscard]] virtual int64_t count_tokens(
        const std::vector<ChatMessage>& messages,
        const std::string& model_id
    ) const = 0;

    /// Validate API key/configuration
    [[nodiscard]] virtual bool validate() = 0;
};

/// Smart pointer for Provider
using ProviderPtr = std::shared_ptr<Provider>;

} // namespace turbot::core::provider
