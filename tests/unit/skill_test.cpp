// Skill module unit tests
// Tests for Skill Discovery, SkillRegistry, and related utilities

#include <turbot/core/skill/skill.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace turbot::core;

// Helper to create temporary skill files for testing
class SkillTestHelper {
public:
    static std::string create_temp_dir() {
        std::string temp_dir = std::tmpnam(nullptr);
        fs::create_directories(temp_dir);
        return temp_dir;
    }

    static void remove_temp_dir(const std::string& path) {
        if (fs::exists(path)) {
            fs::remove_all(path);
        }
    }

    static std::string create_skill_file(
        const std::string& dir,
        const std::string& skill_name,
        const std::string& description,
        const std::string& content = "Test skill content",
        const std::optional<std::string>& license = std::nullopt,
        const std::optional<std::string>& version = std::nullopt
    ) {
        std::string skill_dir = dir + "/" + skill_name;
        fs::create_directories(skill_dir);

        std::string skill_file = skill_dir + "/SKILL.md";
        std::ofstream file(skill_file);

        file << "---\n";
        file << "name: " << skill_name << "\n";
        file << "description: " << description << "\n";
        if (license) {
            file << "license: " << *license << "\n";
        }
        if (version) {
            file << "version: " << *version << "\n";
        }
        file << "---\n\n";
        file << "# " << skill_name << "\n\n";
        file << content << "\n";

        file.close();
        return skill_file;
    }
};

// ===== Skill Constants Tests =====

TEST_CASE("Skill constants", "[skill][constants]") {
    SECTION("Name length limit") {
        REQUIRE(skill_constants::max_name_length == 64);
    }

    SECTION("Description length limit") {
        REQUIRE(skill_constants::max_description_length == 1024);
    }

    SECTION("Skill file name") {
        REQUIRE(std::string(skill_constants::skill_file_name) == "SKILL.md");
    }
}

// ===== Skill Name Validation Tests =====

TEST_CASE("validate_skill_name", "[skill][validation]") {
    SECTION("Valid names") {
        REQUIRE(validate_skill_name("test"));
        REQUIRE(validate_skill_name("test-skill"));
        REQUIRE(validate_skill_name("a"));
        REQUIRE(validate_skill_name("skill123"));
        REQUIRE(validate_skill_name("my-test-skill"));
        REQUIRE(validate_skill_name("abc123-def456"));
    }

    SECTION("Invalid names - empty") {
        REQUIRE_FALSE(validate_skill_name(""));
    }

    SECTION("Invalid names - too long") {
        std::string long_name(skill_constants::max_name_length + 1, 'a');
        REQUIRE_FALSE(validate_skill_name(long_name));
    }

    SECTION("Invalid names - uppercase") {
        REQUIRE_FALSE(validate_skill_name("Test"));
        REQUIRE_FALSE(validate_skill_name("TEST"));
        REQUIRE_FALSE(validate_skill_name("Test-Skill"));
    }

    SECTION("Invalid names - special characters") {
        REQUIRE_FALSE(validate_skill_name("test_skill"));
        REQUIRE_FALSE(validate_skill_name("test.skill"));
        REQUIRE_FALSE(validate_skill_name("test skill"));
        REQUIRE_FALSE(validate_skill_name("test@skill"));
    }

    SECTION("Invalid names - consecutive hyphens") {
        REQUIRE_FALSE(validate_skill_name("test--skill"));
        REQUIRE_FALSE(validate_skill_name("a--b"));
    }

    SECTION("Invalid names - starts or ends with hyphen") {
        REQUIRE_FALSE(validate_skill_name("-test"));
        REQUIRE_FALSE(validate_skill_name("test-"));
        REQUIRE_FALSE(validate_skill_name("-test-"));
    }

    SECTION("Invalid names - numbers only with hyphen") {
        REQUIRE(validate_skill_name("123"));
        REQUIRE(validate_skill_name("123-456"));
    }
}

// ===== SkillSource Conversion Tests =====

