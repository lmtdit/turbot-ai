#include <turbot/core/tool/skill_tool.hpp>
#include <turbot/core/skill/skill.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace turbot::core;
using namespace turbot::core::tool;

// Helper to create a test skill
static void create_test_skill(const std::string& name, const std::string& description, const std::string& instructions) {
    Skill skill = Skill::create(name, description, instructions);
    skill.source = SkillSource::Project;
    SkillRegistry::instance().register_skill(skill);
}

// Helper to clear all skills
static void clear_skills() {
    SkillRegistry::instance().clear();
}

// ===== SkillTool Definition Tests =====

TEST_CASE("SkillTool definition", "[skill_tool][definition]") {
    SkillTool tool;

    SECTION("name is 'skill'") {
        REQUIRE(tool.name() == "skill");
    }

    SECTION("description is not empty") {
        REQUIRE_FALSE(tool.description().empty());
    }

    SECTION("input_schema has name property") {
        auto schema = tool.input_schema();
        REQUIRE(schema["type"] == "object");
        REQUIRE(schema["properties"].contains("name"));
        REQUIRE(schema["required"].size() == 1);
    }
}

// ===== SkillTool Description Tests =====

TEST_CASE("SkillTool description with no skills", "[skill_tool][description]") {
    clear_skills();
    SkillTool tool;

    auto desc = tool.description();
    REQUIRE(desc.find("No skills") != std::string::npos);
}

TEST_CASE("SkillTool description with skills", "[skill_tool][description]") {
    clear_skills();
    create_test_skill("test-skill", "A test skill", "# Test Instructions");
    create_test_skill("another-skill", "Another skill", "# More Instructions");

    SkillTool tool;
    auto desc = tool.description();

    REQUIRE(desc.find("test-skill") != std::string::npos);
    REQUIRE(desc.find("A test skill") != std::string::npos);
    REQUIRE(desc.find("another-skill") != std::string::npos);

    clear_skills();
}

// ===== SkillTool Execute Tests =====

TEST_CASE("SkillTool execute with valid skill", "[skill_tool][execute]") {
    clear_skills();
    create_test_skill("valid-skill", "Valid skill description", "# Valid Instructions\n\nFollow these steps.");

    SkillTool tool;
    ToolContext ctx;
    ctx.session_id = "test-session";

    nlohmann::json input = {{"name", "valid-skill"}};
    auto result = tool.execute(input, ctx);

    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.title.find("valid-skill") != std::string::npos);
    REQUIRE(result.output.find("Valid Instructions") != std::string::npos);
    REQUIRE(result.output.find("<skill_content") != std::string::npos);
    REQUIRE(result.output.find("</skill_content>") != std::string::npos);
    REQUIRE(result.metadata["skill_name"] == "valid-skill");

    clear_skills();
}

TEST_CASE("SkillTool execute with non-existent skill", "[skill_tool][execute]") {
    clear_skills();
    create_test_skill("existing-skill", "Existing", "# Instructions");

    SkillTool tool;
    ToolContext ctx;
    ctx.session_id = "test-session";

    nlohmann::json input = {{"name", "non-existent-skill"}};
    auto result = tool.execute(input, ctx);

    REQUIRE(result.is_error);
    REQUIRE(result.title.find("Not Found") != std::string::npos);
    REQUIRE(result.output.find("non-existent-skill") != std::string::npos);
    REQUIRE(result.output.find("existing-skill") != std::string::npos);  // Lists available

    clear_skills();
}

TEST_CASE("SkillTool execute with missing name parameter", "[skill_tool][execute]") {
    clear_skills();
    SkillTool tool;
    ToolContext ctx;

    nlohmann::json input = {{"other", "value"}};
    auto result = tool.execute(input, ctx);

    REQUIRE(result.is_error);
    REQUIRE(result.title.find("Invalid") != std::string::npos);
}

TEST_CASE("SkillTool execute with invalid name type", "[skill_tool][execute]") {
    clear_skills();
    SkillTool tool;
    ToolContext ctx;

    nlohmann::json input = {{"name", 123}};  // Should be string
    auto result = tool.execute(input, ctx);

    REQUIRE(result.is_error);
}

// ===== SkillTool Output Format Tests =====

