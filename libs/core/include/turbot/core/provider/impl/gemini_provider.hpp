#pragma once

/**
 * @file gemini_provider.hpp
 * @brief Google Gemini (Google AI Studio) provider implementation.
 *
 * C++ port of OpenCode "@ai-sdk/google" createGoogleGenerativeAI usage.
 *
 * API: https://generativelanguage.googleapis.com/v1beta/
 * Auth: GEMINI_API_KEY or GOOGLE_GENERATIVE_AI_API_KEY
 */

#include <turbot/core/provider/provider.hpp>
#include <turbot/network/http_client.hpp>
#include <memory>
#include <string>

namespace turbot::core::provider {

/// Google Gemini (Google AI Studio) provider.
///
/// Supports Gemini 2.0/2.5 model families via the Google AI Studio REST API.
/// Uses the Gemini-native generateContent / streamGenerateContent endpoint
/// (not the OpenAI-compatible layer) for maximum capability alignment.
class TURBOT_CORE_API GeminiProvider : public Provider {
public:
    /// Base URL for Google AI Studio REST API.
    static constexpr std::string_view DEFAULT_BASE_URL =
        "https://generativelanguage.googleapis.com/v1beta";

    /// Primary API key env var (mirrors OpenCode GEMINI_API_KEY).
    static constexpr std::string_view ENV_API_KEY          = "GEMINI_API_KEY";
    /// Fallback env var.
    static constexpr std::string_view ENV_API_KEY_FALLBACK = "GOOGLE_GENERATIVE_AI_API_KEY";

    /// Create Gemini provider with configuration.
    explicit GeminiProvider(const ProviderConfig& config);

    /// Convenience constructor from API key.
    explicit GeminiProvider(const std::string& api_key);

    ~GeminiProvider() override = default;

    // ===== Provider interface =====

    [[nodiscard]] std::string id()   const override { return "google"; }
    [[nodiscard]] std::string name() const override { return "Google Gemini"; }
    [[nodiscard]] bool        is_ready() const override;

    [[nodiscard]] std::vector<ModelInfo>   list_models()    const override;
    [[nodiscard]] std::optional<ModelInfo> get_model(const std::string& model_id) const override;
    [[nodiscard]] bool                     supports_model(const std::string& model_id) const override;

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

private:
    ProviderConfig config_;
    std::unique_ptr<network::HttpClient> http_client_;
    mutable std::vector<ModelInfo> models_cache_;

    void initialize_models();

    /// Build Gemini generateContent request body from chat messages.
    [[nodiscard]] nlohmann::json build_request_body(
        const std::vector<ChatMessage>& messages,
        const ChatOptions&              options,
        bool                            stream
    ) const;

    /// Build Gemini REST URL.
    /// @param model_id  Gemini model name (e.g. "gemini-2.5-flash")
    /// @param stream    true → streamGenerateContent, false → generateContent
    [[nodiscard]] std::string build_url(
        const std::string& model_id, bool stream) const;

    /// Parse Gemini generateContent response into ChatResponse.
    [[nodiscard]] ChatResponse parse_response(const nlohmann::json& response) const;

    /// Parse a single SSE chunk from streamGenerateContent.
    [[nodiscard]] ChatStreamEvent parse_stream_chunk(const std::string& chunk) const;

    /// Build HTTP headers (includes x-goog-api-key).
    [[nodiscard]] network::HttpHeaders build_headers() const;

    /// Convert a ChatMessage to Gemini "content" format.
    [[nodiscard]] static nlohmann::json to_gemini_content(const ChatMessage& msg);
};

} // namespace turbot::core::provider