TEST_CASE("SkillSource conversion", "[skill][source]") {
    SECTION("to_string") {
        REQUIRE(skill_source_to_string(SkillSource::Project) == "project");
        REQUIRE(skill_source_to_string(SkillSource::Global) == "global");
        REQUIRE(skill_source_to_string(SkillSource::External) == "external");
        REQUIRE(skill_source_to_string(SkillSource::Remote) == "remote");
    }

    SECTION("from_string") {
        REQUIRE(skill_source_from_string("project") == SkillSource::Project);
        REQUIRE(skill_source_from_string("global") == SkillSource::Global);
        REQUIRE(skill_source_from_string("external") == SkillSource::External);
        REQUIRE(skill_source_from_string("remote") == SkillSource::Remote);
        REQUIRE(skill_source_from_string("unknown") == SkillSource::Project);
    }

    SECTION("round-trip") {
        for (auto source : {SkillSource::Project, SkillSource::Global, SkillSource::External, SkillSource::Remote}) {
            auto str = skill_source_to_string(source);
            REQUIRE(skill_source_from_string(str) == source);
        }
    }
}

// ===== Skill Parsing Tests =====

TEST_CASE("Skill::parse", "[skill][parse]") {
    std::string temp_dir = SkillTestHelper::create_temp_dir();

    SECTION("Parse valid skill file") {
        std::string skill_file = SkillTestHelper::create_skill_file(
            temp_dir, "test-skill", "A test skill", "Test content"
        );

        auto skill_opt = Skill::parse(skill_file);
        REQUIRE(skill_opt.has_value());

        auto& skill = *skill_opt;
        REQUIRE(skill.name == "test-skill");
        REQUIRE(skill.description == "A test skill");
        REQUIRE_THAT(skill.instructions, Catch::Matchers::ContainsSubstring("Test content"));
        REQUIRE(skill.skill_file_path == skill_file);
        REQUIRE_FALSE(skill.path.empty());
    }

    SECTION("Parse skill with optional fields") {
        std::string skill_file = SkillTestHelper::create_skill_file(
            temp_dir, "full-skill", "Full skill", "Content",
            "MIT", "\"1.0.0\""
        );

        auto skill_opt = Skill::parse(skill_file);
        REQUIRE(skill_opt.has_value());

        auto& skill = *skill_opt;
        REQUIRE(skill.license.has_value());
        REQUIRE(*skill.license == "MIT");
        REQUIRE(skill.version.has_value());
        REQUIRE(*skill.version == "1.0.0");
    }

    SECTION("Parse non-existent file") {
        auto skill_opt = Skill::parse("/nonexistent/path/SKILL.md");
        REQUIRE_FALSE(skill_opt.has_value());
    }

    SECTION("Parse file without frontmatter") {
        std::string skill_dir = temp_dir + "/no-frontmatter";
        fs::create_directories(skill_dir);
        std::string skill_file = skill_dir + "/SKILL.md";

        std::ofstream file(skill_file);
        file << "# No frontmatter\n\nJust content\n";
        file.close();

        auto skill_opt = Skill::parse(skill_file);
        REQUIRE_FALSE(skill_opt.has_value());
    }

    SECTION("Parse skill missing name") {
        std::string skill_dir = temp_dir + "/no-name";
        fs::create_directories(skill_dir);
        std::string skill_file = skill_dir + "/SKILL.md";

        std::ofstream file(skill_file);
        file << "---\ndescription: No name\n---\n\nContent\n";
        file.close();

        auto skill_opt = Skill::parse(skill_file);
        REQUIRE_FALSE(skill_opt.has_value());
    }

    SECTION("Parse skill missing description") {
        std::string skill_dir = temp_dir + "/no-desc";
        fs::create_directories(skill_dir);
        std::string skill_file = skill_dir + "/SKILL.md";

        std::ofstream file(skill_file);
        file << "---\nname: no-desc\n---\n\nContent\n";
        file.close();

        auto skill_opt = Skill::parse(skill_file);
        REQUIRE_FALSE(skill_opt.has_value());
    }

    SECTION("Parse skill with invalid name") {
        std::string skill_dir = temp_dir + "/invalid-name";
        fs::create_directories(skill_dir);
        std::string skill_file = skill_dir + "/SKILL.md";

        std::ofstream file(skill_file);
        file << "---\nname: Invalid_Name\n---\n\nContent\n";
        file.close();

        auto skill_opt = Skill::parse(skill_file);
        REQUIRE_FALSE(skill_opt.has_value());
    }

    SkillTestHelper::remove_temp_dir(temp_dir);
}

