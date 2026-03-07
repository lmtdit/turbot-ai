#pragma once

#include <turbot/core/provider/provider.hpp>
#include <turbot/network/http_client.hpp>
#include <memory>
#include <string>

namespace turbot::core::provider {

/// Alibaba Cloud Bailian (百炼) provider implementation
/// Supports Qwen series models (通义千问)
/// API docs: https://help.aliyun.com/document_detail/2712195.html
class TURBOT_CORE_API BailianProvider : public Provider {
public:
    /// Default API base URL
    static constexpr std::string_view DEFAULT_BASE_URL = "https://dashscope.aliyuncs.com/compatible-mode/v1";

    /// Create Bailian provider with configuration
    explicit BailianProvider(const ProviderConfig& config);

    /// Create Bailian provider with API key
    explicit BailianProvider(const std::string& api_key);

    ~BailianProvider() override = default;

    // ===== Provider interface =====

    [[nodiscard]] std::string id() const override { return "bailian"; }
    [[nodiscard]] std::string name() const override { return "阿里云百炼"; }
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
