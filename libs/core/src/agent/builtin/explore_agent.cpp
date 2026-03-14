#include <turbot/core/agent/builtin/explore_agent.hpp>
#include <turbot/core/provider/provider_manager.hpp>
#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_loop.hpp>
#include <turbot/core/common/logger.hpp>
#include <fmt/format.h>
#include <memory>

namespace turbot::core::agent {

ExploreAgent::ExploreAgent() {
    info_.name = "explore";
    info_.description = "Agent for exploring and analyzing codebases";
    info_.mode = AgentMode::Subagent;
    info_.native = true;
    info_.hidden = false;
    // Explore agent has read-only permissions by default
    using permission::PermissionAction;
    using permission::PermissionRule;
    info_.permission = {
        PermissionRule{"read_file", "*", PermissionAction::Allow},
        PermissionRule{"bash", "ls *", PermissionAction::Allow},
        PermissionRule{"bash", "find *", PermissionAction::Allow},
        PermissionRule{"bash", "grep *", PermissionAction::Allow},
        PermissionRule{"bash", "git *", PermissionAction::Allow},
        PermissionRule{"*", "*", PermissionAction::Deny}  // Deny everything else
    };
    // System prompt: read-only codebase analyst
    info_.prompt = "You are an expert code analyst (read-only mode). "
        "Use read_file, glob, grep, list_dir, and read-only bash commands to explore "
        "and understand codebases. Do NOT write, edit, or delete any files. "
        "Provide clear, structured analysis of what you find.";
}

std::string ExploreAgent::description() const {
    return info_.description.value_or("");
}

ExecuteResult ExploreAgent::execute(const ExecuteParams& params) {
    auto& pm = provider::ProviderManager::instance();
    auto prov_opt = pm.get_default_provider();
    if (!prov_opt) {
        TURBOT_LOG_WARN("ExploreAgent::execute: no provider configured, returning placeholder");
        return ExecuteResult::ok(
            fmt::format("Explore agent analyzed codebase for session {}: {}",
                        params.session_id, params.prompt),
            nlohmann::json{{"agent", "explore"}, {"session_id", params.session_id},
                           {"mode", "exploration"}}
        );
    }

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
        TURBOT_LOG_WARN("ExploreAgent::execute: no model available from provider, returning error");
        return ExecuteResult::error("ExploreAgent: no model available from provider");
    }

    session::SessionLoop sub_loop{params.session_id};
    sub_loop.set_provider(prov_opt->get());
    sub_loop.set_model(model_id);
    sub_loop.set_agent(shared_from_this());

    auto result = sub_loop.run(params.prompt);
    if (result == session::LoopResult::Error) {
        return ExecuteResult::error("ExploreAgent: LLM execution failed");
    }

    std::string output;
    for (const auto& msg : sub_loop.messages()) {
        if (msg.role() == turbot::core::Role::Assistant) {
            output = msg.get_text();
        }
    }
    return ExecuteResult::ok(output,
        nlohmann::json{{"agent", "explore"}, {"session_id", params.session_id},
                       {"mode", "exploration"}, {"model", model_id}});
}

} // namespace turbot::core::agent
