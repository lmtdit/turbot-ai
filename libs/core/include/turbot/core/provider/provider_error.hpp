#pragma once

// provider_error.hpp — C++ equivalent of opencode/src/provider/error.ts
//
// Provides context-overflow and API-error classification logic for all
// Provider implementations, mirroring ProviderError namespace in OpenCode.

#include <turbot/core/common/export.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace turbot::core::provider {

/**
 * @brief Provider error classification utilities.
 *
 * Mirrors OpenCode `ProviderError` namespace from `provider/error.ts`.
 * Classifies raw HTTP/LLM errors into context-overflow vs. retryable API errors.
 */
namespace ProviderError {

/// Parsed stream error (mirrors OpenCode `ParsedStreamError`).
struct TURBOT_CORE_API ParsedStreamError {
    enum class Type { ContextOverflow, ApiError };

    Type        type;
    std::string message;
    std::string response_body;
    bool        is_retryable = false; ///< Only relevant for ApiError type
};

/// Parsed API call error (mirrors OpenCode `ParsedAPICallError`).
struct TURBOT_CORE_API ParsedAPICallError {
    enum class Type { ContextOverflow, ApiError };

    Type        type;
    std::string message;
    std::optional<int>         status_code;
    bool                       is_retryable = false;
    std::optional<std::string> response_body;
    std::optional<nlohmann::json> metadata; ///< e.g. {"url": "..."}
};

/**
 * @brief Classify a streaming error payload (JSON string or object).
 *
 * Mirrors `ProviderError.parseStreamError(input)` in error.ts.
 * @param raw_body  Raw response body string (possibly JSON).
 * @returns Parsed error, or nullopt if not a recognized stream error.
 */
[[nodiscard]] TURBOT_CORE_API std::optional<ParsedStreamError>
parse_stream_error(const std::string& raw_body);

/**
 * @brief Classify an API call error from HTTP status + message text.
 *
 * Mirrors `ProviderError.parseAPICallError(input)` in error.ts.
 * Inspects the error message for context-overflow patterns, applies
 * provider-specific retryability rules.
 *
 * @param provider_id   Provider identifier (e.g. "openai", "anthropic").
 * @param http_status   HTTP status code (0 if not available).
 * @param error_message Human-readable error message from the API.
 * @param response_body Optional raw response body.
 * @param is_retryable  Base retryability flag from the HTTP client.
 * @returns Classified error.
 */
[[nodiscard]] TURBOT_CORE_API ParsedAPICallError
parse_api_call_error(
    const std::string& provider_id,
    int                http_status,
    const std::string& error_message,
    const std::string& response_body,
    bool               is_retryable);

/**
 * @brief Check if an error message matches known context-overflow patterns.
 *
 * Mirrors the OVERFLOW_PATTERNS regex array in error.ts.
 * @param message  Error message string.
 * @returns true if the message looks like a context-overflow error.
 */
[[nodiscard]] TURBOT_CORE_API bool is_overflow(const std::string& message);

/**
 * @brief Extract a human-readable message from API error details.
 *
 * Tries to extract message from response body JSON (body.message / body.error).
 * Falls back to status code text or "Unknown error".
 *
 * @param provider_id   Provider ID for provider-specific handling.
 * @param http_status   HTTP status code.
 * @param raw_message   Raw error message from the provider SDK.
 * @param response_body Raw response body (may be JSON or HTML).
 * @returns Cleaned human-readable message.
 */
[[nodiscard]] TURBOT_CORE_API std::string
format_error_message(
    const std::string& provider_id,
    int                http_status,
    const std::string& raw_message,
    const std::string& response_body);

} // namespace ProviderError
} // namespace turbot::core::provider
