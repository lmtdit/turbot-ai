#pragma once

#include <turbot/core/lsp/server.hpp>

#include <nlohmann/json.hpp>
#include <string>

namespace turbot::core::lsp {

/// Check if a command exists in PATH (similar to `which` on POSIX systems).
[[nodiscard]] bool command_exists(const std::string& cmd);

/// Spawn a subprocess with stdin/stdout pipes.
/// Returns nullopt if fork/exec fails.
/// @param args  argv[0] = program, args[1..] = arguments
/// @param cwd   Working directory for the subprocess
[[nodiscard]] std::optional<ServerHandle> spawn_process(
    const std::vector<std::string>& args,
    const std::string& cwd
);

/// Create Clangd LSP server descriptor.
[[nodiscard]] LSPServerInfo make_clangd_server(const std::string& workspace_root);

/// Create Pyright/ty Python LSP server descriptor.
[[nodiscard]] LSPServerInfo make_pyright_server(const std::string& workspace_root);

/// Create Gopls Go LSP server descriptor.
[[nodiscard]] LSPServerInfo make_gopls_server(const std::string& workspace_root);

/// Create user-defined server from JSON config entry.
[[nodiscard]] LSPServerInfo make_custom_server(
    const std::string& id,
    const nlohmann::json& cfg,
    const std::string& workspace_root
);

}  // namespace turbot::core::lsp
