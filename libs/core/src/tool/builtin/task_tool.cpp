#include <turbot/core/tool/builtin/task_tool.hpp>
#include <fmt/format.h>

namespace turbot::core::tool {

// TaskToolParams implementation
TaskToolParams TaskToolParams::from_json(const nlohmann::json& j) {
    TaskToolParams params;
    params.prompt = j.at("prompt").get<std::string>();
    params.description = j.at("description").get<std::string>();
    params.subagent_type = j.at("subagent_type").get<std::string>();
    
    if (j.contains("task_id") && !j["task_id"].is_null()) {
        params.task_id = j["task_id"].get<std::string>();
    }
    
    return params;
}

nlohmann::json TaskToolParams::to_json() const {
    nlohmann::json j;
    j["prompt"] = prompt;
    j["description"] = description;
    j["subagent_type"] = subagent_type;
    if (task_id) {
        j["task_id"] = *task_id;
    }
    return j;
}

// TaskTool implementation
TaskTool::TaskTool() = default;

nlohmann::json TaskTool::input_schema() const {
    return nlohmann::json{
        {"type", "object"},
        {"properties", {
            {"prompt", {
                {"type", "string"},
                {"description", "The task prompt or instruction for the subagent"}
            }},
            {"description", {
                {"type", "string"},
                {"description", "Human-readable description of the task"}
            }},
            {"subagent_type", {
                {"type", "string"},
                {"description", "Type of subagent to use (e.g., build, plan, explore)"},
                {"enum", nlohmann::json::array({"build", "plan", "explore"})}
            }},
            {"task_id", {
                {"type", "string"},
                {"description", "Existing task ID to resume (optional)"}
            }}
        }},
        {"required", nlohmann::json::array({"prompt", "description", "subagent_type"})}
    };
}

ToolResult TaskTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    // Parse parameters
    TaskToolParams params;
    try {
        params = TaskToolParams::from_json(input);
    } catch (const std::exception& e) {
        return ToolResult::error("task", fmt::format("Invalid parameters: {}", e.what()));
    }
    
    // Check for abort
    if (ctx.should_abort()) {
        return ToolResult::error("task", "Task execution aborted");
    }
    
    // Get the subagent
    auto agent = agent::AgentRegistry::instance().get(params.subagent_type);
    if (!agent) {
        return ToolResult::error("task", 
            fmt::format("Agent not found: {}", params.subagent_type));
    }
    
    // Check permission for task delegation
    permission::PermissionRequest perm_request;
    perm_request.permission = "task";
    perm_request.patterns = {params.subagent_type};
    perm_request.tool = "task";
    
    if (ctx.ask_permission) {
        auto reply = ctx.ask_permission(perm_request);
        if (reply.type == permission::PermissionReply::Type::Reject) {
            return ToolResult::error("task", 
                fmt::format("Task delegation to '{}' rejected by user", params.subagent_type));
        }
    }
    
    // Create or resume sub-session
    session::Session sub_session;
    
    if (params.task_id) {
        // Try to get existing session
        auto existing = session::Session::get(*params.task_id);
        if (existing) {
            sub_session = std::move(*existing);
        }
    }
    
    if (!sub_session.is_valid()) {
        // Create new sub-session
        session::CreateParams create_params;
        create_params.project_id = "subtask";
        create_params.slug = fmt::format("task_{}", params.subagent_type);
        create_params.directory = "/tmp/turbot_subtask";  // Placeholder
        create_params.title = fmt::format("{} (@{} subagent)", 
            params.description, params.subagent_type);
        
        auto created = session::Session::create(create_params);
        if (!created) {
            return ToolResult::error("task", "Failed to create sub-session");
        }
        sub_session = std::move(*created);
    }
    
    // Execute the task in sub-session
    auto result = execute_in_subsession(sub_session, agent, params, ctx);
    
    return result;
}

permission::Ruleset TaskTool::build_subagent_permission(
    std::shared_ptr<agent::Agent> agent,
    const permission::Ruleset& parent_ruleset
) const {
    using permission::PermissionAction;
    using permission::PermissionRule;
    
    // Start with the agent's default permission
    permission::Ruleset ruleset = agent->info().permission;
    
    // Merge with parent ruleset (parent rules override agent defaults)
    for (const auto& rule : parent_ruleset) {
        ruleset.push_back(rule);
    }
    
    // Add task-specific restrictions
    ruleset.push_back(PermissionRule{"task", "*", PermissionAction::Deny});
    
    return ruleset;
}

ToolResult TaskTool::execute_in_subsession(
    session::Session& sub_session,
    std::shared_ptr<agent::Agent> agent,
    const TaskToolParams& params,
    ToolContext& ctx
) {
    // Check for abort
    if (ctx.should_abort()) {
        return ToolResult::error("task", "Task execution aborted");
    }
    
    // Build execution context
    agent::ExecuteParams exec_params;
    exec_params.session_id = sub_session.id();
    exec_params.prompt = params.prompt;
    
    // Execute the agent
    auto result = agent->execute(exec_params);
    
    if (!result.is_success) {
        return ToolResult::error("task",
            fmt::format("Subagent '{}' failed: {}", 
                params.subagent_type, 
                result.error_message.value_or("Unknown error")));
    }
    
    // Build successful result
    nlohmann::json metadata;
    metadata["session_id"] = sub_session.id();
    metadata["subagent"] = params.subagent_type;
    metadata["description"] = params.description;
    
    std::string output = fmt::format(
        "Task ID: {}\n"
        "Subagent: {}\n"
        "Description: {}\n\n"
        "Result:\n{}",
        sub_session.id(),
        params.subagent_type,
        params.description,
        result.output
    );
    
    return ToolResult::success(params.description, output, metadata);
}

} // namespace turbot::core::tool
