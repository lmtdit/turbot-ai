#pragma once

#include <turbot/core/provider/provider.hpp>
#include <turbot/network/http_client.hpp>
#include <memory>
#include <string>

namespace turbot::core::provider {

/// Moonshot AI (Kimi) provider implementation
/// Supports Moonshot/Kimi models
/// API docs: https://platform.moonshot.cn/docs
class TURBOT_CORE_API KimiProvider : public Provider {
public:
    /// Default API base URL
    static constexpr std::string_view DEFAULT_BASE_URL = "https://api.moonshot.cn/v1";

    /// Create Kimi provider with configuration
    explicit KimiProvider(const ProviderConfig& config);

    /// Create Kimi provider with API key
    explicit KimiProvider(const std::string& api_key);

    ~KimiProvider() override = default;

    // ===== Provider interface =====

    [[nodiscard]] std::string id() const override { return "kimi"; }
    [[nodiscard]] std::string name() const override { return "Moonshot AI (Kimi)"; }
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
