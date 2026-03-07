#include <turbot/core/agent/builtin/build_agent.hpp>
#include <fmt/format.h>

namespace turbot::core::agent {

BuildAgent::BuildAgent() {
    info_.name = "build";
    info_.description = "Default agent for executing tools and building code";
    info_.mode = AgentMode::Primary;
    info_.native = true;
    info_.hidden = false;
    info_.permission = build_default_permission();
}

std::string BuildAgent::description() const {
    return info_.description.value_or("");
}

ExecuteResult BuildAgent::execute(const ExecuteParams& params) {
    // Build agent is a placeholder for now
    // In a real implementation, this would interact with the LLM and execute tools
    return ExecuteResult::ok(
        fmt::format("Build agent processed prompt for session {}: {}", 
                    params.session_id, params.prompt),
        nlohmann::json{
            {"agent", "build"},
            {"session_id", params.session_id}
        }
    );
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
