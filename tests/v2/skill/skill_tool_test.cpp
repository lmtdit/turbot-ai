/**
 * @file skill_tool_test.cpp
 * @brief SkillTool tests
 *
 * Tests for:
 * - Tool metadata (name, description, input_schema)
 * - execute() with various inputs
 * - Permission checking
 * - Output generation
 */

#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/tool/skill_tool.hpp>
#include <turbot/core/skill/skill.hpp>
#include <fstream>

using namespace turbot::core;
using namespace turbot::core::tool;
using namespace turbot::core::permission;
using namespace turbot::test;

// ==================== Test Fixtures ====================

class SkillToolFixture {
public:
    SkillToolFixture() {
        // Clear registry before each test
        SkillRegistry::instance().clear();
    }

    ~SkillToolFixture() {
        SkillRegistry::instance().clear();
    }

    void registerTestSkill(const std::string& name, const std::string& description, const std::string& instructions) {
        Skill skill = Skill::create(name, description, instructions);
        SkillRegistry::instance().register_skill(skill);
    }

    ToolContext makeContext(const Ruleset& ruleset = {}) {
        ToolContext ctx;
        ctx.ruleset = ruleset;
        return ctx;
    }
};

// ==================== Tool Metadata Tests ====================

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Name", "[Skill][Tool]") {
    SkillTool tool;
    REQUIRE(tool.name() == "skill");
}

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Description.NoSkills", "[Skill][Tool]") {
    SkillTool tool;
    std::string desc = tool.description();

    REQUIRE(desc.find("skill") != std::string::npos);
    REQUIRE(desc.find("No skills are currently available") != std::string::npos);
}

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Description.WithSkills", "[Skill][Tool]") {
    registerTestSkill("test-skill-a", "First test skill", "Instructions A");
    registerTestSkill("test-skill-b", "Second test skill", "Instructions B");

    SkillTool tool;
    std::string desc = tool.description();

    REQUIRE(desc.find("test-skill-a") != std::string::npos);
    REQUIRE(desc.find("test-skill-b") != std::string::npos);
    REQUIRE(desc.find("First test skill") != std::string::npos);
    REQUIRE(desc.find("Second test skill") != std::string::npos);
}

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.InputSchema", "[Skill][Tool]") {
    SkillTool tool;
    nlohmann::json schema = tool.input_schema();

    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"]["name"]["type"] == "string");
    REQUIRE(schema["required"].is_array());
    REQUIRE(schema["required"].size() == 1);
    REQUIRE(schema["required"][0] == "name");
}

// ==================== execute() Tests ====================

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Execute.MissingName", "[Skill][Tool]") {
    SkillTool tool;
    ToolContext ctx = makeContext();

    nlohmann::json input = {{"other", "field"}};
    ToolResult result = tool.execute(input, ctx);

    REQUIRE(result.is_error);
    REQUIRE(result.title == "Invalid Input");
    REQUIRE(result.output.find("Missing or invalid 'name'") != std::string::npos);
}

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Execute.NameNotString", "[Skill][Tool]") {
    SkillTool tool;
    ToolContext ctx = makeContext();

    nlohmann::json input = {{"name", 123}};
    ToolResult result = tool.execute(input, ctx);

    REQUIRE(result.is_error);
    REQUIRE(result.title == "Invalid Input");
}

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Execute.EmptyName", "[Skill][Tool]") {
    SkillTool tool;
    ToolContext ctx = makeContext();

    nlohmann::json input = {{"name", ""}};
    ToolResult result = tool.execute(input, ctx);

    REQUIRE(result.is_error);
    REQUIRE(result.title == "Skill Not Found");
}

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Execute.SkillNotFound", "[Skill][Tool]") {
    SkillTool tool;
    ToolContext ctx = makeContext();

    nlohmann::json input = {{"name", "nonexistent-skill"}};
    ToolResult result = tool.execute(input, ctx);

    REQUIRE(result.is_error);
    REQUIRE(result.title == "Skill Not Found");
    REQUIRE(result.output.find("nonexistent-skill") != std::string::npos);
}

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Execute.SkillNotFound.ListsAvailable", "[Skill][Tool]") {
    registerTestSkill("available-skill", "An available skill", "Instructions");

    SkillTool tool;
    ToolContext ctx = makeContext();

    nlohmann::json input = {{"name", "missing-skill"}};
    ToolResult result = tool.execute(input, ctx);

    REQUIRE(result.is_error);
    REQUIRE(result.output.find("available-skill") != std::string::npos);
}

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Execute.Success", "[Skill][Tool]") {
    registerTestSkill("loadable-skill", "A loadable skill", "These are the instructions.");

    SkillTool tool;
    ToolContext ctx = makeContext();

    nlohmann::json input = {{"name", "loadable-skill"}};
    ToolResult result = tool.execute(input, ctx);

    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.title.find("loadable-skill") != std::string::npos);
    REQUIRE(result.output.find("These are the instructions") != std::string::npos);
}

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Execute.Success.WithMetadata", "[Skill][Tool]") {
    Skill skill = Skill::create("metadata-skill", "Skill with metadata", "Instructions");
    skill.path = "/path/to/skill";
    skill.source = SkillSource::Global;
    SkillRegistry::instance().register_skill(skill);

    SkillTool tool;
    ToolContext ctx = makeContext();

    nlohmann::json input = {{"name", "metadata-skill"}};
    ToolResult result = tool.execute(input, ctx);

    REQUIRE(result.success);
    REQUIRE(result.metadata["skill_name"] == "metadata-skill");
    REQUIRE(result.metadata["skill_path"] == "/path/to/skill");
    REQUIRE(result.metadata["skill_source"] == "global");
}

