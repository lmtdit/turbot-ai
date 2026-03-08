#include "turbot/core/tool/skill_tool.hpp"
#include <sstream>

namespace turbot::core::tool {

std::string SkillTool::generate_description() const {
    auto all_skills = SkillRegistry::instance().all();

    std::ostringstream oss;
    oss << "Load a specialized skill that provides domain-specific instructions "
        << "and workflows.\n\n";

    if (all_skills.empty()) {
        oss << "No skills are currently available.";
        return oss.str();
    }

    oss << "Available skills:\n";

    for (const auto& skill : all_skills) {
        oss << "  - " << skill.name << ": " << skill.description << "\n";
    }

    oss << "\nUse this tool to load a skill by name. The skill's instructions "
        << "will be returned for you to follow.";

    return oss.str();
}

std::string SkillTool::description() const {
    return generate_description();
}

nlohmann::json SkillTool::input_schema() const {
    return nlohmann::json{
        {"type", "object"},
        {"properties", nlohmann::json{
            {"name", nlohmann::json{
                {"type", "string"},
                {"description", "The name of the skill to load"}
            }}
        }},
        {"required", nlohmann::json::array({"name"})}
    };
}

bool SkillTool::is_skill_accessible(
    const std::string& skill_name,
    const permission::Ruleset& ruleset
) const {
    // Check if there's a specific rule for this skill
    // If no rule exists, default to allow
    // If a rule exists with deny action, block access

    // Use PermissionSystem to evaluate skill access
    // Permission type is "skill", pattern is the skill name
    auto action = permission::PermissionSystem::evaluate("skill", skill_name, ruleset);
    return action != permission::PermissionAction::Deny;
}

std::vector<Skill> SkillTool::get_accessible_skills(
    const permission::Ruleset& ruleset
) const {
    auto all_skills = SkillRegistry::instance().all();
    std::vector<Skill> accessible;

    for (const auto& skill : all_skills) {
        if (is_skill_accessible(skill.name, ruleset)) {
            accessible.push_back(skill);
        }
    }

    return accessible;
}

std::string SkillTool::generate_output(const Skill& skill) const {
    std::ostringstream oss;

    // Use XML-like format for clear skill content
    oss << "<skill_content name=\"" << skill.name << "\">\n";

    if (!skill.description.empty()) {
        oss << "<description>" << skill.description << "</description>\n";
    }

    if (!skill.instructions.empty()) {
        oss << "<instructions>\n" << skill.instructions << "\n</instructions>\n";
    }

    if (skill.version.has_value()) {
        oss << "<version>" << *skill.version << "</version>\n";
    }

    if (skill.dependencies.has_value() && !skill.dependencies->empty()) {
        oss << "<dependencies>\n";
        for (const auto& dep : *skill.dependencies) {
            oss << "  <dependency>" << dep << "</dependency>\n";
        }
        oss << "</dependencies>\n";
    }

    oss << "</skill_content>";

    return oss.str();
}

ToolResult SkillTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    // Validate input
    if (!input.contains("name") || !input["name"].is_string()) {
        return ToolResult::error(
            "Invalid Input",
            "Missing or invalid 'name' parameter. Expected a string."
        );
    }

    std::string skill_name = input["name"].get<std::string>();

    // Check if skill exists
    auto skill_opt = SkillRegistry::instance().get(skill_name);
    if (!skill_opt.has_value()) {
        // List available skills in error message
        auto all_skills = SkillRegistry::instance().all();
        std::ostringstream err_msg;
        err_msg << "Skill not found: " << skill_name << "\n";

        if (!all_skills.empty()) {
            err_msg << "Available skills:\n";
            for (const auto& s : all_skills) {
                err_msg << "  - " << s.name << "\n";
            }
        } else {
            err_msg << "No skills are currently available.";
        }

        return ToolResult::error("Skill Not Found", err_msg.str());
    }

    const auto& skill = *skill_opt;

    // Check permission
    if (!is_skill_accessible(skill_name, ctx.ruleset)) {
        return ToolResult::error(
            "Permission Denied",
            "Access to skill '" + skill_name + "' is denied by permission rules."
        );
    }

    // Generate and return skill content
    std::string output = generate_output(skill);

    // Include metadata about the skill
    nlohmann::json metadata = {
        {"skill_name", skill.name},
        {"skill_path", skill.path},
        {"skill_source", std::string(skill_source_to_string(skill.source))}
    };

    return ToolResult::success(
        "Skill Loaded: " + skill.name,
        output,
        metadata
    );
}

} // namespace turbot::core::tool
