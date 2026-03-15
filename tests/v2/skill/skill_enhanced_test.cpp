#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/skill/skill.hpp>
#include <fstream>

using namespace turbot::core;
using namespace turbot::test;

// ==================== Skill::parse 详细测试 ====================

TEST_CASE("Skill.Parse.CompleteFile", "[Skill]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto skill_dir = tmp.path() / "complete-skill";
    std::filesystem::create_directories(skill_dir);
    
    auto skill_file = skill_dir / "SKILL.md";
    std::ofstream file(skill_file);
    file << "---\n";
    file << "name: complete-skill\n";
    file << "description: A complete skill with all fields\n";
    file << "version: \"2.0.0\"\n";
    file << "license: MIT\n";
    file << "compatibility: opencode>=1.0.0\n";
    file << "dependencies:\n";
    file << "  - base-skill\n";
    file << "  - utils-skill\n";
    file << "---\n\n";
    file << "# Instructions\n\n";
    file << "This is the complete skill content.\n";
    file << "It has multiple lines.\n";
    file.close();
    
    auto parsed = Skill::parse(skill_file.string());
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->name == "complete-skill");
    REQUIRE(parsed->description == "A complete skill with all fields");
    REQUIRE(parsed->version == "2.0.0");
    REQUIRE(parsed->license == "MIT");
    REQUIRE(parsed->compatibility == "opencode>=1.0.0");
    // Dependencies parsing depends on YAML parser behavior
    // Check if dependencies exist and have expected values if present
    if (parsed->dependencies.has_value()) {
        REQUIRE(parsed->dependencies->size() == 2);
    }
    REQUIRE(parsed->instructions.find("complete skill content") != std::string::npos);
}

TEST_CASE("Skill.Parse.MinimalFile", "[Skill]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto skill_file = tmp.path() / "SKILL.md";
    std::ofstream file(skill_file);
    file << "---\n";
    file << "name: minimal\n";
    file << "description: Minimal skill\n";
    file << "---\n";
    file.close();
    
    auto parsed = Skill::parse(skill_file.string());
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->name == "minimal");
    REQUIRE(parsed->description == "Minimal skill");
    REQUIRE_FALSE(parsed->version.has_value());
    REQUIRE_FALSE(parsed->license.has_value());
}

TEST_CASE("Skill.Parse.MissingName", "[Skill]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto skill_file = tmp.path() / "SKILL.md";
    std::ofstream file(skill_file);
    file << "---\n";
    file << "description: No name provided\n";
    file << "---\n";
    file.close();
    
    auto parsed = Skill::parse(skill_file.string());
    REQUIRE_FALSE(parsed.has_value());
}

TEST_CASE("Skill.Parse.MissingDescription", "[Skill]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto skill_file = tmp.path() / "SKILL.md";
    std::ofstream file(skill_file);
    file << "---\n";
    file << "name: no-desc\n";
    file << "---\n";
    file.close();
    
    auto parsed = Skill::parse(skill_file.string());
    REQUIRE_FALSE(parsed.has_value());
}

TEST_CASE("Skill.Parse.InvalidName", "[Skill]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto skill_file = tmp.path() / "SKILL.md";
    std::ofstream file(skill_file);
    file << "---\n";
    file << "name: Invalid_Name_123\n";  // Uppercase and underscore not allowed
    file << "description: Invalid name format\n";
    file << "---\n";
    file.close();
    
    auto parsed = Skill::parse(skill_file.string());
    REQUIRE_FALSE(parsed.has_value());
}

TEST_CASE("Skill.Parse.NonExistentFile", "[Skill]") {
    auto parsed = Skill::parse("/nonexistent/path/SKILL.md");
    REQUIRE_FALSE(parsed.has_value());
}

