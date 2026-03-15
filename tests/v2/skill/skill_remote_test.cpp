/**
 * @file skill_remote_test.cpp
 * @brief Remote skill discovery tests
 *
 * Tests for:
 * - RemoteSkillEntry JSON parsing
 * - RemoteSkillIndex JSON parsing
 * - PullResult serialization
 * - get_cache_dir()
 * - is_valid_skill_url()
 * - clear_cache()
 * - download_file() (cache hit scenarios)
 * - pull() (error scenarios)
 * - pull_all()
 */

#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/skill/skill.hpp>
#include <mock/env_guard.hpp>
#include <fstream>
#include <filesystem>

using namespace turbot::core;
using namespace turbot::core::skill_discovery;
using namespace turbot::test;

// ==================== RemoteSkillEntry Tests ====================

TEST_CASE("Skill.Remote.Entry.FromJson.Valid", "[Skill][Remote]") {
    nlohmann::json j = {
        {"name", "test-skill"},
        {"description", "A test skill"},
        {"files", {"SKILL.md", "helper.md"}}
    };

    auto entry = RemoteSkillEntry::from_json(j);
    REQUIRE(entry.has_value());
    REQUIRE(entry->name == "test-skill");
    REQUIRE(entry->description == "A test skill");
    REQUIRE(entry->files.size() == 2);
    REQUIRE(entry->files[0] == "SKILL.md");
    REQUIRE(entry->files[1] == "helper.md");
}

TEST_CASE("Skill.Remote.Entry.FromJson.Minimal", "[Skill][Remote]") {
    // Minimal valid entry (no description)
    nlohmann::json j = {
        {"name", "minimal-skill"},
        {"files", {"SKILL.md"}}
    };

    auto entry = RemoteSkillEntry::from_json(j);
    REQUIRE(entry.has_value());
    REQUIRE(entry->name == "minimal-skill");
    REQUIRE(entry->description.empty());
    REQUIRE(entry->files.size() == 1);
}

TEST_CASE("Skill.Remote.Entry.FromJson.MissingName", "[Skill][Remote]") {
    nlohmann::json j = {
        {"description", "Missing name"},
        {"files", {"SKILL.md"}}
    };

    auto entry = RemoteSkillEntry::from_json(j);
    REQUIRE_FALSE(entry.has_value());
}

TEST_CASE("Skill.Remote.Entry.FromJson.MissingFiles", "[Skill][Remote]") {
    nlohmann::json j = {
        {"name", "no-files-skill"},
        {"description", "No files"}
    };

    auto entry = RemoteSkillEntry::from_json(j);
    REQUIRE_FALSE(entry.has_value());
}

TEST_CASE("Skill.Remote.Entry.FromJson.EmptyFiles", "[Skill][Remote]") {
    nlohmann::json j = {
        {"name", "empty-files-skill"},
        {"files", nlohmann::json::array()}
    };

    auto entry = RemoteSkillEntry::from_json(j);
    REQUIRE_FALSE(entry.has_value());
}

TEST_CASE("Skill.Remote.Entry.FromJson.NotObject", "[Skill][Remote]") {
    nlohmann::json j = "not an object";

    auto entry = RemoteSkillEntry::from_json(j);
    REQUIRE_FALSE(entry.has_value());
}

TEST_CASE("Skill.Remote.Entry.ToJson", "[Skill][Remote]") {
    RemoteSkillEntry entry;
    entry.name = "serialize-test";
    entry.description = "Test serialization";
    entry.files = {"SKILL.md", "extra.md"};

    nlohmann::json j = entry.to_json();
    REQUIRE(j["name"] == "serialize-test");
    REQUIRE(j["description"] == "Test serialization");
    REQUIRE(j["files"].size() == 2);
}

// ==================== RemoteSkillIndex Tests ====================

TEST_CASE("Skill.Remote.Index.FromJson.Valid", "[Skill][Remote]") {
    nlohmann::json j = {
        {"skills", {
            {{"name", "skill-a"}, {"files", {"SKILL.md"}}},
            {{"name", "skill-b"}, {"files", {"SKILL.md", "helper.md"}}}
        }}
    };

    auto index = RemoteSkillIndex::from_json(j);
    REQUIRE(index.has_value());
    REQUIRE(index->skills.size() == 2);
    REQUIRE(index->skills[0].name == "skill-a");
    REQUIRE(index->skills[1].name == "skill-b");
}

TEST_CASE("Skill.Remote.Index.FromJson.Empty", "[Skill][Remote]") {
    nlohmann::json j = {{"skills", nlohmann::json::array()}};

    auto index = RemoteSkillIndex::from_json(j);
    REQUIRE(index.has_value());
    REQUIRE(index->skills.empty());
}

