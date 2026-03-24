// session_instruction_test.cpp - Unit tests for InstructionPrompt
//
// Covers:
//   - find() locates instruction files
//   - system_paths() with OPENCODE_TEST_HOME isolated temp dir
//   - clear() removes claim state
//   - resolve() walks up directory tree
//   - loaded() collects read tool paths

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/session/session_instruction.hpp>
#include <turbot/core/project/instance.hpp>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace fs = std::filesystem;
using namespace turbot::core::session;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// RAII: set/restore environment variable
struct EnvGuard {
    std::string name;
    std::optional<std::string> old_val;

    EnvGuard(const char* n, const char* v) : name(n) {
        const char* cur = std::getenv(n);
        if (cur) old_val = std::string(cur);
        setenv(n, v, 1);
    }
    ~EnvGuard() {
        if (old_val) setenv(name.c_str(), old_val->c_str(), 1);
        else         unsetenv(name.c_str());
    }
};

/// Create a file with given content; parent directories are created as needed.
static void write_file(const fs::path& p, const std::string& content) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p);
    f << content;
}

// ---------------------------------------------------------------------------
// 1. find()
// ---------------------------------------------------------------------------

TEST_CASE("InstructionPrompt::find() returns path when AGENTS.md exists", "[instruction][find]") {
    auto tmp = fs::temp_directory_path() / "turbot_instr_find_test";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    write_file(tmp / "AGENTS.md", "# System Instructions\n");

    const std::string result = InstructionPrompt::find(tmp.string());
    REQUIRE_FALSE(result.empty());
    REQUIRE(fs::path(result).filename() == "AGENTS.md");
}

TEST_CASE("InstructionPrompt::find() returns empty when no file exists", "[instruction][find]") {
    auto tmp = fs::temp_directory_path() / "turbot_instr_find_empty";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    const std::string result = InstructionPrompt::find(tmp.string());
    REQUIRE(result.empty());
}

TEST_CASE("InstructionPrompt::find() returns CLAUDE.md when AGENTS.md absent", "[instruction][find]") {
    auto tmp = fs::temp_directory_path() / "turbot_instr_find_claude";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    write_file(tmp / "CLAUDE.md", "# Claude Instructions\n");

    const std::string result = InstructionPrompt::find(tmp.string());
    REQUIRE_FALSE(result.empty());
    REQUIRE(fs::path(result).filename() == "CLAUDE.md");
}

// ---------------------------------------------------------------------------
// 2. system_paths() with project config disabled (no Instance context needed)
// ---------------------------------------------------------------------------

TEST_CASE("InstructionPrompt::system_paths() with OPENCODE_DISABLE_PROJECT_CONFIG", "[instruction][system_paths]") {
    // Disable project config scanning (no Instance context required)
    EnvGuard disable_proj("OPENCODE_DISABLE_PROJECT_CONFIG", "1");
    EnvGuard disable_claude("OPENCODE_DISABLE_CLAUDE_CODE_PROMPT", "1");

    // Provide a custom config dir with an AGENTS.md
    auto tmp = fs::temp_directory_path() / "turbot_instr_syspaths";
    fs::remove_all(tmp);
    fs::create_directories(tmp);
    write_file(tmp / "AGENTS.md", "# Global Agents\n");

    EnvGuard cfg_dir("OPENCODE_CONFIG_DIR", tmp.string().c_str());

    const auto paths = InstructionPrompt::system_paths();

    // Should contain our custom AGENTS.md
    bool found = false;
    for (const auto& p : paths) {
        if (p.find("AGENTS.md") != std::string::npos &&
            p.find(tmp.string()) != std::string::npos) {
            found = true;
        }
    }
    REQUIRE(found);
}

// ---------------------------------------------------------------------------
// 3. system() with disabled project + custom config dir
// ---------------------------------------------------------------------------

