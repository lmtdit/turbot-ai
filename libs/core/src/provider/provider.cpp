#include <turbot/core/provider/provider.hpp>
#include <turbot/core/provider/provider_manager.hpp>
#include <algorithm>

namespace turbot::core::provider {

// ===== ModelCapabilities =====

nlohmann::json ModelCapabilities::to_json() const {
    return nlohmann::json{
        {"temperature", temperature},
        {"reasoning", reasoning},
        {"tool_call", tool_call},
        {"streaming", streaming},
        {"vision", vision},
        {"audio", audio}
    };
}

ModelCapabilities ModelCapabilities::from_json(const nlohmann::json& j) {
    ModelCapabilities caps;
    caps.temperature = j.value("temperature", true);
    caps.reasoning = j.value("reasoning", false);
    caps.tool_call = j.value("tool_call", true);
    caps.streaming = j.value("streaming", true);
    caps.vision = j.value("vision", false);
    caps.audio = j.value("audio", false);
    return caps;
}

// ===== ModelInfo =====

nlohmann::json ModelInfo::to_json() const {
    return nlohmann::json{
        {"id", id},
        {"provider_id", provider_id},
        {"name", name},
        {"description", description},
        {"capabilities", capabilities.to_json()},
        {"pricing", pricing},
        {"limits", limits},
        {"context_window", context_window}
    };
}

ModelInfo ModelInfo::from_json(const nlohmann::json& j) {
    ModelInfo info;
    info.id = j.value("id", std::string{});
    info.provider_id = j.value("provider_id", std::string{});
    info.name = j.value("name", std::string{});
    info.description = j.value("description", std::string{});
    if (j.contains("capabilities")) {
        info.capabilities = ModelCapabilities::from_json(j["capabilities"]);
    }
    info.pricing = j.value("pricing", nlohmann::json::object());
    info.limits = j.value("limits", nlohmann::json::object());
    info.context_window = j.value("context_window", int64_t{4096});
    return info;
}

// ===== ChatRole =====

std::string_view chat_role_to_string(ChatRole role) noexcept {
    switch (role) {
        case ChatRole::System:    return "system";
        case ChatRole::User:      return "user";
        case ChatRole::Assistant: return "assistant";
        case ChatRole::Tool:      return "tool";
    }
    return "unknown";
}

ChatRole chat_role_from_string(std::string_view str) {
    if (str == "system")    return ChatRole::System;
    if (str == "user")      return ChatRole::User;
    if (str == "assistant") return ChatRole::Assistant;
    if (str == "tool")      return ChatRole::Tool;
    return ChatRole::User;  // Default
}

// ===== ToolCall =====

nlohmann::json ToolCall::to_json() const {
    nlohmann::json j = {
        {"id", id},
        {"type", type},
        {"function", {
            {"name", name},
            {"arguments", arguments.dump()}
        }}
    };
    return j;
}

ToolCall ToolCall::from_json(const nlohmann::json& j) {
    ToolCall call;
    call.id = j.value("id", std::string{});
    call.type = j.value("type", std::string{"function"});
    
    if (j.contains("function")) {
        const auto& func = j["function"];
        call.name = func.value("name", std::string{});
        std::string args_str = func.value("arguments", std::string{"{}"});
        try {
            call.arguments = nlohmann::json::parse(args_str);
        } catch (const nlohmann::json::parse_error&) {
            call.arguments = nlohmann::json::object();
        }
    }
    return call;
}

// ===== ChatMessage =====

nlohmann::json ChatMessage::to_json() const {
    nlohmann::json j = {
        {"role", std::string(chat_role_to_string(role))},
        {"content", content}
    };
    if (name) j["name"] = *name;
    if (tool_call_id) j["tool_call_id"] = *tool_call_id;
    if (tool_calls) {
        nlohmann::json tc_array = nlohmann::json::array();
        for (const auto& tc : *tool_calls) {
            tc_array.push_back(tc.to_json());
        }
        j["tool_calls"] = tc_array;
    }
    return j;
}

ChatMessage ChatMessage::from_json(const nlohmann::json& j) {
    ChatMessage msg;
    msg.role = chat_role_from_string(j.value("role", std::string{"user"}));
    msg.content = j.value("content", std::string{});
    msg.name = j.contains("name") ? std::optional<std::string>(j["name"].get<std::string>()) : std::nullopt;
    msg.tool_call_id = j.contains("tool_call_id") 
        ? std::optional<std::string>(j["tool_call_id"].get<std::string>()) 
        : std::nullopt;
    
    if (j.contains("tool_calls")) {
        std::vector<ToolCall> calls;
        for (const auto& tc : j["tool_calls"]) {
            calls.push_back(ToolCall::from_json(tc));
        }
        msg.tool_calls = calls;
    }
    return msg;
}

ChatMessage ChatMessage::system(const std::string& content) {
    ChatMessage msg;
    msg.role = ChatRole::System;
    msg.content = content;
    return msg;
}

ChatMessage ChatMessage::user(const std::string& content) {
    ChatMessage msg;
    msg.role = ChatRole::User;
    msg.content = content;
    return msg;
}

ChatMessage ChatMessage::assistant(const std::string& content) {
    ChatMessage msg;
    msg.role = ChatRole::Assistant;
    msg.content = content;
    return msg;
}

ChatMessage ChatMessage::assistant_with_tools(
    const std::string& content,
    const std::vector<ToolCall>& tool_calls
) {
    ChatMessage msg;
    msg.role = ChatRole::Assistant;
    msg.content = content;
    msg.tool_calls = tool_calls;
    return msg;
}

ChatMessage ChatMessage::tool_result(
    const std::string& tool_call_id,
    const std::string& content
) {
    ChatMessage msg;
    msg.role = ChatRole::Tool;
    msg.tool_call_id = tool_call_id;
    msg.content = content;
    return msg;
}

// ===== ToolDefinition =====

nlohmann::json ToolDefinition::to_json() const {
    return nlohmann::json{
        {"type", type},
        {"function", {
            {"name", name},
            {"description", description},
            {"parameters", parameters}
        }}
    };
}

ToolDefinition ToolDefinition::from_json(const nlohmann::json& j) {
    ToolDefinition def;
    def.type = j.value("type", std::string{"function"});
    
    if (j.contains("function")) {
        const auto& func = j["function"];
        def.name = func.value("name", std::string{});
        def.description = func.value("description", std::string{});
        def.parameters = func.value("parameters", nlohmann::json::object());
    }
    return def;
}

// ===== StreamEventType =====

std::string_view stream_event_type_to_string(StreamEventType type) noexcept {
    switch (type) {
        case StreamEventType::TextDelta:  return "text_delta";
        case StreamEventType::ToolCall:   return "tool_call";
        case StreamEventType::Reasoning:  return "reasoning";
        case StreamEventType::Finish:     return "finish";
        case StreamEventType::Error:      return "error";
    }
    return "unknown";
}

StreamEventType stream_event_type_from_string(std::string_view str) {
    if (str == "text_delta") return StreamEventType::TextDelta;
    if (str == "tool_call")  return StreamEventType::ToolCall;
    if (str == "reasoning")  return StreamEventType::Reasoning;
    if (str == "finish")     return StreamEventType::Finish;
    if (str == "error")      return StreamEventType::Error;
    return StreamEventType::TextDelta;
}

// ===== ChatStreamEvent =====

nlohmann::json ChatStreamEvent::to_json() const {
    nlohmann::json j = {
        {"type", std::string(stream_event_type_to_string(type))},
        {"content", content}
    };
    if (tool_call) j["tool_call"] = tool_call->to_json();
    if (finish_reason) j["finish_reason"] = *finish_reason;
    if (error) j["error"] = *error;
    if (usage.total() > 0) j["usage"] = usage.to_json();
    return j;
}

ChatStreamEvent ChatStreamEvent::from_json(const nlohmann::json& j) {
    ChatStreamEvent event;
    event.type = stream_event_type_from_string(j.value("type", std::string{"text_delta"}));
    event.content = j.value("content", std::string{});
    
    if (j.contains("tool_call")) {
        event.tool_call = ToolCall::from_json(j["tool_call"]);
    }
    if (j.contains("finish_reason")) {
        event.finish_reason = j["finish_reason"].get<std::string>();
    }
    if (j.contains("error")) {
        event.error = j["error"];
    }
    if (j.contains("usage")) {
        event.usage = TokenUsage::from_json(j["usage"]);
    }
    return event;
}

// ===== ChatOptions =====

nlohmann::json ChatOptions::to_json() const {
    nlohmann::json j = {
        {"temperature", temperature},
        {"top_p", top_p},
        {"max_tokens", max_tokens},
        {"stream", stream}
    };
    if (!stop.empty()) {
        j["stop"] = stop;
    }
    if (!tools.empty()) {
        nlohmann::json tools_array = nlohmann::json::array();
        for (const auto& tool : tools) {
            tools_array.push_back(tool.to_json());
        }
        j["tools"] = tools_array;
    }
    if (user) {
        j["user"] = *user;
    }
    if (!extra.empty()) {
        for (auto& [key, value] : extra.items()) {
            j[key] = value;
        }
    }
    return j;
}

ChatOptions ChatOptions::from_json(const nlohmann::json& j) {
    ChatOptions opts;
    opts.temperature = j.value("temperature", 1.0);
    opts.top_p = j.value("top_p", 1.0);
    opts.max_tokens = j.value("max_tokens", 4096);
    opts.stream = j.value("stream", false);
    
    if (j.contains("stop")) {
        for (const auto& s : j["stop"]) {
            opts.stop.push_back(s.get<std::string>());
        }
    }
    if (j.contains("tools")) {
        for (const auto& t : j["tools"]) {
            opts.tools.push_back(ToolDefinition::from_json(t));
        }
    }
    opts.user = j.contains("user") 
        ? std::optional<std::string>(j["user"].get<std::string>()) 
        : std::nullopt;
    
    // Store unknown keys as extra
    const std::vector<std::string> known_keys = {
        "temperature", "top_p", "max_tokens", "stream", "stop", "tools", "user"
    };
    for (auto& [key, value] : j.items()) {
        if (std::find(known_keys.begin(), known_keys.end(), key) == known_keys.end()) {
            opts.extra[key] = value;
        }
    }
    return opts;
}

// ===== ChatResponse =====

nlohmann::json ChatResponse::to_json() const {
    nlohmann::json j = {
        {"id", id},
        {"model", model},
        {"finish_reason", finish_reason}
    };
    
    nlohmann::json choices_array = nlohmann::json::array();
    for (const auto& choice : choices) {
        choices_array.push_back(choice.to_json());
    }
    j["choices"] = choices_array;
    j["usage"] = usage.to_json();
    if (error) j["error"] = *error;
    return j;
}

ChatResponse ChatResponse::from_json(const nlohmann::json& j) {
    ChatResponse resp;
    resp.id = j.value("id", std::string{});
    resp.model = j.value("model", std::string{});
    resp.finish_reason = j.value("finish_reason", std::string{});
    
    if (j.contains("choices")) {
        for (const auto& c : j["choices"]) {
            resp.choices.push_back(ChatMessage::from_json(c));
        }
    }
    if (j.contains("usage")) {
        resp.usage = TokenUsage::from_json(j["usage"]);
    }
    if (j.contains("error")) {
        resp.error = j["error"];
    }
    return resp;
}

std::string ChatResponse::get_text() const {
    std::string text;
    for (const auto& choice : choices) {
        if (choice.role == ChatRole::Assistant && !choice.content.empty()) {
            text += choice.content;
        }
    }
    return text;
}

bool ChatResponse::has_tool_calls() const {
    for (const auto& choice : choices) {
        if (choice.tool_calls && !choice.tool_calls->empty()) {
            return true;
        }
    }
    return false;
}

// ===== ProviderConfig =====

nlohmann::json ProviderConfig::to_json() const {
    nlohmann::json j = {
        {"api_key", api_key},
        {"base_url", base_url},
        {"organization", organization},
        {"timeout_seconds", timeout_seconds},
        {"max_retries", max_retries},
        {"verify_ssl", verify_ssl}
    };
    if (!proxy.empty()) {
        j["proxy"] = proxy;
    }
    if (!extra.empty()) {
        j["extra"] = extra;
    }
    return j;
}

ProviderConfig ProviderConfig::from_json(const nlohmann::json& j) {
    ProviderConfig config;
    config.api_key = j.value("api_key", std::string{});
    config.base_url = j.value("base_url", std::string{});
    config.organization = j.value("organization", std::string{});
    config.timeout_seconds = j.value("timeout_seconds", 60);
    config.max_retries = j.value("max_retries", 3);
    config.verify_ssl = j.value("verify_ssl", true);
    config.proxy = j.value("proxy", std::string{});
    config.extra = j.value("extra", nlohmann::json::object());
    return config;
}

// ===== ProviderManager =====

ProviderManager& ProviderManager::instance() noexcept {
    static ProviderManager instance;
    return instance;
}

void ProviderManager::register_provider(ProviderPtr provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (provider) {
        providers_[provider->id()] = std::move(provider);
    }
}

bool ProviderManager::unregister_provider(const std::string& provider_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return providers_.erase(provider_id) > 0;
}

std::optional<ProviderPtr> ProviderManager::get_provider(const std::string& provider_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = providers_.find(provider_id);
    if (it != providers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<ProviderPtr> ProviderManager::list_providers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ProviderPtr> result;
    result.reserve(providers_.size());
    for (const auto& [id, provider] : providers_) {
        result.push_back(provider);
    }
    return result;
}

bool ProviderManager::has_provider(const std::string& provider_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return providers_.find(provider_id) != providers_.end();
}

std::vector<ModelInfo> ProviderManager::list_all_models() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ModelInfo> all_models;
    for (const auto& [id, provider] : providers_) {
        auto models = provider->list_models();
        all_models.insert(all_models.end(), models.begin(), models.end());
    }
    return all_models;
}

std::optional<ModelInfo> ProviderManager::find_model(const std::string& model_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [id, provider] : providers_) {
        auto model = provider->get_model(model_id);
        if (model) {
            return model;
        }
    }
    return std::nullopt;
}

