#include <turbot/core/provider/impl/openai_provider.hpp>
#include <turbot/core/message/message.hpp>
#include <turbot/core/common/logger.hpp>
#include <sstream>

namespace turbot::core::provider {

OpenAIProvider::OpenAIProvider(const ProviderConfig& config)
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

OpenAIProvider::OpenAIProvider(const std::string& api_key)
    : OpenAIProvider(ProviderConfig{.api_key = api_key}) {}

bool OpenAIProvider::is_ready() const {
    return !config_.api_key.empty();
}

void OpenAIProvider::initialize_models() {
    // Define available OpenAI models
    models_cache_ = {
        {
            .id = "gpt-4o",
            .provider_id = "openai",
            .name = "GPT-4o",
            .description = "Most capable GPT-4 model, optimized for speed and intelligence",
            .capabilities = {.temperature = true, .reasoning = false, .tool_call = true, .streaming = true, .vision = true},
            .pricing = {{"input", 0.005}, {"output", 0.015}},
            .limits = {{"max_tokens", 16384}, {"rpm", 500}},
            .context_window = 128000
        },
        {
            .id = "gpt-4o-mini",
            .provider_id = "openai",
            .name = "GPT-4o Mini",
            .description = "Affordable and intelligent small model",
            .capabilities = {.temperature = true, .reasoning = false, .tool_call = true, .streaming = true, .vision = true},
            .pricing = {{"input", 0.00015}, {"output", 0.0006}},
            .limits = {{"max_tokens", 16384}, {"rpm", 500}},
            .context_window = 128000
        },
        {
            .id = "gpt-4-turbo",
            .provider_id = "openai",
            .name = "GPT-4 Turbo",
            .description = "Previous generation GPT-4 model with vision",
            .capabilities = {.temperature = true, .reasoning = false, .tool_call = true, .streaming = true, .vision = true},
            .pricing = {{"input", 0.01}, {"output", 0.03}},
            .limits = {{"max_tokens", 4096}, {"rpm", 500}},
            .context_window = 128000
        },
        {
            .id = "gpt-4",
            .provider_id = "openai",
            .name = "GPT-4",
            .description = "Most capable GPT-4 model",
            .capabilities = {.temperature = true, .reasoning = false, .tool_call = true, .streaming = true},
            .pricing = {{"input", 0.03}, {"output", 0.06}},
            .limits = {{"max_tokens", 4096}, {"rpm", 500}},
            .context_window = 8192
        },
        {
            .id = "gpt-3.5-turbo",
            .provider_id = "openai",
            .name = "GPT-3.5 Turbo",
            .description = "Fast and affordable model",
            .capabilities = {.temperature = true, .reasoning = false, .tool_call = true, .streaming = true},
            .pricing = {{"input", 0.0005}, {"output", 0.0015}},
            .limits = {{"max_tokens", 4096}, {"rpm", 500}},
            .context_window = 16384
        },
        {
            .id = "o1-preview",
            .provider_id = "openai",
            .name = "o1 Preview",
            .description = "Reasoning model for complex tasks",
            .capabilities = {.temperature = false, .reasoning = true, .tool_call = false, .streaming = false},
            .pricing = {{"input", 0.015}, {"output", 0.06}},
            .limits = {{"max_tokens", 32768}, {"rpm", 500}},
            .context_window = 128000
        },
        {
            .id = "o1-mini",
            .provider_id = "openai",
            .name = "o1 Mini",
            .description = "Fast reasoning model",
            .capabilities = {.temperature = false, .reasoning = true, .tool_call = false, .streaming = false},
            .pricing = {{"input", 0.003}, {"output", 0.012}},
            .limits = {{"max_tokens", 65536}, {"rpm", 500}},
            .context_window = 128000
        }
    };
}

std::vector<ModelInfo> OpenAIProvider::list_models() const {
    return models_cache_;
}

std::optional<ModelInfo> OpenAIProvider::get_model(const std::string& model_id) const {
    for (const auto& model : models_cache_) {
        if (model.id == model_id) {
            return model;
        }
    }
    return std::nullopt;
}

bool OpenAIProvider::supports_model(const std::string& model_id) const {
    return get_model(model_id).has_value();
}

network::HttpHeaders OpenAIProvider::build_headers() const {
    network::HttpHeaders headers;
    headers.emplace_back("Content-Type", "application/json");
    headers.emplace_back("Authorization", "Bearer " + config_.api_key);
    if (!config_.organization.empty()) {
        headers.emplace_back("OpenAI-Organization", config_.organization);
    }
    return headers;
}

nlohmann::json OpenAIProvider::build_request_body(
    const std::vector<ChatMessage>& messages,
    const std::string& model_id,
    const ChatOptions& options,
    bool stream
) const {
    nlohmann::json body = {
        {"model", model_id},
        {"stream", stream}
    };

    // Build messages array
    nlohmann::json messages_array = nlohmann::json::array();
    for (const auto& msg : messages) {
        messages_array.push_back(msg.to_json());
    }
    body["messages"] = messages_array;

    // Add options
    auto model_info = get_model(model_id);
    
    // Only add temperature if model supports it
    if (model_info && model_info->capabilities.temperature) {
        body["temperature"] = options.temperature;
    }
    
    body["top_p"] = options.top_p;
    
    // Add max_tokens (different name for o1 models)
    if (model_info && model_info->capabilities.reasoning) {
        body["max_completion_tokens"] = options.max_tokens;
    } else {
        body["max_tokens"] = options.max_tokens;
    }

    if (!options.stop.empty()) {
        body["stop"] = options.stop;
    }

    // Add tools if provided
    if (!options.tools.empty()) {
        nlohmann::json tools_array = nlohmann::json::array();
        for (const auto& tool : options.tools) {
            tools_array.push_back(tool.to_json());
        }
        body["tools"] = tools_array;
    }

    if (options.user) {
        body["user"] = *options.user;
    }

    // Add extra options
    for (auto& [key, value] : options.extra.items()) {
        body[key] = value;
    }

    return body;
}

ChatResponse OpenAIProvider::parse_response(const nlohmann::json& response) const {
    ChatResponse result;
    
    if (response.contains("error")) {
        result.error = response["error"];
        return result;
    }

    result.id = response.value("id", std::string{});
    result.model = response.value("model", std::string{});

    if (response.contains("choices") && response["choices"].is_array()) {
        for (const auto& choice : response["choices"]) {
            ChatMessage msg;
            msg.role = chat_role_from_string(choice.value("message", nlohmann::json::object()).value("role", "assistant"));
            msg.content = choice["message"].value("content", std::string{});
            
            if (choice["message"].contains("tool_calls")) {
                std::vector<ToolCall> calls;
                for (const auto& tc : choice["message"]["tool_calls"]) {
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

ChatStreamEvent OpenAIProvider::parse_stream_chunk(const std::string& chunk) const {
    ChatStreamEvent event;
    
    // Parse SSE format: "data: {...}\n\n"
    if (chunk.empty() || chunk == "data: [DONE]") {
        event.type = StreamEventType::Finish;
        return event;
    }

    std::string json_str;
    if (chunk.starts_with("data: ")) {
        json_str = chunk.substr(6);
    } else {
        json_str = chunk;
    }

    try {
        auto json = nlohmann::json::parse(json_str);
        
        if (json.contains("error")) {
            event.type = StreamEventType::Error;
            event.error = json["error"];
            return event;
        }

        if (json.contains("choices") && json["choices"].is_array() && !json["choices"].empty()) {
            const auto& choice = json["choices"][0];
            
            if (choice.contains("finish_reason") && !choice["finish_reason"].is_null()) {
                event.type = StreamEventType::Finish;
                event.finish_reason = choice["finish_reason"].get<std::string>();
                return event;
            }

            if (choice.contains("delta")) {
                const auto& delta = choice["delta"];
                
                // Check for tool calls in delta
                if (delta.contains("tool_calls")) {
                    event.type = StreamEventType::ToolCall;
                    for (const auto& tc : delta["tool_calls"]) {
                        ToolCall call;
                        call.id = tc.value("id", std::string{});
                        call.type = "function";
                        if (tc.contains("function")) {
                            call.name = tc["function"].value("name", std::string{});
                            std::string args = tc["function"].value("arguments", std::string{});
                            if (!args.empty()) {
                                event.content = args;
                            }
                        }
                        if (!call.id.empty()) {
                            event.tool_call = call;
                        }
                    }
                } else if (delta.contains("content")) {
                    event.type = StreamEventType::TextDelta;
                    event.content = delta.value("content", std::string{});
                }
            }
        }

        // Extract usage if present
        if (json.contains("usage")) {
            event.usage = TokenUsage::from_json(json["usage"]);
        }

    } catch (const nlohmann::json::parse_error& e) {
        event.type = StreamEventType::Error;
        event.error = {{"message", std::string("JSON parse error: ") + e.what()}};
    }

    return event;
}

ChatResponse OpenAIProvider::chat(
    const std::vector<ChatMessage>& messages,
    const std::string& model_id,
    const ChatOptions& options
) {
    if (!is_ready()) {
        ChatResponse error_resp;
        error_resp.error = {{"message", "OpenAI provider not configured"}};
        return error_resp;
    }

    auto body = build_request_body(messages, model_id, options, false);
    auto headers = build_headers();
    
    std::string url = config_.base_url + "/chat/completions";
    
    auto http_response = http_client_->post_json(url, body.dump(), headers);
    
    if (!http_response.is_success()) {
        ChatResponse error_resp;
        try {
            error_resp.error = nlohmann::json::parse(http_response.body);
        } catch (const nlohmann::json::parse_error&) {
            error_resp.error = {{"message", http_response.body}};
        }
        return error_resp;
    }

    try {
        return parse_response(nlohmann::json::parse(http_response.body));
    } catch (const nlohmann::json::parse_error& e) {
        ChatResponse error_resp;
        error_resp.error = {{"message", std::string("Failed to parse response: ") + e.what()}};
        return error_resp;
    }
}

ChatResponse OpenAIProvider::chat_stream(
    const std::vector<ChatMessage>& messages,
    const std::string& model_id,
    const ChatOptions& options,
    StreamCallback callback
) {
    if (!is_ready()) {
        ChatResponse error_resp;
        error_resp.error = {{"message", "OpenAI provider not configured"}};
        return error_resp;
    }

    auto body = build_request_body(messages, model_id, options, true);
    auto headers = build_headers();
    
    std::string url = config_.base_url + "/chat/completions";
    
    ChatResponse final_response;
    std::string accumulated_content;
    std::vector<ToolCall> accumulated_tool_calls;
    std::string current_tool_call_id;
    std::string current_tool_call_name;
    std::string current_tool_args;

    auto http_response = http_client_->request_stream(
        network::HttpRequest::post(url, body.dump())
            .with_headers(headers),
        [&](std::string_view chunk) -> bool {
            // Parse SSE data
            std::string chunk_str(chunk);
            
            // Handle multiple SSE events in one chunk
            size_t pos = 0;
            while (pos < chunk_str.size()) {
                size_t end = chunk_str.find("\n\n", pos);
                std::string event_data;
                
                if (end != std::string::npos) {
                    event_data = chunk_str.substr(pos, end - pos);
                    pos = end + 2;
                } else {
                    event_data = chunk_str.substr(pos);
                    pos = chunk_str.size();
                }
                
                if (event_data.empty()) continue;
                
                auto event = parse_stream_chunk(event_data);
                
                // Accumulate content
                if (event.type == StreamEventType::TextDelta) {
                    accumulated_content += event.content;
                } else if (event.type == StreamEventType::ToolCall) {
                    if (event.tool_call) {
                        if (!event.tool_call->id.empty()) {
                            // New tool call
                            if (!current_tool_call_id.empty()) {
                                // Save previous tool call
                                ToolCall tc;
                                tc.id = current_tool_call_id;
                                tc.type = "function";
                                tc.name = current_tool_call_name;
                                try {
                                    tc.arguments = nlohmann::json::parse(current_tool_args);
                                } catch (const nlohmann::json::parse_error& e) {
                                    TURBOT_LOG_ERROR("Failed to parse tool call arguments: {}", e.what());
                                    tc.arguments = nlohmann::json::object();
                                }
                                accumulated_tool_calls.push_back(tc);
                            }
                            current_tool_call_id = event.tool_call->id;
                            current_tool_call_name = event.tool_call->name;
                            current_tool_args = event.content;
                        } else {
                            // Continuation of tool call arguments
                            current_tool_args += event.content;
                        }
                    }
                } else if (event.type == StreamEventType::Finish) {
                    // Save final tool call if any
                    if (!current_tool_call_id.empty()) {
                        ToolCall tc;
                        tc.id = current_tool_call_id;
                        tc.type = "function";
                        tc.name = current_tool_call_name;
                        try {
                            tc.arguments = nlohmann::json::parse(current_tool_args);
                        } catch (const nlohmann::json::parse_error& e) {
                            TURBOT_LOG_ERROR("Failed to parse tool call arguments: {}", e.what());
                            tc.arguments = nlohmann::json::object();
                        }
                        accumulated_tool_calls.push_back(tc);
                    }
                    final_response.finish_reason = event.finish_reason.value_or("");
                    final_response.usage = event.usage;
                }
                
                // Call user callback
                if (!callback(event)) {
                    return false;  // Abort stream
                }
            }
            
            return true;  // Continue streaming
        }
    );

    // Build final response
    final_response.id = "chatcmpl-" + generate_uuid().substr(0, 8);
    final_response.model = model_id;
    
    ChatMessage assistant_msg;
    assistant_msg.role = ChatRole::Assistant;
    assistant_msg.content = accumulated_content;
    if (!accumulated_tool_calls.empty()) {
        assistant_msg.tool_calls = accumulated_tool_calls;
    }
    final_response.choices.push_back(assistant_msg);

    return final_response;
}

int64_t OpenAIProvider::count_tokens(
    const std::vector<ChatMessage>& messages,
    [[maybe_unused]] const std::string& model_id
) const {
    // Simple approximation: ~4 characters per token for English text
    // This is a rough estimate; real implementations should use tiktoken
    int64_t total_chars = 0;
    for (const auto& msg : messages) {
        total_chars += msg.content.size();
        if (msg.tool_calls) {
            for (const auto& tc : *msg.tool_calls) {
                total_chars += tc.name.size() + tc.arguments.dump().size();
            }
        }
    }
    return total_chars / 4;
}

bool OpenAIProvider::validate() {
    if (!is_ready()) {
        return false;
    }

    // Make a simple API call to validate
    try {
        auto response = chat(
            {ChatMessage::user("hi")},
            "gpt-3.5-turbo",
            ChatOptions{.max_tokens = 5}
        );
        return !response.is_error();
    } catch (const std::exception& e) {
        TURBOT_LOG_DEBUG("OpenAI validation failed: {}", e.what());
        return false;
    }
}

} // namespace turbot::core::provider
