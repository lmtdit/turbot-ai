#include <turbot/core/lsp/builtin_servers.hpp>
#include <turbot/core/common/logger.hpp>

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>

namespace fs = std::filesystem;

namespace turbot::core::lsp {

// ─── LSP Auto-download Configuration ─────────────────────────────────────────

bool is_lsp_download_enabled() {
    // Default: enabled, disabled via TURBOT_DISABLE_LSP_DOWNLOAD=true
    const char* env = std::getenv("TURBOT_DISABLE_LSP_DOWNLOAD");
    if (env == nullptr) return true;  // enabled by default
    std::string value(env);
    // Check for "true", "1", "yes" (case-insensitive)
    for (char& c : value) c = std::tolower(c);
    return !(value == "true" || value == "1" || value == "yes");
}

// ─── command_exists ───────────────────────────────────────────────────────────

bool command_exists(const std::string& cmd) {
    // Search PATH for the command
    const char* path_env = ::getenv("PATH");
    if (!path_env) return false;

    std::string path_str(path_env);
    size_t start = 0;
    while (start < path_str.size()) {
        const size_t colon = path_str.find(':', start);
        const std::string dir = path_str.substr(start, colon == std::string::npos ? std::string::npos : colon - start);
        start = (colon == std::string::npos) ? path_str.size() : colon + 1;
        if (dir.empty()) continue;
        const fs::path candidate = fs::path(dir) / cmd;
        std::error_code ec;
        const auto status = fs::status(candidate, ec);
        if (!ec && fs::is_regular_file(status)) {
            // Check executable bit
            const auto perms = status.permissions();
            if ((perms & fs::perms::owner_exec) != fs::perms::none ||
                (perms & fs::perms::group_exec) != fs::perms::none ||
                (perms & fs::perms::others_exec) != fs::perms::none) {
                return true;
            }
        }
    }
    return false;
}

// ─── spawn_process ────────────────────────────────────────────────────────────

/// Spawn a subprocess with stdin/stdout pipes and optional env overrides.
/// Env overrides are applied in the child process (after fork, before exec),
/// so they are thread-safe from the parent's perspective.
static std::optional<ServerHandle> spawn_process_with_env(
    const std::vector<std::string>& args,
    const std::string& cwd,
    const std::unordered_map<std::string, std::string>& env_overrides)
{
    if (args.empty()) return std::nullopt;

    int stdin_pipe[2]  = {-1, -1};
    int stdout_pipe[2] = {-1, -1};

    if (::pipe(stdin_pipe) != 0 || ::pipe(stdout_pipe) != 0) {
        TURBOT_LOG_ERROR("spawn_process: pipe() failed: {}", strerror(errno));
        if (stdin_pipe[0]  >= 0) { ::close(stdin_pipe[0]);  ::close(stdin_pipe[1]); }
        if (stdout_pipe[0] >= 0) { ::close(stdout_pipe[0]); ::close(stdout_pipe[1]); }
        return std::nullopt;
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        TURBOT_LOG_ERROR("spawn_process: fork() failed: {}", strerror(errno));
        ::close(stdin_pipe[0]);  ::close(stdin_pipe[1]);
        ::close(stdout_pipe[0]); ::close(stdout_pipe[1]);
        return std::nullopt;
    }

    if (pid == 0) {
        // ── Child ────────────────────────────────────────────────────────
        // Apply env overrides (child-only, thread-safe)
        for (const auto& [k, v] : env_overrides) {
            ::setenv(k.c_str(), v.c_str(), 1);
        }
        ::dup2(stdin_pipe[0],  STDIN_FILENO);
        ::dup2(stdout_pipe[1], STDOUT_FILENO);
        ::close(stdin_pipe[0]);  ::close(stdin_pipe[1]);
        ::close(stdout_pipe[0]); ::close(stdout_pipe[1]);
        if (!cwd.empty() && ::chdir(cwd.c_str()) != 0) ::_exit(1);
        std::vector<const char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& a : args) argv.push_back(a.c_str());
        argv.push_back(nullptr);
        ::signal(SIGPIPE, SIG_DFL);
        ::execvp(argv[0], const_cast<char* const*>(argv.data()));
        ::_exit(127);
    }

