/**
 * @file project_id_test.cpp
 * @brief Tests for Project::generate_id() — OpenCode alignment
 *
 * Verifies that generate_id() follows the OpenCode Project.fromDirectory()
 * algorithm: cached id (.git/opencode) → git rev-list --max-parents=0 HEAD
 * → GLOBAL_ID fallback.
 */

#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/project/project.hpp>
#include <filesystem>
#include <fstream>
#include <cstdlib>

namespace fs = std::filesystem;
using namespace turbot::core::project;

// ---------------------------------------------------------------------------
// Helper: run a shell command in a directory, return exit code
// ---------------------------------------------------------------------------
static int sh(const std::string& cmd, const std::string& cwd) {
    std::string full = "cd " + cwd + " && " + cmd + " > /dev/null 2>&1";
    return std::system(full.c_str());
}

// ---------------------------------------------------------------------------
// Helper: initialize a real git repo with one commit in tmp_dir
// Returns false if git is not available.
// ---------------------------------------------------------------------------
static bool init_git_repo(const std::string& dir) {
    if (sh("which git", ".") != 0) return false;
    if (sh("git init -q", dir) != 0) return false;
    if (sh("git config user.email 'test@test.com'", dir) != 0) return false;
    if (sh("git config user.name 'Test'", dir) != 0) return false;
    // Create an initial commit so HEAD exists
    sh("touch .gitkeep && git add .gitkeep && git commit -q -m 'init'", dir);
    return true;
}

// ---------------------------------------------------------------------------

TEST_CASE("Project.GenerateId.NoGit", "[Project]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    // A plain directory with no .git → should fall back to GLOBAL_ID.
    std::string id = Project::from_directory(tmp.path().string()).project.id;
    REQUIRE(id == Project::GLOBAL_ID);
}

TEST_CASE("Project.GenerateId.GitRepo", "[Project]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    if (!init_git_repo(tmp.path().string())) {
        SKIP("git binary not available");
    }

    // First call: should derive ID from root commit hash and cache it.
    std::string id = Project::from_directory(tmp.path().string()).project.id;

    // ID must not be GLOBAL and must look like a 40-char hex sha1.
    REQUIRE(id != Project::GLOBAL_ID);
    REQUIRE(id.size() == 40);
    // Verify only hex chars.
    for (char c : id) {
        bool hex_char = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        REQUIRE(hex_char);
    }
}

TEST_CASE("Project.GenerateId.CachePersistence", "[Project]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    if (!init_git_repo(tmp.path().string())) {
        SKIP("git binary not available");
    }

    // First call derives and caches the ID.
    std::string id1 = Project::from_directory(tmp.path().string()).project.id;
    REQUIRE(id1 != Project::GLOBAL_ID);

    // Cache file must now exist at .git/opencode.
    fs::path cache_file = tmp.path() / ".git" / "opencode";
    REQUIRE(fs::exists(cache_file));

    // Read raw cache content.
    std::ifstream f(cache_file);
    std::string cached_id;
    std::getline(f, cached_id);
    REQUIRE(cached_id == id1);

    // Second call must return the same ID (from cache).
    std::string id2 = Project::from_directory(tmp.path().string()).project.id;
    REQUIRE(id2 == id1);
}

TEST_CASE("Project.GenerateId.ManualCache", "[Project]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    if (!init_git_repo(tmp.path().string())) {
        SKIP("git binary not available");
    }

    // Pre-populate the cache with a known ID.
    std::string expected_id = "aabbccddeeff00112233445566778899aabbccdd";
    fs::path cache_file = tmp.path() / ".git" / "opencode";
    {
        std::ofstream f(cache_file);
        f << expected_id << "\n";
    }

    // generate_id() must honour the cached value.
    std::string id = Project::from_directory(tmp.path().string()).project.id;
    REQUIRE(id == expected_id);
}

TEST_CASE("Project.GenerateId.Stability", "[Project]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    if (!init_git_repo(tmp.path().string())) {
        SKIP("git binary not available");
    }

    // Multiple calls must return identical IDs (stable across calls).
    std::string id1 = Project::from_directory(tmp.path().string()).project.id;
    std::string id2 = Project::from_directory(tmp.path().string()).project.id;
    std::string id3 = Project::from_directory(tmp.path().string()).project.id;
    REQUIRE(id1 == id2);
    REQUIRE(id2 == id3);
}

TEST_CASE("Project.FromDirectory.VcsIsGit", "[Project]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    if (!init_git_repo(tmp.path().string())) {
        SKIP("git binary not available");
    }

    auto result = Project::from_directory(tmp.path().string());
    REQUIRE(result.project.vcs == VcsType::Git);
}

TEST_CASE("Project.FromDirectory.TimestampMilliseconds", "[Project]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    // time.created should be in milliseconds (>1e12 for any post-2001 timestamp).
    auto result = Project::from_directory(tmp.path().string());
    // 2001-09-09T01:46:40Z = 1000000000 seconds = 1000000000000 ms
    REQUIRE(result.project.time.created > 1000000000000LL);
}
