// session_instruction.cpp - InstructionPrompt implementation
//
// Mirrors: opencode/packages/opencode/src/session/instruction.ts (InstructionPrompt)
//
// Key behaviours matched:
//   1. globalFiles()   — OPENCODE_CONFIG_DIR/AGENTS.md, Global.Path.config/AGENTS.md,
//                        ~/.claude/CLAUDE.md (unless OPENCODE_DISABLE_CLAUDE_CODE_PROMPT)
//   2. system_paths()  — glob-up FILES from Instance::directory() up to worktree,
//                        then globalFiles(), then config.instructions (file paths)
//   3. system()        — reads file contents + fetches http(s) URLs (5s timeout via
//                        turbot::network::HttpClient)
//   4. resolve()       — per-message claim map, walks from filepath dir up to root,
//                        finds first unclaimed AGENTS.md/CLAUDE.md/CONTEXT.md
//   5. find()          — check dir for any of FILES
//   6. clear()         — removes per-message claim entries

#include <turbot/core/session/session_instruction.hpp>
#include <turbot/core/global/global.hpp>
#include <turbot/core/project/instance.hpp>
#include <turbot/core/config/config.hpp>
#include <turbot/core/common/logger.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>

namespace turbot::core::session {

namespace {

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Instruction file names (in priority order — mirrors OpenCode's FILES array)
// ---------------------------------------------------------------------------
static constexpr const char* kFiles[] = {
    "AGENTS.md",
    "CLAUDE.md",
    "CONTEXT.md",  // deprecated but still supported
    nullptr
};

// ---------------------------------------------------------------------------
// Per-message claim state
// g_claims_mutex guards ALL accesses to g_claims (is_claimed, claim, clear).
// resolve() acquires the lock for the full duration of the claim-check loop to
// prevent data races when multiple threads call resolve() with different message_ids.
// ---------------------------------------------------------------------------
std::mutex                               g_claims_mutex;
std::map<std::string, std::set<std::string>> g_claims;  // message_id → {claimed paths}

// Must be called with g_claims_mutex held.
bool is_claimed_locked(const std::string& message_id, const std::string& path) {
    auto it = g_claims.find(message_id);
    if (it == g_claims.end()) return false;
    return it->second.count(path) > 0;
}

// Must be called with g_claims_mutex held.
void claim_locked(const std::string& message_id, const std::string& path) {
    g_claims[message_id].insert(path);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Read a text file; returns empty string on error.
std::string read_text(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

/// Return canonical absolute path for p, or p itself on error.
std::string canonical_path(const std::string& p) {
    std::error_code ec;
    auto c = fs::canonical(p, ec);
    if (ec) return p;
    return c.string();
}

/// Collect the global instruction file candidates (independent of project dir).
/// Mirrors OpenCode globalFiles().
std::vector<std::string> global_files() {
    std::vector<std::string> files;

    // OPENCODE_CONFIG_DIR/AGENTS.md (if set)
    const char* cfg_dir = std::getenv("OPENCODE_CONFIG_DIR");
    if (cfg_dir && cfg_dir[0] != '\0') {
        files.push_back(fs::path(cfg_dir) / "AGENTS.md");
    }

    // Global config dir AGENTS.md
    try {
        const auto& gp = turbot::core::global::Global::init();
        files.push_back(fs::path(gp.config) / "AGENTS.md");
    } catch (...) {}

    // ~/.claude/CLAUDE.md (unless disabled)
    const char* disable_claude = std::getenv("OPENCODE_DISABLE_CLAUDE_CODE_PROMPT");
    if (!disable_claude || disable_claude[0] == '\0') {
        const char* home = std::getenv("HOME");
        if (!home || home[0] == '\0') home = std::getenv("USERPROFILE");
        if (home && home[0] != '\0') {
            files.push_back(fs::path(home) / ".claude" / "CLAUDE.md");
        }
    }

    return files;
}

/// Return true if `s` is an http:// or https:// URL.
static bool is_url(const std::string& s) noexcept {
    return (s.size() >= 7 && s.compare(0, 7, "http://") == 0) ||
           (s.size() >= 8 && s.compare(0, 8, "https://") == 0);
}

/// Walk up from `start` toward `root`, returning the first directory that
/// contains any of kFiles[].  Returns the found path, or empty string.
/// Mirrors OpenCode Filesystem.findUp().
std::string find_up(const std::string& start, const std::string& root) {
    // Use canonical paths to handle symlinks (e.g. /tmp → /private/tmp on macOS).
    std::error_code ec1, ec2;
    fs::path current    = fs::canonical(start, ec1);
    if (ec1) current    = fs::path(start).lexically_normal();
    fs::path root_path  = fs::canonical(root, ec2);
    if (ec2) root_path  = fs::path(root).lexically_normal();

    // Ensure current is not above root
    while (true) {
        for (int i = 0; kFiles[i]; ++i) {
            auto candidate = current / kFiles[i];
            if (fs::exists(candidate)) {
                return canonical_path(candidate.string());
            }
        }
        // Move up
        if (current == root_path || current == current.parent_path()) break;
        current = current.parent_path();
    }
    return {};
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::string InstructionPrompt::find(const std::string& dir) {
    for (int i = 0; kFiles[i]; ++i) {
        auto path = fs::path(dir) / kFiles[i];
        if (fs::exists(path)) {
            return canonical_path(path.string());
        }
    }
    return {};
}

std::set<std::string> InstructionPrompt::system_paths() {
    std::set<std::string> paths;

    // 1. Project directory: find first matching instruction file walking up to worktree
    const char* disable_proj = std::getenv("OPENCODE_DISABLE_PROJECT_CONFIG");
    if (!disable_proj || disable_proj[0] == '\0') {
        try {
            const std::string dir = turbot::core::project::Instance::directory();
            const std::string worktree = turbot::core::project::Instance::worktree();
            const std::string found = find_up(dir, worktree);
            if (!found.empty()) {
                paths.insert(found);
            }
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("InstructionPrompt::system_paths: no Instance context ({})", e.what());
        }
    }

    // 2. Global files (config dir + ~/.claude/CLAUDE.md)
    for (const auto& f : global_files()) {
        if (fs::exists(f)) {
            paths.insert(canonical_path(f));
            break;  // First existing global file wins (mirrors OpenCode's early break)
        }
    }

    // 3. Config.instructions file paths (skip http(s)://, handle ~/  and absolute paths)
    try {
        const char* cfg_dir_env = std::getenv("OPENCODE_CONFIG_DIR");
        auto instructions_opt = turbot::core::Config::instance().get<std::vector<std::string>>("instructions");
        if (instructions_opt) {
            for (const auto& instr : *instructions_opt) {
                // Skip URLs (handled separately in system())
                if (is_url(instr)) continue;

                std::string expanded = instr;
                // Expand ~/ prefix
                if (expanded.size() >= 2 && expanded[0] == '~' && expanded[1] == '/') {
                    const char* home = std::getenv("HOME");
                    if (!home || home[0] == '\0') home = std::getenv("USERPROFILE");
                    if (home) expanded = std::string(home) + expanded.substr(1);
                }

                if (fs::path(expanded).is_absolute()) {
                    if (fs::exists(expanded)) {
                        paths.insert(canonical_path(expanded));
                    }
                } else if (cfg_dir_env && cfg_dir_env[0] != '\0') {
                    // Relative: resolve from OPENCODE_CONFIG_DIR or Instance::directory
                    auto abs = fs::path(cfg_dir_env) / expanded;
                    if (fs::exists(abs)) {
                        paths.insert(canonical_path(abs.string()));
                    }
                } else {
                    try {
                        auto abs = fs::path(turbot::core::project::Instance::directory()) / expanded;
                        if (fs::exists(abs)) {
                            paths.insert(canonical_path(abs.string()));
                        }
                    } catch (...) {}
                }
            }
        }
    } catch (...) {}

    return paths;
}

std::vector<std::string> InstructionPrompt::system() {
    const auto paths = system_paths();
    std::vector<std::string> results;
    results.reserve(paths.size() + 4);

    // Read file contents
    for (const auto& p : paths) {
        const std::string content = read_text(p);
        if (!content.empty()) {
            results.push_back("Instructions from: " + p + "\n" + content);
        }
    }

    // Fetch http(s) URLs from config.instructions
    try {
        auto instructions_opt = turbot::core::Config::instance().get<std::vector<std::string>>("instructions");
        if (instructions_opt) {
            for (const auto& instr : *instructions_opt) {
                if (!is_url(instr)) continue;
                // HTTP fetch is best-effort; turbot::network::HttpClient is async — for now
                // we skip URL fetching in the synchronous path (matches OpenCode's Promise.all
                // but without async runtime).
                // TODO(T17): integrate with HttpClient when async session loop is available.
                TURBOT_LOG_DEBUG("InstructionPrompt::system: skipping URL (no async runtime): {}", instr);
            }
        }
    } catch (...) {}

    return results;
}

std::set<std::string> InstructionPrompt::loaded(
    const std::vector<std::string>& read_tool_paths
) {
    // Mirrors OpenCode InstructionPrompt.loaded(): collects paths from read tool calls
    std::set<std::string> result;
    for (const auto& p : read_tool_paths) {
        if (!p.empty()) {
            result.insert(canonical_path(p));
        }
    }
    return result;
}

std::vector<InstructionPrompt::ResolvedInstruction> InstructionPrompt::resolve(
    const std::set<std::string>& already_loaded,
    const std::string& filepath,
    const std::string& message_id
) {
    std::vector<ResolvedInstruction> results;

    const auto sys = system_paths();

    try {
        const std::string root = turbot::core::project::Instance::directory();
        const fs::path root_path = fs::path(root).lexically_normal();

        // Resolve filepath's parent directory.  canonical_path() falls back to
        // the input string if the path does not exist on disk (e.g. a virtual
        // filepath), which can leave a symlink prefix (e.g. /tmp vs /private/tmp
        // on macOS).  We therefore resolve the *parent directory* — which must
        // exist — to get a canonical prefix, then re-attach the filename.
        fs::path file_path_obj = fs::path(filepath);
        fs::path parent_dir    = file_path_obj.parent_path();
        std::error_code ec_p;
        fs::path canon_parent  = fs::canonical(parent_dir, ec_p);
        if (ec_p) canon_parent = parent_dir.lexically_normal();

        fs::path current = canon_parent;

        // Acquire the claims lock for the full scan to prevent data races.
        std::lock_guard<std::mutex> lock(g_claims_mutex);

        // Walk from file's parent dir up to (and including) the instance root.
        // OpenCode scans subdirs AND the project root itself.
        while (current.string().find(root_path.string()) == 0) {
            const std::string found = find(current.string());

            if (!found.empty()) {
                const std::string canon = canonical_path(found);
                if (canon != canonical_path(filepath) &&
                    sys.count(canon) == 0 &&
                    already_loaded.count(canon) == 0 &&
                    !is_claimed_locked(message_id, canon))
                {
                    claim_locked(message_id, canon);
                    const std::string content = read_text(canon);
                    if (!content.empty()) {
                        results.push_back({canon, "Instructions from: " + canon + "\n" + content});
                    }
                }
            }

            auto parent = current.parent_path();
            if (parent == current) break;
            current = parent;
        }
    } catch (const std::exception& e) {
        TURBOT_LOG_DEBUG("InstructionPrompt::resolve: {}", e.what());
    }

    return results;
}

void InstructionPrompt::clear(const std::string& message_id) {
    std::lock_guard<std::mutex> lock(g_claims_mutex);
    g_claims.erase(message_id);
}

} // namespace turbot::core::session
