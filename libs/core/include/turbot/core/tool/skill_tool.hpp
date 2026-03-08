#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/tool/tool.hpp>
#include <turbot/core/skill/skill.hpp>
#include <string>

namespace turbot::core::tool {

/// Skill tool - allows agents to load specialized skills
///
/// This tool provides dynamic skill loading capability to agents.
/// When invoked, it returns the skill's instructions for the agent to follow.
class TURBOT_CORE_API SkillTool : public Tool {
public:
    SkillTool() = default;
    ~SkillTool() override = default;

    // Non-copyable
    SkillTool(const SkillTool&) = delete;
    SkillTool& operator=(const SkillTool&) = delete;
    SkillTool(SkillTool&&) = default;
    SkillTool& operator=(SkillTool&&) = default;

    /// Get the tool name
    [[nodiscard]] std::string name() const override { return "skill"; }

    /// Get the tool description (dynamic - lists available skills)
    [[nodiscard]] std::string description() const override;

    /// Get the JSON Schema for input parameters
    [[nodiscard]] nlohmann::json input_schema() const override;

    /// Execute the tool - load and return skill instructions
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;

private:
    /// Generate description with available skills
    [[nodiscard]] std::string generate_description() const;

    /// Generate output for a loaded skill
    [[nodiscard]] std::string generate_output(const Skill& skill) const;

    /// Check if skill is accessible given the ruleset
    [[nodiscard]] bool is_skill_accessible(
        const std::string& skill_name,
        const permission::Ruleset& ruleset
    ) const;

    /// Get accessible skills for a ruleset
    [[nodiscard]] std::vector<Skill> get_accessible_skills(
        const permission::Ruleset& ruleset
    ) const;
};

} // namespace turbot::core::tool
