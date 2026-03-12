#include <turbot/core/agent/builtin/explore_agent.hpp>
#include <fmt/format.h>

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
}

std::string ExploreAgent::description() const {
    return info_.description.value_or("");
}

ExecuteResult ExploreAgent::execute(const ExecuteParams& params) {
    // Explore agent is a placeholder for codebase exploration
    // It focuses on reading and analyzing code without modifications
    return ExecuteResult::ok(
        fmt::format("Explore agent analyzed codebase for session {}: {}", 
                    params.session_id, params.prompt),
        nlohmann::json{
            {"agent", "explore"},
            {"session_id", params.session_id},
            {"mode", "exploration"}
        }
    );
}

} // namespace turbot::core::agent