std::optional<ProviderPtr> ProviderManager::find_provider_for_model(const std::string& model_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [id, provider] : providers_) {
        if (provider->supports_model(model_id)) {
            return provider;
        }
    }
    return std::nullopt;
}

void ProviderManager::set_default_provider(const std::string& provider_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_provider_id_ = provider_id;
}

std::optional<ProviderPtr> ProviderManager::get_default_provider() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!default_provider_id_.empty()) {
        auto it = providers_.find(default_provider_id_);
        if (it != providers_.end()) {
            return it->second;
        }
    }
    // Return first provider if no default set
    if (!providers_.empty()) {
        return providers_.begin()->second;
    }
    return std::nullopt;
}

void ProviderManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    providers_.clear();
    factories_.clear();
    default_provider_id_.clear();
}

void ProviderManager::register_factory(const std::string& provider_id, ProviderFactory factory) {
    std::lock_guard<std::mutex> lock(mutex_);
    factories_[provider_id] = std::move(factory);
}

std::optional<ProviderPtr> ProviderManager::create_provider(
    const std::string& provider_id,
    const ProviderConfig& config
) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = factories_.find(provider_id);
    if (it != factories_.end()) {
        return it->second(config);
    }
    return std::nullopt;
}

} // namespace turbot::core::provider
