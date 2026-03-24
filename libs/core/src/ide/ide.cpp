#include <turbot/core/ide/ide.hpp>
#include <turbot/core/common/logger.hpp>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/wait.h>

namespace turbot::core::ide {

// ---------------------------------------------------------------------------
// IDE table — mirrors OpenCode SUPPORTED_IDES
// ---------------------------------------------------------------------------
struct IdeEntry {
    IdeKind     kind;
    const char* name;
    const char* cmd;
};

static constexpr std::array<IdeEntry, 5> kSupportedIdes{{
    {IdeKind::Windsurf,                    "Windsurf",                          "windsurf"},
    {IdeKind::VisualStudioCodeInsiders,    "Visual Studio Code - Insiders",     "code-insiders"},
    {IdeKind::VisualStudioCode,            "Visual Studio Code",                "code"},
    {IdeKind::Cursor,                      "Cursor",                            "cursor"},
    {IdeKind::VSCodium,                    "VSCodium",                          "codium"},
}};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
static const char* safe_getenv(const char* key) noexcept {
    const char* v = std::getenv(key);
    return v ? v : "";
}

namespace Ide {

// ---------------------------------------------------------------------------
// detect() — mirrors OpenCode Ide.ide()
// Inspects $TERM_PROGRAM (must be "vscode") + $GIT_ASKPASS for IDE name.
// ---------------------------------------------------------------------------
std::string detect() {
    const char* term_program = safe_getenv("TERM_PROGRAM");
    if (std::strcmp(term_program, "vscode") != 0) {
        return "unknown";
    }
    const char* git_askpass = safe_getenv("GIT_ASKPASS");
    for (const auto& ide : kSupportedIdes) {
        if (std::strstr(git_askpass, ide.name) != nullptr) {
            return ide.name;
        }
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// detect_kind()
// ---------------------------------------------------------------------------
IdeKind detect_kind() {
    const std::string name = detect();
    for (const auto& ide : kSupportedIdes) {
        if (name == ide.name) return ide.kind;
    }
    return IdeKind::Unknown;
}

// ---------------------------------------------------------------------------
// already_installed() — mirrors OpenCode Ide.alreadyInstalled()
// ---------------------------------------------------------------------------
bool already_installed() {
    const char* caller = safe_getenv("OPENCODE_CALLER");
    return std::strcmp(caller, "vscode") == 0 ||
           std::strcmp(caller, "vscode-insiders") == 0;
}

// ---------------------------------------------------------------------------
// install() — mirrors OpenCode Ide.install()
// Runs: <ide-cmd> --install-extension sst-dev.opencode
// ---------------------------------------------------------------------------
void install(const std::string& ide_name) {
    const IdeEntry* entry = nullptr;
    for (const auto& ide : kSupportedIdes) {
        if (ide_name == ide.name) {
            entry = &ide;
            break;
        }
    }
    if (!entry) {
        throw std::invalid_argument("Unknown IDE: " + ide_name);
    }

    // Build command: "<cmd> --install-extension sst-dev.opencode 2>&1"
    const std::string cmd =
        std::string("'") + entry->cmd + "' --install-extension sst-dev.opencode 2>&1";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        throw InstallFailedError("failed to launch IDE process");
    }

    std::string stdout_buf;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        stdout_buf += buf;
    }
    const int raw_status = pclose(pipe);
    const int exit_code = WIFEXITED(raw_status) ? WEXITSTATUS(raw_status) : -1;

    TURBOT_LOG_INFO("Ide::install: ide={} exit_code={} stdout={}", ide_name, exit_code, stdout_buf);

    if (exit_code != 0) {
        throw InstallFailedError(stdout_buf);
    }
    if (stdout_buf.find("already installed") != std::string::npos) {
        throw AlreadyInstalledError();
    }
}

} // namespace Ide

} // namespace turbot::core::ide