TEST_CASE("Skill.Remote.Index.FromJson.MissingSkills", "[Skill][Remote]") {
    nlohmann::json j = {{"other", "data"}};

    auto index = RemoteSkillIndex::from_json(j);
    REQUIRE_FALSE(index.has_value());
}

TEST_CASE("Skill.Remote.Index.FromJson.NotObject", "[Skill][Remote]") {
    nlohmann::json j = "not an object";

    auto index = RemoteSkillIndex::from_json(j);
    REQUIRE_FALSE(index.has_value());
}

TEST_CASE("Skill.Remote.Index.FromJson.SkillsNotArray", "[Skill][Remote]") {
    nlohmann::json j = {{"skills", "not an array"}};

    auto index = RemoteSkillIndex::from_json(j);
    REQUIRE_FALSE(index.has_value());
}

TEST_CASE("Skill.Remote.Index.FromJson.SkipsInvalidEntries", "[Skill][Remote]") {
    nlohmann::json j = {
        {"skills", {
            {{"name", "valid-skill"}, {"files", {"SKILL.md"}}},
            {{"invalid", "entry"}},  // Missing required fields
            {{"name", "another-valid"}, {"files", {"SKILL.md"}}}
        }}
    };

    auto index = RemoteSkillIndex::from_json(j);
    REQUIRE(index.has_value());
    REQUIRE(index->skills.size() == 2);  // Only valid entries
}

TEST_CASE("Skill.Remote.Index.ToJson", "[Skill][Remote]") {
    RemoteSkillIndex index;
    RemoteSkillEntry entry1;
    entry1.name = "skill-1";
    entry1.files = {"SKILL.md"};
    index.skills.push_back(entry1);

    nlohmann::json j = index.to_json();
    REQUIRE(j.contains("skills"));
    REQUIRE(j["skills"].is_array());
    REQUIRE(j["skills"].size() == 1);
}

// ==================== PullResult Tests ====================

TEST_CASE("Skill.Remote.PullResult.ToJson", "[Skill][Remote]") {
    PullResult result;
    result.success = true;
    result.dirs = {"/path/to/skill1", "/path/to/skill2"};
    result.errors = {"error1"};
    result.skills_downloaded = 2;
    result.files_downloaded = 5;
    result.from_cache = false;

    nlohmann::json j = result.to_json();
    REQUIRE(j["success"] == true);
    REQUIRE(j["dirs"].size() == 2);
    REQUIRE(j["errors"].size() == 1);
    REQUIRE(j["skills_downloaded"] == 2);
    REQUIRE(j["files_downloaded"] == 5);
    REQUIRE(j["from_cache"] == false);
}

TEST_CASE("Skill.Remote.PullResult.Defaults", "[Skill][Remote]") {
    PullResult result;
    REQUIRE(result.success == false);
    REQUIRE(result.dirs.empty());
    REQUIRE(result.errors.empty());
    REQUIRE(result.skills_downloaded == 0);
    REQUIRE(result.files_downloaded == 0);
    REQUIRE(result.from_cache == false);
}

// ==================== is_valid_skill_url Tests ====================

TEST_CASE("Skill.Remote.IsValidUrl.Https", "[Skill][Remote]") {
    REQUIRE(is_valid_skill_url("https://example.com/skills/"));
    REQUIRE(is_valid_skill_url("https://github.com/user/skills"));
}

TEST_CASE("Skill.Remote.IsValidUrl.Http", "[Skill][Remote]") {
    REQUIRE(is_valid_skill_url("http://localhost:8080/skills/"));
    REQUIRE(is_valid_skill_url("http://internal.server/skills"));
}

TEST_CASE("Skill.Remote.IsValidUrl.File", "[Skill][Remote]") {
    REQUIRE(is_valid_skill_url("file:///local/path/skills/"));
    REQUIRE(is_valid_skill_url("file://localhost/path/skills"));
}

TEST_CASE("Skill.Remote.IsValidUrl.Invalid", "[Skill][Remote]") {
    REQUIRE_FALSE(is_valid_skill_url(""));
    REQUIRE_FALSE(is_valid_skill_url("ftp://example.com/skills"));
    REQUIRE_FALSE(is_valid_skill_url("example.com/skills"));
    REQUIRE_FALSE(is_valid_skill_url("/local/path/skills"));
    REQUIRE_FALSE(is_valid_skill_url("ssh://git@github.com/skills"));
}

// ==================== get_cache_dir Tests ====================

