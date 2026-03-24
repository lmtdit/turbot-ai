#pragma once

#include <turbot/core/common/export.hpp>
#include <string>
#include <stdexcept>

namespace turbot::core::ide {

// ---------------------------------------------------------------------------
// Supported IDE names — mirrors OpenCode SUPPORTED_IDES
// ---------------------------------------------------------------------------
enum class IdeKind {
    Unknown,
    VisualStudioCode,
    VisualStudioCodeInsiders,
    Cursor,
    Windsurf,
    VSCodium,
};

// ---------------------------------------------------------------------------
// Ide — detect running IDE and install the extension
// Mirrors OpenCode ide/index.ts  Ide namespace
// ---------------------------------------------------------------------------
namespace Ide {

/// Error thrown when the extension is already installed.
struct TURBOT_CORE_API AlreadyInstalledError : public std::runtime_error {
    AlreadyInstalledError() : std::runtime_error("Extension already installed") {}
};

/// Error thrown when installation fails (e.g. the IDE CLI returned non-zero).
struct TURBOT_CORE_API InstallFailedError : public std::runtime_error {
    std::string stderr_output;
    explicit InstallFailedError(const std::string& stderr_msg)
        : std::runtime_error("Extension install failed: " + stderr_msg)
        , stderr_output(stderr_msg) {}
};

/// Detect which IDE is running (inspects TERM_PROGRAM + GIT_ASKPASS env vars).
/// Returns "unknown" when the IDE cannot be determined.
/// Mirrors OpenCode Ide.ide().
[[nodiscard]] TURBOT_CORE_API std::string detect();

/// Returns the IdeKind enum value for the currently running IDE.
[[nodiscard]] TURBOT_CORE_API IdeKind detect_kind();

/// Returns true when the extension is already acting as the IDE caller,
/// i.e. OPENCODE_CALLER env is "vscode" or "vscode-insiders".
/// Mirrors OpenCode Ide.alreadyInstalled().
[[nodiscard]] TURBOT_CORE_API bool already_installed();

/// Install the opencode extension into the given IDE.
/// @param ide_name  One of "Windsurf", "Visual Studio Code - Insiders",
///                  "Visual Studio Code", "Cursor", "VSCodium".
/// @throws AlreadyInstalledError  If the extension is already present.
/// @throws InstallFailedError     If the IDE CLI exits with a non-zero code.
/// @throws std::invalid_argument  If ide_name is not recognised.
/// Mirrors OpenCode Ide.install().
TURBOT_CORE_API void install(const std::string& ide_name);

} // namespace Ide

} // namespace turbot::core::ide