// ===== Skill Validation Tests =====

TEST_CASE("Skill::is_valid", "[skill][validation]") {
    SECTION("Valid skill") {
        Skill skill = Skill::create("test-skill", "A test", "Content");
        REQUIRE(skill.is_valid());
    }

    SECTION("Invalid - empty name") {
        Skill skill = Skill::create("", "A test", "Content");
        REQUIRE_FALSE(skill.is_valid());
    }

    SECTION("Invalid - empty description") {
        Skill skill = Skill::create("test", "", "Content");
        REQUIRE_FALSE(skill.is_valid());
    }

    SECTION("Invalid - bad name format") {
        Skill skill = Skill::create("Test_Skill", "A test", "Content");
        REQUIRE_FALSE(skill.is_valid());
    }
}

TEST_CASE("Skill::get_validation_errors", "[skill][validation]") {
    SECTION("No errors for valid skill") {
        Skill skill = Skill::create("valid-skill", "A description", "Content");
        auto errors = skill.get_validation_errors();
        REQUIRE(errors.empty());
    }

    SECTION("Errors for empty name") {
        Skill skill = Skill::create("", "A description", "Content");
        auto errors = skill.get_validation_errors();
        REQUIRE_FALSE(errors.empty());
        bool found = false;
        for (const auto& err : errors) {
            if (err.find("name") != std::string::npos) {
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }

    SECTION("Errors for empty description") {
        Skill skill = Skill::create("test", "", "Content");
        auto errors = skill.get_validation_errors();
        REQUIRE_FALSE(errors.empty());
    }
}

// ===== Skill Factory Tests =====

TEST_CASE("Skill::create", "[skill][factory]") {
    SECTION("Create basic skill") {
        Skill skill = Skill::create("my-skill", "My skill", "Instructions here");

        REQUIRE(skill.name == "my-skill");
        REQUIRE(skill.description == "My skill");
        REQUIRE(skill.instructions == "Instructions here");
        REQUIRE(skill.source == SkillSource::Project);
        REQUIRE(skill.time_loaded > 0);
    }

    SECTION("Create with path") {
        Skill skill = Skill::create("skill", "Desc", "Content", "/path/to/skill");
        REQUIRE(skill.path == "/path/to/skill");
    }
}

// ===== Skill Serialization Tests =====

TEST_CASE("Skill JSON serialization", "[skill][json]") {
    SECTION("Round-trip serialization") {
        Skill original = Skill::create("json-skill", "JSON test", "Content");
        original.license = "MIT";
        original.version = "2.0.0";
        original.dependencies = std::vector<std::string>{"dep1", "dep2"};

        nlohmann::json j = original.to_json();
        Skill restored = Skill::from_json(j);

        REQUIRE(restored.name == original.name);
        REQUIRE(restored.description == original.description);
        REQUIRE(restored.instructions == original.instructions);
        REQUIRE(restored.license == original.license);
        REQUIRE(restored.version == original.version);
        REQUIRE(restored.dependencies.has_value());
        REQUIRE(restored.dependencies->size() == 2);
    }

    SECTION("Serialization includes all fields") {
        Skill skill = Skill::create("test", "Test", "Content");
        skill.source = SkillSource::Global;

        nlohmann::json j = skill.to_json();

        REQUIRE(j["name"] == "test");
        REQUIRE(j["description"] == "Test");
        REQUIRE(j["instructions"] == "Content");
        REQUIRE(j["source"] == "global");
        REQUIRE(j.contains("time_loaded"));
    }
}

// ===== SkillRegistry Tests =====

TEST_CASE("SkillRegistry basic operations", "[skill][registry]") {
    SkillRegistry& registry = SkillRegistry::instance();
    registry.clear();

    SECTION("Register and retrieve skill") {
        Skill skill = Skill::create("registry-test", "Registry test", "Content");
        REQUIRE(registry.register_skill(skill));
        REQUIRE(registry.has("registry-test"));

        auto retrieved = registry.get("registry-test");
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->name == "registry-test");
    }

    SECTION("Unregister skill") {
        Skill skill = Skill::create("to-remove", "To remove", "Content");
        registry.register_skill(skill);
        REQUIRE(registry.has("to-remove"));

        REQUIRE(registry.unregister("to-remove"));
        REQUIRE_FALSE(registry.has("to-remove"));
    }

    SECTION("Unregister non-existent skill") {
        REQUIRE_FALSE(registry.unregister("nonexistent"));
    }

    SECTION("Get non-existent skill") {
        auto skill = registry.get("nonexistent");
        REQUIRE_FALSE(skill.has_value());
    }

    SECTION("Override existing skill") {
        Skill skill1 = Skill::create("override", "Version 1", "Content 1");
        Skill skill2 = Skill::create("override", "Version 2", "Content 2");

        registry.register_skill(skill1);
        registry.register_skill(skill2);

        auto retrieved = registry.get("override");
        REQUIRE(retrieved->description == "Version 2");
    }

    SECTION("List all skills") {
        registry.register_skill(Skill::create("skill-a", "A", "Content"));
        registry.register_skill(Skill::create("skill-b", "B", "Content"));

        auto all = registry.all();
        REQUIRE(all.size() >= 2);

        auto names = registry.names();
        bool has_a = false, has_b = false;
        for (const auto& name : names) {
            if (name == "skill-a") has_a = true;
            if (name == "skill-b") has_b = true;
        }
        REQUIRE(has_a);
        REQUIRE(has_b);
    }

    SECTION("Clear registry") {
        registry.register_skill(Skill::create("clear-test", "Test", "Content"));
        registry.clear();
        REQUIRE(registry.size() == 0);
    }

    SECTION("Size") {
        registry.clear();
        REQUIRE(registry.size() == 0);

        registry.register_skill(Skill::create("size1", "Test", "Content"));
        REQUIRE(registry.size() == 1);

        registry.register_skill(Skill::create("size2", "Test", "Content"));
        REQUIRE(registry.size() == 2);
    }

    registry.clear();
}

TEST_CASE("SkillRegistry scan_directory", "[skill][registry][scan]") {
    SkillRegistry& registry = SkillRegistry::instance();
    registry.clear();

    std::string temp_dir = SkillTestHelper::create_temp_dir();

    SECTION("Scan directory with skills") {
        SkillTestHelper::create_skill_file(temp_dir, "scan-skill-1", "First skill");
        SkillTestHelper::create_skill_file(temp_dir, "scan-skill-2", "Second skill");

        size_t loaded = registry.scan_directory(temp_dir, SkillSource::Project);

        REQUIRE(loaded == 2);
        REQUIRE(registry.has("scan-skill-1"));
        REQUIRE(registry.has("scan-skill-2"));
    }

    SECTION("Scan empty directory") {
        std::string empty_dir = temp_dir + "/empty";
        fs::create_directories(empty_dir);

        size_t loaded = registry.scan_directory(empty_dir, SkillSource::Project);
        REQUIRE(loaded == 0);
    }

    SECTION("Scan non-existent directory") {
        size_t loaded = registry.scan_directory("/nonexistent/directory", SkillSource::Project);
        REQUIRE(loaded == 0);
    }

    SECTION("Scan directory with mixed content") {
        // Create a valid skill
        SkillTestHelper::create_skill_file(temp_dir, "valid-skill", "Valid");

        // Create a directory without SKILL.md
        fs::create_directories(temp_dir + "/not-a-skill");

        // Create a file (not directory)
        std::ofstream file(temp_dir + "/some-file.txt");
        file << "Not a skill\n";
        file.close();

        size_t loaded = registry.scan_directory(temp_dir, SkillSource::Project);
        REQUIRE(loaded == 1);
        REQUIRE(registry.has("valid-skill"));
    }

    SECTION("Skills get correct source") {
        SkillTestHelper::create_skill_file(temp_dir, "project-skill", "Project skill");
        registry.scan_directory(temp_dir, SkillSource::Project);

        auto skill = registry.get("project-skill");
        REQUIRE(skill->source == SkillSource::Project);
    }

    SkillTestHelper::remove_temp_dir(temp_dir);
    registry.clear();
}

TEST_CASE("SkillRegistry get_by_source", "[skill][registry]") {
    SkillRegistry& registry = SkillRegistry::instance();
    registry.clear();

    Skill project_skill = Skill::create("project-skill", "Project", "Content");
    project_skill.source = SkillSource::Project;
    registry.register_skill(project_skill);

    Skill global_skill = Skill::create("global-skill", "Global", "Content");
    global_skill.source = SkillSource::Global;
    registry.register_skill(global_skill);

    SECTION("Get project skills") {
        auto skills = registry.get_by_source(SkillSource::Project);
        REQUIRE(skills.size() == 1);
        REQUIRE(skills[0].name == "project-skill");
    }

    SECTION("Get global skills") {
        auto skills = registry.get_by_source(SkillSource::Global);
        REQUIRE(skills.size() == 1);
        REQUIRE(skills[0].name == "global-skill");
    }

    SECTION("Get external skills") {
        auto skills = registry.get_by_source(SkillSource::External);
        REQUIRE(skills.empty());
    }

    registry.clear();
}

TEST_CASE("SkillRegistry load_skill", "[skill][registry]") {
    SkillRegistry& registry = SkillRegistry::instance();
    registry.clear();

    std::string temp_dir = SkillTestHelper::create_temp_dir();

    SECTION("Load valid skill") {
        std::string skill_file = SkillTestHelper::create_skill_file(
            temp_dir, "load-test", "Load test skill"
        );

        auto result = registry.load_skill(skill_file, SkillSource::Project);

        REQUIRE(result.success);
        REQUIRE(result.name == "load-test");
        REQUIRE(result.errors.empty());
    }

    SECTION("Load invalid skill") {
        std::string skill_dir = temp_dir + "/invalid-skill";
        fs::create_directories(skill_dir);
        std::string skill_file = skill_dir + "/SKILL.md";

        std::ofstream file(skill_file);
        file << "---\nname: INVALID\n---\n\nContent\n";
        file.close();

        auto result = registry.load_skill(skill_file, SkillSource::Project);

        REQUIRE_FALSE(result.success);
        REQUIRE_FALSE(result.errors.empty());
    }

    SkillTestHelper::remove_temp_dir(temp_dir);
    registry.clear();
}

TEST_CASE("SkillRegistry add_skill_directory", "[skill][registry]") {
    SkillRegistry& registry = SkillRegistry::instance();
    registry.clear();

    registry.add_skill_directory("/custom/path");

    auto dirs = registry.get_skill_directories();
    bool found = false;
    for (const auto& dir : dirs) {
        if (dir == "/custom/path") {
            found = true;
            break;
        }
    }
    REQUIRE(found);

    registry.clear();
}

// ===== Skill Discovery Tests =====

TEST_CASE("skill_discovery::get_search_paths", "[skill][discovery]") {
    SECTION("Returns search paths for project") {
        auto paths = skill_discovery::get_search_paths("/project/root");

        REQUIRE_THAT(paths.project_opencode, Catch::Matchers::ContainsSubstring("/project/root"));
        REQUIRE_THAT(paths.project_opencode, Catch::Matchers::ContainsSubstring(".opencode"));
        REQUIRE_THAT(paths.project_claude, Catch::Matchers::ContainsSubstring(".claude"));
        REQUIRE_THAT(paths.project_agents, Catch::Matchers::ContainsSubstring(".agents"));
    }

    SECTION("to_vector returns all paths") {
        auto paths = skill_discovery::get_search_paths("/project");
        auto vec = paths.to_vector();

        REQUIRE_FALSE(vec.empty());
    }
}

TEST_CASE("skill_discovery::find_skill_files", "[skill][discovery]") {
    std::string temp_dir = SkillTestHelper::create_temp_dir();

    SECTION("Find skill files in directory") {
        SkillTestHelper::create_skill_file(temp_dir, "find-skill-1", "First");
        SkillTestHelper::create_skill_file(temp_dir, "find-skill-2", "Second");

        auto files = skill_discovery::find_skill_files(temp_dir);

        REQUIRE(files.size() == 2);
    }

    SECTION("Returns empty for non-existent directory") {
        auto files = skill_discovery::find_skill_files("/nonexistent");
        REQUIRE(files.empty());
    }

    SkillTestHelper::remove_temp_dir(temp_dir);
}

TEST_CASE("skill_discovery::is_skill_directory", "[skill][discovery]") {
    std::string temp_dir = SkillTestHelper::create_temp_dir();

    SECTION("Returns true for valid skill directory") {
        std::string skill_file = SkillTestHelper::create_skill_file(
            temp_dir, "valid-dir", "Valid"
        );
        std::string skill_dir = fs::path(skill_file).parent_path().string();

        REQUIRE(skill_discovery::is_skill_directory(skill_dir));
    }

    SECTION("Returns false for directory without SKILL.md") {
        std::string no_skill_dir = temp_dir + "/no-skill";
        fs::create_directories(no_skill_dir);

        REQUIRE_FALSE(skill_discovery::is_skill_directory(no_skill_dir));
    }

    SECTION("Returns false for non-existent directory") {
        REQUIRE_FALSE(skill_discovery::is_skill_directory("/nonexistent"));
    }

    SkillTestHelper::remove_temp_dir(temp_dir);
}

// ===== SkillLoadResult Tests =====

TEST_CASE("SkillLoadResult", "[skill][result]") {
    SECTION("Default values") {
        SkillLoadResult result;
        REQUIRE_FALSE(result.success);
        REQUIRE(result.path.empty());
        REQUIRE(result.name.empty());
        REQUIRE(result.errors.empty());
        REQUIRE(result.warnings.empty());
    }
}

// ===== Remote Skill Discovery Tests =====

TEST_CASE("RemoteSkillEntry JSON", "[skill][remote]") {
    SECTION("Parse valid entry") {
        nlohmann::json j = R"({
            "name": "test-skill",
            "description": "A test skill",
            "files": ["SKILL.md", "references/extra.md"]
        })"_json;

        auto entry = skill_discovery::RemoteSkillEntry::from_json(j);
        REQUIRE(entry.has_value());
        REQUIRE(entry->name == "test-skill");
        REQUIRE(entry->description == "A test skill");
        REQUIRE(entry->files.size() == 2);
        REQUIRE(entry->files[0] == "SKILL.md");
    }

    SECTION("Parse entry without description") {
        nlohmann::json j = R"({
            "name": "minimal",
            "files": ["SKILL.md"]
        })"_json;

        auto entry = skill_discovery::RemoteSkillEntry::from_json(j);
        REQUIRE(entry.has_value());
        REQUIRE(entry->name == "minimal");
        REQUIRE(entry->description.empty());
    }

    SECTION("Invalid - missing name") {
        nlohmann::json j = R"({
            "description": "No name",
            "files": ["SKILL.md"]
        })"_json;

        auto entry = skill_discovery::RemoteSkillEntry::from_json(j);
        REQUIRE_FALSE(entry.has_value());
    }

    SECTION("Invalid - missing files") {
        nlohmann::json j = R"({
            "name": "no-files",
            "description": "No files"
        })"_json;

        auto entry = skill_discovery::RemoteSkillEntry::from_json(j);
        REQUIRE_FALSE(entry.has_value());
    }

    SECTION("Invalid - empty files array") {
        nlohmann::json j = R"({
            "name": "empty-files",
            "files": []
        })"_json;

        auto entry = skill_discovery::RemoteSkillEntry::from_json(j);
        REQUIRE_FALSE(entry.has_value());
    }

    SECTION("to_json round-trip") {
        skill_discovery::RemoteSkillEntry original;
        original.name = "round-trip";
        original.description = "Test";
        original.files = {"SKILL.md", "extra.md"};

        auto j = original.to_json();
        auto restored = skill_discovery::RemoteSkillEntry::from_json(j);

        REQUIRE(restored.has_value());
        REQUIRE(restored->name == original.name);
        REQUIRE(restored->description == original.description);
        REQUIRE(restored->files == original.files);
    }
}

