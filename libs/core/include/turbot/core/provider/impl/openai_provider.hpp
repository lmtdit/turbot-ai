#pragma once

#include <turbot/core/provider/provider.hpp>
#include <turbot/network/http_client.hpp>
#include <memory>
#include <string>

namespace turbot::core::provider {

/// OpenAI provider implementation
/// Supports GPT-4, GPT-3.5, o1, and other OpenAI models
class TURBOT_CORE_API OpenAIProvider : public Provider {
public:
    /// Default API base URL
    static constexpr std::string_view DEFAULT_BASE_URL = "https://api.openai.com/v1";

    /// Create OpenAI provider with configuration
    explicit OpenAIProvider(const ProviderConfig& config);

    /// Create OpenAI provider with API key
    explicit OpenAIProvider(const std::string& api_key);

    ~OpenAIProvider() override = default;

    // ===== Provider interface =====

    [[nodiscard]] std::string id() const override { return "openai"; }
    [[nodiscard]] std::string name() const override { return "OpenAI"; }
    [[nodiscard]] bool is_ready() const override;

    [[nodiscard]] std::vector<ModelInfo> list_models() const override;
    [[nodiscard]] std::optional<ModelInfo> get_model(const std::string& model_id) const override;
    [[nodiscard]] bool supports_model(const std::string& model_id) const override;

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
    [[nodiscard]] nlohmann::json build_request_body(
        const std::vector<ChatMessage>& messages,
        const std::string& model_id,
        const ChatOptions& options,
        bool stream
    ) const;
    [[nodiscard]] ChatResponse parse_response(const nlohmann::json& response) const;
    [[nodiscard]] ChatStreamEvent parse_stream_chunk(const std::string& chunk) const;
    [[nodiscard]] network::HttpHeaders build_headers() const;
};

} // namespace turbot::core::provider
