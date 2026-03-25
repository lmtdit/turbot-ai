#pragma once

/**
 * @file azure_provider.hpp
 * @brief Azure OpenAI Service provider implementation.
 *
 * C++ port of OpenCode provider registration for "azure" and
 * "azure-cognitive-services" in provider.ts.
 *
 * Azure OpenAI uses the same wire protocol as OpenAI but with a resource-name
 * based URL: https://{resource}.openai.azure.com/openai/deployments/{model}/…
 *
 * Authentication:
 *   - AZURE_OPENAI_API_KEY (primary)
 *   - AZURE_API_KEY        (fallback)
 */

#include <turbot/core/provider/provider.hpp>
#include <turbot/network/http_client.hpp>
#include <memory>
#include <string>

namespace turbot::core::provider {

/// Azure OpenAI Service provider.
///
/// Wraps the OpenAI-compatible endpoint exposed by Azure Cognitive Services.
/// Supports gpt-4o, gpt-4-turbo, gpt-35-turbo and o-series deployments.
class TURBOT_CORE_API AzureProvider : public Provider {
public:
    /// Default API version string for Azure OpenAI REST API.
    static constexpr std::string_view DEFAULT_API_VERSION = "2024-02-01";

    /// Environment variable for resource name.
    static constexpr std::string_view ENV_RESOURCE_NAME   = "AZURE_RESOURCE_NAME";
    /// Primary env var for API key.
    static constexpr std::string_view ENV_API_KEY          = "AZURE_OPENAI_API_KEY";
    /// Fallback env var for API key.
    static constexpr std::string_view ENV_API_KEY_FALLBACK = "AZURE_API_KEY";

    /// Create Azure provider with full configuration.
    /// @param config   Provider config.  config.base_url may override the
    ///                 auto-computed Azure endpoint.  If resource_name is
    ///                 provided it is stored; otherwise AZURE_RESOURCE_NAME is
    ///                 read from the environment at call time.
    explicit AzureProvider(const ProviderConfig& config);

    /// Convenience constructor: resource name + API key.
    AzureProvider(const std::string& resource_name, const std::string& api_key);

    ~AzureProvider() override = default;

    // ===== Provider interface =====

    [[nodiscard]] std::string id()   const override { return "azure"; }
    [[nodiscard]] std::string name() const override { return "Azure OpenAI"; }
    [[nodiscard]] bool        is_ready() const override;

    [[nodiscard]] std::vector<ModelInfo>         list_models()    const override;
    [[nodiscard]] std::optional<ModelInfo>       get_model(const std::string& model_id) const override;
    [[nodiscard]] bool                           supports_model(const std::string& model_id) const override;

    [[nodiscard]] ChatResponse chat(
        const std::vector<ChatMessage>& messages,
        const std::string&              model_id,
        const ChatOptions&              options = {}
    ) override;

    [[nodiscard]] ChatResponse chat_stream(
        const std::vector<ChatMessage>& messages,
        const std::string&              model_id,
        const ChatOptions&              options,
        StreamCallback                  callback
    ) override;

    [[nodiscard]] int64_t count_tokens(
        const std::vector<ChatMessage>& messages,
        const std::string&              model_id
    ) const override;

    [[nodiscard]] bool validate() override;

    // ===== Azure-specific =====

    /// Return the Azure resource name (from config or environment).
    [[nodiscard]] std::string resource_name() const;

    /// Build the base URL for a given deployment / model ID.
    /// Format: https://{resource}.openai.azure.com/openai/deployments/{model}
    [[nodiscard]] std::string deployment_url(const std::string& model_id) const;

private:
    ProviderConfig config_;
    std::string    resource_name_;
    std::string    api_version_;
    std::unique_ptr<network::HttpClient> http_client_;
    mutable std::vector<ModelInfo> models_cache_;

    void initialize_models();
    [[nodiscard]] network::HttpHeaders build_headers() const;
    [[nodiscard]] nlohmann::json build_request_body(
        const std::vector<ChatMessage>& messages,
        const std::string&              model_id,
        const ChatOptions&              options,
        bool                            stream
    ) const;
    [[nodiscard]] ChatResponse parse_response(const nlohmann::json& response) const;
    [[nodiscard]] ChatStreamEvent parse_stream_chunk(const std::string& chunk) const;
};

} // namespace turbot::core::provider
