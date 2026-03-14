#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/lsp/client.hpp>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace turbot::core::lsp {

// ─── NearestRoot ──────────────────────────────────────────────────────────────

/// Find the nearest parent directory containing any of `include_patterns`.
/// Aligned with OpenCode `NearestRoot`: starts at `start_dir`, walks up to `stop_dir`.
///
/// Behavior:
///   - If `exclude_patterns` is non-empty and any exclude file is found → return nullopt (not applicable)
///   - If include file found → return the directory containing it
///   - If no include file found → return `stop_dir` (fallback, NOT nullopt)
///   - If `stop_dir` is empty → walk up to filesystem root
[[nodiscard]] TURBOT_CORE_API
std::optional<std::string> nearest_root(
    const std::string& start_dir,
    const std::vector<std::string>& include_patterns,
    const std::vector<std::string>& exclude_patterns = {},
    const std::string& stop_dir = {}
);

// ─── LSPServerInfo ────────────────────────────────────────────────────────────

/// Descriptor for a language server (builtin or user-defined).
/// Aligned with OpenCode LSPServer.Info interface.
struct TURBOT_CORE_API LSPServerInfo {
    std::string id;
    std::vector<std::string> extensions;  ///< e.g. {".cpp", ".hpp"}, empty = all files
    bool global = false;

    /// Determine workspace root for the given file.
    /// Returns nullopt if this server does NOT apply to the file (e.g. excluded by pattern).
    /// Returns stop_dir / project_root as fallback if no specific root marker found.
    /// Aligned with OpenCode NearestRoot fallback behavior.
    std::function<std::optional<std::string>(const std::string& file)> root;

    /// Spawn the LSP server subprocess for the given root directory.
    /// Returns nullopt if the server is NOT installed (do not throw).
    /// Returns ServerHandle (spawned subprocess with stdin/stdout pipes).
    std::function<std::optional<ServerHandle>(const std::string& root_dir)> spawn;
};

}  // namespace turbot::core::lsp