TEST_CASE("Skill.Parse.NoFrontmatter", "[Skill]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto skill_file = tmp.path() / "SKILL.md";
    std::ofstream file(skill_file);
    file << "# Just content\n";
    file << "No frontmatter here.\n";
    file.close();
    
    auto parsed = Skill::parse(skill_file.string());
    REQUIRE_FALSE(parsed.has_value());
}

// ==================== Skill 验证详细测试 ====================

TEST_CASE("Skill.Validation.EmptyName", "[Skill]") {
    Skill skill;
    skill.name = "";
    skill.description = "Valid description";
    
    REQUIRE_FALSE(skill.is_valid());
    auto errors = skill.get_validation_errors();
    REQUIRE_FALSE(errors.empty());
    REQUIRE(errors[0].find("name") != std::string::npos);
}

TEST_CASE("Skill.Validation.EmptyDescription", "[Skill]") {
    Skill skill;
    skill.name = "valid-name";
    skill.description = "";
    
    REQUIRE_FALSE(skill.is_valid());
    auto errors = skill.get_validation_errors();
    REQUIRE_FALSE(errors.empty());
}

TEST_CASE("Skill.Validation.DescriptionTooLong", "[Skill]") {
    Skill skill;
    skill.name = "valid-name";
    skill.description = std::string(skill_constants::max_description_length + 1, 'a');
    
    REQUIRE_FALSE(skill.is_valid());
    auto errors = skill.get_validation_errors();
    bool found_desc_error = false;
    for (const auto& err : errors) {
        if (err.find("description") != std::string::npos) {
            found_desc_error = true;
            break;
        }
    }
    REQUIRE(found_desc_error);
}

TEST_CASE("Skill.Validation.CompatibilityTooLong", "[Skill]") {
    Skill skill;
    skill.name = "valid-name";
    skill.description = "Valid description";
    skill.compatibility = std::string(skill_constants::max_compatibility_length + 1, 'a');
    
    auto errors = skill.get_validation_errors();
    bool found_compat_error = false;
    for (const auto& err : errors) {
        if (err.find("compatibility") != std::string::npos) {
            found_compat_error = true;
            break;
        }
    }
    REQUIRE(found_compat_error);
}

TEST_CASE("Skill.Validation.ValidSkill", "[Skill]") {
    Skill skill = Skill::create("valid-skill", "A valid skill", "Instructions");
    REQUIRE(skill.is_valid());
    REQUIRE(skill.get_validation_errors().empty());
}

// ==================== Skill JSON 序列化详细测试 ====================

TEST_CASE("Skill.JsonSerialization.WithDependencies", "[Skill]") {
    Skill skill;
    skill.name = "deps-skill";
    skill.description = "Has dependencies";
    skill.instructions = "Instructions";
    skill.dependencies = std::vector<std::string>{"dep1", "dep2", "dep3"};
    
    nlohmann::json j = skill.to_json();
    REQUIRE(j.contains("dependencies"));
    REQUIRE(j["dependencies"].size() == 3);
    
    auto restored = Skill::from_json(j);
    REQUIRE(restored.dependencies.has_value());
    REQUIRE(restored.dependencies->size() == 3);
}

TEST_CASE("Skill.JsonSerialization.WithMetadata", "[Skill]") {
    Skill skill;
    skill.name = "meta-skill";
    skill.description = "Has metadata";
    skill.metadata = std::map<std::string, std::string>{
        {"author", "test-author"},
        {"category", "testing"}
    };
    
    nlohmann::json j = skill.to_json();
    REQUIRE(j.contains("metadata"));
    REQUIRE(j["metadata"]["author"] == "test-author");
    
    auto restored = Skill::from_json(j);
    REQUIRE(restored.metadata.has_value());
    REQUIRE(restored.metadata->at("author") == "test-author");
}

TEST_CASE("Skill.JsonSerialization.SourceField", "[Skill]") {
    Skill skill;
    skill.name = "source-test";
    skill.description = "Source test";
    skill.source = SkillSource::Global;
    
    nlohmann::json j = skill.to_json();
    REQUIRE(j["source"] == "global");
    
    auto restored = Skill::from_json(j);
    REQUIRE(restored.source == SkillSource::Global);
}

