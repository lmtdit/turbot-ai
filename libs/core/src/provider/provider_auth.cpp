// provider_auth.cpp — OpenCode `provider/auth.ts` C++ equivalent
//
// Implements API key resolution and authentication method descriptors.
// Aligned with: opencode/packages/opencode/src/provider/auth.ts

#include <turbot/core/provider/provider_auth.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <stdexcept>

namespace turbot::core::provider {

// ──────────────────────────────────────────────────────────────────────────────
// AuthMethod serialization
// ──────────────────────────────────────────────────────────────────────────────

nlohmann::json AuthMethod::to_json() const {
    nlohmann::json j;
    j["type"]  = (type == Type::OAuth) ? "oauth" : "api";
    j["label"] = label;
    if (!prompts.empty()) {
        nlohmann::json ps = nlohmann::json::array();
        for (const auto& p : prompts) {
            if (std::holds_alternative<AuthPromptText>(p)) {
                const auto& tp = std::get<AuthPromptText>(p);
                nlohmann::json pj;
                pj["type"]    = "text";
                pj["key"]     = tp.key;
                pj["message"] = tp.message;
                if (tp.placeholder) pj["placeholder"] = *tp.placeholder;
                ps.push_back(std::move(pj));
            } else {
                const auto& sp = std::get<AuthPromptSelect>(p);
                nlohmann::json pj;
                pj["type"]    = "select";
                pj["key"]     = sp.key;
                pj["message"] = sp.message;
                nlohmann::json opts = nlohmann::json::array();
                for (const auto& o : sp.options) {
                    nlohmann::json oj;
                    oj["label"] = o.label;
                    oj["value"] = o.value;
                    if (o.hint) oj["hint"] = *o.hint;
                    opts.push_back(std::move(oj));
                }
                pj["options"] = std::move(opts);
                ps.push_back(std::move(pj));
            }
        }
        j["prompts"] = std::move(ps);
    }
    return j;
}

AuthMethod AuthMethod::from_json(const nlohmann::json& j) {
    AuthMethod m;
    const std::string t = j.value("type", std::string{"api"});
    m.type  = (t == "oauth") ? Type::OAuth : Type::Api;
    m.label = j.value("label", std::string{});
    if (j.contains("prompts") && j["prompts"].is_array()) {
        for (const auto& pj : j["prompts"]) {
            const std::string pt = pj.value("type", std::string{"text"});
            if (pt == "text") {
                AuthPromptText tp;
                tp.key     = pj.value("key",     std::string{});
                tp.message = pj.value("message", std::string{});
                if (pj.contains("placeholder") && !pj["placeholder"].is_null())
                    tp.placeholder = pj["placeholder"].get<std::string>();
                m.prompts.emplace_back(std::move(tp));
            } else {
                AuthPromptSelect sp;
                sp.key     = pj.value("key",     std::string{});
                sp.message = pj.value("message", std::string{});
                if (pj.contains("options") && pj["options"].is_array()) {
                    for (const auto& oj : pj["options"]) {
                        AuthPromptSelect::Option o;
                        o.label = oj.value("label", std::string{});
                        o.value = oj.value("value", std::string{});
                        if (oj.contains("hint") && !oj["hint"].is_null())
                            o.hint = oj["hint"].get<std::string>();
                        sp.options.push_back(std::move(o));
                    }
                }
                m.prompts.emplace_back(std::move(sp));
            }
        }
    }
    return m;
}

// ──────────────────────────────────────────────────────────────────────────────
// Well-known provider → env var mapping
// Mirrors OpenCode's per-provider credential lookup logic.
// ──────────────────────────────────────────────────────────────────────────────

namespace {

struct ProviderEnvEntry {
    const char* provider_prefix; ///< provider_id prefix (e.g. "openai")
    const char* env_var;         ///< Environment variable name
};

// Ordered by most-specific prefix first. Matches by prefix so that
// "openai.compatible" also resolves to OPENAI_API_KEY.
constexpr std::array<ProviderEnvEntry, 16> kEnvVarMap{{
    {"anthropic",    "ANTHROPIC_API_KEY"},
    {"openai",       "OPENAI_API_KEY"},
    {"gemini",       "GEMINI_API_KEY"},
    {"google",       "GEMINI_API_KEY"},  // alias
    {"azure",        "AZURE_OPENAI_API_KEY"},
    {"groq",         "GROQ_API_KEY"},
    {"xai",          "XAI_API_KEY"},
    {"cohere",       "COHERE_API_KEY"},
    {"mistral",      "MISTRAL_API_KEY"},
    {"deepseek",     "DEEPSEEK_API_KEY"},
    {"bailian",      "DASHSCOPE_API_KEY"},
    {"kimi",         "MOONSHOT_API_KEY"},
    {"zhipu",        "ZHIPU_API_KEY"},
    {"iflow",        "IFLOW_API_KEY"},
    {"openrouter",   "OPENROUTER_API_KEY"},
    {"fireworks",    "FIREWORKS_API_KEY"},
}};

} // anonymous namespace

namespace ProviderAuth {

std::string env_var_name(const std::string& provider_id) {
    // Exact prefix match (case-insensitive comparison against lowercase id)
    std::string lower = provider_id;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const auto& entry : kEnvVarMap) {
        if (lower.rfind(entry.provider_prefix, 0) == 0)
            return entry.env_var;
    }
    return {};
}

std::string resolve_api_key(const std::string& provider_id,
                             const std::string& env_override) {
    // 1. Explicit env override
    if (!env_override.empty()) {
        const char* val = std::getenv(env_override.c_str()); // NOLINT(concurrency-mt-unsafe)
        if (val && val[0] != '\0') return val;
    }

    // 2. Well-known env var
    const std::string ev = env_var_name(provider_id);
    if (!ev.empty()) {
        const char* val = std::getenv(ev.c_str()); // NOLINT(concurrency-mt-unsafe)
        if (val && val[0] != '\0') return val;
    }

    // 3. Generic fallback: <PROVIDER_ID_UPPER>_API_KEY
    std::string upper = provider_id;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    // Replace non-alphanumeric chars with underscore
    for (char& c : upper) {
        if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
    }
    upper += "_API_KEY";
    const char* val = std::getenv(upper.c_str()); // NOLINT(concurrency-mt-unsafe)
    if (val && val[0] != '\0') return val;

    return {};
}

bool has_api_key(const std::string& provider_id) {
    return !resolve_api_key(provider_id).empty();
}

} // namespace ProviderAuth
} // namespace turbot::core::provider