TEST_CASE("InstructionPrompt::system() returns content strings", "[instruction][system]") {
    EnvGuard disable_proj("OPENCODE_DISABLE_PROJECT_CONFIG", "1");
    EnvGuard disable_claude("OPENCODE_DISABLE_CLAUDE_CODE_PROMPT", "1");

    auto tmp = fs::temp_directory_path() / "turbot_instr_system";
    fs::remove_all(tmp);
    fs::create_directories(tmp);
    write_file(tmp / "AGENTS.md", "You are a helpful assistant.\n");

    EnvGuard cfg_dir("OPENCODE_CONFIG_DIR", tmp.string().c_str());

    const auto result = InstructionPrompt::system();
    REQUIRE_FALSE(result.empty());
    // First element should start with "Instructions from:"
    REQUIRE(result[0].substr(0, 17) == "Instructions from");
    REQUIRE(result[0].find("You are a helpful") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 4. loaded()
// ---------------------------------------------------------------------------

TEST_CASE("InstructionPrompt::loaded() returns set of canonical paths", "[instruction][loaded]") {
    auto tmp = fs::temp_directory_path();
    const std::string p1 = (tmp / "file1.txt").string();
    const std::string p2 = (tmp / "file2.txt").string();

    const auto result = InstructionPrompt::loaded({p1, p2, ""});
    REQUIRE(result.size() == 2);
    REQUIRE(result.count(p1) == 1);
    REQUIRE(result.count(p2) == 1);
    // Empty string should not be included
    REQUIRE(result.count("") == 0);
}

// ---------------------------------------------------------------------------
// 5. clear() removes claim state
// ---------------------------------------------------------------------------

TEST_CASE("InstructionPrompt::clear() removes message claims", "[instruction][clear]") {
    // Calling clear on a non-existent message_id should not throw
    REQUIRE_NOTHROW(InstructionPrompt::clear("msg_nonexistent123456"));
    REQUIRE_NOTHROW(InstructionPrompt::clear(""));
}

// ---------------------------------------------------------------------------
// 6. resolve() with Instance context
// ---------------------------------------------------------------------------

TEST_CASE("InstructionPrompt::resolve() finds AGENTS.md in intermediate subdir", "[instruction][resolve]") {
    // Scenario:
    //   Instance directory = root/
    //   Instruction file   = root/sub/AGENTS.md   (intermediate, below root)
    //   Target file        = root/sub/src/foo.cpp
    //
    // system_paths() scans from Instance.directory() (root/) upward — it will NOT find
    // root/sub/AGENTS.md (find_up starts at root itself and goes up, not down).
    // resolve() walks from root/sub/src/ up to root/, so it WILL find root/sub/AGENTS.md.
    //
    // Use an empty OPENCODE_CONFIG_DIR to prevent global AGENTS.md contamination.
    EnvGuard disable_claude("OPENCODE_DISABLE_CLAUDE_CODE_PROMPT", "1");
    auto cfg_tmp = fs::temp_directory_path() / "turbot_instr_resolve_cfg";
    fs::remove_all(cfg_tmp);
    fs::create_directories(cfg_tmp);
    EnvGuard cfg_dir("OPENCODE_CONFIG_DIR", cfg_tmp.string().c_str());

    turbot::core::project::Instance::dispose_all();

    auto root    = fs::temp_directory_path() / "turbot_instr_resolve_root";
    auto sub     = root / "sub";
    auto sub_src = sub / "src";
    fs::remove_all(root);
    fs::create_directories(sub_src);

    // AGENTS.md lives in root/sub/, NOT in root/ itself.
    write_file(sub / "AGENTS.md", "# Sub Instructions\n");

    turbot::core::project::Instance::provide(root.string(), [&]{
        // Resolve for a file inside root/sub/src/
        const std::string target_file = (sub_src / "foo.cpp").string();
        const auto already = InstructionPrompt::loaded({});
        const std::string msg_id = "msg_test_resolve";

        const auto results = InstructionPrompt::resolve(already, target_file, msg_id);
        // Should find root/sub/AGENTS.md while walking up from root/sub/src/
        bool found = false;
        for (const auto& r : results) {
            if (r.filepath.find("AGENTS.md") != std::string::npos) {
                found = true;
                REQUIRE(r.content.find("Sub Instructions") != std::string::npos);
            }
        }
        REQUIRE(found);

        // Second call with same message_id should be empty (already claimed)
        const auto results2 = InstructionPrompt::resolve(already, target_file, msg_id);
        REQUIRE(results2.empty());

        InstructionPrompt::clear(msg_id);
    });
}
