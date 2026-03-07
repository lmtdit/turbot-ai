#pragma once

#include <turbot/core/tool/tool.hpp>
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/session/session.hpp>
#include <memory>
#include <optional>
#include <string>

namespace turbot::core::tool {

/// Parameters for the Task tool
struct TaskToolParams {
    std::string prompt;                        ///< The task prompt/instruction
    std::string description;                   ///< Human-readable task description
    std::string subagent_type;                 ///< Type of subagent to use
    std::optional<std::string> task_id;        ///< Existing task ID to resume

    /// Parse from JSON
    static TaskToolParams from_json(const nlohmann::json& j);
    
    /// Convert to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Task tool - delegates subtasks to subagents
/// This tool creates or resumes a sub-session and executes a task using a subagent
class TURBOT_CORE_API TaskTool : public Tool {
public:
    TaskTool();
    ~TaskTool() override = default;

    [[nodiscard]] std::string name() const override { return "task"; }
    
    [[nodiscard]] std::string description() const override {
        return "Execute a subtask using a specialized subagent. "
               "Creates a new sub-session or resumes an existing one.";
    }
    
    [[nodiscard]] nlohmann::json input_schema() const override;
    
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;

private:
    /// Build permission ruleset for the subagent
    [[nodiscard]] permission::Ruleset build_subagent_permission(
        std::shared_ptr<agent::Agent> agent,
        const permission::Ruleset& parent_ruleset
    ) const;

    /// Execute task in a sub-session
    [[nodiscard]] ToolResult execute_in_subsession(
        session::Session& sub_session,
        std::shared_ptr<agent::Agent> agent,
        const TaskToolParams& params,
        ToolContext& ctx
    );
};

} // namespace turbot::core::tool
