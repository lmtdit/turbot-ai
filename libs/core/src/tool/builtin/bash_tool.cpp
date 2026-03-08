#include <turbot/core/tool/builtin/bash_tool.hpp>
#include <turbot/core/permission/permission.hpp>
#include <turbot/core/config/config.hpp>
#include <fmt/format.h>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace turbot::core::tool::builtin {

namespace {

// Default timeout in milliseconds (2 minutes)
constexpr int DEFAULT_TIMEOUT_MS = 120000;
// Maximum timeout in milliseconds (10 minutes)
constexpr int MAX_TIMEOUT_MS = 600000;

/// Validate working directory to prevent path traversal attacks.
/// Returns true if the directory is safe to use.
bool validate_working_directory(const std::string& cwd, std::string& error) {
    namespace fs = std::filesystem;
    
    if (cwd.empty()) {
        return true;  // Empty is ok, will use current directory
    }
    
    std::error_code ec;
    
    // Check if path exists and is a directory
    if (!fs::exists(cwd, ec) || ec) {
        error = fmt::format("Working directory does not exist: {}", cwd);
        return false;
    }
    
    if (!fs::is_directory(cwd, ec) || ec) {
        error = fmt::format("Working directory is not a directory: {}", cwd);
        return false;
    }
    
    // Resolve to canonical path to prevent path traversal
    fs::path canonical_path = fs::canonical(cwd, ec);
    if (ec) {
        error = fmt::format("Failed to resolve working directory: {}", cwd);
        return false;
    }
    
    // Check for dangerous paths (optional: add more as needed)
    std::string canonical_str = canonical_path.string();
    
    // Block access to system directories (basic protection)
    // Note: This is a basic security measure, not a complete sandbox
    const std::vector<std::string> blocked_prefixes = {
        "/etc",           // System configuration
        "/sys",           // Kernel virtual filesystem
        "/proc",          // Process virtual filesystem
        "/root",          // Root user home
        "/boot",          // Boot loader files
        "/dev",           // Device files (already protected but explicit)
        "/lib",           // System libraries
        "/lib64",         // System libraries (64-bit)
        "/usr/lib",       // User libraries
        "/usr/lib64",     // User libraries (64-bit)
        "/var/lib",       // Variable state data
        "/var/log",       // System logs
        "/var/run",       // Runtime variable data (symlink to /run)
        "/run"            // Runtime variable data
    };
    
    for (const auto& prefix : blocked_prefixes) {
        if (canonical_str == prefix || canonical_str.substr(0, prefix.size() + 1) == prefix + "/") {
            error = fmt::format("Access to system directory is not allowed: {}", cwd);
            return false;
        }
    }
    
    return true;
}

/// Escape a string for use as a single-quoted shell argument.
std::string escape_shell_arg(const std::string& cmd) {
    std::string escaped = "'";
    for (char c : cmd) {
        if (c == '\'') {
            escaped += "'\\''";
        } else {
            escaped += c;
        }
    }
    escaped += "'";
    return escaped;
}

/// Detect available timeout command (checked once at startup)
const std::string& timeout_command() {
    static const std::string cmd = []() -> std::string {
        namespace fs = std::filesystem;
        for (const char* path : {
                "/usr/bin/timeout",
                "/usr/local/bin/timeout",
                "/opt/homebrew/bin/gtimeout",
                "/usr/local/bin/gtimeout"
            }) {
            std::error_code ec;
            if (fs::exists(path, ec) && !ec) {
                return path;
            }
        }
        return {};
    }();
    return cmd;
}

/// Get shell executable path
const std::string& shell_path() {
    static const std::string shell = []() -> std::string {
        const char* env_shell = std::getenv("SHELL");
        if (env_shell && env_shell[0] != '\0') {
            return env_shell;
        }
        // Fallback
        if (std::filesystem::exists("/bin/zsh")) return "/bin/zsh";
        if (std::filesystem::exists("/bin/bash")) return "/bin/bash";
        return "/bin/sh";
    }();
    return shell;
}

} // anonymous namespace

// ============================================================================
// BashToolParams
// ============================================================================

