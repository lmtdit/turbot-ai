/**
 * @file gemini_provider.cpp
 * @brief Google Gemini (Google AI Studio) provider implementation.
 *
 * Implements the Gemini generateContent REST API.
 * Reference: https://ai.google.dev/api/generate-content
 */

#include <turbot/core/provider/impl/gemini_provider.hpp>
#include <turbot/core/message/message.hpp>
#include <turbot/core/common/logger.hpp>
#include <cstdlib>
#include <sstream>

namespace turbot::core::provider {

namespace {

/// Trim leading and trailing whitespace from a string.
static std::string trim_whitespace(const std::string& s) {
    const std::size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    const std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

GeminiProvider::GeminiProvider(const ProviderConfig& config)
    : config_(config)
    , http_client_(std::make_unique<network::HttpClient>()) {

    if (config_.base_url.empty()) {
        config_.base_url = std::string(DEFAULT_BASE_URL);
    }

    // Resolve API key from environment if not in config
    if (config_.api_key.empty()) {
        const char* k1 = std::getenv("GEMINI_API_KEY");
        if (k1 && *k1 != '\0') {
            config_.api_key = k1;
        } else {
            const char* k2 = std::getenv("GOOGLE_GENERATIVE_AI_API_KEY");
            if (k2 && *k2 != '\0') config_.api_key = k2;
        }
    }

    http_client_->set_timeout(config_.timeout_seconds);
    if (!config_.proxy.empty()) {
        http_client_->set_proxy(config_.proxy);
    }
    http_client_->set_ssl_verify(config_.verify_ssl);

    initialize_models();
}

GeminiProvider::GeminiProvider(const std::string& api_key)
    : GeminiProvider(ProviderConfig{.api_key = api_key}) {}

// ---------------------------------------------------------------------------
// Provider interface
// ---------------------------------------------------------------------------

bool GeminiProvider::is_ready() const {
    return !config_.api_key.empty();
}

void GeminiProvider::initialize_models() {
    models_cache_ = {
        // Gemini 2.5 Flash — most capable and fast (reasoning)
        {
            .id = "gemini-2.5-flash",
            .provider_id = "google",
            .name = "Gemini 2.5 Flash",
            .description = "Most capable Gemini model with adaptive thinking",
            .capabilities = {.temperature = true, .reasoning = true,
                             .tool_call = true, .streaming = true, .vision = true},
            .pricing = {{"input", 0.000075}, {"output", 0.0003}},
            .limits = {{"max_tokens", 65536}, {"rpm", 1000}},
            .context_window = 1048576
        },
        // Gemini 2.5 Pro — highest intelligence
        {
            .id = "gemini-2.5-pro",
            .provider_id = "google",
            .name = "Gemini 2.5 Pro",
            .description = "Most intelligent Gemini model for complex reasoning",
            .capabilities = {.temperature = true, .reasoning = true,
                             .tool_call = true, .streaming = true, .vision = true},
            .pricing = {{"input", 0.00125}, {"output", 0.01}},
            .limits = {{"max_tokens", 65536}, {"rpm", 150}},
            .context_window = 1048576
        },
        // Gemini 2.0 Flash — speed focused
        {
            .id = "gemini-2.0-flash",
            .provider_id = "google",
            .name = "Gemini 2.0 Flash",
            .description = "Fast and efficient Gemini 2.0 model",
            .capabilities = {.temperature = true, .reasoning = false,
                             .tool_call = true, .streaming = true, .vision = true},
            .pricing = {{"input", 0.0001}, {"output", 0.0004}},
            .limits = {{"max_tokens", 8192}, {"rpm", 1000}},
            .context_window = 1048576
        },
        // Gemini 2.0 Flash Thinking — reasoning
        {
            .id = "gemini-2.0-flash-thinking-exp",
            .provider_id = "google",
            .name = "Gemini 2.0 Flash Thinking",
            .description = "Gemini 2.0 Flash with thinking / reasoning capability",
            .capabilities = {.temperature = true, .reasoning = true,
                             .tool_call = true, .streaming = true, .vision = true},
            .pricing = {{"input", 0.0}, {"output", 0.0}},
            .limits = {{"max_tokens", 8192}, {"rpm", 10}},
            .context_window = 1048576
        },
        // Gemini 1.5 Pro
        {
            .id = "gemini-1.5-pro",
            .provider_id = "google",
            .name = "Gemini 1.5 Pro",
            .description = "Mid-size multimodal model optimised for complex tasks",
            .capabilities = {.temperature = true, .reasoning = false,
                             .tool_call = true, .streaming = true, .vision = true},
            .pricing = {{"input", 0.00125}, {"output", 0.005}},
            .limits = {{"max_tokens", 8192}, {"rpm", 360}},
            .context_window = 2097152
        },
        // Gemini 1.5 Flash
        {
            .id = "gemini-1.5-flash",
            .provider_id = "google",
            .name = "Gemini 1.5 Flash",
            .description = "Fast and versatile model for diverse tasks",
            .capabilities = {.temperature = true, .reasoning = false,
                             .tool_call = true, .streaming = true, .vision = true},
            .pricing = {{"input", 0.000075}, {"output", 0.0003}},
            .limits = {{"max_tokens", 8192}, {"rpm", 1000}},
            .context_window = 1048576
        }
    };
}

std::vector<ModelInfo> GeminiProvider::list_models() const {
    return models_cache_;
}

std::optional<ModelInfo> GeminiProvider::get_model(const std::string& model_id) const {
    for (const auto& m : models_cache_) {
        if (m.id == model_id) return m;
    }
    return std::nullopt;
}

bool GeminiProvider::supports_model(const std::string& model_id) const {
    return get_model(model_id).has_value();
}

// ---------------------------------------------------------------------------
// HTTP helpers
// ---------------------------------------------------------------------------

network::HttpHeaders GeminiProvider::build_headers() const {
    network::HttpHeaders headers;
    headers.emplace_back("Content-Type", "application/json");
    // Gemini uses x-goog-api-key header.
    // Guard: only add header if key is non-empty (is_ready() prevents
    // reaching here with empty key, but defensive guard is cheap).
    if (!config_.api_key.empty()) {
        headers.emplace_back("x-goog-api-key", config_.api_key);
    }
    return headers;
}

std::string GeminiProvider::build_url(
        const std::string& model_id, bool stream) const {
    const std::string endpoint = stream ? "streamGenerateContent?alt=sse"
                                        : "generateContent";
    return config_.base_url + "/models/" + model_id + ":" + endpoint;
}

/// Convert ChatMessage to Gemini "contents" array element.
nlohmann::json GeminiProvider::to_gemini_content(const ChatMessage& msg) {
    std::string role;
    switch (msg.role) {
        case ChatRole::User:      role = "user";  break;
        case ChatRole::Assistant: role = "model"; break;
        case ChatRole::System:    role = "user";  break;  // System as user with preamble
        case ChatRole::Tool:      role = "user";  break;
        default:                  role = "user";  break;
    }

    nlohmann::json parts = nlohmann::json::array();

    if (!msg.content.empty()) {
        parts.push_back({{"text", msg.content}});
    }

    // Tool call result
    if (msg.tool_call_id && !msg.content.empty()) {
        parts = nlohmann::json::array();
        parts.push_back({
            {"functionResponse", {
                {"name",     msg.name.value_or("unknown")},
                {"response", {{"output", msg.content}}}
            }}
        });
        role = "user";
    }

    // Tool calls from assistant
    if (msg.tool_calls) {
        parts = nlohmann::json::array();
        if (!msg.content.empty()) {
            parts.push_back({{"text", msg.content}});
        }
        for (const auto& tc : *msg.tool_calls) {
            parts.push_back({
                {"functionCall", {
                    {"name", tc.name},
                    {"args", tc.arguments}
                }}
            });
        }
        role = "model";
    }

    return {{"role", role}, {"parts", parts}};
}

nlohmann::json GeminiProvider::build_request_body(
    const std::vector<ChatMessage>& messages,
    const ChatOptions&              options,
    bool                            /*stream*/
) const {
    nlohmann::json body = nlohmann::json::object();

    // Separate system message from conversation
    std::string system_text;
    nlohmann::json contents = nlohmann::json::array();
    for (const auto& msg : messages) {
        if (msg.role == ChatRole::System) {
            system_text += msg.content + "\n";
        } else {
            contents.push_back(to_gemini_content(msg));
        }
    }

    if (!system_text.empty()) {
        body["systemInstruction"] = {
            {"parts", nlohmann::json::array({{{"text", system_text}}})}
        };
    }

    body["contents"] = contents;

    // Generation config
    nlohmann::json gen_config = nlohmann::json::object();
    gen_config["temperature"] = options.temperature;
    gen_config["topP"]        = options.top_p;
    gen_config["maxOutputTokens"] = options.max_tokens;

    if (!options.stop.empty()) {
        gen_config["stopSequences"] = options.stop;
    }

    body["generationConfig"] = gen_config;

    // Tools (function declarations)
    if (!options.tools.empty()) {
        nlohmann::json func_decls = nlohmann::json::array();
        for (const auto& tool : options.tools) {
            func_decls.push_back({
                {"name",        tool.name},
                {"description", tool.description},
                {"parameters",  tool.parameters}
            });
        }
        body["tools"] = {{{"functionDeclarations", func_decls}}};
    }

    return body;
}

// ---------------------------------------------------------------------------
// Response parsing
// ---------------------------------------------------------------------------

ChatResponse GeminiProvider::parse_response(const nlohmann::json& response) const {
    ChatResponse result;

    if (response.contains("error")) {
        result.error = response["error"];
        return result;
    }

    result.model = response.value("modelVersion", std::string{});

    if (!response.contains("candidates") || !response["candidates"].is_array() ||
            response["candidates"].empty()) {
        return result;
    }

    const auto& candidate = response["candidates"][0];
    ChatMessage msg;
    msg.role = ChatRole::Assistant;

    if (candidate.contains("content") && candidate["content"].contains("parts")) {
        int tc_index = 0;
        for (const auto& part : candidate["content"]["parts"]) {
            if (part.contains("text")) {
                msg.content += part["text"].get<std::string>();
            } else if (part.contains("functionCall")) {
                const auto& fc = part["functionCall"];
                ToolCall tc;
                // Use index suffix to ensure unique IDs when multiple tool calls
                // share the same function name in a single response.
                tc.id        = "call_" + fc.value("name", std::string{"fn"})
                               + "_" + std::to_string(tc_index++);
                tc.type      = "function";
                tc.name      = fc.value("name", std::string{});
                tc.arguments = fc.value("args", nlohmann::json::object());
                if (!msg.tool_calls) msg.tool_calls = std::vector<ToolCall>{};
                msg.tool_calls->push_back(tc);
            }
        }
    }

    result.finish_reason = candidate.value("finishReason", std::string{});
    result.choices.push_back(msg);

    // Usage metadata
    if (response.contains("usageMetadata")) {
        const auto& um = response["usageMetadata"];
        TokenUsage usage;
        usage.input  = um.value("promptTokenCount",     0);
        usage.output = um.value("candidatesTokenCount", 0);
        result.usage = usage;
    }

    return result;
}

ChatStreamEvent GeminiProvider::parse_stream_chunk(const std::string& chunk) const {
    ChatStreamEvent event;

    if (chunk.empty()) {
        // Empty chunk: signal completion to avoid callers seeing an un-typed event.
        event.type = StreamEventType::Finish;
        return event;
    }

    std::string json_str;
    if (chunk.rfind("data: ", 0) == 0) json_str = chunk.substr(6);
    else                               json_str = chunk;

    json_str = trim_whitespace(json_str);
    if (json_str.empty() || json_str == "[DONE]") {
        event.type = StreamEventType::Finish;
        return event;
    }

    try {
        auto json = nlohmann::json::parse(json_str);

        if (json.contains("error")) {
            event.type  = StreamEventType::Error;
            event.error = json["error"];
            return event;
        }

        if (!json.contains("candidates") || !json["candidates"].is_array() ||
                json["candidates"].empty()) {
            return event;
        }

        const auto& candidate = json["candidates"][0];
        const std::string finish = candidate.value("finishReason", std::string{});

        if (!finish.empty() && finish != "STOP" && finish != "MAX_TOKENS") {
            // Unusual finish reasons are treated as done
        }

        if (candidate.contains("content") && candidate["content"].contains("parts")) {
            int tc_index = 0;
            for (const auto& part : candidate["content"]["parts"]) {
                if (part.contains("text")) {
                    event.type    = StreamEventType::TextDelta;
                    event.content += part["text"].get<std::string>();
                } else if (part.contains("functionCall")) {
                    const auto& fc = part["functionCall"];
                    ToolCall tc;
                    tc.id        = "call_" + fc.value("name", std::string{"fn"})
                                   + "_" + std::to_string(tc_index++);
                    tc.type      = "function";
                    tc.name      = fc.value("name", std::string{});
                    tc.arguments = fc.value("args", nlohmann::json::object());
                    event.type       = StreamEventType::ToolCall;
                    event.tool_call  = tc;
                    event.content    = tc.arguments.dump();
                }
            }
        }

        if (!finish.empty()) {
            event.type          = StreamEventType::Finish;
            event.finish_reason = finish;
        }

        // Usage
        if (json.contains("usageMetadata")) {
            const auto& um = json["usageMetadata"];
            TokenUsage usage;
            usage.input  = um.value("promptTokenCount",     0);
            usage.output = um.value("candidatesTokenCount", 0);
            event.usage = usage;
        }

    } catch (const nlohmann::json::parse_error& e) {
        event.type  = StreamEventType::Error;
        event.error = {{"message", std::string("JSON parse error: ") + e.what()}};
    }

    return event;
}

// ---------------------------------------------------------------------------
// Chat / ChatStream
// ---------------------------------------------------------------------------

ChatResponse GeminiProvider::chat(
    const std::vector<ChatMessage>& messages,
    const std::string&              model_id,
    const ChatOptions&              options
) {
    if (!is_ready()) {
        ChatResponse err;
        err.error = {{"message", "Gemini provider not configured (missing GEMINI_API_KEY)"}};
        return err;
    }

    const auto body    = build_request_body(messages, options, false);
    const auto headers = build_headers();
    const auto url     = build_url(model_id, false);

    auto resp = http_client_->post_json(url, body.dump(), headers);

    if (!resp.is_success()) {
        ChatResponse err;
        try {
            err.error = nlohmann::json::parse(resp.body);
        } catch (...) {
            err.error = {{"message", resp.body}};
        }
        return err;
    }

    try {
        return parse_response(nlohmann::json::parse(resp.body));
    } catch (const nlohmann::json::parse_error& e) {
        ChatResponse err;
        err.error = {{"message", std::string("Failed to parse Gemini response: ") + e.what()}};
        return err;
    }
}

ChatResponse GeminiProvider::chat_stream(
    const std::vector<ChatMessage>& messages,
    const std::string&              model_id,
    const ChatOptions&              options,
    StreamCallback                  callback
) {
    if (!is_ready()) {
        ChatResponse err;
        err.error = {{"message", "Gemini provider not configured"}};
        return err;
    }

    const auto body    = build_request_body(messages, options, true);
    const auto headers = build_headers();
    const auto url     = build_url(model_id, true);

    ChatResponse final_response;
    std::string  accumulated_content;
    std::vector<ToolCall> accumulated_tool_calls;

    (void)http_client_->request_stream(
        network::HttpRequest::post(url, body.dump()).with_headers(headers),
        [&](std::string_view chunk) -> bool {
            std::string chunk_str(chunk);
            size_t pos = 0;
            while (pos < chunk_str.size()) {
                size_t end = chunk_str.find("\n\n", pos);
                std::string ev_data;
                if (end != std::string::npos) {
                    ev_data = chunk_str.substr(pos, end - pos);
                    pos     = end + 2;
                } else {
                    ev_data = chunk_str.substr(pos);
                    pos     = chunk_str.size();
                }
                if (ev_data.empty()) continue;

                auto event = parse_stream_chunk(ev_data);

                if (event.type == StreamEventType::TextDelta) {
                    accumulated_content += event.content;
                } else if (event.type == StreamEventType::ToolCall && event.tool_call) {
                    accumulated_tool_calls.push_back(*event.tool_call);
                } else if (event.type == StreamEventType::Finish) {
                    final_response.finish_reason = event.finish_reason.value_or("");
                    final_response.usage         = event.usage;
                }

                if (!callback(event)) return false;
            }
            return true;
        }
    );

    final_response.model = model_id;
    ChatMessage assistant_msg;
    assistant_msg.role    = ChatRole::Assistant;
    assistant_msg.content = accumulated_content;
    if (!accumulated_tool_calls.empty()) {
        assistant_msg.tool_calls = accumulated_tool_calls;
    }
    final_response.choices.push_back(assistant_msg);

    return final_response;
}

int64_t GeminiProvider::count_tokens(
    const std::vector<ChatMessage>& messages,
    [[maybe_unused]] const std::string& model_id
) const {
    // Approximate: 4 chars ≈ 1 token for English / CJK
    int64_t total = 0;
    for (const auto& msg : messages) {
        total += static_cast<int64_t>(msg.content.size());
    }
    return total / 4;
}

bool GeminiProvider::validate() {
    if (!is_ready()) return false;
    try {
        auto resp = chat({ChatMessage::user("hi")},
                         "gemini-2.0-flash",
                         ChatOptions{.max_tokens = 5});
        return !resp.is_error();
    } catch (const std::exception& e) {
        TURBOT_LOG_DEBUG("GeminiProvider::validate failed: {}", e.what());
        return false;
    }
}

} // namespace turbot::core::provider