TEST_CASE("RemoteSkillIndex JSON", "[skill][remote]") {
    SECTION("Parse valid index") {
        nlohmann::json j = R"({
            "skills": [
                {
                    "name": "skill-one",
                    "description": "First skill",
                    "files": ["SKILL.md"]
                },
                {
                    "name": "skill-two",
                    "description": "Second skill",
                    "files": ["SKILL.md", "refs/guide.md"]
                }
            ]
        })"_json;

        auto index = skill_discovery::RemoteSkillIndex::from_json(j);
        REQUIRE(index.has_value());
        REQUIRE(index->skills.size() == 2);
        REQUIRE(index->skills[0].name == "skill-one");
        REQUIRE(index->skills[1].name == "skill-two");
    }

    SECTION("Invalid - missing skills array") {
        nlohmann::json j = R"({"other": "data"})"_json;

        auto index = skill_discovery::RemoteSkillIndex::from_json(j);
        REQUIRE_FALSE(index.has_value());
    }

    SECTION("Skips invalid entries") {
        nlohmann::json j = R"({
            "skills": [
                {
                    "name": "valid",
                    "files": ["SKILL.md"]
                },
                {
                    "description": "Invalid - no name or files"
                }
            ]
        })"_json;

        auto index = skill_discovery::RemoteSkillIndex::from_json(j);
        REQUIRE(index.has_value());
        REQUIRE(index->skills.size() == 1);
        REQUIRE(index->skills[0].name == "valid");
    }

    SECTION("to_json round-trip") {
        skill_discovery::RemoteSkillIndex original;
        skill_discovery::RemoteSkillEntry entry1;
        entry1.name = "test1";
        entry1.files = {"SKILL.md"};
        skill_discovery::RemoteSkillEntry entry2;
        entry2.name = "test2";
        entry2.files = {"SKILL.md", "extra.md"};
        original.skills = {entry1, entry2};

        auto j = original.to_json();
        auto restored = skill_discovery::RemoteSkillIndex::from_json(j);

        REQUIRE(restored.has_value());
        REQUIRE(restored->skills.size() == 2);
    }
}