    // ── Parent ────────────────────────────────────────────────────────────
    ::close(stdin_pipe[0]);
    ::close(stdout_pipe[1]);
    ::fcntl(stdin_pipe[1],  F_SETFD, FD_CLOEXEC);
    ::fcntl(stdout_pipe[0], F_SETFD, FD_CLOEXEC);

    ServerHandle handle;
    handle.pid       = static_cast<int>(pid);
    handle.stdin_fd  = stdin_pipe[1];
    handle.stdout_fd = stdout_pipe[0];
    return handle;
}

std::optional<ServerHandle> spawn_process(
    const std::vector<std::string>& args,
    const std::string& cwd)
{
    return spawn_process_with_env(args, cwd, {});
}

LSPServerInfo make_clangd_server(const std::string& workspace_root) {
    LSPServerInfo info;
    info.id         = "clangd";
    info.extensions = {".c", ".cpp", ".h", ".hpp", ".cc", ".cxx", ".hxx"};
    info.global     = false;

    const std::string root = workspace_root;
    info.root = [root](const std::string& file) -> std::optional<std::string> {
        const std::string dir = fs::path(file).parent_path().string();
        return nearest_root(dir,
            {"compile_commands.json", "CMakeLists.txt", ".clangd"},
            {},
            root);
    };
    info.spawn = [](const std::string& root_dir) -> std::optional<ServerHandle> {
        if (!command_exists("clangd")) {
            TURBOT_LOG_INFO("clangd not found, skipping C/C++ LSP");
            return std::nullopt;
        }
        return spawn_process(
            {"clangd", "--compile-commands-dir=" + root_dir, "--background-index"},
            root_dir);
    };
    return info;
}

// ─── Pyright server ───────────────────────────────────────────────────────────

LSPServerInfo make_pyright_server(const std::string& workspace_root) {
    LSPServerInfo info;
    info.id         = "pyright";
    info.extensions = {".py"};
    info.global     = false;

    const std::string root = workspace_root;
    info.root = [root](const std::string& file) -> std::optional<std::string> {
        const std::string dir = fs::path(file).parent_path().string();
        return nearest_root(dir,
            {"pyproject.toml", "setup.py", "setup.cfg", "requirements.txt"},
            {},
            root);
    };
    info.spawn = [](const std::string& root_dir) -> std::optional<ServerHandle> {
        // Check OPENCODE_EXPERIMENTAL_LSP_TY env var
        const bool use_ty = (::getenv("OPENCODE_EXPERIMENTAL_LSP_TY") != nullptr);
        const std::string cmd = use_ty ? "ty" : "pyright-langserver";
        if (!command_exists(cmd)) {
            TURBOT_LOG_INFO("{} not found, skipping Python LSP", cmd);
            return std::nullopt;
        }
        return spawn_process({cmd, "--stdio"}, root_dir);
    };
    return info;
}

// ─── Gopls server ─────────────────────────────────────────────────────────────

LSPServerInfo make_gopls_server(const std::string& workspace_root) {
    LSPServerInfo info;
    info.id         = "gopls";
    info.extensions = {".go"};
    info.global     = false;

    const std::string root = workspace_root;
    info.root = [root](const std::string& file) -> std::optional<std::string> {
        const std::string dir = fs::path(file).parent_path().string();
        // Prefer go.work, then go.mod
        auto r = nearest_root(dir, {"go.work"}, {}, root);
        if (r && *r != root) return r;  // found go.work above workspace root
        return nearest_root(dir, {"go.mod"}, {}, root);
    };
    info.spawn = [](const std::string& root_dir) -> std::optional<ServerHandle> {
        if (!command_exists("gopls")) {
            TURBOT_LOG_INFO("gopls not found, skipping Go LSP");
            return std::nullopt;
        }
        return spawn_process({"gopls"}, root_dir);
    };
    return info;
}

// ─── Shared factory for simple (command + root-files) LSP servers ───────────

