#include <turbot/core/provider/impl/anthropic_provider.hpp>
#include <turbot/core/message/message.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/string_utils.hpp>
#include <sstream>

namespace turbot::core::provider {

// ===== Constructors =====

AnthropicProvider::AnthropicProvider(const ProviderConfig& config)
    : config_(config), http_client_(std::make_unique<network::HttpClient>()) {
    if (config_.base_url.empty()) {
        config_.base_url = std::string(DEFAULT_BASE_URL);
    }
    http_client_->set_timeout(config_.timeout_seconds);
    if (!config_.proxy.empty()) {
        http_client_->set_proxy(config_.proxy);
    }
    http_client_->set_ssl_verify(config_.verify_ssl);
    initialize_models();
}

AnthropicProvider::AnthropicProvider(const std::string& api_key)
    : AnthropicProvider(ProviderConfig{.api_key = api_key}) {}

// ===== Provider interface =====

bool AnthropicProvider::is_ready() const {
    return !config_.api_key.empty();
}

void AnthropicProvider::initialize_models() {
    // Claude 3.5 family (latest generation)
    models_cache_ = {
        {
            .id = "claude-3-5-sonnet-latest",
            .provider_id = "anthropic",
            .name = "Claude 3.5 Sonnet",
            .description = "Most intelligent model in the Claude 3.5 family",
            .capabilities = {.temperature = true, .reasoning = false, .tool_call = true, .streaming = true, .vision = true},
            .pricing = {{"input", 0.003}, {"output", 0.015}},
            .limits = {{"max_tokens", 8192}, {"rpm", 50}},
            .context_window = 200000
        },
        {
            .id = "claude-3-5-haiku-latest",
            .provider_id = "anthropic",
            .name = "Claude 3.5 Haiku",
            .description = "Fastest model in the Claude 3.5 family",
            .capabilities = {.temperature = true, .reasoning = false, .tool_call = true, .streaming = true, .vision = true},
            .pricing = {{"input", 0.0008}, {"output", 0.004}},
            .limits = {{"max_tokens", 8192}, {"rpm", 50}},
            .context_window = 200000
        },
        // Claude 3 family
        {
            .id = "claude-3-opus-latest",
            .provider_id = "anthropic",
            .name = "Claude 3 Opus",
            .description = "Most powerful model for highly complex tasks",
            .capabilities = {.temperature = true, .reasoning = false, .tool_call = true, .streaming = true, .vision = true},
            .pricing = {{"input", 0.015}, {"output", 0.075}},
            .limits = {{"max_tokens", 4096}, {"rpm", 50}},
            .context_window = 200000
        },
        {
            .id = "claude-3-sonnet-20240229",
            .provider_id = "anthropic",
            .name = "Claude 3 Sonnet",
            .description = "Balance of speed and intelligence",
            .capabilities = {.temperature = true, .reasoning = false, .tool_call = true, .streaming = true, .vision = true},
            .pricing = {{"input", 0.003}, {"output", 0.015}},
            .limits = {{"max_tokens", 4096}, {"rpm", 50}},
            .context_window = 200000
        },
        {
            .id = "claude-3-haiku-20240307",
            .provider_id = "anthropic",
            .name = "Claude 3 Haiku",
            .description = "Fastest and most compact model",
            .capabilities = {.temperature = true, .reasoning = false, .tool_call = true, .streaming = true, .vision = true},
            .pricing = {{"input", 0.00025}, {"output", 0.00125}},
            .limits = {{"max_tokens", 4096}, {"rpm", 50}},
            .context_window = 200000
        }
    };
}

std::vector<ModelInfo> AnthropicProvider::list_models() const {
    return models_cache_;
}

std::optional<ModelInfo> AnthropicProvider::get_model(const std::string& model_id) const {
    for (const auto& model : models_cache_) {
        if (model.id == model_id) {
            return model;
        }
    }
    return std::nullopt;
}

bool AnthropicProvider::supports_model(const std::string& model_id) const {
    return get_model(model_id).has_value();
}

// ===== HTTP helpers =====

network::HttpHeaders AnthropicProvider::build_headers() const {
    network::HttpHeaders headers;
    headers.emplace_back("Content-Type", "application/json");
    // Anthropic uses "x-api-key" instead of "Authorization: Bearer ..."
    headers.emplace_back("x-api-key", config_.api_key);
    // Required Anthropic API version header
    headers.emplace_back("anthropic-version", std::string(API_VERSION));
    return headers;
}

nlohmann::json AnthropicProvider::build_request_body(
    const std::vector<ChatMessage>& messages,
    const std::string& model_id,
    const ChatOptions& options,
    bool stream
) const {
    nlohmann::json body = {
        {"model",  model_id},
        {"stream", stream},
        // max_tokens is REQUIRED by Anthropic API
        {"max_tokens", options.max_tokens > 0 ? options.max_tokens : 4096}
    };

    if (options.temperature >= 0.0) {
        body["temperature"] = options.temperature;
    }
    if (options.top_p > 0.0 && options.top_p < 1.0) {
        body["top_p"] = options.top_p;
    }

    // Separate system messages from the conversation.
    // Anthropic passes system instructions as a top-level "system" field.
    std::string system_text;
    nlohmann::json messages_array = nlohmann::json::array();

    for (const auto& msg : messages) {
        if (msg.role == ChatRole::System) {
            if (!system_text.empty()) system_text += "\n";
            system_text += msg.content;
        } else {
            messages_array.push_back(msg.to_json());
        }
    }

    if (!system_text.empty()) {
        body["system"] = system_text;
    }
    body["messages"] = messages_array;

    if (!options.stop.empty()) {
        body["stop_sequences"] = options.stop;
    }

    // Tool definitions
    if (!options.tools.empty()) {
        nlohmann::json tools_array = nlohmann::json::array();
        for (const auto& tool : options.tools) {
            // Anthropic tools schema: { name, description, input_schema }
            nlohmann::json t;
            const auto& tj = tool.to_json();
            t["name"] = tj.value("name", std::string{});
            t["description"] = tj.value("description", std::string{});
            t["input_schema"] = tj.contains("parameters") ? tj["parameters"]
                                                           : nlohmann::json::object();
            tools_array.push_back(std::move(t));
        }
        body["tools"] = tools_array;
    }

    // Extra passthrough options
    for (auto& [key, value] : options.extra.items()) {
        body[key] = value;
    }

    return body;
}

// ===== Response parsing =====

ChatResponse AnthropicProvider::parse_response(const nlohmann::json& response) const {
    ChatResponse result;

    if (response.contains("error")) {
        result.error = response["error"];
        return result;
    }

    result.id    = response.value("id",    std::string{});
    result.model = response.value("model", std::string{});

    ChatMessage msg;
    msg.role = ChatRole::Assistant;

    // Anthropic response: content[] array of typed blocks
    if (response.contains("content") && response["content"].is_array()) {
        std::string text_buf;
        std::vector<ToolCall> tool_calls;

        for (const auto& block : response["content"]) {
            const std::string type = block.value("type", std::string{});

            if (type == "text") {
                if (!text_buf.empty()) text_buf += "\n";
                text_buf += block.value("text", std::string{});
            } else if (type == "tool_use") {
                ToolCall tc;
                tc.id   = block.value("id",   std::string{});
                tc.name = block.value("name",  std::string{});
                tc.type = "function";
                tc.arguments = block.contains("input") ? block["input"]
                                                       : nlohmann::json::object();
                tool_calls.push_back(std::move(tc));
            }
        }

        msg.content = std::move(text_buf);
        if (!tool_calls.empty()) {
            msg.tool_calls = std::move(tool_calls);
        }
    }

    result.choices.push_back(std::move(msg));
    result.finish_reason = response.value("stop_reason", std::string{});

    if (response.contains("usage")) {
        const auto& u = response["usage"];
        TokenUsage usage;
        usage.input  = u.value("input_tokens",  int64_t{0});
        usage.output = u.value("output_tokens", int64_t{0});
        result.usage = usage;
    }

    return result;
}

ChatStreamEvent AnthropicProvider::parse_stream_chunk(const std::string& chunk) const {
    ChatStreamEvent event;

    // Strip leading "data: " prefix if present
    std::string_view sv = chunk;
    if (sv.starts_with("data: ")) {
        sv = sv.substr(6);
    }

    const std::string_view trimmed = sv;
    if (trimmed.empty() || trimmed == "[DONE]") {
        event.type = StreamEventType::Finish;
        return event;
    }

    try {
        const auto json = nlohmann::json::parse(trimmed);
        const std::string ev_type = json.value("type", std::string{});

        if (ev_type == "error") {
            event.type  = StreamEventType::Error;
            event.error = json.contains("error") ? json["error"]
                                                 : nlohmann::json{{"message", "stream error"}};
            return event;
        }

        if (ev_type == "message_stop") {
            event.type = StreamEventType::Finish;
            return event;
        }

        if (ev_type == "message_delta") {
            // Contains stop_reason and token usage
            event.type = StreamEventType::Finish;
            if (json.contains("delta")) {
                event.finish_reason = json["delta"].value("stop_reason", std::string{});
            }
            if (json.contains("usage")) {
                TokenUsage usage;
                usage.output = json["usage"].value("output_tokens", int64_t{0});
                event.usage = usage;
            }
            return event;
        }

        if (ev_type == "content_block_delta") {
            if (!json.contains("delta")) return event;
            const auto& delta = json["delta"];
            const std::string delta_type = delta.value("type", std::string{});

            if (delta_type == "text_delta") {
                event.type    = StreamEventType::TextDelta;
                event.content = delta.value("text", std::string{});
            } else if (delta_type == "input_json_delta") {
                // Streaming tool-use argument fragments
                event.type    = StreamEventType::ToolCall;
                event.content = delta.value("partial_json", std::string{});
            }
            return event;
        }

        if (ev_type == "content_block_start") {
            // Signals the beginning of a tool_use block
            if (json.contains("content_block")) {
                const auto& cb = json["content_block"];
                if (cb.value("type", std::string{}) == "tool_use") {
                    event.type = StreamEventType::ToolCall;
                    ToolCall tc;
                    tc.id   = cb.value("id",   std::string{});
                    tc.name = cb.value("name",  std::string{});
                    tc.type = "function";
                    event.tool_call = tc;
                }
            }
            return event;
        }

    } catch (const nlohmann::json::parse_error& e) {
        TURBOT_LOG_DEBUG("AnthropicProvider::parse_stream_chunk: JSON error: {}", e.what());
        event.type  = StreamEventType::Error;
        event.error = {{"message", std::string("JSON parse error: ") + e.what()}};
    }

    return event;
}

// ===== Public API =====

ChatResponse AnthropicProvider::chat(
    const std::vector<ChatMessage>& messages,
    const std::string& model_id,
    const ChatOptions& options
) {
    if (!is_ready()) {
        ChatResponse err;
        err.error = {{"message", "Anthropic provider not configured: missing api_key"}};
        return err;
    }

    const auto body    = build_request_body(messages, model_id, options, false);
    const auto headers = build_headers();
    const std::string url = config_.base_url + "/messages";

    auto http_response = http_client_->post_json(url, body.dump(), headers);

    if (!http_response.is_success()) {
        ChatResponse err;
        try {
            err.error = nlohmann::json::parse(http_response.body);
        } catch (const nlohmann::json::parse_error&) {
            err.error = {{"message", http_response.body}};
        }
        TURBOT_LOG_WARN("AnthropicProvider::chat: HTTP {} for model '{}'",
                        http_response.status_code, model_id);
        return err;
    }

    try {
        return parse_response(nlohmann::json::parse(http_response.body));
    } catch (const nlohmann::json::parse_error& e) {
        ChatResponse err;
        err.error = {{"message", std::string("Failed to parse Anthropic response: ") + e.what()}};
        return err;
    }
}

ChatResponse AnthropicProvider::chat_stream(
    const std::vector<ChatMessage>& messages,
    const std::string& model_id,
    const ChatOptions& options,
    StreamCallback callback
) {
    if (!is_ready()) {
        ChatResponse err;
        err.error = {{"message", "Anthropic provider not configured: missing api_key"}};
        return err;
    }

    const auto body    = build_request_body(messages, model_id, options, true);
    const auto headers = build_headers();
    const std::string url = config_.base_url + "/messages";

    ChatResponse final_response;
    std::string accumulated_content;
    std::vector<ToolCall> accumulated_tool_calls;
    std::string current_tool_id;
    std::string current_tool_name;
    std::string current_tool_args;

    auto http_response = http_client_->request_stream(
        network::HttpRequest::post(url, body.dump())
            .with_headers(headers),
        [&](std::string_view chunk) -> bool {
            std::string chunk_str(chunk);
            size_t pos = 0;

            while (pos < chunk_str.size()) {
                const size_t end = chunk_str.find("\n\n", pos);
                std::string event_data;

                if (end != std::string::npos) {
                    event_data = chunk_str.substr(pos, end - pos);
                    pos = end + 2;
                } else {
                    event_data = chunk_str.substr(pos);
                    pos = chunk_str.size();
                }

                if (event_data.empty()) continue;

                auto ev = parse_stream_chunk(event_data);

                if (ev.type == StreamEventType::TextDelta) {
                    accumulated_content += ev.content;
                } else if (ev.type == StreamEventType::ToolCall) {
                    if (ev.tool_call) {
                        // Start of a new tool_use block
                        if (!current_tool_id.empty()) {
                            // Flush the previous tool call
                            ToolCall tc;
                            tc.id   = current_tool_id;
                            tc.name = current_tool_name;
                            tc.type = "function";
                            try {
                                tc.arguments = nlohmann::json::parse(current_tool_args);
                            } catch (const nlohmann::json::parse_error& e) {
                                TURBOT_LOG_ERROR("AnthropicProvider: tool arg JSON error: {}",
                                                 e.what());
                                tc.arguments = nlohmann::json::object();
                            }
                            accumulated_tool_calls.push_back(tc);
                        }
                        current_tool_id   = ev.tool_call->id;
                        current_tool_name = ev.tool_call->name;
                        current_tool_args.clear();
                    } else {
                        // Argument fragment (input_json_delta)
                        current_tool_args += ev.content;
                    }
                } else if (ev.type == StreamEventType::Finish) {
                    // Flush any pending tool call
                    if (!current_tool_id.empty()) {
                        ToolCall tc;
                        tc.id   = current_tool_id;
                        tc.name = current_tool_name;
                        tc.type = "function";
                        try {
                            tc.arguments = nlohmann::json::parse(current_tool_args);
                        } catch (const nlohmann::json::parse_error& e) {
                            TURBOT_LOG_ERROR("AnthropicProvider: tool arg JSON error: {}",
                                             e.what());
                            tc.arguments = nlohmann::json::object();
                        }
                        accumulated_tool_calls.push_back(tc);
                        current_tool_id.clear();
                    }
                    final_response.finish_reason = ev.finish_reason.value_or("");
                    final_response.usage = ev.usage;
                }

                if (!callback(ev)) {
                    return false; // Caller aborted stream
                }
            }

            return true; // Continue
        }
    );

    // Build the final aggregated response
    final_response.id    = "msg_" + turbot::utils::generate_uuid().substr(0, 8);
    final_response.model = model_id;

    ChatMessage assistant_msg;
    assistant_msg.role    = ChatRole::Assistant;
    assistant_msg.content = accumulated_content;
    if (!accumulated_tool_calls.empty()) {
        assistant_msg.tool_calls = accumulated_tool_calls;
    }
    final_response.choices.push_back(std::move(assistant_msg));

    return final_response;
}

int64_t AnthropicProvider::count_tokens(
    const std::vector<ChatMessage>& messages,
    [[maybe_unused]] const std::string& model_id
) const {
    // Simple character-based approximation (~4 chars / token)
    int64_t total_chars = 0;
    for (const auto& msg : messages) {
        total_chars += static_cast<int64_t>(msg.content.size());
        if (msg.tool_calls) {
            for (const auto& tc : *msg.tool_calls) {
                total_chars += static_cast<int64_t>(tc.name.size() +
                                                    tc.arguments.dump().size());
            }
        }
    }
    return total_chars / 4;
}

bool AnthropicProvider::validate() {
    if (!is_ready()) {
        return false;
    }

    try {
        // Minimal single-token request to verify the API key
        auto response = chat(
            {ChatMessage::user("hi")},
            "claude-3-haiku-20240307",
            ChatOptions{.max_tokens = 5}
        );
        return !response.is_error();
    } catch (const std::exception& e) {
        TURBOT_LOG_DEBUG("AnthropicProvider::validate failed: {}", e.what());
        return false;
    }
}

} // namespace turbot::core::provider