TEST_CASE("SkillTool output format", "[skill_tool][output]") {
    clear_skills();

    Skill skill = Skill::create("format-test", "Format test", "# Instructions\n\nContent here");
    skill.version = "1.0.0";
    skill.dependencies = {"dep1", "dep2"};
    SkillRegistry::instance().register_skill(skill);

    SkillTool tool;
    ToolContext ctx;

    nlohmann::json input = {{"name", "format-test"}};
    auto result = tool.execute(input, ctx);

    REQUIRE_FALSE(result.is_error);

    // Check XML format
    REQUIRE(result.output.find("<skill_content name=\"format-test\">") != std::string::npos);
    REQUIRE(result.output.find("<description>Format test</description>") != std::string::npos);
    REQUIRE(result.output.find("<instructions>") != std::string::npos);
    REQUIRE(result.output.find("<version>1.0.0</version>") != std::string::npos);
    REQUIRE(result.output.find("<dependency>dep1</dependency>") != std::string::npos);
    REQUIRE(result.output.find("<dependency>dep2</dependency>") != std::string::npos);

    clear_skills();
}

TEST_CASE("SkillTool metadata", "[skill_tool][metadata]") {
    clear_skills();
    create_test_skill("meta-test", "Meta test", "# Instructions");

    SkillTool tool;
    ToolContext ctx;

    nlohmann::json input = {{"name", "meta-test"}};
    auto result = tool.execute(input, ctx);

    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.metadata["skill_name"] == "meta-test");
    REQUIRE(result.metadata.contains("skill_path"));
    REQUIRE(result.metadata.contains("skill_source"));

    clear_skills();
}

// ===== SkillTool Permission Tests =====

TEST_CASE("SkillTool permission denied", "[skill_tool][permission]") {
    clear_skills();
    create_test_skill("denied-skill", "Denied skill", "# Instructions");

    SkillTool tool;
    ToolContext ctx;

    // Add a deny rule for this skill
    permission::PermissionRule rule;
    rule.permission = "skill";
    rule.pattern = "denied-skill";
    rule.action = permission::PermissionAction::Deny;
    ctx.ruleset.push_back(rule);

    nlohmann::json input = {{"name", "denied-skill"}};
    auto result = tool.execute(input, ctx);

    REQUIRE(result.is_error);
    REQUIRE(result.title.find("Permission Denied") != std::string::npos);

    clear_skills();
}

TEST_CASE("SkillTool permission allowed", "[skill_tool][permission]") {
    clear_skills();
    create_test_skill("allowed-skill", "Allowed skill", "# Instructions");

    SkillTool tool;
    ToolContext ctx;

    // Add an allow rule for this skill
    permission::PermissionRule rule;
    rule.permission = "skill";
    rule.pattern = "allowed-skill";
    rule.action = permission::PermissionAction::Allow;
    ctx.ruleset.push_back(rule);

    nlohmann::json input = {{"name", "allowed-skill"}};
    auto result = tool.execute(input, ctx);

    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.output.find("Allowed skill") != std::string::npos);

    clear_skills();
}

TEST_CASE("SkillTool wildcard permission", "[skill_tool][permission]") {
    clear_skills();
    create_test_skill("test-skill-1", "Test 1", "# Instructions");
    create_test_skill("test-skill-2", "Test 2", "# Instructions");
    create_test_skill("other-skill", "Other", "# Instructions");

    SkillTool tool;
    ToolContext ctx;

    // Deny all test-* skills
    permission::PermissionRule rule;
    rule.permission = "skill";
    rule.pattern = "test-*";
    rule.action = permission::PermissionAction::Deny;
    ctx.ruleset.push_back(rule);

    // test-skill-1 should be denied
    nlohmann::json input1 = {{"name", "test-skill-1"}};
    auto result1 = tool.execute(input1, ctx);
    REQUIRE(result1.is_error);

    // test-skill-2 should be denied
    nlohmann::json input2 = {{"name", "test-skill-2"}};
    auto result2 = tool.execute(input2, ctx);
    REQUIRE(result2.is_error);

    // other-skill should be allowed (no matching deny rule)
    nlohmann::json input3 = {{"name", "other-skill"}};
    auto result3 = tool.execute(input3, ctx);
    REQUIRE_FALSE(result3.is_error);

    clear_skills();
}

// ===== Integration Tests =====

TEST_CASE("SkillTool full workflow", "[skill_tool][integration]") {
    clear_skills();

    // Create multiple skills
    create_test_skill("workflow-skill", "Workflow description", "# Workflow Instructions\n\nStep 1\nStep 2");

    SkillTool tool;

    // Check description includes the skill
    auto desc = tool.description();
    REQUIRE(desc.find("workflow-skill") != std::string::npos);

    // Execute the skill
    ToolContext ctx;
    nlohmann::json input = {{"name", "workflow-skill"}};
    auto result = tool.execute(input, ctx);

    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.output.find("Workflow Instructions") != std::string::npos);
    REQUIRE(result.output.find("Step 1") != std::string::npos);

    clear_skills();
}