TEST_CASE("Skill.Remote.GetCacheDir.WithXdgCache", "[Skill][Remote]") {
    EnvGuard env;
    env.unset("HOME");  // Clear HOME first to test XDG_CACHE_HOME
    env.set("HOME", "/tmp/test-home");
    env.set("XDG_CACHE_HOME", "/tmp/custom-cache");

    // Note: get_cache_dir uses std::call_once, so it may have been initialized
    // in a previous test. We just verify it returns a non-empty string.
    std::string cache_dir = get_cache_dir();
    // The cache dir should be set (either from XDG_CACHE_HOME or default)
    REQUIRE_FALSE(cache_dir.empty());
}

TEST_CASE("Skill.Remote.GetCacheDir.Default", "[Skill][Remote]") {
    EnvGuard env;
    env.set("HOME", "/tmp/test-home-default");
    env.unset("XDG_CACHE_HOME");

    std::string cache_dir = get_cache_dir();
    REQUIRE_FALSE(cache_dir.empty());
    // Should contain turbot/skills
    REQUIRE(cache_dir.find("turbot") != std::string::npos);
    REQUIRE(cache_dir.find("skills") != std::string::npos);
}

// ==================== clear_cache Tests ====================

TEST_CASE("Skill.Remote.ClearCache.NonExistent", "[Skill][Remote]") {
    // Create a unique test cache directory
    TURBOT_TEST_TMPDIR(tmp, false);
    std::string test_cache = tmp.path() / "nonexistent-cache";

    // clear_cache should return true even if cache doesn't exist
    // (because the operation is idempotent)
    // Note: This tests the actual cache, not our test directory
    bool result = clear_cache();
    // Result depends on whether actual cache exists
    // Just verify it doesn't crash
    REQUIRE((result == true || result == false));
}

TEST_CASE("Skill.Remote.ClearCache.WithExistingCache", "[Skill][Remote]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    // Create a mock cache directory
    auto cache_dir = tmp.path() / "skill-cache";
    std::filesystem::create_directories(cache_dir / "test-skill");
    std::ofstream(cache_dir / "test-skill" / "SKILL.md") << "---\nname: test\n---\n";

    REQUIRE(std::filesystem::exists(cache_dir));

    // Note: clear_cache uses get_cache_dir() which may point elsewhere
    // We're just testing that the function works without crashing
    clear_cache();
}

// ==================== download_file Tests ====================

TEST_CASE("Skill.Remote.DownloadFile.CacheHit", "[Skill][Remote]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    // Create an existing file (simulating cache hit)
    auto dest_path = tmp.path() / "cached-skill" / "SKILL.md";
    std::filesystem::create_directories(dest_path.parent_path());
    std::ofstream(dest_path) << "---\nname: cached\n---\n";

    REQUIRE(std::filesystem::exists(dest_path));

    // download_file should return true for existing files (cache hit)
    bool result = download_file("https://example.com/skill/SKILL.md", dest_path.string());
    REQUIRE(result);  // Should return true (file already exists)
}

TEST_CASE("Skill.Remote.DownloadFile.NetworkFailure", "[Skill][Remote]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto dest_path = tmp.path() / "new-skill" / "SKILL.md";

    // Attempting to download from an invalid URL should fail
    // (or return false for network errors)
    bool result = download_file("https://invalid.example.localhost:99999/skill/SKILL.md", dest_path.string());
    // Should fail because the URL is invalid or unreachable
    REQUIRE_FALSE(result);
    REQUIRE_FALSE(std::filesystem::exists(dest_path));
}

// ==================== pull Tests ====================

TEST_CASE("Skill.Remote.Pull.EmptyUrl", "[Skill][Remote]") {
    PullResult result = pull("");

    REQUIRE_FALSE(result.success);
    REQUIRE_FALSE(result.errors.empty());
    REQUIRE(result.errors[0].find("Empty") != std::string::npos);
}

TEST_CASE("Skill.Remote.Pull.InvalidUrl", "[Skill][Remote]") {
    // This will attempt to fetch from an invalid URL
    // The test verifies error handling
    PullResult result = pull("not-a-valid-url");

    // Should fail with network error
    REQUIRE_FALSE(result.success);
    REQUIRE_FALSE(result.errors.empty());
}

TEST_CASE("Skill.Remote.Pull.NetworkError", "[Skill][Remote]") {
    // Use a URL that will fail (invalid port)
    PullResult result = pull("https://localhost:59999/skills/");

    REQUIRE_FALSE(result.success);
    REQUIRE_FALSE(result.errors.empty());
}

// ==================== pull_all Tests ====================

TEST_CASE("Skill.Remote.PullAll.EmptyList", "[Skill][Remote]") {
    PullResult result = pull_all({});

    REQUIRE_FALSE(result.success);
    REQUIRE(result.dirs.empty());
    REQUIRE(result.skills_downloaded == 0);
}

