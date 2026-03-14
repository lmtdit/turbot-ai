#include <turbot/core/agent/builtin/build_agent.hpp>
#include <turbot/core/provider/provider_manager.hpp>
#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_loop.hpp>
#include <turbot/core/common/logger.hpp>
#include <fmt/format.h>
#include <memory>

namespace turbot::core::agent {

BuildAgent::BuildAgent() {
    info_.name = "build";
    info_.description = "Default agent for executing tools and building code";
    info_.mode = AgentMode::Primary;
    info_.native = true;
    info_.hidden = false;
    info_.permission = build_default_permission();
    // System prompt: code-building expert with full tool access
    info_.prompt = "You are an expert software engineer. "
        "Use the available tools to complete coding tasks, run builds, fix tests, and "
        "write or modify files as needed. "
        "Be concise and direct — prefer minimal diffs over full rewrites.";
}

std::string BuildAgent::description() const {
    return info_.description.value_or("");
}

ExecuteResult BuildAgent::execute(const ExecuteParams& params) {
    auto& pm = provider::ProviderManager::instance();
    auto prov_opt = pm.get_default_provider();
    if (!prov_opt) {
        TURBOT_LOG_WARN("BuildAgent::execute: no provider configured, returning placeholder");
        return ExecuteResult::ok(
            fmt::format("Build agent processed prompt for session {}: {}",
                        params.session_id, params.prompt),
            nlohmann::json{{"agent", "build"}, {"session_id", params.session_id}}
        );
    }

    // Choose model: respect model_override param > agent preferred > provider first model
    std::string model_id;
    if (params.model_override) {
        model_id = *params.model_override;
    } else if (info_.model) {
        model_id = info_.model->model_id;
    } else {
        auto models = (*prov_opt)->list_models();
        model_id = models.empty() ? "" : models.front().id;
    }

    if (model_id.empty()) {
        TURBOT_LOG_WARN("BuildAgent::execute: no model available from provider, returning error");
        return ExecuteResult::error("BuildAgent: no model available from provider");
    }

    // Create a sub-SessionLoop using the caller's session_id
    session::SessionLoop sub_loop{params.session_id};
    sub_loop.set_provider(prov_opt->get());
    sub_loop.set_model(model_id);
    sub_loop.set_agent(shared_from_this());

    auto result = sub_loop.run(params.prompt);
    if (result == session::LoopResult::Error) {
        return ExecuteResult::error("BuildAgent: LLM execution failed");
    }

    // Collect last assistant message as output
    std::string output;
    for (const auto& msg : sub_loop.messages()) {
        if (msg.role() == turbot::core::Role::Assistant) {
            output = msg.get_text();
        }
    }
    return ExecuteResult::ok(output,
        nlohmann::json{{"agent", "build"}, {"session_id", params.session_id},
                       {"model", model_id}});
}

permission::Ruleset BuildAgent::build_default_permission() const {
    using namespace turbot::core::permission;
    
    Ruleset rules;
    // Default: allow everything
    rules.push_back(PermissionRule{"*", "*", PermissionAction::Allow});
    // But ask for dangerous operations
    rules.push_back(PermissionRule{"bash", "rm -rf *", PermissionAction::Ask});
    rules.push_back(PermissionRule{"bash", "sudo *", PermissionAction::Ask});
    rules.push_back(PermissionRule{"write_file", "*.env", PermissionAction::Ask});
    
    return rules;
}

} // namespace turbot::core::agent