/// Create an LSPServerInfo for a language server that follows the common pattern:
///   - detect workspace root by finding one of `root_files`
///   - launch with `command` in that directory
///   - silently skip if `command[0]` is not on PATH
static LSPServerInfo make_simple_lsp_server(
    const std::string& id,
    const std::vector<std::string>& extensions,
    const std::vector<std::string>& root_files,
    const std::vector<std::string>& command,
    const std::string& skip_log_msg,
    const std::string& workspace_root)
{
    LSPServerInfo info;
    info.id         = id;
    info.extensions = extensions;
    info.global     = false;

    const std::string root = workspace_root;
    info.root = [root, root_files](const std::string& file) -> std::optional<std::string> {
        const std::string dir = fs::path(file).parent_path().string();
        return nearest_root(dir, root_files, {}, root);
    };

    const std::string exe = command.empty() ? "" : command[0];
    info.spawn = [command, exe, skip_log_msg](const std::string& root_dir) -> std::optional<ServerHandle> {
        if (!command_exists(exe)) {
            TURBOT_LOG_INFO("{}", skip_log_msg);
            return std::nullopt;
        }
        return spawn_process(command, root_dir);
    };

    return info;
}

// ─── Deno LSP server ─────────────────────────────────────────────────────────

LSPServerInfo make_deno_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "deno",
        {".ts", ".tsx", ".js", ".jsx", ".mts", ".mjs", ".cts", ".cjs"},
        {"deno.json", "deno.jsonc"},
        {"deno", "lsp"},
        "deno not found, skipping Deno LSP",
        workspace_root);
}

// ─── TypeScript / Node LSP server ─────────────────────────────────────────────

LSPServerInfo make_typescript_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "typescript",
        {".ts", ".tsx", ".js", ".jsx", ".mts", ".mjs", ".cts", ".cjs"},
        {"tsconfig.json", "package.json"},
        {"typescript-language-server", "--stdio"},
        "typescript-language-server not found, skipping TypeScript/Node LSP",
        workspace_root);
}

// ─── Vue LSP server (Volar) ─────────────────────────────────────────────────

LSPServerInfo make_vue_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "vue",
        {".vue"},
        {"package.json", "vite.config.ts", "vite.config.js"},
        {"vue-language-server", "--stdio"},
        "vue-language-server not found, skipping Vue LSP (Volar)",
        workspace_root);
}

// ─── ESLint LSP server ─────────────────────────────────────────────────────

LSPServerInfo make_eslint_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "eslint",
        {".ts", ".tsx", ".js", ".jsx", ".mts", ".mjs", ".vue"},
        {"eslint.config.js", ".eslintrc.js", ".eslintrc.json", ".eslintrc", "package.json"},
        {"vscode-eslint-language-server", "--stdio"},
        "vscode-eslint-language-server not found, skipping ESLint LSP",
        workspace_root);
}

// ─── Biome LSP server ──────────────────────────────────────────────────────

LSPServerInfo make_biome_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "biome",
        {".ts", ".tsx", ".js", ".jsx", ".mts", ".mjs", ".json", ".jsonc"},
        {"biome.json", "biome.jsonc", "package.json"},
        {"biome", "lsp-proxy"},
        "biome not found, skipping Biome LSP",
        workspace_root);
}

// ─── Rust Analyzer LSP server ─────────────────────────────────────────────────

LSPServerInfo make_rust_analyzer_server(const std::string& workspace_root) {
    LSPServerInfo info;
    info.id         = "rust";
    info.extensions = {".rs"};
    info.global     = false;

    const std::string root = workspace_root;
    info.root = [root](const std::string& file) -> std::optional<std::string> {
        const std::string dir = fs::path(file).parent_path().string();
        // First, find nearest Cargo.toml or Cargo.lock
        auto crate_root = nearest_root(dir, {"Cargo.toml", "Cargo.lock"}, {}, root);
        if (!crate_root) return std::nullopt;

        // Then, walk up to find workspace root (Cargo.toml with [workspace])
        fs::path current = *crate_root;
        while (current != current.parent_path()) {
            fs::path cargo_toml = current / "Cargo.toml";
            std::error_code ec;
            if (fs::exists(cargo_toml, ec)) {
                std::ifstream f(cargo_toml);
                std::string content((std::istreambuf_iterator<char>(f)),
                                    std::istreambuf_iterator<char>());
                if (content.find("[workspace]") != std::string::npos) {
                    return current.string();
                }
            }
            current = current.parent_path();
        }
        return crate_root;
    };

    info.spawn = [](const std::string& root_dir) -> std::optional<ServerHandle> {
        if (!command_exists("rust-analyzer")) {
            TURBOT_LOG_INFO("rust-analyzer not found, skipping Rust LSP");
            return std::nullopt;
        }
        return spawn_process({"rust-analyzer"}, root_dir);
    };

    return info;
}

