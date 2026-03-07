#include <turbot/core/agent/agent.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <shared_mutex>
#include <unordered_map>

namespace turbot::core::agent {

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

// AgentInfo implementation
nlohmann::json AgentInfo::to_json() const {
    nlohmann::json j;
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
    
    if (model_id) {
        j["model_id"] = *model_id;
    }
    if (options.is_object() && !options.empty()) {
        j["options"] = options;
    }
    return j;
}

AgentInfo AgentInfo::from_json(const nlohmann::json& j) {
    AgentInfo info;
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
    
    if (j.contains("model_id") && !j["model_id"].is_null()) {
        info.model_id = j["model_id"].get<std::string>();
    }
    if (j.contains("options") && j["options"].is_object() && !j["options"].empty()) {
        info.options = j["options"];
    }
    
    return info;
}

bool AgentInfo::operator==(const AgentInfo& other) const noexcept {
    return name == other.name &&
           description == other.description &&
           mode == other.mode &&
           native == other.native &&
           hidden == other.hidden &&
           permission == other.permission &&
           model_id == other.model_id;
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

std::vector<std::string> AgentRegistry::names() const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> result;
    result.reserve(agents_.size());
    for (const auto& [name, agent] : agents_) {
        result.push_back(name);
    }
    return result;
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