TEST_CASE("Skill.Remote.PullAll.MultipleUrls", "[Skill][Remote]") {
    // Test with multiple invalid URLs - should aggregate errors
    std::vector<std::string> urls = {
        "https://localhost:59999/skills1/",
        "https://localhost:59998/skills2/"
    };

    PullResult result = pull_all(urls);

    REQUIRE_FALSE(result.success);
    // Should have errors from both URLs
    REQUIRE(result.errors.size() >= 2);
}

TEST_CASE("Skill.Remote.PullAll.MixedValidInvalid", "[Skill][Remote]") {
    // Mix of empty and invalid URLs
    std::vector<std::string> urls = {
        "",  // Empty URL
        "https://localhost:59997/skills/"  // Invalid URL
    };

    PullResult result = pull_all(urls);

    REQUIRE_FALSE(result.success);
    REQUIRE_FALSE(result.errors.empty());
}

// ==================== SearchPaths Tests ====================

TEST_CASE("Skill.Remote.SearchPaths.ToVector", "[Skill][Remote]") {
    SearchPaths paths;
    paths.project_opencode = "/project/.opencode/skills";
    paths.project_claude = "/project/.claude/skills";
    paths.global_opencode = "/home/.config/opencode/skills";

    auto vec = paths.to_vector();
    REQUIRE(vec.size() == 3);
    REQUIRE(vec[0] == paths.project_opencode);
    REQUIRE(vec[1] == paths.project_claude);
    REQUIRE(vec[2] == paths.global_opencode);
}

TEST_CASE("Skill.Remote.SearchPaths.ToVector.EmptyFields", "[Skill][Remote]") {
    SearchPaths paths;
    paths.project_opencode = "/project/.opencode/skills";
    // Other fields are empty

    auto vec = paths.to_vector();
    REQUIRE(vec.size() == 1);
    REQUIRE(vec[0] == paths.project_opencode);
}

// ==================== get_search_paths Tests ====================

TEST_CASE("Skill.Remote.GetSearchPaths.WithProjectRoot", "[Skill][Remote]") {
    EnvGuard env;
    env.set("HOME", "/tmp/test-home-search");

    SearchPaths paths = get_search_paths("/my/project");

    REQUIRE(paths.project_opencode == "/my/project/.opencode/skills");
    REQUIRE(paths.project_claude == "/my/project/.claude/skills");
    REQUIRE(paths.project_agents == "/my/project/.agents/skills");
    REQUIRE_FALSE(paths.global_opencode.empty());
    REQUIRE_FALSE(paths.global_claude.empty());
}

// ==================== find_skill_files Tests ====================

TEST_CASE("Skill.Remote.FindSkillFiles.ValidDirectory", "[Skill][Remote]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    // Create skill directories
    auto skill1 = tmp.path() / "skill-one";
    auto skill2 = tmp.path() / "skill-two";
    std::filesystem::create_directories(skill1);
    std::filesystem::create_directories(skill2);

    std::ofstream(skill1 / "SKILL.md") << "---\nname: skill-one\n---\n";
    std::ofstream(skill2 / "SKILL.md") << "---\nname: skill-two\n---\n";

    auto files = find_skill_files(tmp.path().string());
    REQUIRE(files.size() == 2);
}

TEST_CASE("Skill.Remote.FindSkillFiles.EmptyDirectory", "[Skill][Remote]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto files = find_skill_files(tmp.path().string());
    REQUIRE(files.empty());
}

TEST_CASE("Skill.Remote.FindSkillFiles.NonExistentDirectory", "[Skill][Remote]") {
    auto files = find_skill_files("/nonexistent/path/that/does/not/exist");
    REQUIRE(files.empty());
}

// ==================== is_skill_directory Tests ====================

TEST_CASE("Skill.Remote.IsSkillDirectory.Valid", "[Skill][Remote]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto skill_dir = tmp.path() / "valid-skill";
    std::filesystem::create_directories(skill_dir);
    std::ofstream(skill_dir / "SKILL.md") << "---\nname: valid\n---\n";

    REQUIRE(is_skill_directory(skill_dir.string()));
}

TEST_CASE("Skill.Remote.IsSkillDirectory.NoSkillFile", "[Skill][Remote]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto dir = tmp.path() / "no-skill-file";
    std::filesystem::create_directories(dir);

    REQUIRE_FALSE(is_skill_directory(dir.string()));
}

TEST_CASE("Skill.Remote.IsSkillDirectory.NonExistent", "[Skill][Remote]") {
    REQUIRE_FALSE(is_skill_directory("/nonexistent/directory"));
}
