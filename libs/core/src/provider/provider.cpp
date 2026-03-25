#include <turbot/core/provider/provider.hpp>
#include <turbot/core/provider/provider_manager.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/core/global/global.hpp>
#include <turbot/network/http_client.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace turbot::core::provider {

// ===== ModelCapabilities =====

nlohmann::json ModelCapabilities::to_json() const {
    nlohmann::json j{
        {"temperature", temperature},
        {"reasoning", reasoning},
        {"tool_call", tool_call},
        {"streaming", streaming},
        {"vision", vision},
        {"audio", audio}
    };
    // Only emit "interleavedField" when set (keeps JSON compact for non-interleaved models).
    if (!interleaved_field.empty()) {
        j["interleavedField"] = interleaved_field;
    }
    return j;
}

ModelCapabilities ModelCapabilities::from_json(const nlohmann::json& j) {
    ModelCapabilities caps;
    caps.temperature = j.value("temperature", true);
    caps.reasoning = j.value("reasoning", false);
    caps.tool_call = j.value("tool_call", true);
    caps.streaming = j.value("streaming", true);
    caps.vision = j.value("vision", false);
    caps.audio = j.value("audio", false);
    // Accept both camelCase "interleavedField" and snake_case "interleaved_field".
    if (j.contains("interleavedField") && j["interleavedField"].is_string()) {
        caps.interleaved_field = j["interleavedField"].get<std::string>();
    } else if (j.contains("interleaved_field") && j["interleaved_field"].is_string()) {
        caps.interleaved_field = j["interleaved_field"].get<std::string>();
    }
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
        // Mask api_key to prevent accidental credential leakage in logs/responses
        {"api_key", api_key.empty() ? "" : "***"},
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
    // Copy provider pointers under the lock, then call virtual methods outside
    // the lock to avoid holding the mutex during potentially slow provider calls
    // (which could re-enter through a registered callback and deadlock).
    std::vector<ProviderPtr> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot.reserve(providers_.size());
        for (const auto& [id, provider] : providers_) {
            snapshot.push_back(provider);
        }
    }
    std::vector<ModelInfo> all_models;
    for (const auto& provider : snapshot) {
        auto models = provider->list_models();
        all_models.insert(all_models.end(), models.begin(), models.end());
    }
    return all_models;
}

std::optional<ModelInfo> ProviderManager::find_model(const std::string& model_id) const {
    std::vector<ProviderPtr> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot.reserve(providers_.size());
        for (const auto& [id, provider] : providers_) {
            snapshot.push_back(provider);
        }
    }
    for (const auto& provider : snapshot) {
        auto model = provider->get_model(model_id);
        if (model) {
            return model;
        }
    }
    return std::nullopt;
}