// ─── Svelte LSP server ───────────────────────────────────────────────────────

LSPServerInfo make_svelte_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "svelte",
        {".svelte"},
        {"package-lock.json", "bun.lockb", "bun.lock", "pnpm-lock.yaml", "yarn.lock", "package.json"},
        {"svelteserver", "--stdio"},
        "svelteserver not found, skipping Svelte LSP",
        workspace_root);
}

// ─── Astro LSP server ─────────────────────────────────────────────────────────

LSPServerInfo make_astro_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "astro",
        {".astro"},
        {"package-lock.json", "bun.lockb", "bun.lock", "pnpm-lock.yaml", "yarn.lock", "package.json"},
        {"astro-ls", "--stdio"},
        "astro-ls not found, skipping Astro LSP",
        workspace_root);
}

// ─── Additional LSP servers (v4.2 feature alignment) ─────────────────────────

/// Bash — bash-language-server
LSPServerInfo make_bash_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "bash",
        {".sh", ".bash", ".zsh"},
        {},
        {"bash-language-server", "start"},
        "bash-language-server not found, skipping Bash LSP",
        workspace_root);
}

/// Java — jdtls (Eclipse JDT Language Server)
LSPServerInfo make_java_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "java",
        {".java"},
        {"pom.xml", "build.gradle", "build.gradle.kts", "settings.gradle", "settings.gradle.kts"},
        {"jdtls"},
        "jdtls not found, skipping Java LSP",
        workspace_root);
}

/// Kotlin — kotlin-language-server
LSPServerInfo make_kotlin_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "kotlin",
        {".kt", ".kts", ".ktm"},
        {"build.gradle.kts", "settings.gradle.kts", "pom.xml"},
        {"kotlin-language-server"},
        "kotlin-language-server not found, skipping Kotlin LSP",
        workspace_root);
}

/// C# — OmniSharp
LSPServerInfo make_csharp_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "csharp",
        {".cs", ".csx", ".cake"},
        {"*.sln", "*.csproj", "project.json", "omnisharp.json"},
        {"OmniSharp", "-lsp"},
        "OmniSharp not found, skipping C# LSP",
        workspace_root);
}

/// Clojure — clojure-lsp
LSPServerInfo make_clojure_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "clojure",
        {".clj", ".cljs", ".cljc", ".edn"},
        {"deps.edn", "project.clj", "shadow-cljs.edn", "bb.edn"},
        {"clojure-lsp"},
        "clojure-lsp not found, skipping Clojure LSP",
        workspace_root);
}

/// Dart — dart analysis server
LSPServerInfo make_dart_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "dart",
        {".dart"},
        {"pubspec.yaml", ".dart_tool"},
        {"dart", "language-server", "--protocol=lsp"},
        "dart not found, skipping Dart LSP",
        workspace_root);
}

/// Elixir — elixir-ls
LSPServerInfo make_elixir_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "elixir",
        {".ex", ".exs", ".eex", ".leex", ".heex"},
        {"mix.exs"},
        {"elixir-ls"},
        "elixir-ls not found, skipping Elixir LSP",
        workspace_root);
}

/// Erlang — erlang-ls
LSPServerInfo make_erlang_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "erlang",
        {".erl", ".hrl", ".erl", ".app", ".app.src", ".config", ".script", ".escript"},
        {"rebar.config", "erlang.mk", "Emakefile"},
        {"erlang_ls"},
        "erlang_ls not found, skipping Erlang LSP",
        workspace_root);
}

/// Haskell — hls (Haskell Language Server)
LSPServerInfo make_haskell_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "haskell",
        {".hs", ".lhs", ".hs-boot", ".lhs-boot"},
        {"hie.yaml", "stack.yaml", "cabal.project", "package.yaml"},
        {"haskell-language-server-wrapper", "--lsp"},
        "haskell-language-server not found, skipping Haskell LSP",
        workspace_root);
}

/// Lua — lua-language-server
LSPServerInfo make_lua_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "lua",
        {".lua", ".luau"},
        {".luarc.json", ".luarc.jsonc", ".luacheckrc", "selene.toml"},
        {"lua-language-server"},
        "lua-language-server not found, skipping Lua LSP",
        workspace_root);
}