// ==================== Permission Tests ====================

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Execute.PermissionDenied", "[Skill][Tool]") {
    registerTestSkill("denied-skill", "A denied skill", "Instructions");

    // Create a ruleset that denies this skill
    Ruleset ruleset;
    PermissionRule rule;
    rule.permission = "skill";
    rule.pattern = "denied-skill";
    rule.action = PermissionAction::Deny;
    ruleset.push_back(rule);

    SkillTool tool;
    ToolContext ctx = makeContext(ruleset);

    nlohmann::json input = {{"name", "denied-skill"}};
    ToolResult result = tool.execute(input, ctx);

    REQUIRE(result.is_error);
    REQUIRE(result.title == "Permission Denied");
    REQUIRE(result.output.find("denied-skill") != std::string::npos);
}

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Execute.PermissionAllowed", "[Skill][Tool]") {
    registerTestSkill("allowed-skill", "An allowed skill", "Instructions");

    // Create a ruleset that allows this skill
    Ruleset ruleset;
    PermissionRule rule;
    rule.permission = "skill";
    rule.pattern = "allowed-skill";
    rule.action = PermissionAction::Allow;
    ruleset.push_back(rule);

    SkillTool tool;
    ToolContext ctx = makeContext(ruleset);

    nlohmann::json input = {{"name", "allowed-skill"}};
    ToolResult result = tool.execute(input, ctx);

    REQUIRE(result.success);
}

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Execute.PermissionWildcard", "[Skill][Tool]") {
    registerTestSkill("test-admin", "Admin skill", "Instructions");
    registerTestSkill("test-user", "User skill", "Instructions");

    // Create a ruleset that denies test-* pattern
    Ruleset ruleset;
    PermissionRule rule;
    rule.permission = "skill";
    rule.pattern = "test-*";
    rule.action = PermissionAction::Deny;
    ruleset.push_back(rule);

    SkillTool tool;
    ToolContext ctx = makeContext(ruleset);

    // Both test-* skills should be denied
    ToolResult result1 = tool.execute({{"name", "test-admin"}}, ctx);
    REQUIRE(result1.is_error);
    REQUIRE(result1.title == "Permission Denied");

    ToolResult result2 = tool.execute({{"name", "test-user"}}, ctx);
    REQUIRE(result2.is_error);
    REQUIRE(result2.title == "Permission Denied");
}

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Execute.DefaultAllow", "[Skill][Tool]") {
    registerTestSkill("default-allow-skill", "Default allowed", "Instructions");

    // Empty ruleset - should allow by default
    Ruleset ruleset;
    SkillTool tool;
    ToolContext ctx = makeContext(ruleset);

    nlohmann::json input = {{"name", "default-allow-skill"}};
    ToolResult result = tool.execute(input, ctx);

    REQUIRE(result.success);
}

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Execute.LastRuleWins", "[Skill][Tool]") {
    registerTestSkill("conflict-skill", "Conflicting rules", "Instructions");

    // Create conflicting rules - last one should win
    Ruleset ruleset;
    PermissionRule deny_rule;
    deny_rule.permission = "skill";
    deny_rule.pattern = "conflict-skill";
    deny_rule.action = PermissionAction::Deny;
    ruleset.push_back(deny_rule);

    PermissionRule allow_rule;
    allow_rule.permission = "skill";
    allow_rule.pattern = "conflict-skill";
    allow_rule.action = PermissionAction::Allow;
    ruleset.push_back(allow_rule);

    SkillTool tool;
    ToolContext ctx = makeContext(ruleset);

    nlohmann::json input = {{"name", "conflict-skill"}};
    ToolResult result = tool.execute(input, ctx);

    // Last rule (Allow) should win
    REQUIRE(result.success);
}

