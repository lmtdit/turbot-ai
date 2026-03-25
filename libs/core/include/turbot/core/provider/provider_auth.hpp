#pragma once

// provider_auth.hpp — C++ equivalent of opencode/src/provider/auth.ts
//
// Provides provider authentication method types and API key resolution logic.
// Note: OpenCode's auth.ts uses Effect + Plugin OAuth flows; this C++ equivalent
// provides the same *data model* and *API key validation* without the Effect runtime.
//
// Aligned with: opencode/packages/opencode/src/provider/auth.ts

#include <turbot/core/common/export.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace turbot::core::provider {

/**
 * @brief Prompt types for API key / credential collection.
 *
 * Mirrors OpenCode `ProviderAuth.Method.prompts` array item schemas.
 */
struct TURBOT_CORE_API AuthPromptText {
    std::string key;
    std::string message;
    std::optional<std::string> placeholder;
};

struct TURBOT_CORE_API AuthPromptSelect {
    struct Option {
        std::string label;
        std::string value;
        std::optional<std::string> hint;
    };
    std::string key;
    std::string message;
    std::vector<Option> options;
};

using AuthPrompt = std::variant<AuthPromptText, AuthPromptSelect>;

/**
 * @brief Authentication method descriptor.
 *
 * Mirrors OpenCode `ProviderAuth.Method` schema:
 *   { type: "oauth" | "api", label, prompts? }
 */
struct TURBOT_CORE_API AuthMethod {
    enum class Type { OAuth, Api };

    Type        type;
    std::string label;
    std::vector<AuthPrompt> prompts;  ///< Credential collection prompts (optional)

    [[nodiscard]] nlohmann::json to_json() const;
    static AuthMethod from_json(const nlohmann::json& j);
};

/**
 * @brief Provider authentication utilities.
 *
 * Mirrors the operational parts of OpenCode `ProviderAuth` namespace.
 * Provides API key resolution from environment variables and config.
 */
namespace ProviderAuth {

/**
 * @brief Resolve API key for a given provider.
 *
 * Resolution order (mirrors OpenCode config + Plugin credential lookup):
 *   1. Config file: providers.<provider_id>.apiKey
 *   2. Environment variable: <PROVIDER_ID>_API_KEY  (e.g. OPENAI_API_KEY)
 *   3. Well-known env fallbacks per provider (e.g. ANTHROPIC_API_KEY, GEMINI_API_KEY)
 *
 * @param provider_id  Provider identifier (e.g. "openai", "anthropic").
 * @param env_override Optional env-var name override (skips auto-detection).
 * @returns API key string, or empty if not found.
 */
[[nodiscard]] TURBOT_CORE_API std::string
resolve_api_key(const std::string& provider_id,
                const std::string& env_override = {});

/**
 * @brief Check whether an API key is available for a provider.
 *
 * @param provider_id  Provider identifier.
 * @returns true if an API key can be resolved.
 */
[[nodiscard]] TURBOT_CORE_API bool
has_api_key(const std::string& provider_id);

/**
 * @brief Return the well-known environment variable name for a provider.
 *
 * E.g. "openai" → "OPENAI_API_KEY", "anthropic" → "ANTHROPIC_API_KEY".
 * Returns empty string if no well-known mapping exists.
 *
 * @param provider_id  Provider identifier.
 */
[[nodiscard]] TURBOT_CORE_API std::string
env_var_name(const std::string& provider_id);

} // namespace ProviderAuth
} // namespace turbot::core::provider
