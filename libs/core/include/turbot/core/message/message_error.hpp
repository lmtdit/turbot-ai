#pragma once

// message_error.hpp
// Mirrors OpenCode MessageV2 named error types from message-v2.ts:
//   OutputLengthError, AbortedError, StructuredOutputError, AuthError,
//   APIError, ContextOverflowError
//
// Each type provides:
//  - A static `name` string (matches OpenCode NamedError.create first arg)
//  - A `to_json()` method that produces the serialized form stored in
//    MessageInfo::error (compatible with opencode JSON wire format)
//
// Usage:
//   message.set_error(MessageError::AbortedError{"context window exceeded"}.to_json());

#include <turbot/core/common/export.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>

namespace turbot::core::message {

namespace MessageError {

// ---------------------------------------------------------------------------
// OutputLengthError — mirrors OpenCode MessageV2.OutputLengthError
// NamedError.create("MessageOutputLengthError", z.object({}))
// ---------------------------------------------------------------------------
struct TURBOT_CORE_API OutputLengthError {
    static constexpr const char* kName = "MessageOutputLengthError";

    [[nodiscard]] nlohmann::json to_json() const {
        return nlohmann::json{{"name", kName}, {"data", nlohmann::json::object()}};
    }

    /// Check if a serialized error JSON is an OutputLengthError.
    [[nodiscard]] static bool is_instance(const nlohmann::json& j) {
        return j.is_object() && j.value("name", std::string{}) == kName;
    }
};

// ---------------------------------------------------------------------------
// AbortedError — mirrors OpenCode MessageV2.AbortedError
// NamedError.create("MessageAbortedError", z.object({ message: z.string() }))
// ---------------------------------------------------------------------------
struct TURBOT_CORE_API AbortedError {
    static constexpr const char* kName = "MessageAbortedError";
    std::string message;

    [[nodiscard]] nlohmann::json to_json() const {
        return nlohmann::json{{"name", kName}, {"data", {{"message", message}}}};
    }

    [[nodiscard]] static bool is_instance(const nlohmann::json& j) {
        return j.is_object() && j.value("name", std::string{}) == kName;
    }
};

// ---------------------------------------------------------------------------
// StructuredOutputError — mirrors OpenCode MessageV2.StructuredOutputError
// NamedError.create("StructuredOutputError",
//   z.object({ message: z.string(), retries: z.number() }))
// ---------------------------------------------------------------------------
struct TURBOT_CORE_API StructuredOutputError {
    static constexpr const char* kName = "StructuredOutputError";
    std::string message;
    int retries = 0;

    [[nodiscard]] nlohmann::json to_json() const {
        return nlohmann::json{
            {"name", kName},
            {"data", {{"message", message}, {"retries", retries}}}
        };
    }

    [[nodiscard]] static bool is_instance(const nlohmann::json& j) {
        return j.is_object() && j.value("name", std::string{}) == kName;
    }
};

// ---------------------------------------------------------------------------
// AuthError — mirrors OpenCode MessageV2.AuthError
// NamedError.create("ProviderAuthError",
//   z.object({ providerID: z.string(), message: z.string() }))
// ---------------------------------------------------------------------------
struct TURBOT_CORE_API AuthError {
    static constexpr const char* kName = "ProviderAuthError";
    std::string provider_id;
    std::string message;

    [[nodiscard]] nlohmann::json to_json() const {
        return nlohmann::json{
            {"name", kName},
            {"data", {{"providerID", provider_id}, {"message", message}}}
        };
    }

    [[nodiscard]] static bool is_instance(const nlohmann::json& j) {
        return j.is_object() && j.value("name", std::string{}) == kName;
    }
};

// ---------------------------------------------------------------------------
// APIError — mirrors OpenCode MessageV2.APIError
// NamedError.create("APIError", z.object({
//   message, statusCode?, isRetryable, responseHeaders?, responseBody?, metadata?
// }))
// ---------------------------------------------------------------------------
struct TURBOT_CORE_API APIError {
    static constexpr const char* kName = "APIError";
    std::string message;
    std::optional<int> status_code;
    bool is_retryable = false;
    std::optional<std::unordered_map<std::string, std::string>> response_headers;
    std::optional<std::string> response_body;
    std::optional<std::unordered_map<std::string, std::string>> metadata;

    [[nodiscard]] nlohmann::json to_json() const {
        nlohmann::json data{
            {"message", message},
            {"isRetryable", is_retryable}
        };
        if (status_code) data["statusCode"] = *status_code;
        if (response_body) data["responseBody"] = *response_body;
        if (response_headers) {
            nlohmann::json hdrs;
            for (const auto& [k, v] : *response_headers) hdrs[k] = v;
            data["responseHeaders"] = hdrs;
        }
        if (metadata) {
            nlohmann::json meta;
            for (const auto& [k, v] : *metadata) meta[k] = v;
            data["metadata"] = meta;
        }
        return nlohmann::json{{"name", kName}, {"data", data}};
    }

    [[nodiscard]] static bool is_instance(const nlohmann::json& j) {
        return j.is_object() && j.value("name", std::string{}) == kName;
    }
};

// ---------------------------------------------------------------------------
// ContextOverflowError — mirrors OpenCode MessageV2.ContextOverflowError
// NamedError.create("ContextOverflowError",
//   z.object({ message: z.string(), responseBody?: z.string() }))
// ---------------------------------------------------------------------------
struct TURBOT_CORE_API ContextOverflowError {
    static constexpr const char* kName = "ContextOverflowError";
    std::string message;
    std::optional<std::string> response_body;

    [[nodiscard]] nlohmann::json to_json() const {
        nlohmann::json data{{"message", message}};
        if (response_body) data["responseBody"] = *response_body;
        return nlohmann::json{{"name", kName}, {"data", data}};
    }

    [[nodiscard]] static bool is_instance(const nlohmann::json& j) {
        return j.is_object() && j.value("name", std::string{}) == kName;
    }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Returns the "name" field of a serialized error JSON, or empty string.
[[nodiscard]] inline std::string error_name(const nlohmann::json& j) {
    if (!j.is_object()) return {};
    return j.value("name", std::string{});
}

} // namespace MessageError
} // namespace turbot::core::message
