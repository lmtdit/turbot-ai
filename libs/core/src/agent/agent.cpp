#include <turbot/core/agent/agent.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <shared_mutex>
#include <unordered_map>

namespace turbot::core::agent {

// Constants for field validation
namespace {
    constexpr double TEMPERATURE_MIN = 0.0;
    constexpr double TEMPERATURE_MAX = 2.0;
    constexpr double TOP_P_MIN = 0.0;
    constexpr double TOP_P_MAX = 1.0;
    constexpr int STEPS_MIN = 1;
}

// AgentMode conversion functions
std::string agent_mode_to_string(AgentMode mode) {
    switch (mode) {
        case AgentMode::Primary: return "primary";
        case AgentMode::Subagent: return "subagent";
        case AgentMode::All: return "all";
    }
    throw std::invalid_argument(fmt::format("Invalid AgentMode value: {}", static_cast<int>(mode)));
}

AgentMode string_to_agent_mode(const std::string& str) {
    if (str == "primary") return AgentMode::Primary;
    if (str == "subagent") return AgentMode::Subagent;
    if (str == "all") return AgentMode::All;
    throw std::invalid_argument(fmt::format("Invalid agent mode: {}", str));
}

// ModelRef implementation
nlohmann::json ModelRef::to_json() const {
    return nlohmann::json{
        {"model_id", model_id},
        {"provider_id", provider_id}
    };
}

ModelRef ModelRef::from_json(const nlohmann::json& j) {
    ModelRef ref;
    ref.model_id = j.at("model_id").get<std::string>();
    ref.provider_id = j.at("provider_id").get<std::string>();
    return ref;
}

// AgentInfo validation
bool AgentInfo::validate() const noexcept {
    // Validate temperature range
    if (temperature.has_value()) {
        if (*temperature < TEMPERATURE_MIN || *temperature > TEMPERATURE_MAX) {
            return false;
        }
    }
    
    // Validate top_p range
    if (top_p.has_value()) {
        if (*top_p < TOP_P_MIN || *top_p > TOP_P_MAX) {
            return false;
        }
    }
    
    // Validate steps (must be positive)
    if (steps.has_value()) {
        if (*steps < STEPS_MIN) {
            return false;
        }
    }
    
    return true;
}

// AgentInfo implementation
nlohmann::json AgentInfo::to_json() const {
    nlohmann::json j;
    // Basic fields (v1.0)
    j["name"] = name;
    if (description) {
        j["description"] = *description;
    }
    j["mode"] = agent_mode_to_string(mode);
    j["native"] = native;
    j["hidden"] = hidden;
    
    // Serialize permission ruleset
    nlohmann::json perm_array = nlohmann::json::array();
    for (const auto& rule : permission) {
        perm_array.push_back(rule.to_json());
    }
    j["permission"] = perm_array;
    
    if (model) {
        j["model"] = model->to_json();
    }
    if (options.is_object() && !options.empty()) {
        j["options"] = options;
    }
    
    // Extended fields (v2.0)
    if (prompt) {
        j["prompt"] = *prompt;
    }
    if (temperature) {
        j["temperature"] = *temperature;
    }
    if (top_p) {
        j["top_p"] = *top_p;
    }
    if (steps) {
        j["steps"] = *steps;
    }
    if (color) {
        j["color"] = *color;
    }
    if (variant) {
        j["variant"] = *variant;
    }
    
    return j;
}

AgentInfo AgentInfo::from_json(const nlohmann::json& j) {
    AgentInfo info;
    
    // Basic fields (v1.0)
    info.name = j.at("name").get<std::string>();
    
    if (j.contains("description") && !j["description"].is_null()) {
        info.description = j["description"].get<std::string>();
    }
    
    if (j.contains("mode")) {
        info.mode = string_to_agent_mode(j["mode"].get<std::string>());
    }
    
    if (j.contains("native")) {
        info.native = j["native"].get<bool>();
    }
    if (j.contains("hidden")) {
        info.hidden = j["hidden"].get<bool>();
    }
    
    if (j.contains("permission") && j["permission"].is_array()) {
        for (const auto& rule_json : j["permission"]) {
            info.permission.push_back(permission::PermissionRule::from_json(rule_json));
        }
    }
    
    if (j.contains("model") && j["model"].is_object()) {
        info.model = ModelRef::from_json(j["model"]);
    }
    if (j.contains("options") && j["options"].is_object() && !j["options"].empty()) {
        info.options = j["options"];
    }
    
    // Extended fields (v2.0)
    if (j.contains("prompt") && !j["prompt"].is_null()) {
        info.prompt = j["prompt"].get<std::string>();
    }
    if (j.contains("temperature") && !j["temperature"].is_null()) {
        double temp = j["temperature"].get<double>();
        if (temp < TEMPERATURE_MIN || temp > TEMPERATURE_MAX) {
            throw std::out_of_range(
                fmt::format("temperature must be between {} and {}, got {}", 
                           TEMPERATURE_MIN, TEMPERATURE_MAX, temp));
        }
        info.temperature = temp;
    }
    if (j.contains("top_p") && !j["top_p"].is_null()) {
        double tp = j["top_p"].get<double>();
        if (tp < TOP_P_MIN || tp > TOP_P_MAX) {
            throw std::out_of_range(
                fmt::format("top_p must be between {} and {}, got {}", 
                           TOP_P_MIN, TOP_P_MAX, tp));
        }
        info.top_p = tp;
    }
    if (j.contains("steps") && !j["steps"].is_null()) {
        int s = j["steps"].get<int>();
        if (s < STEPS_MIN) {
            throw std::out_of_range(
                fmt::format("steps must be >= {}, got {}", STEPS_MIN, s));
        }
        info.steps = s;
    }
    if (j.contains("color") && !j["color"].is_null()) {
        info.color = j["color"].get<std::string>();
    }
    if (j.contains("variant") && !j["variant"].is_null()) {
        info.variant = j["variant"].get<std::string>();
    }
    
    return info;
}

bool AgentInfo::operator==(const AgentInfo& other) const noexcept {
    // Basic fields (v1.0)
    bool basic_equal = name == other.name &&
           description == other.description &&
           mode == other.mode &&
           native == other.native &&
           hidden == other.hidden &&
           permission == other.permission &&
           model == other.model;
    
    // Extended fields (v2.0)
    bool extended_equal = prompt == other.prompt &&
           temperature == other.temperature &&
           top_p == other.top_p &&
           steps == other.steps &&
           color == other.color &&
           variant == other.variant;
    
    return basic_equal && extended_equal;
    // Note: options field intentionally excluded from comparison
}

// ExecuteResult implementation
ExecuteResult ExecuteResult::ok(const std::string& output, const nlohmann::json& metadata) {
    ExecuteResult result;
    result.output = output;
    result.metadata = metadata;
    result.is_success = true;
    return result;
}

ExecuteResult ExecuteResult::error(const std::string& error_msg) {
    ExecuteResult result;
    result.is_success = false;
    result.error_message = error_msg;
    return result;
}

// Agent implementation
nlohmann::json Agent::to_tool_definition() const {
    return nlohmann::json{
        {"type", "function"},
        {"function", {
            {"name", name()},
            {"description", description()},
            {"parameters", nlohmann::json::object()}
        }}
    };
}

// AgentRegistry implementation
AgentRegistry& AgentRegistry::instance() {
    static AgentRegistry registry;
    return registry;
}

bool AgentRegistry::register_agent(AgentPtr agent) {
    if (!agent) {
        return false;
    }
    
    std::unique_lock lock(mutex_);
    const std::string agent_name = agent->name();
    auto [it, inserted] = agents_.try_emplace(agent_name, std::move(agent));
    return inserted;
}

bool AgentRegistry::unregister_agent(const std::string& name) {
    std::unique_lock lock(mutex_);
    return agents_.erase(name) > 0;
}

AgentPtr AgentRegistry::get(const std::string& name) const {
    std::shared_lock lock(mutex_);
    auto it = agents_.find(name);
    if (it != agents_.end()) {
        return it->second;
    }
    return nullptr;
}

bool AgentRegistry::has(const std::string& name) const {
    std::shared_lock lock(mutex_);
    return agents_.find(name) != agents_.end();
}

std::vector<AgentPtr> AgentRegistry::list_locked() const {
    std::vector<AgentPtr> result;
    result.reserve(agents_.size());
    for (const auto& [name, agent] : agents_) {
        result.push_back(agent);
    }
    return result;
}

std::vector<AgentPtr> AgentRegistry::list() const {
    std::shared_lock lock(mutex_);
    return list_locked();
}

std::vector<AgentPtr> AgentRegistry::list_by_mode(AgentMode mode) const {
    std::shared_lock lock(mutex_);
    std::vector<AgentPtr> result;
    result.reserve(agents_.size());
    for (const auto& [name, agent] : agents_) {
        if (agent->info().mode == mode) {
            result.push_back(agent);
        }
    }
    return result;
}

std::vector<AgentPtr> AgentRegistry::list_visible() const {
    std::shared_lock lock(mutex_);
    std::vector<AgentPtr> result;
    result.reserve(agents_.size());
    for (const auto& [name, agent] : agents_) {
        if (!agent->is_hidden()) {
            result.push_back(agent);
        }
    }
    return result;
}

std::vector<AgentPtr> AgentRegistry::list_primary() const {
    std::shared_lock lock(mutex_);
    std::vector<AgentPtr> result;
    result.reserve(agents_.size());
    for (const auto& [name, agent] : agents_) {
        if (agent->mode() == AgentMode::Primary && !agent->is_hidden()) {
            result.push_back(agent);
        }
    }
    return result;
}

std::vector<std::string> AgentRegistry::names() const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> result;
    result.reserve(agents_.size());
    for (const auto& [name, agent] : agents_) {
        result.push_back(name);
    }
    return result;
}

std::string AgentRegistry::default_agent() const {
    std::shared_lock lock(mutex_);
    
    // First, try to find "build" agent (the default)
    if (agents_.count("build") > 0) {
        auto build = agents_.at("build");
        if (build && build->mode() == AgentMode::Primary && !build->is_hidden()) {
            return "build";
        }
    }
    
    // Otherwise, find the first visible primary agent
    for (const auto& [name, agent] : agents_) {
        if (agent && agent->mode() == AgentMode::Primary && !agent->is_hidden()) {
            return name;
        }
    }
    
    return "";  // No suitable default found
}

void AgentRegistry::clear() {
    std::unique_lock lock(mutex_);
    agents_.clear();
}

size_t AgentRegistry::size() const {
    std::shared_lock lock(mutex_);
    return agents_.size();
}

} // namespace turbot::core::agent
