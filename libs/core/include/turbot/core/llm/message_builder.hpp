#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/message/part.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace turbot::core {

/// Message format for different LLM providers
enum class MessageFormat {
    OpenAI,      ///< OpenAI chat completions format
    Anthropic,   ///< Anthropic messages format
    OpenAICompat ///< OpenAI-compatible format (for other providers)
};

/// Message role for LLM API
enum class LlmRole {
    System,    ///< System message
    User,      ///< User message
    Assistant, ///< Assistant message
    Tool       ///< Tool response message
};

/// Convert LlmRole to string
[[nodiscard]] TURBOT_CORE_API std::string_view llm_role_to_string(LlmRole role) noexcept;

/// Parse LlmRole from string
[[nodiscard]] TURBOT_CORE_API LlmRole llm_role_from_string(std::string_view str);

/// LLM Message - A single message formatted for LLM API
struct TURBOT_CORE_API LlmMessage {
    LlmRole role = LlmRole::User;
    std::optional<std::string> content;                           ///< Text content (for simple messages)
    std::optional<std::vector<nlohmann::json>> content_parts;     ///< Multi-part content
    std::optional<std::string> tool_call_id;                      ///< Tool call ID (for tool messages)
    std::optional<std::vector<nlohmann::json>> tool_calls;        ///< Tool calls (for assistant messages)
    std::optional<nlohmann::json> provider_options;               ///< Provider-specific options

    /// Create a simple text message
    [[nodiscard]] static LlmMessage create_system(const std::string& content);
    [[nodiscard]] static LlmMessage create_user(const std::string& content);
    [[nodiscard]] static LlmMessage create_assistant(const std::string& content);

    /// Create a multi-part message
    [[nodiscard]] static LlmMessage create_user_parts(std::vector<nlohmann::json> parts);
    [[nodiscard]] static LlmMessage create_assistant_parts(std::vector<nlohmann::json> parts);

    /// Create a tool response message
    [[nodiscard]] static LlmMessage create_tool_response(
        const std::string& tool_call_id,
        const std::string& content
    );

    /// Create an assistant message with tool calls
    [[nodiscard]] static LlmMessage create_assistant_with_tools(
        const std::optional<std::string>& content,
        const std::vector<nlohmann::json>& tool_calls
    );

    /// Convert to OpenAI format
    [[nodiscard]] nlohmann::json to_openai() const;

    /// Convert to Anthropic format
    [[nodiscard]] nlohmann::json to_anthropic() const;

    /// Convert to specified format
    [[nodiscard]] nlohmann::json to_format(MessageFormat format) const;
};

/// Provider capabilities for message transformation
struct TURBOT_CORE_API ProviderCapabilities {
    bool supports_system_messages = true;
    bool supports_vision = false;
    bool supports_audio = false;
    bool supports_video = false;
    bool supports_pdf = false;
    bool supports_interleaved_thinking = false;
    std::optional<std::string> thinking_field;  ///< Field name for interleaved thinking
};

/// Options for message building
struct TURBOT_CORE_API MessageBuilderOptions {
    MessageFormat format = MessageFormat::OpenAI;
    ProviderCapabilities capabilities;
    bool apply_caching = false;
    bool filter_empty_content = true;
    std::optional<std::string> provider_id;
    std::optional<std::string> model_id;
};

/// Content part builder for different part types
namespace content_parts {

/// Create a text content part
[[nodiscard]] TURBOT_CORE_API nlohmann::json text(const std::string& text);

/// Create an image content part from base64
[[nodiscard]] TURBOT_CORE_API nlohmann::json image_base64(
    const std::string& base64_data,
    const std::string& mime_type = "image/png"
);

/// Create an image content part from URL
[[nodiscard]] TURBOT_CORE_API nlohmann::json image_url(const std::string& url);

/// Create a file content part
[[nodiscard]] TURBOT_CORE_API nlohmann::json file(
    const std::string& filename,
    const std::string& base64_data,
    const std::string& mime_type
);

/// Create a tool call content part
[[nodiscard]] TURBOT_CORE_API nlohmann::json tool_call(
    const std::string& tool_call_id,
    const std::string& name,
    const nlohmann::json& arguments
);

/// Create a tool result content part
[[nodiscard]] TURBOT_CORE_API nlohmann::json tool_result(
    const std::string& tool_call_id,
    const std::string& content
);

/// Create a reasoning content part
[[nodiscard]] TURBOT_CORE_API nlohmann::json reasoning(const std::string& text);

} // namespace content_parts

