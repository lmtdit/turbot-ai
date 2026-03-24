// global.cpp - Global application paths implementation
//
// Mirrors: opencode/packages/opencode/src/global/index.ts
//
// XDG base directory spec:
//   https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
//
// Defaults (Linux/macOS):
//   XDG_DATA_HOME   = $HOME/.local/share
//   XDG_CONFIG_HOME = $HOME/.config
//   XDG_CACHE_HOME  = $HOME/.cache
//   XDG_STATE_HOME  = $HOME/.local/state
//
// On macOS, $HOME/.local/share etc. are conventional when no custom XDG vars are set.

#include <turbot/core/global/global.hpp>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <cstdlib>

namespace turbot::core::global {

namespace {

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helper: resolve XDG directory with fallback
// ---------------------------------------------------------------------------
std::string xdg_dir(const char* env_var, const std::string& home, const char* fallback_subpath) {
    const char* val = std::getenv(env_var);
    if (val && val[0] != '\0') {
        return std::string(val);
    }
    return home + fallback_subpath;
}

// ---------------------------------------------------------------------------
// Helper: get home directory
// Checks OPENCODE_TEST_HOME first (test isolation), then HOME / USERPROFILE
// ---------------------------------------------------------------------------
std::string resolve_home() {
    // Allow test override — mirrors OpenCode's process.env.OPENCODE_TEST_HOME
    const char* test_home = std::getenv("OPENCODE_TEST_HOME");
    if (test_home && test_home[0] != '\0') {
        return std::string(test_home);
    }
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return std::string(home);
    }
    // Windows fallback
    const char* userprofile = std::getenv("USERPROFILE");
    if (userprofile && userprofile[0] != '\0') {
        return std::string(userprofile);
    }
    throw std::runtime_error("Cannot determine home directory: HOME / USERPROFILE / OPENCODE_TEST_HOME not set");
}

// ---------------------------------------------------------------------------
// Helper: create directory, ignore errors
// ---------------------------------------------------------------------------
void ensure_dir(const std::string& path) {
    std::error_code ec;
    fs::create_directories(path, ec);
    // Ignore errors — best effort, consistent with OpenCode's fs.mkdir({recursive:true})
}

// ---------------------------------------------------------------------------
// Singleton storage
// ---------------------------------------------------------------------------
std::mutex          g_mutex;
std::optional<Global::Path> g_path;

// ---------------------------------------------------------------------------
// Cache version management
// Mirrors OpenCode:
//   if (version !== CACHE_VERSION) { rm cache/* ; write version file }
// ---------------------------------------------------------------------------
void manage_cache_version(const std::string& cache_path) {
    const std::string version_file = cache_path + "/version";

    // Read existing version
    std::string existing_version;
    {
        std::ifstream f(version_file);
        if (f.is_open()) {
            std::getline(f, existing_version);
        }
    }

    if (existing_version != Global::CACHE_VERSION) {
        // Wipe cache contents (excluding the version file itself)
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(cache_path, ec)) {
            fs::remove_all(entry.path(), ec);
        }
        // Write new version
        std::ofstream f(version_file, std::ios::trunc);
        if (f.is_open()) {
            f << Global::CACHE_VERSION;
        }
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

const Global::Path& Global::init() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_path) return *g_path;

    Global::Path p;

    p.home   = resolve_home();

    const std::string xdg_data   = xdg_dir("XDG_DATA_HOME",   p.home, "/.local/share");
    const std::string xdg_config = xdg_dir("XDG_CONFIG_HOME", p.home, "/.config");
    const std::string xdg_cache  = xdg_dir("XDG_CACHE_HOME",  p.home, "/.cache");
    const std::string xdg_state  = xdg_dir("XDG_STATE_HOME",  p.home, "/.local/state");

    p.data   = xdg_data   + "/opencode";
    p.config = xdg_config + "/opencode";
    p.cache  = xdg_cache  + "/opencode";
    p.state  = xdg_state  + "/opencode";
    p.bin    = p.cache + "/bin";
    p.log    = p.data  + "/log";

    // Create all required directories (mirrors OpenCode's Promise.all([mkdir...]))
    ensure_dir(p.data);
    ensure_dir(p.config);
    ensure_dir(p.state);
    ensure_dir(p.log);
    ensure_dir(p.cache);
    ensure_dir(p.bin);

    // Cache version management
    manage_cache_version(p.cache);

    g_path = p;
    return *g_path;
}

const Global::Path& Global::path() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_path) {
        throw std::logic_error("Global::init() must be called before Global::path()");
    }
    return *g_path;
}

} // namespace turbot::core::global