TEST_CASE("PullResult JSON", "[skill][remote]") {
    SECTION("Serialize to JSON") {
        skill_discovery::PullResult result;
        result.success = true;
        result.dirs = {"/cache/skills/skill1", "/cache/skills/skill2"};
        result.skills_downloaded = 2;
        result.files_downloaded = 5;
        result.from_cache = false;

        auto j = result.to_json();

        REQUIRE(j["success"] == true);
        REQUIRE(j["dirs"].size() == 2);
        REQUIRE(j["skills_downloaded"] == 2);
        REQUIRE(j["files_downloaded"] == 5);
        REQUIRE(j["from_cache"] == false);
    }
}

TEST_CASE("skill_discovery::get_cache_dir", "[skill][remote]") {
    SECTION("Returns non-empty path") {
        std::string cache_dir = skill_discovery::get_cache_dir();
        REQUIRE_FALSE(cache_dir.empty());
        // Should contain "turbot" and "skills"
        REQUIRE_THAT(cache_dir, Catch::Matchers::ContainsSubstring("turbot"));
        REQUIRE_THAT(cache_dir, Catch::Matchers::ContainsSubstring("skills"));
    }

    SECTION("Returns same path on multiple calls") {
        auto dir1 = skill_discovery::get_cache_dir();
        auto dir2 = skill_discovery::get_cache_dir();
        REQUIRE(dir1 == dir2);
    }
}

