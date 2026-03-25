// provider_error.cpp — OpenCode `provider/error.ts` C++ equivalent
//
// Implements context-overflow and API-error classification logic used by
// all Provider implementations.
//
// Aligned with: opencode/packages/opencode/src/provider/error.ts

#include <turbot/core/provider/provider_error.hpp>
#include <turbot/core/common/logger.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <regex>
#include <sstream>
#include <string_view>

namespace turbot::core::provider::ProviderError {

// ──────────────────────────────────────────────────────────────────────────────
// Context-overflow pattern matching (mirrors OVERFLOW_PATTERNS in error.ts)
// ──────────────────────────────────────────────────────────────────────────────

namespace {

// Compiled once at startup. Mirrors the OVERFLOW_PATTERNS array from error.ts.
const std::vector<std::regex>& overflow_patterns() {
    static const std::vector<std::regex> kPatterns = {
        std::regex("prompt is too long",                              std::regex::icase),
        std::regex("input is too long for requested model",           std::regex::icase),
        std::regex("exceeds the context window",                      std::regex::icase),
        std::regex("input token count.*exceeds the maximum",          std::regex::icase),
        std::regex("maximum prompt length is [0-9]+",                 std::regex::icase),
        std::regex("reduce the length of the messages",               std::regex::icase),
        std::regex("maximum context length is [0-9]+ tokens",         std::regex::icase),
        std::regex("exceeds the limit of [0-9]+",                     std::regex::icase),
        std::regex("exceeds the available context size",              std::regex::icase),
        std::regex("greater than the context length",                 std::regex::icase),
        std::regex("context window exceeds limit",                    std::regex::icase),
        std::regex("exceeded model token limit",                      std::regex::icase),
        std::regex("context[_ ]length[_ ]exceeded",                   std::regex::icase),
        std::regex("request entity too large",                        std::regex::icase),
        std::regex("context length is only [0-9]+ tokens",            std::regex::icase),
        std::regex("input length.*exceeds.*context length",           std::regex::icase),
    };
    return kPatterns;
}

// Mirrors the no-body 400/413 check at the end of isOverflow().
bool is_no_body_overflow(const std::string& message) {
    static const std::regex kNoBody(
        "^4(00|13)\\s*(status code)?\\s*\\(no body\\)",
        std::regex::icase);
    return std::regex_search(message, kNoBody);
}

// Try to parse a JSON string and return the object. Returns empty json on failure.
nlohmann::json try_parse_json(const std::string& s) {
    if (s.empty()) return {};
    try {
        auto j = nlohmann::json::parse(s);
        if (j.is_object() || j.is_array()) return j;
    } catch (...) {}
    return {};
}

// Simple HTTP status-code text table (subset used in error messages).
constexpr std::array<std::pair<int, const char*>, 8> kStatusTexts{{
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {413, "Request Entity Too Large"},
    {429, "Too Many Requests"},
    {500, "Internal Server Error"},
    {503, "Service Unavailable"},
}};

const char* status_text(int code) {
    for (const auto& [k, v] : kStatusTexts) {
        if (k == code) return v;
    }
    return nullptr;
}

} // anonymous namespace

// ──────────────────────────────────────────────────────────────────────────────
// Public API
// ──────────────────────────────────────────────────────────────────────────────

bool is_overflow(const std::string& message) {
    for (const auto& re : overflow_patterns()) {
        if (std::regex_search(message, re)) return true;
    }
    return is_no_body_overflow(message);
}

std::string format_error_message(
        const std::string& /*provider_id*/,
        int                http_status,
        const std::string& raw_message,
        const std::string& response_body) {
    // If the message is empty, fall back to response body or status text
    if (raw_message.empty()) {
        if (!response_body.empty()) return response_body;
        if (http_status != 0) {
            const char* txt = status_text(http_status);
            if (txt) return txt;
        }
        return "Unknown error";
    }

    // If no response body or if raw_message is already descriptive, return as-is
    if (response_body.empty()) return raw_message;
    if (http_status != 0 && raw_message == (status_text(http_status) ? status_text(http_status) : ""))
        return raw_message;

    // Response body is HTML — return raw_message with friendly override for auth errors
    static const std::regex kHtml("^\\s*<!doctype|^\\s*<html", std::regex::icase);
    if (std::regex_search(response_body, kHtml)) {
        if (http_status == 401)
            return "Unauthorized: request was blocked by a gateway or proxy. "
                   "Your authentication token may be missing or expired.";
        if (http_status == 403)
            return "Forbidden: request was blocked by a gateway or proxy. "
                   "You may not have permission to access this resource.";
        return raw_message;
    }

    // Try to extract a nested error message from JSON body
    const auto body_json = try_parse_json(response_body);
    if (!body_json.empty()) {
        std::string nested;
        if (body_json.contains("message") && body_json["message"].is_string())
            nested = body_json["message"].get<std::string>();
        else if (body_json.contains("error")) {
            const auto& err = body_json["error"];
            if (err.is_string())
                nested = err.get<std::string>();
            else if (err.is_object() && err.contains("message") && err["message"].is_string())
                nested = err["message"].get<std::string>();
        }
        if (!nested.empty())
            return raw_message + ": " + nested;
    }

    return raw_message + ": " + response_body;
}

// mirrors `ProviderError.parseStreamError(input)` from error.ts
std::optional<ParsedStreamError> parse_stream_error(const std::string& raw_body) {
    const auto body = try_parse_json(raw_body);
    if (body.empty()) return std::nullopt;
    if (!body.contains("type") || body["type"] != "error") return std::nullopt;

    const auto& err = body.value("error", nlohmann::json{});
    const std::string code = err.value("code", std::string{});

    if (code == "context_length_exceeded") {
        return ParsedStreamError{
            ParsedStreamError::Type::ContextOverflow,
            "Input exceeds context window of this model",
            raw_body,
            false,
        };
    }
    if (code == "insufficient_quota") {
        return ParsedStreamError{
            ParsedStreamError::Type::ApiError,
            "Quota exceeded. Check your plan and billing details.",
            raw_body,
            false,
        };
    }
    if (code == "usage_not_included") {
        return ParsedStreamError{
            ParsedStreamError::Type::ApiError,
            "To use Codex with your plan, upgrade to Plus.",
            raw_body,
            false,
        };
    }
    if (code == "invalid_prompt") {
        const std::string msg = (err.contains("message") && err["message"].is_string())
            ? err["message"].get<std::string>()
            : "Invalid prompt.";
        return ParsedStreamError{
            ParsedStreamError::Type::ApiError,
            msg,
            raw_body,
            false,
        };
    }
    return std::nullopt;
}

// mirrors `ProviderError.parseAPICallError(input)` from error.ts
ParsedAPICallError parse_api_call_error(
        const std::string& provider_id,
        int                http_status,
        const std::string& error_message,
        const std::string& response_body,
        bool               is_retryable_flag) {
    const std::string msg = format_error_message(
        provider_id, http_status, error_message, response_body);

    // Check for context-overflow patterns
    const auto body_json = try_parse_json(response_body);
    const bool ctx_overflow_code =
        !body_json.empty() &&
        body_json.contains("error") &&
        body_json["error"].is_object() &&
        body_json["error"].value("code", std::string{}) == "context_length_exceeded";

    if (is_overflow(msg) || http_status == 413 || ctx_overflow_code) {
        return ParsedAPICallError{
            ParsedAPICallError::Type::ContextOverflow,
            msg,
            http_status == 0 ? std::optional<int>{} : http_status,
            false,
            response_body.empty() ? std::optional<std::string>{} : response_body,
            std::nullopt,
        };
    }

    // OpenAI: 404 may be retryable (model temporarily unavailable)
    bool retryable = is_retryable_flag;
    if (provider_id.substr(0, 6) == "openai" && http_status == 404)
        retryable = true;

    return ParsedAPICallError{
        ParsedAPICallError::Type::ApiError,
        msg,
        http_status == 0 ? std::optional<int>{} : http_status,
        retryable,
        response_body.empty() ? std::optional<std::string>{} : response_body,
        std::nullopt,
    };
}

} // namespace turbot::core::provider::ProviderError