BashToolParams BashToolParams::from_json(const nlohmann::json& j) {
    BashToolParams params;
    params.command = j.at("command").get<std::string>();
    
    if (j.contains("description") && !j["description"].is_null()) {
        params.description = j["description"].get<std::string>();
    }
    if (j.contains("timeout") && !j["timeout"].is_null()) {
        params.timeout = j["timeout"].get<int>();
    }
    if (j.contains("workdir") && !j["workdir"].is_null()) {
        params.workdir = j["workdir"].get<std::string>();
    }
    
    return params;
}

nlohmann::json BashToolParams::to_json() const {
    nlohmann::json j;
    j["command"] = command;
    if (!description.empty()) j["description"] = description;
    if (timeout) j["timeout"] = *timeout;
    if (workdir) j["workdir"] = *workdir;
    return j;
}

// ============================================================================
// BashTool
// ============================================================================

std::string BashTool::description() const {
    return "Execute a shell command and return the output. "
           "Use this tool to run shell commands, scripts, and system operations. "
           "Supports normal and sandbox execution modes. "
           "Commands run in the current working directory by default.";
}

nlohmann::json BashTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"command", {
                {"type", "string"},
                {"description", "The command to execute"}
            }},
            {"description", {
                {"type", "string"},
                {"description", "Clear, concise description of what this command does in 5-10 words"}
            }},
            {"timeout", {
                {"type", "integer"},
                {"description", "Optional timeout in milliseconds (default: 120000, max: 600000)"}
            }},
            {"workdir", {
                {"type", "string"},
                {"description", "The working directory to run the command in. Defaults to current directory."}
            }}
        }},
        {"required", nlohmann::json::array({"command"})}
    };
}

bool BashTool::validate_input(const nlohmann::json& input) const {
    return input.contains("command") && input["command"].is_string() &&
           !input["command"].get<std::string>().empty();
}

ShellMode BashTool::ask_shell_mode(ToolContext& ctx) {
    if (!ctx.ask_permission) {
        return ShellMode::Normal;  // Default to normal if no callback
    }
    
    permission::PermissionRequest req;
    req.id = "shell_mode_ask";
    req.permission = "shell_mode";
    req.patterns = {"normal", "sandbox"};
    req.tool = name();
    
    auto reply = ctx.ask_permission(req);
    
    // If user says "always", save to config
    if (reply.type == permission::PermissionReply::Type::Always) {
        // Default to normal mode when user says always
        Config::instance().set("tools.shell_mode", "normal");
        return ShellMode::Normal;
    }
    
    // Default based on reply
    if (reply.type == permission::PermissionReply::Type::Reject) {
        return ShellMode::Sandbox;  // Rejection means use sandbox
    }
    
    return ShellMode::Normal;
}

ToolResult BashTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    if (!validate_input(input)) {
        return ToolResult::error(
            "Bash execution failed",
            "Invalid input: 'command' is required and must be a non-empty string"
        );
    }

    // Parse parameters
    BashToolParams params;
    try {
        params = BashToolParams::from_json(input);
    } catch (const std::exception& e) {
        return ToolResult::error("Bash", fmt::format("Invalid parameters: {}", e.what()));
    }

    // Determine timeout
    int timeout_ms = params.timeout.value_or(ctx.tool_config.default_timeout * 1000);
    timeout_ms = std::min(std::max(timeout_ms, 1000), ctx.tool_config.max_timeout * 1000);

    // Check permission for command execution
    const auto action = permission::PermissionSystem::evaluate("execute", params.command, ctx.ruleset);

    if (action == permission::PermissionAction::Deny) {
        return ToolResult::error(
            fmt::format("Bash: {}", params.command.substr(0, 50)),
            "Permission denied for executing command"
        );
    }

    if (action == permission::PermissionAction::Ask) {
        if (!ctx.ask_permission) {
            return ToolResult::error(
                fmt::format("Bash: {}", params.command.substr(0, 50)),
                "Permission requires user confirmation but no callback is set"
            );
        }
        
        permission::PermissionRequest req;
        req.id = fmt::format("bash_{:x}", std::hash<std::string>{}(params.command));
        req.permission = "execute";
        req.patterns = {params.command};
        req.tool = name();

        const auto reply = ctx.ask_permission(req);
        if (reply.type == permission::PermissionReply::Type::Reject) {
            return ToolResult::error(
                fmt::format("Bash: {}", params.command.substr(0, 50)),
                "User rejected permission for executing command"
            );
        }
    }

    // Check abort flag
    if (ctx.should_abort()) {
        return ToolResult::error("Bash aborted", "Operation was aborted");
    }

    // Determine execution mode
    ShellMode mode = ctx.tool_config.shell_mode;
    if (mode == ShellMode::Ask) {
        mode = ask_shell_mode(ctx);
    }

    // Execute based on mode
    if (mode == ShellMode::Sandbox) {
        return execute_sandbox(params, ctx, timeout_ms);
    } else {
        return execute_normal(params, ctx, timeout_ms);
    }
}

