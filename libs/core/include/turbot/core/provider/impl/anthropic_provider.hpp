#pragma once

#include <turbot/core/provider/provider.hpp>
#include <turbot/network/http_client.hpp>
#include <memory>
#include <string>

namespace turbot::core::provider {

/// Anthropic provider implementation.
///
/// Supports Claude 3 and Claude 3.5 model families via the Anthropic Messages API.
/// API reference: https://docs.anthropic.com/en/api/messages
///
/// Differences from OpenAI:
///   - Auth header: "x-api-key" (not "Authorization: Bearer ...")
///   - Required header: "anthropic-version: 2023-06-01"
///   - Response format: content[] blocks (not choices[])
///   - System prompt passed as top-level "system" field
///   - Tool results use "tool_use" / "tool_result" content types
class TURBOT_CORE_API AnthropicProvider : public Provider {
public:
    /// Default API base URL.
    static constexpr std::string_view DEFAULT_BASE_URL = "https://api.anthropic.com/v1";

    /// Anthropic API version header value.
    static constexpr std::string_view API_VERSION = "2023-06-01";

    /// Create Anthropic provider with configuration.
    explicit AnthropicProvider(const ProviderConfig& config);

    /// Create Anthropic provider with API key.
    explicit AnthropicProvider(const std::string& api_key);

    ~AnthropicProvider() override = default;

    // ===== Provider interface =====

    [[nodiscard]] std::string id()   const override { return "anthropic"; }
    [[nodiscard]] std::string name() const override { return "Anthropic"; }
    [[nodiscard]] bool is_ready()    const override;

    [[nodiscard]] std::vector<ModelInfo>         list_models() const override;
    [[nodiscard]] std::optional<ModelInfo>        get_model(const std::string& model_id) const override;
    [[nodiscard]] bool                            supports_model(const std::string& model_id) const override;

    [[nodiscard]] ChatResponse chat(
        const std::vector<ChatMessage>& messages,
        const std::string& model_id,
        const ChatOptions& options = {}
    ) override;

    [[nodiscard]] ChatResponse chat_stream(
        const std::vector<ChatMessage>& messages,
        const std::string& model_id,
        const ChatOptions& options,
        StreamCallback callback
    ) override;

    [[nodiscard]] int64_t count_tokens(
        const std::vector<ChatMessage>& messages,
        const std::string& model_id
    ) const override;

    [[nodiscard]] bool validate() override;

private:
    ProviderConfig config_;
    std::unique_ptr<network::HttpClient> http_client_;
    mutable std::vector<ModelInfo> models_cache_;

    void initialize_models();

    [[nodiscard]] network::HttpHeaders build_headers() const;

    /// Build the Anthropic Messages API request body.
    /// Separates system messages into the top-level "system" field.
    [[nodiscard]] nlohmann::json build_request_body(
        const std::vector<ChatMessage>& messages,
        const std::string& model_id,
        const ChatOptions& options,
        bool stream
    ) const;

    /// Parse a non-streaming Anthropic response.
    [[nodiscard]] ChatResponse parse_response(const nlohmann::json& response) const;

    /// Parse a single SSE chunk from the streaming response.
    [[nodiscard]] ChatStreamEvent parse_stream_chunk(const std::string& chunk) const;
};

} // namespace turbot::core::provider