// ==================== Output Generation Tests ====================

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Output.BasicSkill", "[Skill][Tool]") {
    Skill skill = Skill::create("output-test", "Output test skill", "Test instructions");
    SkillRegistry::instance().register_skill(skill);

    SkillTool tool;
    ToolContext ctx = makeContext();

    ToolResult result = tool.execute({{"name", "output-test"}}, ctx);

    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.output.find("<skill_content") != std::string::npos);
    REQUIRE(result.output.find("output-test") != std::string::npos);
    REQUIRE(result.output.find("Output test skill") != std::string::npos);
    REQUIRE(result.output.find("Test instructions") != std::string::npos);
    REQUIRE(result.output.find("</skill_content>") != std::string::npos);
}

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Output.WithVersion", "[Skill][Tool]") {
    Skill skill = Skill::create("versioned-skill", "Versioned", "Instructions");
    skill.version = "2.0.0";
    SkillRegistry::instance().register_skill(skill);

    SkillTool tool;
    ToolContext ctx = makeContext();

    ToolResult result = tool.execute({{"name", "versioned-skill"}}, ctx);

    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.output.find("<version>2.0.0</version>") != std::string::npos);
}

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Output.WithDependencies", "[Skill][Tool]") {
    Skill skill = Skill::create("deps-skill", "With dependencies", "Instructions");
    skill.dependencies = std::vector<std::string>{"dep-a", "dep-b"};
    SkillRegistry::instance().register_skill(skill);

    SkillTool tool;
    ToolContext ctx = makeContext();

    ToolResult result = tool.execute({{"name", "deps-skill"}}, ctx);

    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.output.find("<dependencies>") != std::string::npos);
    REQUIRE(result.output.find("<dependency>dep-a</dependency>") != std::string::npos);
    REQUIRE(result.output.find("<dependency>dep-b</dependency>") != std::string::npos);
}

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Output.WithEmptyDependencies", "[Skill][Tool]") {
    Skill skill = Skill::create("empty-deps-skill", "Empty deps", "Instructions");
    skill.dependencies = std::vector<std::string>{};  // Empty vector
    SkillRegistry::instance().register_skill(skill);

    SkillTool tool;
    ToolContext ctx = makeContext();

    ToolResult result = tool.execute({{"name", "empty-deps-skill"}}, ctx);

    REQUIRE_FALSE(result.is_error);
    // Should not include dependencies section for empty vector
    REQUIRE(result.output.find("<dependencies>") == std::string::npos);
}

// ==================== Integration Tests ====================

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Integration.FullWorkflow", "[Skill][Tool]") {
    // Register multiple skills
    Skill skill1 = Skill::create("workflow-a", "First workflow skill", "Instructions A");
    skill1.version = "1.0.0";
    Skill skill2 = Skill::create("workflow-b", "Second workflow skill", "Instructions B");
    skill2.dependencies = std::vector<std::string>{"workflow-a"};

    SkillRegistry::instance().register_skill(skill1);
    SkillRegistry::instance().register_skill(skill2);

    SkillTool tool;
    ToolContext ctx = makeContext();

    // Load first skill
    ToolResult result1 = tool.execute({{"name", "workflow-a"}}, ctx);
    REQUIRE_FALSE(result1.is_error);
    REQUIRE(result1.output.find("Instructions A") != std::string::npos);

    // Load second skill
    ToolResult result2 = tool.execute({{"name", "workflow-b"}}, ctx);
    REQUIRE_FALSE(result2.is_error);
    REQUIRE(result2.output.find("workflow-a") != std::string::npos);  // Listed as dependency

    // Verify description includes both
    std::string desc = tool.description();
    REQUIRE(desc.find("workflow-a") != std::string::npos);
    REQUIRE(desc.find("workflow-b") != std::string::npos);
}

TEST_CASE_METHOD(SkillToolFixture, "Skill.Tool.Integration.PermissionFiltering", "[Skill][Tool]") {
    // Register multiple skills
    registerTestSkill("public-skill", "Public skill", "Instructions");
    registerTestSkill("private-skill", "Private skill", "Instructions");

    // Create ruleset that denies private-*
    Ruleset ruleset;
    PermissionRule rule;
    rule.permission = "skill";
    rule.pattern = "private-*";
    rule.action = PermissionAction::Deny;
    ruleset.push_back(rule);

    SkillTool tool;
    ToolContext ctx = makeContext(ruleset);

    // Public skill should be accessible
    ToolResult public_result = tool.execute({{"name", "public-skill"}}, ctx);
    REQUIRE_FALSE(public_result.is_error);

    // Private skill should be denied
    ToolResult private_result = tool.execute({{"name", "private-skill"}}, ctx);
    REQUIRE(private_result.is_error);
    REQUIRE(private_result.title == "Permission Denied");
}
