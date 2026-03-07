#include <turbot/core/agent/builtin/plan_agent.hpp>
#include <fmt/format.h>

namespace turbot::core::agent {

PlanAgent::PlanAgent() {
    info_.name = "plan";
    info_.description = "Agent for planning and analysis without tool execution";
    info_.mode = AgentMode::Primary;
    info_.native = true;
    info_.hidden = false;
    // Plan agent has no tool execution permissions by default
}

std::string PlanAgent::description() const {
    return info_.description.value_or("");
}

ExecuteResult PlanAgent::execute(const ExecuteParams& params) {
    // Plan agent is a placeholder for planning tasks
    // It analyzes requests and creates plans without executing tools
    return ExecuteResult::ok(
        fmt::format("Plan agent analyzed request for session {}: {}", 
                    params.session_id, params.prompt),
        nlohmann::json{
            {"agent", "plan"},
            {"session_id", params.session_id},
            {"mode", "analysis"}
        }
    );
}

} // namespace turbot::core::agent
