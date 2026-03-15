#pragma once

#include <turbot/core/lsp/server.hpp>

#include <nlohmann/json.hpp>
#include <string>

namespace turbot::core::lsp {

/// Check if LSP auto-download is enabled (default: true, disabled via TURBOT_DISABLE_LSP_DOWNLOAD)
[[nodiscard]] bool is_lsp_download_enabled();

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

/// Rust — rust-analyzer
[[nodiscard]] LSPServerInfo make_rust_analyzer_server(const std::string& workspace_root);

/// Svelte — svelte-language-server
[[nodiscard]] LSPServerInfo make_svelte_server(const std::string& workspace_root);

/// Astro — astro-ls
[[nodiscard]] LSPServerInfo make_astro_server(const std::string& workspace_root);

// ─── Additional LSP servers (v4.2 feature alignment) ─────────────────────────

/// Bash — bash-language-server
[[nodiscard]] LSPServerInfo make_bash_server(const std::string& workspace_root);

/// Java — jdtls (Eclipse JDT Language Server)
[[nodiscard]] LSPServerInfo make_java_server(const std::string& workspace_root);

/// Kotlin — kotlin-language-server
[[nodiscard]] LSPServerInfo make_kotlin_server(const std::string& workspace_root);

/// C# — OmniSharp
[[nodiscard]] LSPServerInfo make_csharp_server(const std::string& workspace_root);

/// Clojure — clojure-lsp
[[nodiscard]] LSPServerInfo make_clojure_server(const std::string& workspace_root);

/// Dart — dart analysis server
[[nodiscard]] LSPServerInfo make_dart_server(const std::string& workspace_root);

/// Elixir — elixir-ls
[[nodiscard]] LSPServerInfo make_elixir_server(const std::string& workspace_root);

/// Erlang — erlang-ls
[[nodiscard]] LSPServerInfo make_erlang_server(const std::string& workspace_root);

/// Haskell — hls (Haskell Language Server)
[[nodiscard]] LSPServerInfo make_haskell_server(const std::string& workspace_root);

/// Lua — lua-language-server
[[nodiscard]] LSPServerInfo make_lua_server(const std::string& workspace_root);

/// Nix — nixd
[[nodiscard]] LSPServerInfo make_nix_server(const std::string& workspace_root);

/// OCaml — ocamllsp
[[nodiscard]] LSPServerInfo make_ocaml_server(const std::string& workspace_root);

/// PHP — intelephense
[[nodiscard]] LSPServerInfo make_php_server(const std::string& workspace_root);

/// Ruby — solargraph
[[nodiscard]] LSPServerInfo make_ruby_server(const std::string& workspace_root);

/// Scala — metals
[[nodiscard]] LSPServerInfo make_scala_server(const std::string& workspace_root);

/// Swift — sourcekit-lsp
[[nodiscard]] LSPServerInfo make_swift_server(const std::string& workspace_root);

/// Terraform — terraform-ls
[[nodiscard]] LSPServerInfo make_terraform_server(const std::string& workspace_root);

/// Zig — zls
[[nodiscard]] LSPServerInfo make_zig_server(const std::string& workspace_root);

/// Create user-defined server from JSON config entry.
[[nodiscard]] LSPServerInfo make_custom_server(
    const std::string& id,
    const nlohmann::json& cfg,
    const std::string& workspace_root
);

}  // namespace turbot::core::lsp