TEST_CASE("skill_discovery::is_valid_skill_url", "[skill][remote]") {
    SECTION("Valid URLs") {
        REQUIRE(skill_discovery::is_valid_skill_url("https://example.com/skills"));
        REQUIRE(skill_discovery::is_valid_skill_url("http://localhost:8080/skills/"));
        REQUIRE(skill_discovery::is_valid_skill_url("file:///local/path/skills"));
    }

    SECTION("Invalid URLs") {
        REQUIRE_FALSE(skill_discovery::is_valid_skill_url(""));
        REQUIRE_FALSE(skill_discovery::is_valid_skill_url("ftp://example.com/skills"));
        REQUIRE_FALSE(skill_discovery::is_valid_skill_url("/local/path/skills"));
        REQUIRE_FALSE(skill_discovery::is_valid_skill_url("example.com/skills"));
    }
}

TEST_CASE("skill_discovery::download_file", "[skill][remote][download]") {
    std::string temp_dir = SkillTestHelper::create_temp_dir();

    SECTION("Creates parent directories") {
        std::string dest = temp_dir + "/nested/dir/file.txt";

        // This will fail because URL doesn't exist, but should create parent dirs
        skill_discovery::download_file("http://nonexistent.invalid/file.txt", dest);

        // Parent directory should have been created
        REQUIRE(fs::exists(temp_dir + "/nested/dir"));
    }

    SECTION("Returns true for existing file (cache)") {
        std::string dest = temp_dir + "/cached.txt";
        std::ofstream file(dest);
        file << "cached content";
        file.close();

        // Should return true without making network request
        REQUIRE(skill_discovery::download_file("http://any.url/file.txt", dest));
    }

    SkillTestHelper::remove_temp_dir(temp_dir);
}

