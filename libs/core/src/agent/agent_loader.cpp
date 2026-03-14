#include <turbot/core/agent/agent.hpp>
#include <turbot/core/agent/builtin/build_agent.hpp>
#include <turbot/core/agent/builtin/plan_agent.hpp>
#include <turbot/core/agent/builtin/explore_agent.hpp>
#include <turbot/core/agent/builtin/configurable_agent.hpp>
#include <turbot/core/permission/permission.hpp>
#include <turbot/core/provider/provider_manager.hpp>
#include <turbot/core/provider/provider.hpp>
#include <turbot/core/common/logger.hpp>
#include <fmt/format.h>
#include <iostream>
#include <stdexcept>

namespace turbot::core::agent {

// ============================================================================
// Agent Loader Implementation
// ============================================================================

namespace agent_loader {

size_t initialize_builtin_agents() {
    auto& registry = AgentRegistry::instance();
    size_t count = 0;

    // -----------------------------------------------------------------------
    // 1. Build — default primary agent with full permissions
    // -----------------------------------------------------------------------
    {
        auto agent = std::make_shared<BuildAgent>();
        if (registry.register_agent(agent)) count++;
    }

    // -----------------------------------------------------------------------
    // 2. Plan — primary agent; edit access limited to plan files
    // -----------------------------------------------------------------------
    {
        auto agent = std::make_shared<PlanAgent>();
        if (registry.register_agent(agent)) count++;
    }

    // -----------------------------------------------------------------------
    // 3. Explore — subagent; read-only (grep/glob/list/read/bash)
    // -----------------------------------------------------------------------
    {
        auto agent = std::make_shared<ExploreAgent>();
        if (registry.register_agent(agent)) count++;
    }

    // -----------------------------------------------------------------------
    // 4. General — subagent; no todo tools
    // -----------------------------------------------------------------------
    {
        using namespace turbot::core::permission;
        AgentInfo info;
        info.name        = "general";
        info.description = "General-purpose subagent (no todo tools)";
        info.mode        = AgentMode::Subagent;
        info.native      = true;
        info.hidden      = false;
        // Allow everything except todoread / todowrite
        info.permission.push_back(PermissionRule{"todoread",  "*", PermissionAction::Deny});
        info.permission.push_back(PermissionRule{"todowrite", "*", PermissionAction::Deny});
        info.permission.push_back(PermissionRule{"*",         "*", PermissionAction::Allow});
        if (registry.register_agent(std::make_shared<ConfigurableAgent>(std::move(info)))) count++;
    }

    // -----------------------------------------------------------------------
    // 5. Compaction — hidden primary; no tools (summarises conversation)
    // -----------------------------------------------------------------------
    {
        using namespace turbot::core::permission;
        AgentInfo info;
        info.name        = "compaction";
        info.description = "Internal agent used during context-window compaction";
        info.mode        = AgentMode::Primary;
        info.native      = true;
        info.hidden      = true;
        // Deny all tool calls — compaction agent must only produce text
        info.permission.push_back(PermissionRule{"*", "*", PermissionAction::Deny});
        if (registry.register_agent(std::make_shared<ConfigurableAgent>(std::move(info)))) count++;
    }

    // -----------------------------------------------------------------------
    // 6. Title — hidden primary; no tools; low temperature
    //    Generates a short session title after the session finishes.
    // -----------------------------------------------------------------------
    {
        using namespace turbot::core::permission;
        AgentInfo info;
        info.name        = "title";
        info.description = "Internal agent that generates a concise session title";
        info.mode        = AgentMode::Primary;
        info.native      = true;
        info.hidden      = true;
        info.temperature = 0.5;
        info.steps       = 1;
        info.prompt      =
            "Generate an extremely concise (≤ 8 words) title for the session "
            "based on the user's first message.  Return ONLY the title — no "
            "punctuation, no quotes, no explanation.";
        // Deny all tool calls
        info.permission.push_back(PermissionRule{"*", "*", PermissionAction::Deny});
        if (registry.register_agent(std::make_shared<ConfigurableAgent>(std::move(info)))) count++;
    }

    // -----------------------------------------------------------------------
    // 7. Summary — hidden primary; no tools
    //    Computes a brief summary of what changed in a step.
    // -----------------------------------------------------------------------
    {
        using namespace turbot::core::permission;
        AgentInfo info;
        info.name        = "summary";
        info.description = "Internal agent that generates a step-level change summary";
        info.mode        = AgentMode::Primary;
        info.native      = true;
        info.hidden      = true;
        info.steps       = 1;
        info.prompt      =
            "Summarise the file changes made in this step in one short sentence.  "
            "Return ONLY the sentence.";
        info.permission.push_back(PermissionRule{"*", "*", PermissionAction::Deny});
        if (registry.register_agent(std::make_shared<ConfigurableAgent>(std::move(info)))) count++;
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
            
            // Create and register a ConfigurableAgent from the parsed info.
            auto ca = std::make_shared<ConfigurableAgent>(std::move(info));
            if (registry.register_agent(std::move(ca))) count++;
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

/// Build a simple slug from an arbitrary string (ASCII lowercase + underscore).
static std::string make_slug(const std::string& s, std::size_t max_len = 32) {
    std::string out;
    out.reserve(std::min(s.size(), max_len));
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_') {
            out += static_cast<char>(std::tolower(c));
        } else if (std::isspace(c) && !out.empty() && out.back() != '_') {
            out += '_';
        }
        if (out.size() >= max_len) break;
    }
    // Strip trailing underscore
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out;
}

/// Try to extract a JSON string field from a possibly-markdown-fenced LLM response.
static std::optional<nlohmann::json> extract_json(const std::string& raw) {
    // Strip markdown code fences if present
    auto start = raw.find('{');
    auto end   = raw.rfind('}');
    if (start == std::string::npos || end == std::string::npos || end < start) {
        return std::nullopt;
    }
    try {
        return nlohmann::json::parse(raw.substr(start, end - start + 1));
    } catch (...) {
        return std::nullopt;
    }
}

AgentGenerateResult generate(const GenerateParams& params) {
    using namespace turbot::core::provider;

    // ----------------------------------------------------------------
    // Attempt LLM-assisted generation
    // ----------------------------------------------------------------
    auto& pm = ProviderManager::instance();
    auto prov_opt = pm.get_default_provider();

    if (prov_opt) {
        // Determine model id
        std::string model_id;
        if (params.model) {
            model_id = params.model->model_id;
        } else {
            auto models = (*prov_opt)->list_models();
            model_id = models.empty() ? "" : models.front().id;
        }

        if (model_id.empty()) {
            TURBOT_LOG_WARN("agent_generator::generate: no model available from provider, "
                            "falling back to placeholder");
        } else {
            const std::string system_prompt =
                "You are an expert AI-agent designer. "
                "Given a description of a task, return ONLY a JSON object (no markdown) "
                "with exactly three string fields:\n"
                "  \"identifier\" : a short snake_case name (max 32 chars)\n"
                "  \"when_to_use\": one concise sentence describing when to use this agent\n"
                "  \"system_prompt\": a detailed system prompt that makes the agent expert at the task\n"
                "Do NOT include any explanation outside the JSON object.";

            const std::string user_msg =
                "Create an agent configuration for the following task:\n" + params.description;

            std::vector<ChatMessage> messages = {
                ChatMessage::system(system_prompt),
                ChatMessage::user(user_msg)
            };

            try {
                auto response = (*prov_opt)->chat(messages, model_id);
                if (!response.is_error()) {
                    auto json_opt = extract_json(response.get_text());
                    if (json_opt) {
                        const auto& j = *json_opt;
                        AgentGenerateResult result;
                        result.identifier    = j.value("identifier",    make_slug(params.description));
                        result.when_to_use   = j.value("when_to_use",   "Use this agent for: " + params.description);
                        result.system_prompt = j.value("system_prompt",
                            "You are a specialized agent for: " + params.description +
                            "\n\nFollow best practices and be thorough in your work.");
                        // Ensure identifier is a valid slug
                        if (result.identifier.empty()) {
                            result.identifier = make_slug(params.description);
                        }
                        TURBOT_LOG_DEBUG("agent_generator::generate: LLM produced identifier='{}'",
                                         result.identifier);
                        return result;
                    }
                    TURBOT_LOG_WARN("agent_generator::generate: LLM response was not valid JSON, "
                                    "falling back to placeholder");
                } else {
                    TURBOT_LOG_WARN("agent_generator::generate: LLM returned error, "
                                    "falling back to placeholder");
                }
            } catch (const std::exception& ex) {
                TURBOT_LOG_WARN("agent_generator::generate: exception during LLM call: {}, "
                                "falling back to placeholder", ex.what());
            }
        } // end if (!model_id.empty())
    } else {
        TURBOT_LOG_DEBUG("agent_generator::generate: no provider configured, "
                         "using placeholder generation");
    }

    // ----------------------------------------------------------------
    // Fallback: deterministic placeholder (no LLM available)
    // ----------------------------------------------------------------
    AgentGenerateResult result;
    result.identifier    = make_slug(params.description);
    result.when_to_use   = "Use this agent for: " + params.description;
    result.system_prompt = "You are a specialized agent for: " + params.description +
                           "\n\nFollow best practices and be thorough in your work.";
    return result;
}

} // namespace agent_generator

} // namespace turbot::core::agent
