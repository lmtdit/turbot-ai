#include <turbot/core/agent/builtin/plan_agent.hpp>
#include <turbot/core/provider/provider_manager.hpp>
#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_loop.hpp>
#include <turbot/core/common/logger.hpp>
#include <fmt/format.h>
#include <memory>

namespace turbot::core::agent {

PlanAgent::PlanAgent() {
    info_.name = "plan";
    info_.description = "Agent for planning and analysis without tool execution";
    info_.mode = AgentMode::Primary;
    info_.native = true;
    info_.hidden = false;
    // Plan agent has no tool execution permissions by default
    // System prompt: task planner, no tool calls
    info_.prompt = "You are an expert task planner. "
        "Analyse the user request and produce a detailed, step-by-step execution plan. "
        "Do NOT call any tools — output only the plan as structured text. "
        "Each step must be actionable and include a clear success criterion.";
}

std::string PlanAgent::description() const {
    return info_.description.value_or("");
}

ExecuteResult PlanAgent::execute(const ExecuteParams& params) {
    auto& pm = provider::ProviderManager::instance();
    auto prov_opt = pm.get_default_provider();
    if (!prov_opt) {
        TURBOT_LOG_WARN("PlanAgent::execute: no provider configured, returning placeholder");
        return ExecuteResult::ok(
            fmt::format("Plan agent analyzed request for session {}: {}",
                        params.session_id, params.prompt),
            nlohmann::json{{"agent", "plan"}, {"session_id", params.session_id},
                           {"mode", "analysis"}}
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
        TURBOT_LOG_WARN("PlanAgent::execute: no model available from provider, returning error");
        return ExecuteResult::error("PlanAgent: no model available from provider");
    }

    session::SessionLoop sub_loop{params.session_id};
    sub_loop.set_provider(prov_opt->get());
    sub_loop.set_model(model_id);
    sub_loop.set_agent(shared_from_this());

    auto result = sub_loop.run(params.prompt);
    if (result == session::LoopResult::Error) {
        return ExecuteResult::error("PlanAgent: LLM execution failed");
    }

    std::string output;
    for (const auto& msg : sub_loop.messages()) {
        if (msg.role() == turbot::core::Role::Assistant) {
            output = msg.get_text();
        }
    }
    return ExecuteResult::ok(output,
        nlohmann::json{{"agent", "plan"}, {"session_id", params.session_id},
                       {"mode", "analysis"}, {"model", model_id}});
}

} // namespace turbot::core::agent