ToolResult BashTool::execute_normal(
    const BashToolParams& params,
    ToolContext& ctx,
    int timeout_ms
) {
    const std::string escaped_cmd = escape_shell_arg(params.command);
    const int timeout_sec = timeout_ms / 1000;

    // Build command with timeout if available
    std::string full_command;
    const std::string& tc = timeout_command();
    if (!tc.empty()) {
        full_command = fmt::format("{} {} {} -c {} 2>&1", tc, timeout_sec, shell_path(), escaped_cmd);
    } else {
        full_command = fmt::format("{} -c {} 2>&1", shell_path(), escaped_cmd);
    }

    // Determine working directory with validation
    std::string cwd = params.workdir.value_or(ctx.working_directory);
    if (cwd.empty()) {
        cwd = std::filesystem::current_path().string();
    }
    
    // Validate working directory to prevent path traversal
    std::string cwd_error;
    if (!validate_working_directory(cwd, cwd_error)) {
        return ToolResult::error(
            fmt::format("Bash: {}", params.command.substr(0, 50)),
            cwd_error
        );
    }

    // Change to working directory if needed
    if (!cwd.empty() && cwd != ".") {
        // Build a compound command: 'cd CWD && ORIGINAL_COMMAND'
        // escape_shell_arg wraps the whole thing so all characters are safe,
        // including single quotes inside the original command.
        std::string cd_and_run = fmt::format("cd {} && {}", escape_shell_arg(cwd), params.command);
        std::string escaped_cd_and_run = escape_shell_arg(cd_and_run);
        full_command = tc.empty()
            ? fmt::format("{} -c {} 2>&1", shell_path(), escaped_cd_and_run)
            : fmt::format("{} {} {} -c {} 2>&1", tc, timeout_sec, shell_path(), escaped_cd_and_run);
    }

    // Execute the command
    std::string output;
    output.reserve(4096);

    std::array<char, 256> buffer{};
    FILE* pipe = popen(full_command.c_str(), "r");
    if (!pipe) {
        return ToolResult::error(
            fmt::format("Bash: {}", params.command.substr(0, 50)),
            "Failed to execute command: could not open process"
        );
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
        if (ctx.should_abort()) {
            pclose(pipe);
            return ToolResult::error("Bash aborted", "Operation was aborted during execution");
        }
    }

    const int wait_status = pclose(pipe);
    int actual_exit = -1;
    if (wait_status == -1) {
        return ToolResult::error(
            fmt::format("Bash: {}", params.command.substr(0, 50)),
            fmt::format("pclose failed: {}", strerror(errno))
        );
    }
    if (WIFEXITED(wait_status)) {
        actual_exit = WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        actual_exit = 128 + WTERMSIG(wait_status);
    }

    nlohmann::json metadata = {
        {"command", params.command},
        {"exit_code", actual_exit},
        {"timeout_ms", timeout_ms},
        {"mode", "normal"},
        {"workdir", cwd}
    };

    // Exit code 124 is from GNU timeout indicating timeout occurred
    if (!tc.empty() && actual_exit == 124) {
        return ToolResult::error(
            fmt::format("Bash: {}", params.command.substr(0, 50)),
            fmt::format("Command timed out after {} seconds\n{}", timeout_sec, output),
            metadata
        );
    }

    const std::string title = params.description.empty()
        ? fmt::format("Bash (exit {}): {}", actual_exit, params.command.substr(0, 50))
        : params.description;

    if (actual_exit != 0) {
        return ToolResult{
            .title = title,
            .output = output,
            .metadata = metadata,
            .is_error = true
        };
    }

    return ToolResult::success(title, output, metadata);
}