/// MessageBuilder - Build messages for LLM API calls
///
/// This class provides a fluent API for constructing LLM messages,
/// with automatic format conversion for different providers.
///
/// Example usage:
/// @code
/// auto builder = MessageBuilder()
///     .set_format(MessageFormat::OpenAI)
///     .add_system("You are a helpful assistant.")
///     .add_user("Hello!")
///     .add_assistant("Hi! How can I help you today?");
///
/// auto messages = builder.build();
/// @endcode
class TURBOT_CORE_API MessageBuilder {
public:
    MessageBuilder() = default;
    explicit MessageBuilder(const MessageBuilderOptions& options);

    // ===== Configuration =====

    /// Set the output format
    MessageBuilder& set_format(MessageFormat format);

    /// Set provider capabilities
    MessageBuilder& set_capabilities(const ProviderCapabilities& caps);

    /// Set provider ID for provider-specific transformations
    MessageBuilder& set_provider(const std::string& provider_id, const std::string& model_id = "");

    /// Enable/disable caching hints
    MessageBuilder& set_caching(bool enabled);

    // ===== Add messages =====

    /// Add a system message
    MessageBuilder& add_system(const std::string& content);

    /// Add a user message
    MessageBuilder& add_user(const std::string& content);
    MessageBuilder& add_user_parts(std::vector<nlohmann::json> parts);

    /// Add an assistant message
    MessageBuilder& add_assistant(const std::string& content);
    MessageBuilder& add_assistant_parts(std::vector<nlohmann::json> parts);
    MessageBuilder& add_assistant_with_tools(
        const std::optional<std::string>& content,
        const std::vector<nlohmann::json>& tool_calls
    );

    /// Add a tool response message
    MessageBuilder& add_tool_response(const std::string& tool_call_id, const std::string& content);

    /// Add a pre-built LlmMessage
    MessageBuilder& add_message(const LlmMessage& message);

    /// Add messages from conversation history
    MessageBuilder& add_messages(const std::vector<LlmMessage>& messages);

    // ===== Build =====

    /// Build messages in the configured format
    [[nodiscard]] std::vector<nlohmann::json> build() const;

    /// Build messages with system messages separated (for Anthropic)
    [[nodiscard]] std::pair<std::vector<std::string>, std::vector<nlohmann::json>> build_with_system() const;

    /// Get the raw messages
    [[nodiscard]] const std::vector<LlmMessage>& get_messages() const noexcept { return messages_; }

    /// Clear all messages
    void clear() noexcept { messages_.clear(); }

    /// Get message count
    [[nodiscard]] size_t size() const noexcept { return messages_.size(); }

    /// Check if empty
    [[nodiscard]] bool empty() const noexcept { return messages_.empty(); }

private:
    std::vector<LlmMessage> messages_;
    MessageBuilderOptions options_;

    // Transform messages for specific providers
    void transform_for_provider();
    void apply_anthropic_caching(std::vector<nlohmann::json>& msgs) const;
    void filter_unsupported_parts();
    void normalize_tool_call_ids();
};

/// Message transformer utilities
namespace message_transform {

/// Convert Part to content part JSON for LLM API
[[nodiscard]] TURBOT_CORE_API nlohmann::json part_to_content(
    const Part& part,
    MessageFormat format
);

/// Convert multiple Parts to content parts
[[nodiscard]] TURBOT_CORE_API std::vector<nlohmann::json> parts_to_content(
    const std::vector<Part>& parts,
    MessageFormat format,
    const ProviderCapabilities& caps
);

/// Normalize messages for specific provider requirements
void TURBOT_CORE_API normalize_messages(
    std::vector<LlmMessage>& messages,
    const std::string& provider_id,
    const ProviderCapabilities& caps
);

/// Apply caching hints to messages (for Anthropic, etc.)
void TURBOT_CORE_API apply_caching_hints(
    std::vector<nlohmann::json>& messages,
    const std::string& provider_id
);

} // namespace message_transform

} // namespace turbot::core