/// Nix — nixd
LSPServerInfo make_nix_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "nix",
        {".nix"},
        {"flake.nix", "default.nix", "shell.nix"},
        {"nixd"},
        "nixd not found, skipping Nix LSP",
        workspace_root);
}

/// OCaml — ocamllsp
LSPServerInfo make_ocaml_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "ocaml",
        {".ml", ".mli", ".mll", ".mly"},
        {"dune-project", "esy.json", "opam"},
        {"ocamllsp"},
        "ocamllsp not found, skipping OCaml LSP",
        workspace_root);
}

/// PHP — intelephense
LSPServerInfo make_php_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "php",
        {".php", ".phtml", ".php3", ".php4", ".php5", ".php7", ".phps"},
        {"composer.json", "vendor"},
        {"intelephense", "--stdio"},
        "intelephense not found, skipping PHP LSP",
        workspace_root);
}

/// Ruby — solargraph
LSPServerInfo make_ruby_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "ruby",
        {".rb", ".rake", ".gemspec", ".ru"},
        {"Gemfile", "Rakefile", ".solargraph.yml"},
        {"solargraph", "stdio"},
        "solargraph not found, skipping Ruby LSP",
        workspace_root);
}

/// Scala — metals
LSPServerInfo make_scala_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "scala",
        {".scala", ".sc", ".sbt"},
        {"build.sbt", "project/build.properties", "project/plugins.sbt"},
        {"metals"},
        "metals not found, skipping Scala LSP",
        workspace_root);
}

/// Swift — sourcekit-lsp
LSPServerInfo make_swift_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "swift",
        {".swift", ".swiftinterface"},
        {"Package.swift", ".swift-version", "Podfile", "Cartfile"},
        {"sourcekit-lsp"},
        "sourcekit-lsp not found, skipping Swift LSP",
        workspace_root);
}

/// Terraform — terraform-ls
LSPServerInfo make_terraform_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "terraform",
        {".tf", ".tfvars", ".hcl"},
        {".terraform", ".terraform.lock.hcl"},
        {"terraform-ls", "serve"},
        "terraform-ls not found, skipping Terraform LSP",
        workspace_root);
}

/// Zig — zls
LSPServerInfo make_zig_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "zig",
        {".zig", ".zon"},
        {"build.zig", "zls.json"},
        {"zls"},
        "zls not found, skipping Zig LSP",
        workspace_root);
}