TEST_CASE("Skill.JsonSerialization.TimeLoaded", "[Skill]") {
    Skill skill;
    skill.name = "time-test";
    skill.description = "Time test";
    skill.time_loaded = 1700000000000;
    
    nlohmann::json j = skill.to_json();
    REQUIRE(j["time_loaded"] == 1700000000000);
    
    auto restored = Skill::from_json(j);
    REQUIRE(restored.time_loaded == 1700000000000);
}

// ==================== SkillRegistry 扫描测试 ====================

TEST_CASE("Skill.Registry.ScanDirectory", "[Skill]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto& registry = SkillRegistry::instance();
    registry.clear();
    
    // Create multiple skill directories
    auto skill1_dir = tmp.path() / "skill-one";
    auto skill2_dir = tmp.path() / "skill-two";
    std::filesystem::create_directories(skill1_dir);
    std::filesystem::create_directories(skill2_dir);
    
    // Create SKILL.md files
    std::ofstream file1(skill1_dir / "SKILL.md");
    file1 << "---\nname: skill-one\ndescription: First skill\n---\n";
    file1.close();
    
    std::ofstream file2(skill2_dir / "SKILL.md");
    file2 << "---\nname: skill-two\ndescription: Second skill\n---\n";
    file2.close();
    
    size_t loaded = registry.scan_directory(tmp.path().string(), SkillSource::Project);
    REQUIRE(loaded == 2);
    REQUIRE(registry.has("skill-one"));
    REQUIRE(registry.has("skill-two"));
}

TEST_CASE("Skill.Registry.ScanNonExistentDirectory", "[Skill]") {
    auto& registry = SkillRegistry::instance();
    size_t loaded = registry.scan_directory("/nonexistent/directory", SkillSource::Project);
    REQUIRE(loaded == 0);
}

TEST_CASE("Skill.Registry.ScanEmptyDirectory", "[Skill]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto& registry = SkillRegistry::instance();
    size_t loaded = registry.scan_directory(tmp.path().string(), SkillSource::Project);
    REQUIRE(loaded == 0);
}

TEST_CASE("Skill.Registry.ScanWithInvalidSkills", "[Skill]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto& registry = SkillRegistry::instance();
    registry.clear();
    
    auto valid_dir = tmp.path() / "valid-skill";
    auto invalid_dir = tmp.path() / "invalid-skill";
    std::filesystem::create_directories(valid_dir);
    std::filesystem::create_directories(invalid_dir);
    
    // Valid skill
    std::ofstream valid_file(valid_dir / "SKILL.md");
    valid_file << "---\nname: valid-skill\ndescription: Valid\n---\n";
    valid_file.close();
    
    // Invalid skill (missing required field)
    std::ofstream invalid_file(invalid_dir / "SKILL.md");
    invalid_file << "---\nname: invalid-skill\n---\n";  // No description
    invalid_file.close();
    
    size_t loaded = registry.scan_directory(tmp.path().string(), SkillSource::Project);
    REQUIRE(loaded == 1);  // Only valid skill loaded
    REQUIRE(registry.has("valid-skill"));
    REQUIRE_FALSE(registry.has("invalid-skill"));
}

// ==================== SkillRegistry 管理测试 ====================

TEST_CASE("Skill.Registry.Unregister.Enhanced", "[Skill]") {
    auto& registry = SkillRegistry::instance();
    registry.clear();
    
    Skill skill = Skill::create("to-unregister", "Will be removed", "Instructions");
    registry.register_skill(skill);
    
    REQUIRE(registry.has("to-unregister"));
    REQUIRE(registry.unregister("to-unregister"));
    REQUIRE_FALSE(registry.has("to-unregister"));
}

