/**
 * @file azure_provider.cpp
 * @brief Azure OpenAI Service provider implementation.
 *
 * C++ port of OpenCode "azure" provider registration in provider.ts.
 * Wire protocol is identical to OpenAI; only the URL scheme and auth header differ.
 */

#include <turbot/core/provider/impl/azure_provider.hpp>
#include <turbot/core/message/message.hpp>
#include <turbot/core/common/logger.hpp>
#include <cstdlib>
#include <sstream>

namespace turbot::core::provider {

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

AzureProvider::AzureProvider(const ProviderConfig& config)
    : config_(config)
    , http_client_(std::make_unique<network::HttpClient>()) {

    // Resolve resource name: config → env var
    if (!config_.base_url.empty()) {
        resource_name_ = config_.base_url;   // allow raw override via base_url
    } else {
        const char* env_rn = std::getenv("AZURE_RESOURCE_NAME");
        resource_name_ = env_rn ? env_rn : "";
    }

    // Resolve API key: config → AZURE_OPENAI_API_KEY → AZURE_API_KEY
    if (config_.api_key.empty()) {
        const char* k1 = std::getenv("AZURE_OPENAI_API_KEY");
        if (k1 && *k1 != '\0') {
            config_.api_key = k1;
        } else {
            const char* k2 = std::getenv("AZURE_API_KEY");
            if (k2 && *k2 != '\0') config_.api_key = k2;
        }
    }

    api_version_ = std::string(DEFAULT_API_VERSION);

    http_client_->set_timeout(config_.timeout_seconds);
    if (!config_.proxy.empty()) {
        http_client_->set_proxy(config_.proxy);
    }
    http_client_->set_ssl_verify(config_.verify_ssl);

    initialize_models();
}

AzureProvider::AzureProvider(const std::string& resource_name,
                             const std::string& api_key)
    : AzureProvider(ProviderConfig{.api_key = api_key, .base_url = resource_name}) {}

// ---------------------------------------------------------------------------
// Provider interface
// ---------------------------------------------------------------------------

bool AzureProvider::is_ready() const {
    return !config_.api_key.empty() && !resource_name_.empty();
}

std::string AzureProvider::resource_name() const { return resource_name_; }

std::string AzureProvider::deployment_url(const std::string& model_id) const {
    // Sanitise model_id to prevent path traversal: only allow alphanum, '-', '_', '.'
    // Azure deployment names follow the same constraints.
    std::string safe_id;
    safe_id.reserve(model_id.size());
    for (char c : model_id) {
        if (std::isalnum(static_cast<unsigned char>(c)) ||
            c == '-' || c == '_' || c == '.') {
            safe_id += c;
        }
    }
    return "https://" + resource_name_ +
           ".openai.azure.com/openai/deployments/" + safe_id;
}

void AzureProvider::initialize_models() {
    // Common Azure OpenAI deployments (deployment names are user-configurable;
    // these are the canonical model IDs from Azure portal)
    models_cache_ = {
        {
            .id = "gpt-4o",
            .provider_id = "azure",
            .name = "GPT-4o (Azure)",
            .description = "Azure-hosted GPT-4o, optimised for speed and intelligence",
            .capabilities = {.temperature = true, .reasoning = false,
                             .tool_call = true, .streaming = true, .input = {.image = true}},
            .pricing = {{"input", 0.005}, {"output", 0.015}},
            .limits = {{"max_tokens", 16384}, {"rpm", 500}},
            .context_window = 128000
        },
        {
            .id = "gpt-4o-mini",
            .provider_id = "azure",
            .name = "GPT-4o Mini (Azure)",
            .description = "Affordable and intelligent small model on Azure",
            .capabilities = {.temperature = true, .reasoning = false,
                             .tool_call = true, .streaming = true, .input = {.image = true}},
            .pricing = {{"input", 0.00015}, {"output", 0.0006}},
            .limits = {{"max_tokens", 16384}, {"rpm", 500}},
            .context_window = 128000
        },
        {
            .id = "gpt-4-turbo",
            .provider_id = "azure",
            .name = "GPT-4 Turbo (Azure)",
            .description = "GPT-4 Turbo on Azure",
            .capabilities = {.temperature = true, .reasoning = false,
                             .tool_call = true, .streaming = true, .input = {.image = true}},
            .pricing = {{"input", 0.01}, {"output", 0.03}},
            .limits = {{"max_tokens", 4096}, {"rpm", 500}},
            .context_window = 128000
        },
        {
            .id = "gpt-4",
            .provider_id = "azure",
            .name = "GPT-4 (Azure)",
            .description = "GPT-4 on Azure OpenAI Service",
            .capabilities = {.temperature = true, .reasoning = false,
                             .tool_call = true, .streaming = true},
            .pricing = {{"input", 0.03}, {"output", 0.06}},
            .limits = {{"max_tokens", 4096}, {"rpm", 500}},
            .context_window = 8192
        },
        {
            .id = "gpt-35-turbo",
            .provider_id = "azure",
            .name = "GPT-3.5 Turbo (Azure)",
            .description = "Azure-hosted GPT-3.5 Turbo",
            .capabilities = {.temperature = true, .reasoning = false,
                             .tool_call = true, .streaming = true},
            .pricing = {{"input", 0.0005}, {"output", 0.0015}},
            .limits = {{"max_tokens", 4096}, {"rpm", 500}},
            .context_window = 16384
        },
        {
            .id = "o1",
            .provider_id = "azure",
            .name = "o1 (Azure)",
            .description = "Azure-hosted o1 reasoning model",
            .capabilities = {.temperature = false, .reasoning = true,
                             .tool_call = true, .streaming = true},
            .pricing = {{"input", 0.015}, {"output", 0.06}},
            .limits = {{"max_tokens", 32768}, {"rpm", 50}},
            .context_window = 128000
        },
        {
            .id = "o1-mini",
            .provider_id = "azure",
            .name = "o1-mini (Azure)",
            .description = "Azure-hosted o1-mini reasoning model",
            .capabilities = {.temperature = false, .reasoning = true,
                             .tool_call = false, .streaming = false},
            .pricing = {{"input", 0.003}, {"output", 0.012}},
            .limits = {{"max_tokens", 65536}, {"rpm", 50}},
            .context_window = 128000
        }
    };
}

std::vector<ModelInfo> AzureProvider::list_models() const {
    return models_cache_;
}

std::optional<ModelInfo> AzureProvider::get_model(const std::string& model_id) const {
    for (const auto& m : models_cache_) {
        if (m.id == model_id) return m;
    }
    return std::nullopt;
}

bool AzureProvider::supports_model(const std::string& model_id) const {
    return get_model(model_id).has_value();
}

// ---------------------------------------------------------------------------
// HTTP helpers
// ---------------------------------------------------------------------------

network::HttpHeaders AzureProvider::build_headers() const {
    network::HttpHeaders headers;
    headers.emplace_back("Content-Type", "application/json");
    // Azure uses api-key header instead of Bearer token.
    // Guard: only add header if key is non-empty (is_ready() should have been
    // checked before reaching here, but defensive guard prevents sending a
    // bare "api-key: " header which Azure would reject with 401 anyway).
    if (!config_.api_key.empty()) {
        headers.emplace_back("api-key", config_.api_key);
    }
    return headers;
}

nlohmann::json AzureProvider::build_request_body(
    const std::vector<ChatMessage>& messages,
    const std::string& /*model_id*/,
    const ChatOptions& options,
    bool stream
) const {
    // Azure omits the "model" field — the deployment name is in the URL
    nlohmann::json body = {{"stream", stream}};

    nlohmann::json messages_array = nlohmann::json::array();
    for (const auto& msg : messages) {
        messages_array.push_back(msg.to_json());
    }
    body["messages"] = messages_array;

    body["temperature"] = options.temperature;
    body["top_p"]       = options.top_p;
    body["max_tokens"]  = options.max_tokens;

    if (!options.stop.empty()) body["stop"] = options.stop;

    if (!options.tools.empty()) {
        nlohmann::json tools_array = nlohmann::json::array();
        for (const auto& tool : options.tools) {
            tools_array.push_back(tool.to_json());
        }
        body["tools"] = tools_array;
    }

    if (options.user) body["user"] = *options.user;

    for (const auto& [k, v] : options.extra.items()) {
        body[k] = v;
    }

    return body;
}

// ---------------------------------------------------------------------------
// Response parsing (identical to OpenAI)
// ---------------------------------------------------------------------------

ChatResponse AzureProvider::parse_response(const nlohmann::json& response) const {
    ChatResponse result;

    if (response.contains("error")) {
        result.error = response["error"];
        return result;
    }

    result.id    = response.value("id",    std::string{});
    result.model = response.value("model", std::string{});

    if (response.contains("choices") && response["choices"].is_array()) {
        for (const auto& choice : response["choices"]) {
            ChatMessage msg;
            const auto& msg_obj = choice.value("message", nlohmann::json::object());
            msg.role    = chat_role_from_string(msg_obj.value("role", "assistant"));
            msg.content = msg_obj.value("content", std::string{});

            if (msg_obj.contains("tool_calls")) {
                std::vector<ToolCall> calls;
                for (const auto& tc : msg_obj["tool_calls"]) {
                    calls.push_back(ToolCall::from_json(tc));
                }
                msg.tool_calls = calls;
            }

            result.choices.push_back(msg);
            result.finish_reason = choice.value("finish_reason", std::string{});
        }
    }

    if (response.contains("usage")) {
        result.usage = TokenUsage::from_json(response["usage"]);
    }

    return result;
}

ChatStreamEvent AzureProvider::parse_stream_chunk(const std::string& chunk) const {
    ChatStreamEvent event;

    if (chunk.empty() || chunk == "data: [DONE]") {
        event.type = StreamEventType::Finish;
        return event;
    }

    std::string json_str;
    if (chunk.rfind("data: ", 0) == 0) json_str = chunk.substr(6);
    else                               json_str = chunk;

    try {
        auto json = nlohmann::json::parse(json_str);

        if (json.contains("error")) {
            event.type  = StreamEventType::Error;
            event.error = json["error"];
            return event;
        }

        if (json.contains("choices") && json["choices"].is_array() &&
                !json["choices"].empty()) {
            const auto& choice = json["choices"][0];

            if (choice.contains("finish_reason") && !choice["finish_reason"].is_null()) {
                event.type          = StreamEventType::Finish;
                event.finish_reason = choice["finish_reason"].get<std::string>();
                return event;
            }

            if (choice.contains("delta")) {
                const auto& delta = choice["delta"];

                if (delta.contains("tool_calls")) {
                    event.type = StreamEventType::ToolCall;
                    for (const auto& tc : delta["tool_calls"]) {
                        ToolCall call;
                        call.id   = tc.value("id", std::string{});
                        call.type = "function";
                        if (tc.contains("function")) {
                            call.name    = tc["function"].value("name", std::string{});
                            std::string args = tc["function"].value("arguments", std::string{});
                            if (!args.empty()) event.content = args;
                        }
                        if (!call.id.empty()) event.tool_call = call;
                    }
                } else if (delta.contains("content")) {
                    event.type    = StreamEventType::TextDelta;
                    event.content = delta.value("content", std::string{});
                }
            }
        }

        if (json.contains("usage")) {
            event.usage = TokenUsage::from_json(json["usage"]);
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

ChatResponse AzureProvider::chat(
    const std::vector<ChatMessage>& messages,
    const std::string&              model_id,
    const ChatOptions&              options
) {
    if (!is_ready()) {
        ChatResponse err;
        err.error = {{"message", "Azure provider not configured "
                                 "(missing api-key or resource-name)"}};
        return err;
    }

    const auto body    = build_request_body(messages, model_id, options, false);
    const auto headers = build_headers();
    const auto url     = deployment_url(model_id) +
                         "/chat/completions?api-version=" + api_version_;

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
        err.error = {{"message", std::string("Failed to parse Azure response: ") + e.what()}};
        return err;
    }
}

ChatResponse AzureProvider::chat_stream(
    const std::vector<ChatMessage>& messages,
    const std::string&              model_id,
    const ChatOptions&              options,
    StreamCallback                  callback
) {
    if (!is_ready()) {
        ChatResponse err;
        err.error = {{"message", "Azure provider not configured"}};
        return err;
    }

    const auto body    = build_request_body(messages, model_id, options, true);
    const auto headers = build_headers();
    const auto url     = deployment_url(model_id) +
                         "/chat/completions?api-version=" + api_version_;

    ChatResponse       final_response;
    std::string        accumulated_content;
    std::vector<ToolCall> accumulated_tool_calls;
    std::string        current_tc_id;
    std::string        current_tc_name;
    std::string        current_tc_args;

    http_client_->request_stream(
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
                    if (!event.tool_call->id.empty()) {
                        if (!current_tc_id.empty()) {
                            ToolCall tc;
                            tc.id   = current_tc_id;
                            tc.type = "function";
                            tc.name = current_tc_name;
                            try { tc.arguments = nlohmann::json::parse(current_tc_args); }
                            catch (...) { tc.arguments = nlohmann::json::object(); }
                            accumulated_tool_calls.push_back(tc);
                        }
                        current_tc_id   = event.tool_call->id;
                        current_tc_name = event.tool_call->name;
                        current_tc_args = event.content;
                    } else {
                        current_tc_args += event.content;
                    }
                } else if (event.type == StreamEventType::Finish) {
                    if (!current_tc_id.empty()) {
                        ToolCall tc;
                        tc.id   = current_tc_id;
                        tc.type = "function";
                        tc.name = current_tc_name;
                        try { tc.arguments = nlohmann::json::parse(current_tc_args); }
                        catch (...) { tc.arguments = nlohmann::json::object(); }
                        accumulated_tool_calls.push_back(tc);
                    }
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
    if (!accumulated_tool_calls.empty()) assistant_msg.tool_calls = accumulated_tool_calls;
    final_response.choices.push_back(assistant_msg);

    return final_response;
}

int64_t AzureProvider::count_tokens(
    const std::vector<ChatMessage>& messages,
    [[maybe_unused]] const std::string& model_id
) const {
    int64_t total = 0;
    for (const auto& msg : messages) {
        total += static_cast<int64_t>(msg.content.size());
    }
    return total / 4;
}

bool AzureProvider::validate() {
    if (!is_ready()) return false;
    try {
        auto resp = chat({ChatMessage::user("hi")},
                         "gpt-4o",
                         ChatOptions{.max_tokens = 5});
        return !resp.is_error();
    } catch (const std::exception& e) {
        TURBOT_LOG_DEBUG("AzureProvider::validate failed: {}", e.what());
        return false;
    }
}

} // namespace turbot::core::provider