/// Oxlint — oxc_language_server / oxlint --lsp (P2 feature alignment, T17)
///
/// Resolution order (mirrors OpenCode lsp/server.ts):
///   1. node_modules/.bin/oxlint --lsp   (supports --lsp flag)
///   2. node_modules/.bin/oxc_language_server
///   3. oxlint in PATH  (--lsp variant)
///   4. oxc_language_server in PATH
LSPServerInfo make_oxlint_server(const std::string& workspace_root) {
    LSPServerInfo info;
    info.id         = "oxlint";
    info.extensions = {".ts", ".tsx", ".js", ".jsx", ".mjs", ".cjs",
                       ".mts", ".cts", ".vue", ".astro", ".svelte"};

    const std::string root = workspace_root;
    const std::vector<std::string> root_files = {
        ".oxlintrc.json", "package-lock.json", "bun.lockb", "bun.lock",
        "pnpm-lock.yaml", "yarn.lock", "package.json"
    };
    info.root = [root, root_files](const std::string& file) -> std::optional<std::string> {
        const std::string dir = fs::path(file).parent_path().string();
        return nearest_root(dir, root_files, {}, root);
    };

    info.spawn = [workspace_root](const std::string& root_dir) -> std::optional<ServerHandle> {
        namespace fs = std::filesystem;

        // Helper: check PATH for a binary
        auto in_path = [](const std::string& cmd) -> bool {
            return command_exists(cmd);
        };

        // 1. Try node_modules/.bin/oxlint --lsp in project or ancestors
        for (fs::path p = root_dir; !p.empty() && p != p.parent_path(); p = p.parent_path()) {
            const fs::path candidate = p / "node_modules" / ".bin" / "oxlint";
            std::error_code ec;
            if (fs::exists(candidate, ec)) {
                // Check whether this oxlint supports --lsp by running --help
                // Use single-quote escaping to prevent shell injection
                const std::string cmd = "'" + candidate.string() + "' --help 2>&1";
                FILE* pipe = popen(cmd.c_str(), "r");
                bool supports_lsp = false;
                if (pipe) {
                    char buf[512];
                    while (fgets(buf, sizeof(buf), pipe)) {
                        if (std::string(buf).find("--lsp") != std::string::npos) {
                            supports_lsp = true;
                            break;
                        }
                    }
                    pclose(pipe);
                }
                if (supports_lsp) {
                    TURBOT_LOG_INFO("oxlint LSP: using {} --lsp", candidate.string());
                    return spawn_process({candidate.string(), "--lsp"}, root_dir);
                }
            }
            // 2. Try node_modules/.bin/oxc_language_server
            const fs::path server_candidate = p / "node_modules" / ".bin" / "oxc_language_server";
            if (fs::exists(server_candidate, ec)) {
                TURBOT_LOG_INFO("oxlint LSP: using {}", server_candidate.string());
                return spawn_process({server_candidate.string()}, root_dir);
            }
        }

        // 3. Try oxlint in PATH with --lsp
        if (in_path("oxlint")) {
            FILE* pipe = popen("oxlint --help 2>&1", "r");
            bool supports_lsp = false;
            if (pipe) {
                char buf[512];
                while (fgets(buf, sizeof(buf), pipe)) {
                    if (std::string(buf).find("--lsp") != std::string::npos) {
                        supports_lsp = true;
                        break;
                    }
                }
                pclose(pipe);
            }
            if (supports_lsp) {
                TURBOT_LOG_INFO("oxlint LSP: using oxlint --lsp from PATH");
                return spawn_process({"oxlint", "--lsp"}, root_dir);
            }
        }

        // 4. Try oxc_language_server in PATH
        if (in_path("oxc_language_server")) {
            TURBOT_LOG_INFO("oxlint LSP: using oxc_language_server from PATH");
            return spawn_process({"oxc_language_server"}, root_dir);
        }

        TURBOT_LOG_INFO("oxlint not found, please install oxlint or oxc_language_server");
        return std::nullopt;
    };

    return info;
}

/// Gleam — gleam lsp (P3 feature alignment, T17)
LSPServerInfo make_gleam_server(const std::string& workspace_root) {
    return make_simple_lsp_server(
        "gleam",
        {".gleam"},
        {"gleam.toml"},
        {"gleam", "lsp"},
        "gleam not found, skipping Gleam LSP",
        workspace_root);
}

// ─── Custom (user-defined) server ─────────────────────────────────────────────

LSPServerInfo make_custom_server(
    const std::string& id,
    const nlohmann::json& cfg,
    const std::string& workspace_root)
{
    LSPServerInfo info;
    info.id = id;

    // extensions
    if (cfg.contains("extensions") && cfg["extensions"].is_array()) {
        for (const auto& ext : cfg["extensions"]) {
            if (ext.is_string()) info.extensions.push_back(ext.get<std::string>());
        }
    }

    // root: always returns workspace_root (no special root detection for custom servers)
    info.root = [workspace_root](const std::string&) -> std::optional<std::string> {
        return workspace_root;
    };

    // Build command
    std::vector<std::string> command;
    if (cfg.contains("command") && cfg["command"].is_array()) {
        for (const auto& part : cfg["command"]) {
            if (part.is_string()) command.push_back(part.get<std::string>());
        }
    }

    // env overrides
    std::unordered_map<std::string, std::string> env_overrides;
    if (cfg.contains("env") && cfg["env"].is_object()) {
        for (const auto& [k, v] : cfg["env"].items()) {
            if (v.is_string()) env_overrides[k] = v.get<std::string>();
        }
    }

    // initialization options
    nlohmann::json init_opts = nullptr;
    if (cfg.contains("initialization")) {
        init_opts = cfg["initialization"];
    }

    info.spawn = [command, env_overrides, init_opts, id](const std::string& root_dir) -> std::optional<ServerHandle> {
        if (command.empty()) {
            TURBOT_LOG_WARN("LSP custom server '{}': no command configured", id);
            return std::nullopt;
        }
        // Apply env overrides in the child process (thread-safe)
        auto handle = spawn_process_with_env(command, root_dir, env_overrides);
        if (handle && !init_opts.is_null()) {
            handle->initialization = init_opts;
        }
        return handle;
    };

    return info;
}

}  // namespace turbot::core::lsp
