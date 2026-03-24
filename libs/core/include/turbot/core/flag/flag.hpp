#pragma once

#include <turbot/core/common/export.hpp>
#include <cstdlib>
#include <optional>
#include <string>

namespace turbot::core::flag {

/// Feature flags for turbot-ai.
///
/// Mirrors OpenCode flag/flag.ts:  environment variables that control runtime
/// behaviour.  All flags are read at access time (not module-load time) so that
/// test harnesses can set env vars after the binary starts.
///
/// Boolean flags:
///   - truthy: env value is "true" or "1" (case-insensitive)
///   - falsy:  env value is "false" or "0" (case-insensitive)
///
/// String flags: raw env value, or nullopt when unset.
namespace Flag {

// ─── helpers ──────────────────────────────────────────────────────────────────

/// Returns true if the environment variable is set to "true" or "1".
[[nodiscard]] inline bool truthy(const char* key) noexcept {
    const char* v = std::getenv(key);
    if (!v) return false;
    return (v[0] == '1' && v[1] == '\0') ||
           ((v[0] == 't' || v[0] == 'T') &&  // "true"
            (v[1] == 'r' || v[1] == 'R') &&
            (v[2] == 'u' || v[2] == 'U') &&
            (v[3] == 'e' || v[3] == 'E') &&
            v[4] == '\0');
}

/// Returns true if the environment variable is set to "false" or "0".
[[nodiscard]] inline bool falsy(const char* key) noexcept {
    const char* v = std::getenv(key);
    if (!v) return false;
    return (v[0] == '0' && v[1] == '\0') ||
           ((v[0] == 'f' || v[0] == 'F') &&  // "false"
            (v[1] == 'a' || v[1] == 'A') &&
            (v[2] == 'l' || v[2] == 'L') &&
            (v[3] == 's' || v[3] == 'S') &&
            (v[4] == 'e' || v[4] == 'E') &&
            v[5] == '\0');
}

/// Returns the raw env value as optional<string>.
[[nodiscard]] inline std::optional<std::string> str(const char* key) {
    const char* v = std::getenv(key);
    if (!v) return std::nullopt;
    return std::string(v);
}

// ─── string flags ─────────────────────────────────────────────────────────────

[[nodiscard]] inline std::optional<std::string> OPENCODE_GIT_BASH_PATH()   { return str("OPENCODE_GIT_BASH_PATH"); }
[[nodiscard]] inline std::optional<std::string> OPENCODE_CONFIG()           { return str("OPENCODE_CONFIG"); }
[[nodiscard]] inline std::optional<std::string> OPENCODE_CONFIG_CONTENT()   { return str("OPENCODE_CONFIG_CONTENT"); }
[[nodiscard]] inline std::optional<std::string> OPENCODE_PERMISSION()       { return str("OPENCODE_PERMISSION"); }
[[nodiscard]] inline std::optional<std::string> OPENCODE_SERVER_PASSWORD()  { return str("OPENCODE_SERVER_PASSWORD"); }
[[nodiscard]] inline std::optional<std::string> OPENCODE_SERVER_USERNAME()  { return str("OPENCODE_SERVER_USERNAME"); }
[[nodiscard]] inline std::optional<std::string> OPENCODE_FAKE_VCS()         { return str("OPENCODE_FAKE_VCS"); }
[[nodiscard]] inline std::optional<std::string> OPENCODE_MODELS_URL()       { return str("OPENCODE_MODELS_URL"); }
[[nodiscard]] inline std::optional<std::string> OPENCODE_MODELS_PATH()      { return str("OPENCODE_MODELS_PATH"); }
[[nodiscard]] inline std::optional<std::string> OPENCODE_DB()               { return str("OPENCODE_DB"); }

/// The active client identifier (default "cli").
[[nodiscard]] inline std::string OPENCODE_CLIENT() {
    const char* v = std::getenv("OPENCODE_CLIENT");
    return v ? std::string(v) : "cli";
}

// ─── boolean flags ─────────────────────────────────────────────────────────────

[[nodiscard]] inline bool OPENCODE_AUTO_SHARE()                    { return truthy("OPENCODE_AUTO_SHARE"); }
[[nodiscard]] inline bool OPENCODE_DISABLE_AUTOUPDATE()            { return truthy("OPENCODE_DISABLE_AUTOUPDATE"); }
[[nodiscard]] inline bool OPENCODE_ALWAYS_NOTIFY_UPDATE()          { return truthy("OPENCODE_ALWAYS_NOTIFY_UPDATE"); }
[[nodiscard]] inline bool OPENCODE_DISABLE_PRUNE()                 { return truthy("OPENCODE_DISABLE_PRUNE"); }
[[nodiscard]] inline bool OPENCODE_DISABLE_TERMINAL_TITLE()        { return truthy("OPENCODE_DISABLE_TERMINAL_TITLE"); }
[[nodiscard]] inline bool OPENCODE_DISABLE_DEFAULT_PLUGINS()       { return truthy("OPENCODE_DISABLE_DEFAULT_PLUGINS"); }
[[nodiscard]] inline bool OPENCODE_DISABLE_LSP_DOWNLOAD()          { return truthy("OPENCODE_DISABLE_LSP_DOWNLOAD"); }
[[nodiscard]] inline bool OPENCODE_ENABLE_EXPERIMENTAL_MODELS()    { return truthy("OPENCODE_ENABLE_EXPERIMENTAL_MODELS"); }
[[nodiscard]] inline bool OPENCODE_DISABLE_AUTOCOMPACT()           { return truthy("OPENCODE_DISABLE_AUTOCOMPACT"); }
[[nodiscard]] inline bool OPENCODE_DISABLE_MODELS_FETCH()          { return truthy("OPENCODE_DISABLE_MODELS_FETCH"); }
[[nodiscard]] inline bool OPENCODE_DISABLE_CHANNEL_DB()            { return truthy("OPENCODE_DISABLE_CHANNEL_DB"); }
[[nodiscard]] inline bool OPENCODE_SKIP_MIGRATIONS()               { return truthy("OPENCODE_SKIP_MIGRATIONS"); }
[[nodiscard]] inline bool OPENCODE_STRICT_CONFIG_DEPS()            { return truthy("OPENCODE_STRICT_CONFIG_DEPS"); }
[[nodiscard]] inline bool OPENCODE_ENABLE_QUESTION_TOOL()          { return truthy("OPENCODE_ENABLE_QUESTION_TOOL"); }

// Dynamic getter (evaluated at access time for test-harness compatibility)
[[nodiscard]] inline bool OPENCODE_DISABLE_PROJECT_CONFIG()        { return truthy("OPENCODE_DISABLE_PROJECT_CONFIG"); }
[[nodiscard]] inline bool OPENCODE_DISABLE_CLAUDE_CODE()           { return truthy("OPENCODE_DISABLE_CLAUDE_CODE"); }
[[nodiscard]] inline bool OPENCODE_DISABLE_CLAUDE_CODE_PROMPT() {
    return OPENCODE_DISABLE_CLAUDE_CODE() || truthy("OPENCODE_DISABLE_CLAUDE_CODE_PROMPT");
}
[[nodiscard]] inline bool OPENCODE_DISABLE_CLAUDE_CODE_SKILLS() {
    return OPENCODE_DISABLE_CLAUDE_CODE() || truthy("OPENCODE_DISABLE_CLAUDE_CODE_SKILLS");
}
[[nodiscard]] inline bool OPENCODE_DISABLE_EXTERNAL_SKILLS() {
    return OPENCODE_DISABLE_CLAUDE_CODE_SKILLS() || truthy("OPENCODE_DISABLE_EXTERNAL_SKILLS");
}

// ─── experimental flags ───────────────────────────────────────────────────────

[[nodiscard]] inline bool OPENCODE_EXPERIMENTAL()                  { return truthy("OPENCODE_EXPERIMENTAL"); }
[[nodiscard]] inline bool OPENCODE_EXPERIMENTAL_FILEWATCHER() {
    return truthy("OPENCODE_EXPERIMENTAL_FILEWATCHER");
}
[[nodiscard]] inline bool OPENCODE_EXPERIMENTAL_DISABLE_FILEWATCHER() {
    return truthy("OPENCODE_EXPERIMENTAL_DISABLE_FILEWATCHER");
}
[[nodiscard]] inline bool OPENCODE_EXPERIMENTAL_ICON_DISCOVERY() {
    return OPENCODE_EXPERIMENTAL() || truthy("OPENCODE_EXPERIMENTAL_ICON_DISCOVERY");
}
[[nodiscard]] inline bool OPENCODE_EXPERIMENTAL_LSP_TY()           { return truthy("OPENCODE_EXPERIMENTAL_LSP_TY"); }
[[nodiscard]] inline bool OPENCODE_EXPERIMENTAL_LSP_TOOL() {
    return OPENCODE_EXPERIMENTAL() || truthy("OPENCODE_EXPERIMENTAL_LSP_TOOL");
}
[[nodiscard]] inline bool OPENCODE_EXPERIMENTAL_PLAN_MODE() {
    return OPENCODE_EXPERIMENTAL() || truthy("OPENCODE_EXPERIMENTAL_PLAN_MODE");
}
[[nodiscard]] inline bool OPENCODE_EXPERIMENTAL_WORKSPACES() {
    return OPENCODE_EXPERIMENTAL() || truthy("OPENCODE_EXPERIMENTAL_WORKSPACES");
}
[[nodiscard]] inline bool OPENCODE_EXPERIMENTAL_MARKDOWN()         { return !falsy("OPENCODE_EXPERIMENTAL_MARKDOWN"); }
[[nodiscard]] inline bool OPENCODE_EXPERIMENTAL_OXFMT() {
    return OPENCODE_EXPERIMENTAL() || truthy("OPENCODE_EXPERIMENTAL_OXFMT");
}
[[nodiscard]] inline bool OPENCODE_ENABLE_EXA() {
    return truthy("OPENCODE_ENABLE_EXA") || OPENCODE_EXPERIMENTAL() || truthy("OPENCODE_EXPERIMENTAL_EXA");
}
[[nodiscard]] inline bool OPENCODE_DISABLE_FILETIME_CHECK()        { return truthy("OPENCODE_DISABLE_FILETIME_CHECK"); }

/// Optional numeric flags — return nullopt when unset or non-positive.
[[nodiscard]] inline std::optional<int> OPENCODE_EXPERIMENTAL_BASH_DEFAULT_TIMEOUT_MS() {
    const char* v = std::getenv("OPENCODE_EXPERIMENTAL_BASH_DEFAULT_TIMEOUT_MS");
    if (!v) return std::nullopt;
    int n = std::atoi(v);
    return (n > 0) ? std::optional<int>(n) : std::nullopt;
}
[[nodiscard]] inline std::optional<int> OPENCODE_EXPERIMENTAL_OUTPUT_TOKEN_MAX() {
    const char* v = std::getenv("OPENCODE_EXPERIMENTAL_OUTPUT_TOKEN_MAX");
    if (!v) return std::nullopt;
    int n = std::atoi(v);
    return (n > 0) ? std::optional<int>(n) : std::nullopt;
}

/// OPENCODE_EXPERIMENTAL_DISABLE_COPY_ON_SELECT:
///   unset → default true on win32, false elsewhere;
///   set    → honour the truthy/falsy value.
[[nodiscard]] inline bool OPENCODE_EXPERIMENTAL_DISABLE_COPY_ON_SELECT() noexcept {
    const char* v = std::getenv("OPENCODE_EXPERIMENTAL_DISABLE_COPY_ON_SELECT");
    if (!v) {
#ifdef _WIN32
        return true;
#else
        return false;
#endif
    }
    return truthy("OPENCODE_EXPERIMENTAL_DISABLE_COPY_ON_SELECT");
}

} // namespace Flag
} // namespace turbot::core::flag