std::optional<ProviderPtr> ProviderManager::find_provider_for_model(const std::string& model_id) const {
    std::vector<ProviderPtr> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot.reserve(providers_.size());
        for (const auto& [id, provider] : providers_) {
            snapshot.push_back(provider);
        }
    }
    for (const auto& provider : snapshot) {
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

// ============================================================================
// T10: ModelsDev implementation
// Mirrors OpenCode provider/models.ts ModelsDev namespace.
// Priority chain: in-memory cache → local file cache → network → built-in fallback.
// ============================================================================

namespace turbot::core::provider {

namespace fs = std::filesystem;

ModelsDev& ModelsDev::instance() noexcept {
    static ModelsDev inst;
    return inst;
}

std::string ModelsDev::cache_file_path() {
    // Mirror OpenCode: Global.Path.cache / "models.json"
    try {
        const auto& p = turbot::core::global::Global::path();
        return (fs::path(p.cache) / "models.json").string();
    } catch (...) {
        // Fallback if Global is not initialised (tests)
        const char* home = std::getenv("HOME");
        if (home) return std::string(home) + "/.turbot/cache/models.json";
        return "/tmp/turbot_models_cache.json";
    }
}

nlohmann::json ModelsDev::load_from_cache() {
    const std::string path = cache_file_path();
    std::ifstream f(path);
    if (!f.is_open()) return {};
    try {
        nlohmann::json j;
        f >> j;
        TURBOT_LOG_DEBUG("ModelsDev: loaded snapshot from cache file {}", path);
        return j;
    } catch (const std::exception& ex) {
        TURBOT_LOG_WARN("ModelsDev: failed to parse cache file {}: {}", path, ex.what());
        return {};
    }
}

nlohmann::json ModelsDev::fetch_from_network() {
    // Check for env-var override (mirrors OpenCode Flag.OPENCODE_MODELS_URL)
    const char* url_override = std::getenv("OPENCODE_MODELS_URL");
    const std::string base_url = url_override ? url_override : kDefaultModelsUrl;
    const std::string endpoint = base_url + "/api.json";

    TURBOT_LOG_INFO("ModelsDev: fetching snapshot from {}", endpoint);
    try {
        turbot::network::HttpClient http;
        const turbot::network::HttpHeaders headers = {
            {"Accept", "application/json"},
            {"User-Agent", "turbot-ai/1.0"}
        };
        auto resp = http.get(endpoint, headers);
        if (!resp.is_success()) {
            TURBOT_LOG_WARN("ModelsDev: GET {} returned status {}", endpoint, resp.status_code);
            return {};
        }
        auto j = nlohmann::json::parse(resp.body);
        TURBOT_LOG_INFO("ModelsDev: fetched {} provider(s) from models.dev", j.size());

        // Persist to local cache for next session
        const std::string cache_path = cache_file_path();
        try {
            fs::path p(cache_path);
            if (p.has_parent_path()) {
                std::error_code ec;
                fs::create_directories(p.parent_path(), ec);
            }
            std::ofstream cf(cache_path);
            if (cf.is_open()) {
                cf << j.dump();
                TURBOT_LOG_DEBUG("ModelsDev: persisted snapshot to {}", cache_path);
            }
        } catch (const std::exception& ex) {
            TURBOT_LOG_WARN("ModelsDev: failed to write cache: {}", ex.what());
        }

        return j;
    } catch (const std::exception& ex) {
        TURBOT_LOG_ERROR("ModelsDev: fetch_from_network exception: {}", ex.what());
        return {};
    }
}

nlohmann::json ModelsDev::build_fallback_snapshot() {
    // Build a minimal snapshot from the models registered with ProviderManager.
    // This ensures TUI model lists work offline even without a models.dev fetch.
    nlohmann::json snapshot = nlohmann::json::object();
    const auto providers = ProviderManager::instance().list_providers();
    for (const auto& prov : providers) {
        nlohmann::json models_obj = nlohmann::json::object();
        for (const auto& m : prov->list_models()) {
            models_obj[m.id] = {
                {"id",      m.id},
                {"name",    m.name},
                {"limit",   {{"context", m.context_window}, {"output", m.limits.value("max_tokens", 4096)}}},
                {"temperature", m.capabilities.temperature},
                {"reasoning",   m.capabilities.reasoning},
                {"tool_call",   m.capabilities.tool_call},
                {"attachment",  m.capabilities.vision},
                {"release_date", "2024-01-01"}  // placeholder
            };
        }
        snapshot[prov->id()] = {
            {"id",     prov->id()},
            {"name",   prov->name()},
            {"env",    nlohmann::json::array()},
            {"models", models_obj}
        };
    }
    TURBOT_LOG_DEBUG("ModelsDev: built fallback snapshot with {} provider(s)", providers.size());
    return snapshot;
}

nlohmann::json ModelsDev::get(bool force_refresh) {
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        if (!force_refresh && cache_loaded_ && !cached_.empty()) {
            return cached_;
        }
    }
    // NOTE: Two concurrent callers may both reach this point and each trigger
    // load_from_cache()/fetch_from_network() once.  This is intentionally
    // tolerated: the operations are idempotent and the worst-case overhead is a
    // single duplicate network request.  Using a secondary "loading" flag and a
    // condition-variable would be overkill for this low-frequency cold-start path.

    // Check for env-var override to skip network fetch
    const bool disable_fetch = std::getenv("OPENCODE_DISABLE_MODELS_FETCH") != nullptr;

    // Priority chain (mirrors OpenCode models.ts Data lazy):
    // 1. Local cache file
    auto snapshot = load_from_cache();

    // 2. Network fetch (unless disabled)
    if (snapshot.empty() && !disable_fetch) {
        snapshot = fetch_from_network();
    }

    // 3. Built-in fallback from ProviderManager
    if (snapshot.empty()) {
        TURBOT_LOG_WARN("ModelsDev: no snapshot available; using built-in fallback");
        snapshot = build_fallback_snapshot();
    }

    std::lock_guard<std::mutex> wlock(cache_mutex_);
    cached_ = snapshot;
    cache_loaded_ = true;
    return cached_;
}

bool ModelsDev::refresh() {
    auto snapshot = fetch_from_network();
    if (snapshot.empty()) return false;
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cached_ = snapshot;
    cache_loaded_ = true;
    return true;
}

void ModelsDev::clear_cache() noexcept {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cached_ = {};
    cache_loaded_ = false;
}

} // namespace turbot::core::provider

