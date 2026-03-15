#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/skill/skill.hpp>
#include <fstream>

using namespace turbot::core;
using namespace turbot::test;

// ==================== SkillSource 测试 ====================

TEST_CASE("Skill.Source.ToString", "[Skill]") {
    REQUIRE(skill_source_to_string(SkillSource::Project) == "project");
    REQUIRE(skill_source_to_string(SkillSource::Global) == "global");
    REQUIRE(skill_source_to_string(SkillSource::External) == "external");
    REQUIRE(skill_source_to_string(SkillSource::Remote) == "remote");
}

TEST_CASE("Skill.Source.FromString", "[Skill]") {
    REQUIRE(skill_source_from_string("project") == SkillSource::Project);
    REQUIRE(skill_source_from_string("global") == SkillSource::Global);
    REQUIRE(skill_source_from_string("external") == SkillSource::External);
    REQUIRE(skill_source_from_string("remote") == SkillSource::Remote);
}

// ==================== validate_skill_name 测试 ====================

TEST_CASE("Skill.ValidateName.Valid", "[Skill]") {
    REQUIRE(validate_skill_name("simple"));
    REQUIRE(validate_skill_name("with-hyphen"));
    REQUIRE(validate_skill_name("with123numbers"));
    REQUIRE(validate_skill_name("a"));  // minimum length
}

TEST_CASE("Skill.ValidateName.Invalid", "[Skill]") {
    REQUIRE_FALSE(validate_skill_name(""));  // empty
    REQUIRE_FALSE(validate_skill_name("With-Uppercase"));
    REQUIRE_FALSE(validate_skill_name("with_underscore"));
    REQUIRE_FALSE(validate_skill_name("with space"));
    // Test max length (64 chars) - this is exactly 65 characters
    REQUIRE_FALSE(validate_skill_name("a-skill-name-that-is-exactly-sixty-five-characters-long-should-fail"));
}

// ==================== Skill 结构体测试 ====================

TEST_CASE("Skill.Struct.Defaults", "[Skill]") {
    Skill skill;
    REQUIRE(skill.name.empty());
    REQUIRE(skill.description.empty());
    REQUIRE(skill.instructions.empty());
    REQUIRE(skill.source == SkillSource::Project);
    REQUIRE_FALSE(skill.version.has_value());
    REQUIRE_FALSE(skill.license.has_value());
}

TEST_CASE("Skill.Struct.Create", "[Skill]") {
    auto skill = Skill::create(
        "test-skill",
        "A test skill for unit testing",
        "This is the instruction content."
    );
    
    REQUIRE(skill.name == "test-skill");
    REQUIRE(skill.description == "A test skill for unit testing");
    REQUIRE(skill.instructions == "This is the instruction content.");
    REQUIRE(skill.is_valid());
}

TEST_CASE("Skill.Struct.JsonSerialization", "[Skill]") {
    Skill skill;
    skill.name = "json-test";
    skill.description = "JSON serialization test";
    skill.instructions = "Instructions here";
    skill.version = "1.0.0";
    skill.license = "MIT";
    skill.source = SkillSource::Global;
    
    nlohmann::json j = skill.to_json();
    REQUIRE(j["name"] == "json-test");
    REQUIRE(j["description"] == "JSON serialization test");
    REQUIRE(j["version"] == "1.0.0");
    REQUIRE(j["license"] == "MIT");
    REQUIRE(j["source"] == "global");
    
    auto restored = Skill::from_json(j);
    REQUIRE(restored.name == "json-test");
    REQUIRE(restored.description == "JSON serialization test");
    REQUIRE(restored.version == "1.0.0");
    REQUIRE(restored.source == SkillSource::Global);
}

TEST_CASE("Skill.Struct.Validation", "[Skill]") {
    // Valid skill
    Skill valid = Skill::create("valid", "Valid skill", "Instructions");
    REQUIRE(valid.is_valid());
    REQUIRE(valid.get_validation_errors().empty());
    
    // Invalid skill - empty name
    Skill invalid;
    invalid.name = "";
    invalid.description = "Missing name";
    REQUIRE_FALSE(invalid.is_valid());
    REQUIRE_FALSE(invalid.get_validation_errors().empty());
}

// ==================== SkillRegistry 测试 ====================