ToolResult BashTool::execute_sandbox(
    const BashToolParams& params,
    ToolContext& ctx,
    int timeout_ms
) {
    // Sandbox mode: restricted environment
    // In a real implementation, this would use containers, namespaces, or other isolation
    // For now, we implement a basic sandbox with restricted commands
    
    const std::string escaped_cmd = escape_shell_arg(params.command);
    const int timeout_sec = timeout_ms / 1000;

    // Build sandbox command
    // This is a simplified sandbox - real implementation would use proper isolation
    const std::string& tc = timeout_command();
    
    // Sandbox: restrict to safe commands via a clean environment.
    // Uses 'env -i' to clear the environment and set only minimal vars.
    // Note: This provides environment-level isolation only; full filesystem/network
    // isolation requires platform-specific mechanisms (namespaces, containers, etc.).
    std::string sandbox_prefix = fmt::format("env -i PATH=/usr/bin:/bin HOME={} ",
        escape_shell_arg(params.workdir.value_or(ctx.working_directory)));
    
    // Determine working directory with validation
    std::string cwd = params.workdir.value_or(ctx.working_directory);
    if (cwd.empty()) {
        cwd = std::filesystem::current_path().string();
    }
    
    // Validate working directory to prevent path traversal
    std::string cwd_error;
    if (!validate_working_directory(cwd, cwd_error)) {
        return ToolResult::error(
            fmt::format("Bash (sandbox): {}", params.command.substr(0, 50)),
            cwd_error
        );
    }

    // Build the sandboxed command:
    // env -i PATH=... HOME=... shell -c 'cmd'
    // The sandbox_prefix is a shell command prefix (not a shell -c script),
    // so it must come before the shell invocation, not inside -c '...'.
    std::string inner_cmd;
    if (!cwd.empty() && cwd != ".") {
        std::string cd_and_run = fmt::format("cd {} && {}", escape_shell_arg(cwd), params.command);
        inner_cmd = escape_shell_arg(cd_and_run);
    } else {
        inner_cmd = escaped_cmd;
    }

    std::string sandbox_cmd;
    if (!tc.empty()) {
        sandbox_cmd = fmt::format("{} {} {}{} -c {} 2>&1",
            tc, timeout_sec, sandbox_prefix, shell_path(), inner_cmd);
    } else {
        sandbox_cmd = fmt::format("{}{} -c {} 2>&1", sandbox_prefix, shell_path(), inner_cmd);
    }

    // Execute the command
    std::string output;
    output.reserve(4096);

    std::array<char, 256> buffer{};
    FILE* pipe = popen(sandbox_cmd.c_str(), "r");
    if (!pipe) {
        return ToolResult::error(
            fmt::format("Bash (sandbox): {}", params.command.substr(0, 50)),
            "Failed to execute sandboxed command"
        );
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
        if (ctx.should_abort()) {
            pclose(pipe);
            return ToolResult::error("Bash aborted", "Operation was aborted during execution");
        }
    }

    const int wait_status = pclose(pipe);
    int actual_exit = -1;
    if (WIFEXITED(wait_status)) {
        actual_exit = WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        actual_exit = 128 + WTERMSIG(wait_status);
    }

    nlohmann::json metadata = {
        {"command", params.command},
        {"exit_code", actual_exit},
        {"timeout_ms", timeout_ms},
        {"mode", "sandbox"},
        {"workdir", cwd}
    };

    if (!tc.empty() && actual_exit == 124) {
        return ToolResult::error(
            fmt::format("Bash (sandbox): {}", params.command.substr(0, 50)),
            fmt::format("Sandbox command timed out after {} seconds\n{}", timeout_sec, output),
            metadata
        );
    }

    const std::string title = params.description.empty()
        ? fmt::format("Bash/sandbox (exit {}): {}", actual_exit, params.command.substr(0, 50))
        : fmt::format("[sandbox] {}", params.description);

    if (actual_exit != 0) {
        return ToolResult{
            .title = title,
            .output = output,
            .metadata = metadata,
            .is_error = true
        };
    }

    return ToolResult::success(title, output, metadata);
}

} // namespace turbot::core::tool::builtin
