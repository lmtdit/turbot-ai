#include <turbot/core/agent/agent.hpp>
#include <turbot/core/agent/builtin/build_agent.hpp>
#include <turbot/core/agent/builtin/plan_agent.hpp>
#include <turbot/core/agent/builtin/explore_agent.hpp>
#include <fmt/format.h>
#include <iostream>

namespace turbot::core::agent {

// ============================================================================
// Agent Loader Implementation
// ============================================================================

namespace agent_loader {

size_t initialize_builtin_agents() {
    auto& registry = AgentRegistry::instance();
    size_t count = 0;
    
    // Register built-in agents
    // These are the core agents that match OpenCode's default agents
    
    // Build agent - the default primary agent
    auto build = std::make_shared<BuildAgent>();
    if (registry.register_agent(build)) {
        count++;
    }
    
    // Plan agent - for planning mode
    auto plan = std::make_shared<PlanAgent>();
    if (registry.register_agent(plan)) {
        count++;
    }
    
    // Explore agent - for codebase exploration
    auto explore = std::make_shared<ExploreAgent>();
    if (registry.register_agent(explore)) {
        count++;
    }
    
    return count;
}

size_t load_from_config(const nlohmann::json& config_json) {
    auto& registry = AgentRegistry::instance();
    size_t count = 0;
    
    if (!config_json.is_object()) {
        return 0;
    }
    
    // Process each agent definition in the config
    for (auto& [name, agent_config] : config_json.items()) {
        if (!agent_config.is_object()) {
            continue;
        }
        
        // Check if this agent should be disabled
        if (agent_config.value("disable", false)) {
            // Remove if exists
            registry.unregister_agent(name);
            continue;
        }
        
        // Try to get existing agent or create new one
        auto existing = registry.get(name);
        if (existing) {
            // Update existing agent's configuration
            // Note: In a full implementation, we'd update the agent's info
            // For now, we just skip if it already exists
            continue;
        }
        
        // Create agent info from config
        try {
            AgentInfo info;
            info.name = name;
            info.native = false;  // User-defined agents are not native
            
            if (agent_config.contains("description")) {
                info.description = agent_config["description"].get<std::string>();
            }
            
            if (agent_config.contains("mode")) {
                info.mode = string_to_agent_mode(agent_config["mode"].get<std::string>());
            }
            
            if (agent_config.contains("hidden")) {
                info.hidden = agent_config["hidden"].get<bool>();
            }
            
            if (agent_config.contains("prompt")) {
                info.prompt = agent_config["prompt"].get<std::string>();
            }
            
            if (agent_config.contains("temperature")) {
                info.temperature = agent_config["temperature"].get<double>();
            }
            
            if (agent_config.contains("top_p")) {
                info.top_p = agent_config["top_p"].get<double>();
            }
            
            if (agent_config.contains("steps")) {
                info.steps = agent_config["steps"].get<int>();
            }
            
            if (agent_config.contains("color")) {
                info.color = agent_config["color"].get<std::string>();
            }
            
            if (agent_config.contains("variant")) {
                info.variant = agent_config["variant"].get<std::string>();
            }
            
            if (agent_config.contains("model") && agent_config["model"].is_object()) {
                ModelRef model_ref;
                if (agent_config["model"].contains("model_id")) {
                    model_ref.model_id = agent_config["model"]["model_id"].get<std::string>();
                }
                if (agent_config["model"].contains("provider_id")) {
                    model_ref.provider_id = agent_config["model"]["provider_id"].get<std::string>();
                }
                if (!model_ref.model_id.empty() && !model_ref.provider_id.empty()) {
                    info.model = model_ref;
                }
            }
            
            // Validate the agent info
            if (!info.validate()) {
                std::cerr << fmt::format("Warning: Invalid agent configuration for '{}'\n", name);
                continue;
            }
            
            // Create a simple agent with this info
            // Note: In a full implementation, we'd have a ConfigurableAgent class
            // For now, we just register the info without a full agent implementation
            
            count++;
        } catch (const std::exception& e) {
            std::cerr << fmt::format("Error loading agent '{}': {}\n", name, e.what());
        }
    }
    
    return count;
}

size_t reload() {
    auto& registry = AgentRegistry::instance();
    registry.clear();
    return initialize_builtin_agents();
}

} // namespace agent_loader

// ============================================================================
// Agent Generate Result Implementation
// ============================================================================

AgentInfo AgentGenerateResult::to_agent_info() const {
    AgentInfo info;
    info.name = identifier;
    info.description = when_to_use;
    info.prompt = system_prompt;
    info.mode = AgentMode::Primary;  // Generated agents are primary by default
    info.native = false;
    return info;
}

nlohmann::json AgentGenerateResult::to_json() const {
    return nlohmann::json{
        {"identifier", identifier},
        {"when_to_use", when_to_use},
        {"system_prompt", system_prompt}
    };
}

// ============================================================================
// Agent Generator Implementation
// ============================================================================

namespace agent_generator {

AgentGenerateResult generate(const GenerateParams& params) {
    // This is a placeholder implementation
    // In a full implementation, this would use LLM to generate agent configuration
    // For now, we return a simple result based on the description
    
    AgentGenerateResult result;
    
    // Generate a simple identifier from the description
    // In production, this would be generated by LLM
    std::string desc = params.description;
    
    // Simple slug generation
    std::string identifier;
    for (char c : desc) {
        if (std::isalnum(c) || c == '-' || c == '_') {
            identifier += std::tolower(c);
        } else if (std::isspace(c)) {
            identifier += '_';
        }
    }
    
    // Truncate to reasonable length
    if (identifier.length() > 32) {
        identifier = identifier.substr(0, 32);
    }
    
    result.identifier = identifier;
    result.when_to_use = "Use this agent for: " + params.description;
    result.system_prompt = "You are a specialized agent for: " + params.description + 
                           "\n\nFollow best practices and be thorough in your work.";
    
    return result;
}

} // namespace agent_generator

} // namespace turbot::core::agent