TEST_CASE("Skill.Registry.Instance", "[Skill]") {
    auto& registry1 = SkillRegistry::instance();
    auto& registry2 = SkillRegistry::instance();
    REQUIRE(&registry1 == &registry2);
}

TEST_CASE("Skill.Registry.RegisterAndRetrieve", "[Skill]") {
    auto& registry = SkillRegistry::instance();
    registry.clear();
    
    Skill skill = Skill::create("registry-test", "Test skill", "Instructions");
    REQUIRE(registry.register_skill(skill));
    
    REQUIRE(registry.has("registry-test"));
    
    auto retrieved = registry.get("registry-test");
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->name == "registry-test");
    REQUIRE(retrieved->description == "Test skill");
}

TEST_CASE("Skill.Registry.Unregister", "[Skill]") {
    auto& registry = SkillRegistry::instance();
    registry.clear();
    
    Skill skill = Skill::create("to-remove", "Will be removed", "Instructions");
    registry.register_skill(skill);
    
    REQUIRE(registry.has("to-remove"));
    REQUIRE(registry.unregister("to-remove"));
    REQUIRE_FALSE(registry.has("to-remove"));
}

TEST_CASE("Skill.Registry.All", "[Skill]") {
    auto& registry = SkillRegistry::instance();
    registry.clear();
    
    registry.register_skill(Skill::create("skill-a", "A", "Instructions"));
    registry.register_skill(Skill::create("skill-b", "B", "Instructions"));
    
    auto all = registry.all();
    REQUIRE(all.size() == 2);
    
    auto names = registry.names();
    REQUIRE(names.size() == 2);
}

TEST_CASE("Skill.Registry.GetBySource", "[Skill]") {
    auto& registry = SkillRegistry::instance();
    registry.clear();
    
    Skill project_skill = Skill::create("project-skill", "Project", "Instructions");
    project_skill.source = SkillSource::Project;
    
    Skill global_skill = Skill::create("global-skill", "Global", "Instructions");
    global_skill.source = SkillSource::Global;
    
    registry.register_skill(project_skill);
    registry.register_skill(global_skill);
    
    auto project_skills = registry.get_by_source(SkillSource::Project);
    REQUIRE(project_skills.size() == 1);
    REQUIRE(project_skills[0].name == "project-skill");
    
    auto global_skills = registry.get_by_source(SkillSource::Global);
    REQUIRE(global_skills.size() == 1);
}

// ==================== Skill::parse 测试 ====================

TEST_CASE("Skill.Parse.ValidFile", "[Skill]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    // Create a valid SKILL.md file
    auto skill_dir = tmp.path() / "test-skill";
    std::filesystem::create_directories(skill_dir);
    
    auto skill_file = skill_dir / "SKILL.md";
    std::ofstream file(skill_file);
    file << "---\n";
    file << "name: parsed-skill\n";
    file << "description: A parsed skill\n";
    file << "version: \"1.0\"\n";
    file << "---\n\n";
    file << "# Instructions\n\n";
    file << "This is the skill content.\n";
    file.close();
    
    auto parsed = Skill::parse(skill_file.string());
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->name == "parsed-skill");
    REQUIRE(parsed->description == "A parsed skill");
    REQUIRE(parsed->version == "1.0");
    REQUIRE(parsed->instructions.find("skill content") != std::string::npos);
}

TEST_CASE("Skill.Parse.InvalidFile", "[Skill]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    // Create an invalid SKILL.md file (missing required fields)
    auto skill_file = tmp.path() / "SKILL.md";
    std::ofstream file(skill_file);
    file << "---\n";
    file << "name: \"\"\n";  // Empty name
    file << "---\n";
    file.close();
    
    auto parsed = Skill::parse(skill_file.string());
    // Should return nullopt or an invalid skill
    if (parsed.has_value()) {
        REQUIRE_FALSE(parsed->is_valid());
    }
}

// ==================== SkillLoadResult 测试 ====================

TEST_CASE("Skill.LoadResult.Defaults", "[Skill]") {
    SkillLoadResult result;
    REQUIRE(result.success == false);
    REQUIRE(result.path.empty());
    REQUIRE(result.name.empty());
    REQUIRE(result.errors.empty());
    REQUIRE(result.warnings.empty());
}
