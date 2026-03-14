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

// ─── Built-in server factories (aligned with OpenCode lsp/server.ts) ───────

/// C/C++ — clangd
[[nodiscard]] LSPServerInfo make_clangd_server(const std::string& workspace_root);

/// Python — Pyright / ty (via OPENCODE_EXPERIMENTAL_LSP_TY env var)
[[nodiscard]] LSPServerInfo make_pyright_server(const std::string& workspace_root);

/// Go — gopls
[[nodiscard]] LSPServerInfo make_gopls_server(const std::string& workspace_root);

/// TypeScript/JavaScript (Deno runtime) — deno lsp
[[nodiscard]] LSPServerInfo make_deno_server(const std::string& workspace_root);

/// TypeScript/JavaScript (Node runtime) — typescript-language-server
[[nodiscard]] LSPServerInfo make_typescript_server(const std::string& workspace_root);

/// Vue.js — volar / vue-language-server
[[nodiscard]] LSPServerInfo make_vue_server(const std::string& workspace_root);

/// JavaScript/TypeScript lint — eslint LSP (vscode-langservers-extracted)
[[nodiscard]] LSPServerInfo make_eslint_server(const std::string& workspace_root);

/// JavaScript/TypeScript format + lint — biome LSP
[[nodiscard]] LSPServerInfo make_biome_server(const std::string& workspace_root);

/// Create user-defined server from JSON config entry.
[[nodiscard]] LSPServerInfo make_custom_server(
    const std::string& id,
    const nlohmann::json& cfg,
    const std::string& workspace_root
);

}  // namespace turbot::core::lsp