TEST_CASE("skill_discovery::clear_cache", "[skill][remote]") {
    // This test just verifies the function runs without error
    // Actual cache clearing is tested indirectly
    REQUIRE_NOTHROW(skill_discovery::clear_cache());
}

TEST_CASE("skill_discovery::pull with invalid URL", "[skill][remote][pull]") {
    SECTION("Empty URL returns error") {
        auto result = skill_discovery::pull("");
        REQUIRE_FALSE(result.success);
        REQUIRE_FALSE(result.errors.empty());
    }

    SECTION("Invalid URL returns error") {
        // Use a non-routable IP address that will fail fast
        auto result = skill_discovery::pull("http://10.255.255.1/skills/");
        REQUIRE_FALSE(result.success);
        REQUIRE_FALSE(result.errors.empty());
    }
}

TEST_CASE("skill_discovery::pull_all", "[skill][remote][pull]") {
    SECTION("Empty URL list") {
        auto result = skill_discovery::pull_all({});
        REQUIRE_FALSE(result.success);
        REQUIRE(result.dirs.empty());
    }

    SECTION("Multiple invalid URLs") {
        // Use non-routable IP addresses that will fail fast
        auto result = skill_discovery::pull_all({
            "http://10.255.255.1/skills/",
            "http://10.255.255.2/skills/"
        });

        REQUIRE_FALSE(result.success);
        // Should have errors from both URLs
        REQUIRE(result.errors.size() >= 2);
    }
}

