/**
 * @file provider_transform.cpp
 * @brief Provider message/option transformation layer implementation.
 *
 * C++ port of OpenCode packages/opencode/src/provider/transform.ts
 */

#include <turbot/core/provider/provider_transform.hpp>
#include <turbot/core/common/logger.hpp>
#include <algorithm>
#include <cctype>
#include <functional>
#include <regex>

namespace turbot::core::provider::ProviderTransform {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

/// Lowercase a string for case-insensitive matching.
std::string to_lower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return r;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

/// Replace unsupported file/image message parts with error text.
/// Mirrors ProviderTransform.unsupportedParts()
/// Sub-G69: checks all modalities (image/audio/video/pdf) via capabilities.input.{image,audio,video,pdf}
std::vector<nlohmann::json> unsupported_parts(
        std::vector<nlohmann::json> msgs, const ModelInfo& model) {
    const auto& inp = model.capabilities.input;

    for (auto& msg : msgs) {
        if (!msg.contains("role")) continue;
        if (msg["role"] != "user") continue;
        if (!msg.contains("content") || !msg["content"].is_array()) continue;

        auto& content = msg["content"];
        for (auto& part : content) {
            if (!part.contains("type")) continue;
            const std::string type = part["type"].get<std::string>();
            if (type != "file" && type != "image") continue;

            // Determine mime / modality
            std::string mime;
            std::string filename;
            if (type == "image" && part.contains("image")) {
                const std::string img_str = part["image"].get<std::string>();
                if (img_str.rfind("data:", 0) == 0) {
                    const auto sc = img_str.find(';');
                    if (sc != std::string::npos) {
                        mime = img_str.substr(5, sc - 5);
                    }
                    // Check for empty base64 data
                    const auto comma_pos = img_str.find(',');
                    if (comma_pos != std::string::npos) {
                        const std::string b64 = img_str.substr(comma_pos + 1);
                        if (b64.empty()) {
                            part = {{"type", "text"},
                                    {"text", "ERROR: Image file is empty or corrupted."
                                             " Please provide a valid image."}};
                            continue;
                        }
                    }
                }
            } else if (type == "file") {
                if (part.contains("mediaType")) mime = part["mediaType"].get<std::string>();
                if (part.contains("filename"))  filename = part["filename"].get<std::string>();
            }

            // Determine modality from mime
            std::string modality;
            if (mime.rfind("image/", 0) == 0) modality = "image";
            else if (mime.rfind("audio/", 0) == 0) modality = "audio";
            else if (mime.rfind("video/", 0) == 0) modality = "video";
            else if (mime == "application/pdf") modality = "pdf";

            if (modality.empty()) continue;

            // Sub-G69: check all 4 modalities via capabilities.input struct
            bool supported = false;
            if (modality == "image") supported = inp.image;
            else if (modality == "audio") supported = inp.audio;
            else if (modality == "video") supported = inp.video;
            else if (modality == "pdf")   supported = inp.pdf;

            if (!supported) {
                const std::string name_str = filename.empty()
                    ? modality
                    : ("\"" + filename + "\"");
                part = {{"type", "text"},
                        {"text", "ERROR: Cannot read " + name_str +
                                 " (this model does not support " + modality +
                                 " input). Inform the user."}};
            }
        }
    }
    return msgs;
}

/// Provider-specific message normalisation.
/// Mirrors ProviderTransform.normalizeMessages()
std::vector<nlohmann::json> normalize_messages(
        std::vector<nlohmann::json> msgs, const ModelInfo& model) {
    const std::string id_lower = to_lower(model.id);
    // Sub-G78: use model.api.npm for Anthropic/Bedrock detection (mirrors opencode L54)
    // Fall back to provider_id for compatibility
    const std::string& npm      = model.api.npm;
    const std::string& pid      = model.provider_id;
    const std::string  api_id_l = to_lower(model.api.id);

    // --- Anthropic: remove empty string/text/reasoning parts ---
    // opencode: model.api.npm === "@ai-sdk/anthropic" || model.api.npm === "@ai-sdk/amazon-bedrock"
    const bool is_anthropic_npm = (npm == "@ai-sdk/anthropic" || npm == "@ai-sdk/amazon-bedrock");
    const bool is_anthropic_pid = (pid == "anthropic" || pid == "amazon-bedrock");
    if (is_anthropic_npm || is_anthropic_pid) {
        std::vector<nlohmann::json> out;
        out.reserve(msgs.size());
        for (auto& msg : msgs) {
            if (!msg.contains("content")) { out.push_back(msg); continue; }
            if (msg["content"].is_string()) {
                if (msg["content"].get<std::string>().empty()) continue;
                out.push_back(msg);
                continue;
            }
            if (!msg["content"].is_array()) { out.push_back(msg); continue; }
            nlohmann::json filtered = nlohmann::json::array();
            for (const auto& part : msg["content"]) {
                if (part.contains("type")) {
                    const std::string t = part["type"].get<std::string>();
                    if ((t == "text" || t == "reasoning") &&
                        part.contains("text") && part["text"].is_string()) {
                        if (part["text"].get<std::string>().empty()) continue;
                    }
                }
                filtered.push_back(part);
            }
            if (filtered.empty()) continue;
            msg["content"] = std::move(filtered);
            out.push_back(msg);
        }
        msgs = std::move(out);
    }

    // --- Claude: sanitise toolCallId chars ---
    // Sub-G78: opencode uses model.api.id.includes("claude") — use api_id_l with fallback
    if (contains(api_id_l, "claude") || contains(id_lower, "claude")) {
        for (auto& msg : msgs) {
            if (!msg.contains("role") || !msg.contains("content")) continue;
            const std::string role = msg["role"].get<std::string>();
            if ((role == "assistant" || role == "tool") && msg["content"].is_array()) {
                for (auto& part : msg["content"]) {
                    if (!part.contains("type") || !part.contains("toolCallId")) continue;
                    const std::string t = part["type"].get<std::string>();
                    if (t == "tool-call" || t == "tool-result") {
                        std::string tcid = part["toolCallId"].get<std::string>();
                        std::string sanitised;
                        sanitised.reserve(tcid.size());
                        for (char c : tcid) {
                            if (std::isalnum(static_cast<unsigned char>(c)) ||
                                c == '_' || c == '-') {
                                sanitised += c;
                            } else {
                                sanitised += '_';
                            }
                        }
                        part["toolCallId"] = sanitised;
                    }
                }
            }
        }
        return msgs;
    }

    // --- Mistral: sanitise toolCallId to 9 alphanumeric chars ---
    // Sub-G78: use api_id_l for model id check (mirrors opencode model.api.id.toLowerCase())
    if (pid == "mistral" || contains(api_id_l, "mistral") || contains(api_id_l, "devstral") ||
        contains(id_lower, "mistral") || contains(id_lower, "devstral")) {
        std::vector<nlohmann::json> result;
        result.reserve(msgs.size() + 4);
        for (std::size_t i = 0; i < msgs.size(); ++i) {
            auto& msg = msgs[i];
            const std::string role = msg.value("role", std::string{});
            if ((role == "assistant" || role == "tool") && msg["content"].is_array()) {
                for (auto& part : msg["content"]) {
                    if (!part.contains("type") || !part.contains("toolCallId")) continue;
                    const std::string t = part["type"].get<std::string>();
                    if (t == "tool-call" || t == "tool-result") {
                        std::string tcid = part["toolCallId"].get<std::string>();
                        std::string alnum;
                        alnum.reserve(tcid.size());
                        for (char c : tcid) {
                            if (std::isalnum(static_cast<unsigned char>(c))) alnum += c;
                        }
                        if (alnum.size() > 9) alnum = alnum.substr(0, 9);
                        while (alnum.size() < 9) alnum += '0';
                        part["toolCallId"] = alnum;
                    }
                }
            }
            result.push_back(msg);
            // Mistral: tool → user must be separated by an assistant message
            if (role == "tool" && i + 1 < msgs.size()) {
                const std::string next_role = msgs[i + 1].value("role", std::string{});
                if (next_role == "user") {
                    result.push_back({
                        {"role", "assistant"},
                        {"content", nlohmann::json::array({{{"type","text"},{"text","Done."}}})}
                    });
                }
            }
        }
        return result;
    }

    // --- Interleaved reasoning (G42): extract reasoning parts from assistant messages ---
    // Aligned with OpenCode transform.ts L136-169:
    //   if (typeof model.capabilities.interleaved === "object" && interleaved.field) { … }
    // When interleaved_field is set (e.g. "reasoning_content" for DeepSeek-R1),
    // reasoning parts are moved to providerOptions.openaiCompatible[field].
    if (!model.capabilities.interleaved_field.empty()) {
        const std::string& field = model.capabilities.interleaved_field;
        for (auto& msg : msgs) {
            if (msg.value("role", std::string{}) != "assistant") continue;
            if (!msg.contains("content") || !msg["content"].is_array()) continue;

            // Collect reasoning text and build filtered content (no reasoning parts).
            std::string reasoning_text;
            nlohmann::json filtered = nlohmann::json::array();
            for (const auto& part : msg["content"]) {
                if (part.value("type", std::string{}) == "reasoning") {
                    if (part.contains("text") && part["text"].is_string()) {
                        reasoning_text += part["text"].get<std::string>();
                    }
                } else {
                    filtered.push_back(part);
                }
            }

            msg["content"] = std::move(filtered);

            if (!reasoning_text.empty()) {
                // Mirrors: providerOptions: { openaiCompatible: { [field]: reasoningText } }
                msg["providerOptions"]["openaiCompatible"][field] = std::move(reasoning_text);
            }
        }
        return msgs;
    }

    return msgs;
}

/// Apply prompt caching hints for Anthropic / Bedrock models.
/// Mirrors ProviderTransform.applyCaching()
std::vector<nlohmann::json> apply_caching(
        std::vector<nlohmann::json> msgs, const ModelInfo& model) {
    // Collect system messages (first 2) and final non-system messages (last 2)
    std::vector<nlohmann::json*> targets;
    {
        int sys_count = 0;
        for (auto& m : msgs) {
            if (m.value("role", "") == "system" && sys_count < 2) {
                targets.push_back(&m);
                ++sys_count;
            }
        }
    }
    {
        int non_sys_count = 0;
        for (int i = static_cast<int>(msgs.size()) - 1; i >= 0 && non_sys_count < 2; --i) {
            if (msgs[static_cast<std::size_t>(i)].value("role", "") != "system") {
                targets.push_back(&msgs[static_cast<std::size_t>(i)]);
                ++non_sys_count;
            }
        }
    }

    const nlohmann::json anthropic_cache = {{"type", "ephemeral"}};
    const nlohmann::json bedrock_cache   = {{"type", "default"}};

    const std::string& pid = model.provider_id;
    const bool is_anthropic  = pid == "anthropic";
    const bool is_bedrock    = pid == "amazon-bedrock" || contains(pid, "bedrock");
    const bool is_openrouter = pid == "openrouter";
    const bool is_copilot    = pid == "copilot" || pid == "github-copilot";
    // All others (openai-compatible, azure, etc.) use openaiCompatible key
    const bool is_openai_compat = !is_anthropic && !is_bedrock && !is_openrouter && !is_copilot;
    (void)is_openai_compat; // used implicitly via else branch

    for (auto* mp : targets) {
        auto& msg = *mp;
        // Anthropic and Bedrock use message-level providerOptions.
        // All other providers use content-part-level providerOptions (last part).
        const bool use_msg_level = is_anthropic || is_bedrock;

        if (!use_msg_level && msg.contains("content") && msg["content"].is_array()
                           && !msg["content"].empty()) {
            auto& last = msg["content"].back();
            if (last.is_object()) {
                // Route cache hints into the correct SDK providerOptions namespace
                if (is_openrouter)
                    last["providerOptions"]["openrouter"]["cacheControl"] = anthropic_cache;
                else if (is_copilot)
                    last["providerOptions"]["copilot"]["copilot_cache_control"] = anthropic_cache;
                else  // openaiCompatible / azure / openai / etc.
                    last["providerOptions"]["openaiCompatible"]["cache_control"] = anthropic_cache;
                continue;
            }
        }

        // Message-level providerOptions for Anthropic / Bedrock
        if (is_anthropic)
            msg["providerOptions"]["anthropic"]["cacheControl"] = anthropic_cache;
        else if (is_bedrock)
            msg["providerOptions"]["bedrock"]["cachePoint"] = bedrock_cache;
        else if (is_openrouter)
            msg["providerOptions"]["openrouter"]["cacheControl"] = anthropic_cache;
        else if (is_copilot)
            msg["providerOptions"]["copilot"]["copilot_cache_control"] = anthropic_cache;
        else
            msg["providerOptions"]["openaiCompatible"]["cache_control"] = anthropic_cache;
    }

    return msgs;
}

/// Map provider_id → SDK key for providerOptions remapping.
/// Mirrors sdkKey() in transform.ts — adapted for Turbot provider IDs.
/// Sub-G73: extended to cover google-vertex-anthropic and gateway mappings.
std::optional<std::string> sdk_key(const std::string& provider_id) {
    if (provider_id == "azure")                     return "openai";
    if (provider_id == "anthropic")                 return "anthropic";
    if (provider_id == "amazon-bedrock")            return "bedrock";
    if (provider_id == "google")                    return "google";
    if (provider_id == "google-vertex")             return "google";
    if (provider_id == "google-vertex-anthropic" ||
        provider_id == "vertex-anthropic")          return "anthropic";  // NEW: gv/anthropic → "anthropic"
    if (provider_id == "gateway")                   return "gateway";   // NEW
    if (provider_id == "openrouter")                return "openrouter";
    if (provider_id == "copilot" ||
        provider_id == "github-copilot")            return "copilot";
    return std::nullopt;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::vector<nlohmann::json>
message(std::vector<nlohmann::json> messages,
        const ModelInfo&             model,
        const nlohmann::json&        /*options*/) {
    messages = unsupported_parts(std::move(messages), model);
    messages = normalize_messages(std::move(messages), model);

    const std::string id_lower = to_lower(model.id);
    // Apply caching for Anthropic/Claude models (but NOT through gateway).
    // Mirrors opencode transform.ts message() L256-263:
    //   if (providerID === "anthropic" || api.id.includes("anthropic") || api.id.includes("claude")
    //       || model.id.includes("anthropic") || model.id.includes("claude")
    //       || api.npm === "@ai-sdk/anthropic")
    //   && model.api.npm !== "@ai-sdk/gateway"
    // Sub-G99: add npm == "@ai-sdk/anthropic" condition
    // Sub-G103: add api_id (model.api.id) check for anthropic/claude
    const std::string msg_api_id = to_lower(model.api.id);
    const bool is_gateway = model.provider_id == "gateway" || model.api.npm == "@ai-sdk/gateway";
    if (!is_gateway &&
        (model.provider_id == "anthropic" ||
         contains(msg_api_id, "anthropic") || contains(msg_api_id, "claude") ||
         contains(id_lower,   "anthropic") || contains(id_lower,   "claude") ||
         model.api.npm == "@ai-sdk/anthropic")) {
        messages = apply_caching(std::move(messages), model);
    }

    // Remap providerOptions keys (npm → SDK key)
    // Sub-G100: mirrors opencode L268 — sdkKey(model.api.npm) not model.provider_id
    // Skip remap for azure: opencode L269 model.api.npm !== "@ai-sdk/azure"
    const bool is_azure = model.api.npm == "@ai-sdk/azure" || model.provider_id == "azure";
    const auto key_npm = sdk_key(model.api.npm);
    const auto key = key_npm ? key_npm : sdk_key(model.provider_id);
    if (key && *key != model.provider_id && !is_azure) {
        for (auto& msg : messages) {
            const auto remap = [&](nlohmann::json& opts) {
                if (opts.is_null() || !opts.contains(model.provider_id)) return;
                opts[*key] = opts[model.provider_id];
                opts.erase(model.provider_id);
            };
            if (msg.contains("providerOptions")) remap(msg["providerOptions"]);
            if (msg.contains("content") && msg["content"].is_array()) {
                for (auto& part : msg["content"]) {
                    if (part.contains("providerOptions")) remap(part["providerOptions"]);
                }
            }
        }
    }

    return messages;
}

std::optional<double> temperature(const ModelInfo& model) {
    const std::string id = to_lower(model.id);
    if (contains(id, "qwen"))    return 0.55;
    if (contains(id, "claude"))  return std::nullopt;  // undefined in TS
    if (contains(id, "gemini"))  return 1.0;
    if (contains(id, "glm-4.6")) return 1.0;
    if (contains(id, "glm-4.7")) return 1.0;
    if (contains(id, "minimax-m2")) return 1.0;
    if (contains(id, "kimi-k2")) {
        if (contains(id, "thinking") || contains(id, "k2.") ||
            contains(id, "k2p")      || contains(id, "k2-5"))
            return 1.0;
        return 0.6;
    }
    return std::nullopt;
}

std::optional<double> top_p(const ModelInfo& model) {
    const std::string id = to_lower(model.id);
    if (contains(id, "qwen")) return 1.0;
    const std::vector<std::string> keys = {"minimax-m2","gemini","kimi-k2.5","kimi-k2p5","kimi-k2-5"};
    for (const auto& k : keys) {
        if (contains(id, k)) return 0.95;
    }
    return std::nullopt;
}

std::optional<int> top_k(const ModelInfo& model) {
    const std::string id = to_lower(model.id);
    if (contains(id, "minimax-m2")) {
        if (contains(id, "m2.") || contains(id, "m25") || contains(id, "m21")) return 40;
        return 20;
    }
    if (contains(id, "gemini")) return 64;
    return std::nullopt;
}

int max_output_tokens(const ModelInfo& model) {
    // Sub-G80: use model.limit.output (mirrors opencode: Math.min(model.limit.output, OUTPUT_TOKEN_MAX))
    const int model_limit = model.limit.output > 0 ? model.limit.output : OUTPUT_TOKEN_MAX;
    return std::min(model_limit, OUTPUT_TOKEN_MAX);
}

nlohmann::json options(const ModelInfo&     model,
                       const std::string&   session_id,
                       const nlohmann::json& extra_options) {
    nlohmann::json result = nlohmann::json::object();
    const std::string pid    = model.provider_id;
    const std::string id     = to_lower(model.id);
    // Sub-G85/G86/G87/G88/G89/G91: use model.api.npm and model.api.id for finer-grained matching
    // Mirrors opencode options() which dispatches on model.api.npm as well as model.providerID.
    const std::string& npm    = model.api.npm;
    const std::string  api_id = to_lower(model.api.id);

    // OpenAI / Copilot: store=false by default
    // Sub-G87: also match via model.api.npm (mirrors opencode L722-727)
    if (pid == "openai" || pid == "copilot" ||
        npm == "@ai-sdk/openai" || npm == "@ai-sdk/github-copilot") {
        result["store"] = false;
    }

    // OpenRouter: include usage; gemini-3 reasoning
    // Sub: use api_id for gemini-3 check (mirrors opencode L734: model.api.id.includes("gemini-3"))
    if (pid == "openrouter" || npm == "@openrouter/ai-sdk-provider") {
        result["usage"] = {{"include", true}};
        if (contains(api_id, "gemini-3") || contains(id, "gemini-3")) {
            result["reasoning"] = {{"effort", "high"}};
        }
    }

    // Baseten / ZhipuAI thinking
    // Sub-G91: use model.api.id for opencode provider kimi/glm check (mirrors opencode L741)
    if (pid == "baseten" ||
        (pid == "opencode" && (api_id == "kimi-k2-thinking" || api_id == "glm-4.6" ||
                               contains(id, "kimi-k2-thinking") || contains(id, "glm-4.6")))) {
        result["chat_template_args"] = {{"enable_thinking", true}};
    }
    // Sub-G90: add npm check for zai/zhipuai (mirrors opencode L746: npm === "@ai-sdk/openai-compatible")
    if ((pid == "zai" || pid == "zhipuai") && npm == "@ai-sdk/openai-compatible") {
        result["thinking"] = {{"type","enabled"}, {"clear_thinking", false}};
    }

    // Prompt cache key
    if (pid == "openai" || npm == "@ai-sdk/openai" || extra_options.value("setCacheKey", false)) {
        result["promptCacheKey"] = session_id;
    }

    // Google / Google-Vertex: thinkingConfig for reasoning models
    // Mirrors: model.api.npm === "@ai-sdk/google" || model.api.npm === "@ai-sdk/google-vertex"
    if (pid == "google" || pid == "google-vertex" || contains(pid, "vertex") ||
        npm == "@ai-sdk/google" || npm == "@ai-sdk/google-vertex") {
        if (model.capabilities.reasoning) {
            result["thinkingConfig"] = {{"includeThoughts", true}};
            if (contains(api_id, "gemini-3") || contains(id, "gemini-3")) {
                result["thinkingConfig"]["thinkingLevel"] = "high";
            }
        }
    }

    // Anthropic: Kimi-k2.5 thinking
    // Mirrors: (model.api.npm === "@ai-sdk/anthropic" || model.api.npm === "@ai-sdk/google-vertex/anthropic")
    // Sub-G81: use model.api.id (modelId) for kimi check (mirrors opencode L769-777)
    if (pid == "anthropic" || pid == "google-vertex-anthropic" || pid == "vertex-anthropic" ||
        npm == "@ai-sdk/anthropic" || npm == "@ai-sdk/google-vertex/anthropic") {
        const bool kimi_k2p5 =
            contains(api_id, "k2p5") || contains(api_id, "kimi-k2.5") || contains(api_id, "kimi-k2p5") ||
            contains(id,     "k2p5") || contains(id,     "kimi-k2.5") || contains(id,     "kimi-k2p5");
        if (kimi_k2p5) {
            // Sub-G81: use model.limit.output (mirrors opencode: model.limit.output / 2 - 1)
            const int output_limit = model.limit.output > 0 ? model.limit.output : 8192;
            result["thinking"] = {{"type","enabled"},
                                  {"budgetTokens", std::min(16000, output_limit / 2 - 1)}};
        }
    }

    // Alibaba-CN: enable_thinking for reasoning models (except kimi-k2-thinking)
    // Sub-G86: add npm check (mirrors opencode L788: npm === "@ai-sdk/openai-compatible")
    if (pid == "alibaba-cn" && model.capabilities.reasoning &&
        npm == "@ai-sdk/openai-compatible" &&
        !contains(api_id, "kimi-k2-thinking") && !contains(id, "kimi-k2-thinking")) {
        result["enable_thinking"] = true;
    }

    // gpt-5 series
    // Sub-G85: use api_id (model.api.id) to mirror opencode L794: input.model.api.id.includes("gpt-5")
    const bool is_gpt5 = !api_id.empty()
        ? (contains(api_id, "gpt-5") && !contains(api_id, "gpt-5-chat"))
        : (contains(id,     "gpt-5") && !contains(id,     "gpt-5-chat"));
    if (is_gpt5) {
        const bool no_pro = !api_id.empty() ? !contains(api_id, "gpt-5-pro") : !contains(id, "gpt-5-pro");
        if (no_pro) {
            result["reasoningEffort"] = "medium";
            result["reasoningSummary"] = "auto";
        }
        const std::string& gpt5_ref = !api_id.empty() ? api_id : id;
        if (contains(gpt5_ref, "gpt-5.") && !contains(gpt5_ref, "codex") &&
            !contains(gpt5_ref, "-chat") && pid != "azure") {
            result["textVerbosity"] = "low";
        }
        // opencode provider specific
        if (pid == "opencode" || pid.rfind("opencode", 0) == 0) {
            result["promptCacheKey"] = session_id;
            result["include"] = nlohmann::json::array({"reasoning.encrypted_content"});
            result["reasoningSummary"] = "auto";
        }
    }

    // Venice
    if (pid == "venice") {
        result["promptCacheKey"] = session_id;
    }

    // OpenRouter cache
    if (pid == "openrouter") {
        result["prompt_cache_key"] = session_id;
    }

    // Gateway
    if (pid == "gateway") {
        result["gateway"] = {{"caching", "auto"}};
    }

    return result;
}

nlohmann::json small_options(const ModelInfo& model) {
    const std::string pid    = model.provider_id;
    const std::string id     = to_lower(model.id);
    // Sub-G88/G89: use model.api.npm and model.api.id (mirrors opencode L836-866)
    const std::string& npm    = model.api.npm;
    const std::string  api_id = to_lower(model.api.id);

    // Sub-G88: add npm paths; gpt-5 check uses api_id (mirrors opencode L836-847)
    if (pid == "openai" || pid == "copilot" ||
        npm == "@ai-sdk/openai" || npm == "@ai-sdk/github-copilot") {
        // Use api_id when available (mirrors opencode: model.api.id.includes("gpt-5"))
        const std::string& ref = !api_id.empty() ? api_id : id;
        if (contains(ref, "gpt-5")) {
            if (contains(ref, "5."))
                return {{"store", false}, {"reasoningEffort", "low"}};
            return {{"store", false}, {"reasoningEffort", "minimal"}};
        }
        return {{"store", false}};
    }
    // Sub-G89: google gemini-3/thinkingBudget use api_id (mirrors opencode L848-854)
    if (pid == "google") {
        const std::string& ref = !api_id.empty() ? api_id : id;
        if (contains(ref, "gemini-3"))
            return {{"thinkingConfig", {{"thinkingLevel", "minimal"}}}};
        return {{"thinkingConfig", {{"thinkingBudget", 0}}}};
    }
    // Sub-G89: openrouter google check uses api_id (mirrors opencode L856-859)
    if (pid == "openrouter") {
        const std::string& ref = !api_id.empty() ? api_id : id;
        if (contains(ref, "google"))
            return {{"reasoning", {{"enabled", false}}}};
        return {{"reasoningEffort", "minimal"}};
    }
    if (pid == "venice") {
        return {{"veniceParameters", {{"disableThinking", true}}}};
    }
    return nlohmann::json::object();
}

nlohmann::json provider_options(const ModelInfo& model, const nlohmann::json& opts) {
    if (model.provider_id == "gateway") {
        // Split gateway vs upstream provider namespace
        const auto& api_id = model.id;
        const std::string slug = [&]() -> std::string {
            const auto slash = api_id.find('/');
            if (slash == std::string::npos) return {};
            const std::string raw = api_id.substr(0, slash);
            // SLUG_OVERRIDES: amazon → bedrock
            if (raw == "amazon") return "bedrock";
            return raw;
        }();

        nlohmann::json gw  = opts.contains("gateway") ? opts["gateway"] : nlohmann::json{};
        nlohmann::json rest = nlohmann::json::object();
        for (auto& [k, v] : opts.items()) {
            if (k != "gateway") rest[k] = v;
        }

        nlohmann::json result = nlohmann::json::object();
        if (!gw.is_null()) result["gateway"] = gw;
        if (!rest.empty()) {
            if (!slug.empty()) {
                result[slug] = rest;
            } else if (!gw.is_null() && gw.is_object() && !gw.is_array()) {
                nlohmann::json merged = gw;
                for (auto& [k, v] : rest.items()) merged[k] = v;
                result["gateway"] = merged;
            } else {
                result["gateway"] = rest;
            }
        }
        return result;
    }

    // Sub-G101: mirrors opencode L906 — sdkKey(model.api.npm) ?? model.providerID
    // Use npm first, fall back to provider_id
    const std::string key = sdk_key(model.api.npm)
        .value_or(sdk_key(model.provider_id).value_or(model.provider_id));
    return {{key, opts}};
}

nlohmann::json variants(const ModelInfo& model) {
    if (!model.capabilities.reasoning) return nlohmann::json::object();

    const std::string pid = model.provider_id;
    const std::string id  = to_lower(model.id);
    const std::string npm = model.api.npm;  // Sub-G92/G93: npm dispatch mirrors opencode switch(model.api.npm)

    // Sub-G74: isAnthropicAdaptive checks model.api.id (mirrors opencode: model.api.id.includes(v))
    // Fall back to model.id for compat.
    const std::string api_id = to_lower(model.api.id);
    const bool is_anthropic_adaptive =
        contains(api_id, "opus-4-6") || contains(api_id, "opus-4.6") ||
        contains(api_id, "sonnet-4-6") || contains(api_id, "sonnet-4.6") ||
        // fallback to model.id
        contains(id, "opus-4-6") || contains(id, "opus-4.6") ||
        contains(id, "sonnet-4-6") || contains(id, "sonnet-4.6");

    const std::vector<std::string> WIDELY   = {"low","medium","high"};
    const std::vector<std::string> ADAPTIVE = {"low","medium","high","max"};

    // Helper: build map of effort → { key: effort }
    auto make_efforts = [](const std::vector<std::string>& efforts,
                           const std::string& key) -> nlohmann::json {
        nlohmann::json r = nlohmann::json::object();
        for (const auto& e : efforts) r[e] = {{key, e}};
        return r;
    };

    // Anthropic Adaptive helper
    auto make_adaptive = [&](const std::string& thinking_field) -> nlohmann::json {
        nlohmann::json r = nlohmann::json::object();
        for (const auto& e : ADAPTIVE) {
            r[e] = {{thinking_field, {{"type","adaptive"}}}, {"effort", e}};
        }
        return r;
    };

    // Models that explicitly return no variants (per opencode)
    if (contains(id, "deepseek") || contains(id, "minimax") || contains(id, "glm") ||
        contains(id, "mistral")  || contains(id, "kimi")    || contains(id, "k2p5")) {
        return nlohmann::json::object();
    }

    // grok-3-mini special case
    if (contains(id, "grok") && contains(id, "grok-3-mini")) {
        // Sub-G93: mirrors opencode L353 — check npm === "@openrouter/ai-sdk-provider" OR pid == "openrouter"
        if (pid == "openrouter" || npm == "@openrouter/ai-sdk-provider") {
            return {
                {"low",  {{"reasoning", {{"effort","low"}}}}},
                {"high", {{"reasoning", {{"effort","high"}}}}}
            };
        }
        return {
            {"low",  {{"reasoningEffort","low"}}},
            {"high", {{"reasoningEffort","high"}}}
        };
    }
    if (contains(id, "grok")) return nlohmann::json::object();

    // --- Dispatch on provider_id (turbot uses provider_id, not npm package) ---
    // Sub-G97/G98: mirrors opencode switch(model.api.npm) — add npm fallback to all major branches

    if (pid == "openrouter" || npm == "@openrouter/ai-sdk-provider") {
        if (!contains(model.id, "gpt") && !contains(model.id, "gemini-3") &&
            !contains(model.id, "claude")) {
            return nlohmann::json::object();
        }
        // openrouter uses reasoning.effort wrapper
        const std::vector<std::string> eff = {"none","minimal","low","medium","high","xhigh"};
        nlohmann::json r = nlohmann::json::object();
        for (const auto& e : eff) r[e] = {{"reasoning", {{"effort", e}}}};
        return r;
    }

    if (pid == "github-copilot" || pid == "copilot" || npm == "@ai-sdk/github-copilot") {
        if (contains(model.id, "gemini")) return nlohmann::json::object();
        if (contains(model.id, "claude")) {
            return {{"thinking", {{"thinking_budget",4000}}}};
        }
        // Sub-G76: copilot efforts — "xhigh" added for codex-max/5.2/5.3 or gpt-5 with release_date gate
        // Mirrors opencode L441-446
        std::vector<std::string> efforts = WIDELY;
        if (contains(id, "5.1-codex-max") || contains(id, "5.2") || contains(id, "5.3")) {
            efforts.push_back("xhigh");
        } else if (contains(id, "gpt-5") &&
                   !model.release_date.empty() && model.release_date >= "2025-12-04") {
            efforts.push_back("xhigh");
        }
        nlohmann::json r = nlohmann::json::object();
        for (const auto& e : efforts) {
            r[e] = {{"reasoningEffort",e},{"reasoningSummary","auto"},
                    {"include", nlohmann::json::array({"reasoning.encrypted_content"})}};
        }
        return r;
    }

    if (pid == "azure" || npm == "@ai-sdk/azure") {
        if (id == "o1-mini") return nlohmann::json::object();
        std::vector<std::string> efforts = {"low","medium","high"};
        if (contains(id, "gpt-5-") || id == "gpt-5") efforts.insert(efforts.begin(), "minimal");
        nlohmann::json r = nlohmann::json::object();
        for (const auto& e : efforts) {
            r[e] = {{"reasoningEffort",e},{"reasoningSummary","auto"},
                    {"include", nlohmann::json::array({"reasoning.encrypted_content"})}};
        }
        return r;
    }

    if (pid == "openai" || npm == "@ai-sdk/openai") {
        if (id == "gpt-5-pro") return nlohmann::json::object();
        std::vector<std::string> efforts = WIDELY;
        if (contains(id, "codex")) {
            if (contains(id, "5.2") || contains(id, "5.3")) efforts.push_back("xhigh");
        } else {
            if (contains(id, "gpt-5-") || id == "gpt-5") efforts.insert(efforts.begin(), "minimal");
            // Sub-G75: release_date gates for "none" and "xhigh" (mirrors opencode L500-505)
            if (!model.release_date.empty() && model.release_date >= "2025-11-13") {
                efforts.insert(efforts.begin(), "none");
            }
            if (!model.release_date.empty() && model.release_date >= "2025-12-04") {
                efforts.push_back("xhigh");
            }
        }
        nlohmann::json r = nlohmann::json::object();
        for (const auto& e : efforts) {
            r[e] = {{"reasoningEffort",e},{"reasoningSummary","auto"},
                    {"include", nlohmann::json::array({"reasoning.encrypted_content"})}};
        }
        return r;
    }

    if (pid == "anthropic" || npm == "@ai-sdk/anthropic" || npm == "@ai-sdk/google-vertex/anthropic") {
        if (is_anthropic_adaptive) return make_adaptive("thinking");
        // Sub-G82: use model.limit.output (mirrors opencode: model.limit.output / 2 - 1)
        int out_limit = 32768;
        if (model.limit.output > 0) {
            out_limit = model.limit.output;
        } else if (model.limits.contains("max_tokens") && model.limits["max_tokens"].is_number_integer()) {
            out_limit = model.limits["max_tokens"].get<int>();
        }
        const int budget_high = std::min(16000, static_cast<int>(out_limit / 2 - 1));
        const int budget_max  = std::min(31999, out_limit - 1);
        return {
            {"high", {{"thinking",{{"type","enabled"},{"budgetTokens",budget_high}}}}},
            {"max",  {{"thinking",{{"type","enabled"},{"budgetTokens",budget_max}}}}}
        };
    }

    if (pid == "amazon-bedrock" || contains(pid, "bedrock") || npm == "@ai-sdk/amazon-bedrock") {
        if (is_anthropic_adaptive) {
            nlohmann::json r = nlohmann::json::object();
            for (const auto& e : ADAPTIVE) {
                r[e] = {{"reasoningConfig",{{"type","adaptive"},{"maxReasoningEffort",e}}}};
            }
            return r;
        }
        // Sub-G94: mirrors opencode L569 — use model.api.id (api_id) to detect anthropic/claude,
        // fall back to model.id for compat.
        if (contains(api_id, "anthropic") || contains(api_id, "claude") ||
            contains(id,     "anthropic") || contains(id,     "claude")) {
            return {
                {"high", {{"reasoningConfig",{{"type","enabled"},{"budgetTokens",16000}}}}},
                {"max",  {{"reasoningConfig",{{"type","enabled"},{"budgetTokens",31999}}}}}
            };
        }
        // Amazon Nova
        nlohmann::json r = nlohmann::json::object();
        for (const auto& e : WIDELY) {
            r[e] = {{"reasoningConfig",{{"type","enabled"},{"maxReasoningEffort",e}}}};
        }
        return r;
    }

    if (pid == "google" || contains(pid, "google-vertex") || contains(pid, "vertex") ||
        npm == "@ai-sdk/google" || npm == "@ai-sdk/google-vertex") {
        if (contains(id, "2.5")) {
            return {
                {"high", {{"thinkingConfig",{{"includeThoughts",true},{"thinkingBudget",16000}}}}},
                {"max",  {{"thinkingConfig",{{"includeThoughts",true},{"thinkingBudget",24576}}}}}
            };
        }
        std::vector<std::string> levels = {"low","high"};
        if (contains(id, "3.1")) levels = {"low","medium","high"};
        nlohmann::json r = nlohmann::json::object();
        for (const auto& e : levels) {
            r[e] = {{"thinkingConfig",{{"includeThoughts",true},{"thinkingLevel",e}}}};
        }
        return r;
    }

    if (pid == "mistral" || pid == "cohere" || pid == "perplexity" ||
        npm == "@ai-sdk/mistral" || npm == "@ai-sdk/cohere" || npm == "@ai-sdk/perplexity") {
        return nlohmann::json::object();
    }

    if (pid == "groq" || npm == "@ai-sdk/groq") {
        return make_efforts({"none","low","medium","high"}, "reasoningEffort");
    }

    // Gateway — has sub-routing based on upstream model type
    // Mirrors opencode @ai-sdk/gateway case in transform.ts
    if (pid == "gateway" || npm == "@ai-sdk/gateway") {
        if (contains(model.id, "anthropic")) {
            if (is_anthropic_adaptive) return make_adaptive("thinking");
            return {
                {"high", {{"thinking",{{"type","enabled"},{"budgetTokens",16000}}}}},
                {"max",  {{"thinking",{{"type","enabled"},{"budgetTokens",31999}}}}}
            };
        }
        if (contains(model.id, "google")) {
            if (contains(id, "2.5")) {
                return {
                    {"high", {{"thinkingConfig",{{"includeThoughts",true},{"thinkingBudget",16000}}}}},
                    {"max",  {{"thinkingConfig",{{"includeThoughts",true},{"thinkingBudget",24576}}}}}
                };
            }
            // Gemini-3 / other Google: thinkingLevel low/high
            nlohmann::json r = nlohmann::json::object();
            for (const auto& e : std::vector<std::string>{"low","high"}) {
                r[e] = {{"includeThoughts",true},{"thinkingLevel",e}};
            }
            return r;
        }
        // All other gateway models: full openai-style effort range
        const std::vector<std::string> eff = {"none","minimal","low","medium","high","xhigh"};
        return make_efforts(eff, "reasoningEffort");
    }

    // SAP AI provider — @jerome-benoit/sap-ai-provider-v2
    // Mirrors opencode @jerome-benoit/sap-ai-provider-v2 case in transform.ts
    if (pid == "sap" || npm == "@jerome-benoit/sap-ai-provider-v2") {
        if (contains(model.id, "anthropic")) {
            if (is_anthropic_adaptive) return make_adaptive("thinking");
            return {
                {"high", {{"thinking",{{"type","enabled"},{"budgetTokens",16000}}}}},
                {"max",  {{"thinking",{{"type","enabled"},{"budgetTokens",31999}}}}}
            };
        }
        if (contains(model.id, "gemini") && contains(id, "2.5")) {
            return {
                {"high", {{"thinkingConfig",{{"includeThoughts",true},{"thinkingBudget",16000}}}}},
                {"max",  {{"thinkingConfig",{{"includeThoughts",true},{"thinkingBudget",24576}}}}}
            };
        }
        // GPT / o-series models on SAP
        // o[1-9] pattern: check for "o1", "o2", ... "o9" prefixed by word boundary
        const bool is_gpt_or_o = contains(model.id, "gpt") || [&]() -> bool {
            for (char c = '1'; c <= '9'; ++c) {
                const std::string pat = std::string("o") + c;
                const auto pos = model.id.find(pat);
                if (pos != std::string::npos) {
                    // Ensure 'o' is at word boundary (start or preceded by non-alnum)
                    if (pos == 0 || !std::isalnum(static_cast<unsigned char>(model.id[pos-1])))
                        return true;
                }
            }
            return false;
        }();
        if (is_gpt_or_o) {
            return make_efforts(WIDELY, "reasoningEffort");
        }
        return nlohmann::json::object();
    }

    // openai-compatible / cerebras / togetherai / xai / deepinfra / venice
    // Sub-G92: mirrors opencode L458-469 switch(model.api.npm) — also match by npm package name
    if (pid == "openai-compatible" || pid == "cerebras" ||
        pid == "togetherai" || pid == "xai" || pid == "deepinfra" || pid == "venice" ||
        npm == "@ai-sdk/cerebras" || npm == "@ai-sdk/togetherai" ||
        npm == "@ai-sdk/xai" || npm == "@ai-sdk/deepinfra" ||
        npm == "venice-ai-sdk-provider" || npm == "@ai-sdk/openai-compatible") {
        return make_efforts(WIDELY, "reasoningEffort");
    }

    return nlohmann::json::object();
}

nlohmann::json schema(const ModelInfo& model, nlohmann::json sch) {
    const std::string id     = to_lower(model.id);
    const std::string api_id = to_lower(model.api.id);  // Sub-G95: mirrors opencode L934 model.api.id.includes("gemini")
    // Gemini sanitisation applies when provider is google OR model/api id contains "gemini"
    if (model.provider_id != "google" && !contains(id, "gemini") && !contains(api_id, "gemini")) {
        return sch;
    }

    // Gemini sanitisation: recursive lambda via std::function
    std::function<nlohmann::json(nlohmann::json)> sanitize = [&](nlohmann::json obj) -> nlohmann::json {
        if (obj.is_null() || (!obj.is_object() && !obj.is_array())) return obj;

        if (obj.is_array()) {
            nlohmann::json arr = nlohmann::json::array();
            for (auto& el : obj) arr.push_back(sanitize(el));
            return arr;
        }

        nlohmann::json result = nlohmann::json::object();
        for (auto& [k, v] : obj.items()) {
            if (k == "enum" && v.is_array()) {
                // Convert enum values to strings
                nlohmann::json str_enum = nlohmann::json::array();
                for (auto& ev : v) str_enum.push_back(ev.dump());
                result[k] = str_enum;
                // Change integer/number type to string
                if (result.contains("type")) {
                    const std::string t = result["type"].get<std::string>();
                    if (t == "integer" || t == "number") result["type"] = "string";
                }
                if (obj.contains("type")) {
                    const std::string t = obj["type"].get<std::string>();
                    if (t == "integer" || t == "number") result["type"] = "string";
                }
            } else if (v.is_object() || v.is_array()) {
                result[k] = sanitize(v);
            } else {
                result[k] = v;
            }
        }

        // Ensure array items has a type
        // Sub-G102: mirrors opencode hasSchemaIntent check — test all schema-intent keys
        // hasSchemaIntent = hasCombiner(anyOf/oneOf/allOf) OR has schema keys
        if (result.value("type","") == "array") {
            if (!result.contains("items") || result["items"].is_null()) {
                result["items"] = nlohmann::json::object();
            }
            // Only set items.type if items has no schema intent (mirrors opencode L990-998)
            auto has_schema_intent = [](const nlohmann::json& node) -> bool {
                if (!node.is_object()) return false;
                // hasCombiner: anyOf / oneOf / allOf
                if (node.contains("anyOf") || node.contains("oneOf") || node.contains("allOf"))
                    return true;
                // hasSchemaIntent keys
                static const std::vector<std::string> schema_keys = {
                    "type","properties","items","prefixItems","enum","const","$ref",
                    "additionalProperties","patternProperties","required",
                    "not","if","then","else"
                };
                for (const auto& k : schema_keys)
                    if (node.contains(k)) return true;
                return false;
            };
            if (result["items"].is_object() && !has_schema_intent(result["items"])) {
                result["items"]["type"] = "string";
            }
        }

        // Filter required to only existing properties
        if (result.value("type","") == "object" && result.contains("properties")
                && result.contains("required") && result["required"].is_array()) {
            nlohmann::json new_req = nlohmann::json::array();
            for (const auto& f : result["required"]) {
                if (result["properties"].contains(f.get<std::string>())) {
                    new_req.push_back(f);
                }
            }
            result["required"] = new_req;
        }

        // Remove properties/required from non-object types
        if (result.contains("type") && result["type"] != "object") {
            result.erase("properties");
            result.erase("required");
        }

        return result;
    };

    return sanitize(std::move(sch));
}

} // namespace turbot::core::provider::ProviderTransform