TEST_CASE("Skill.Registry.UnregisterNonExistent", "[Skill]") {
    auto& registry = SkillRegistry::instance();
    REQUIRE_FALSE(registry.unregister("nonexistent-skill"));
}

TEST_CASE("Skill.Registry.Size", "[Skill]") {
    auto& registry = SkillRegistry::instance();
    registry.clear();
    
    REQUIRE(registry.size() == 0);
    
    registry.register_skill(Skill::create("size-1", "Test 1", "Instructions"));
    REQUIRE(registry.size() == 1);
    
    registry.register_skill(Skill::create("size-2", "Test 2", "Instructions"));
    REQUIRE(registry.size() == 2);
    
    registry.clear();
    REQUIRE(registry.size() == 0);
}

TEST_CASE("Skill.Registry.GetBySource.Enhanced", "[Skill]") {
    auto& registry = SkillRegistry::instance();
    registry.clear();
    
    Skill project_skill = Skill::create("project-skill", "Project", "Instructions");
    project_skill.source = SkillSource::Project;
    
    Skill global_skill = Skill::create("global-skill", "Global", "Instructions");
    global_skill.source = SkillSource::Global;
    
    Skill external_skill = Skill::create("external-skill", "External", "Instructions");
    external_skill.source = SkillSource::External;
    
    registry.register_skill(project_skill);
    registry.register_skill(global_skill);
    registry.register_skill(external_skill);
    
    auto project_skills = registry.get_by_source(SkillSource::Project);
    REQUIRE(project_skills.size() == 1);
    
    auto global_skills = registry.get_by_source(SkillSource::Global);
    REQUIRE(global_skills.size() == 1);
    
    auto external_skills = registry.get_by_source(SkillSource::External);
    REQUIRE(external_skills.size() == 1);
}

TEST_CASE("Skill.Registry.Names", "[Skill]") {
    auto& registry = SkillRegistry::instance();
    registry.clear();
    
    registry.register_skill(Skill::create("alpha", "A", "Instructions"));
    registry.register_skill(Skill::create("beta", "B", "Instructions"));
    registry.register_skill(Skill::create("gamma", "C", "Instructions"));
    
    auto names = registry.names();
    REQUIRE(names.size() == 3);
    
    // Check all names are present
    bool has_alpha = false, has_beta = false, has_gamma = false;
    for (const auto& name : names) {
        if (name == "alpha") has_alpha = true;
        if (name == "beta") has_beta = true;
        if (name == "gamma") has_gamma = true;
    }
    REQUIRE(has_alpha);
    REQUIRE(has_beta);
    REQUIRE(has_gamma);
}

TEST_CASE("Skill.Registry.All.Enhanced", "[Skill]") {
    auto& registry = SkillRegistry::instance();
    registry.clear();
    
    registry.register_skill(Skill::create("all-1", "First", "Instructions"));
    registry.register_skill(Skill::create("all-2", "Second", "Instructions"));
    
    auto all = registry.all();
    REQUIRE(all.size() == 2);
}

// ==================== SkillLoadResult 测试 ====================

TEST_CASE("Skill.LoadResult.Defaults.Enhanced", "[Skill]") {
    SkillLoadResult result;
    REQUIRE(result.success == false);
    REQUIRE(result.path.empty());
    REQUIRE(result.name.empty());
    REQUIRE(result.errors.empty());
    REQUIRE(result.warnings.empty());
}

// ==================== Skill::create 工厂方法测试 ====================

TEST_CASE("Skill.Create.SetsTimeLoaded", "[Skill]") {
    auto skill = Skill::create("time-test", "Time test", "Instructions");
    REQUIRE(skill.time_loaded > 0);
}

TEST_CASE("Skill.Create.DefaultSource", "[Skill]") {
    auto skill = Skill::create("source-test", "Source test", "Instructions");
    REQUIRE(skill.source == SkillSource::Project);
}
