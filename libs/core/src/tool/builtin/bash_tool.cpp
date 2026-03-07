#include <turbot/core/tool/builtin/bash_tool.hpp>
#include <turbot/core/permission/permission.hpp>
#include <fmt/format.h>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <sys/wait.h>

namespace turbot::core::tool::builtin {

namespace {

/// Escape a string for use as a single-quoted shell argument.
/// Replaces ' with '\'' and wraps in single quotes.
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
        // Common paths for timeout / gtimeout
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
        return {};  // No timeout command available
    }();
    return cmd;
}

} // anonymous namespace

nlohmann::json BashTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"command", {
                {"type", "string"},
                {"description", "The bash command to execute"}
            }},
            {"timeout", {
                {"type", "integer"},
                {"description", "Timeout in seconds (default: 30, max: 300)"},
                {"default", 30}
            }}
        }},
        {"required", nlohmann::json::array({"command"})}
    };
}

bool BashTool::validate_input(const nlohmann::json& input) const {
    return input.contains("command") && input["command"].is_string() &&
           !input["command"].get<std::string>().empty();
}

ToolResult BashTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    if (!validate_input(input)) {
        return ToolResult::error(
            "Bash execution failed",
            "Invalid input: 'command' is required and must be a non-empty string"
        );
    }

    const std::string command = input["command"].get<std::string>();
    const int timeout = input.value("timeout", 30);

    // Clamp timeout to reasonable range
    const int effective_timeout = std::min(std::max(timeout, 1), 300);

    // Check permission via ruleset first
    const auto action = permission::PermissionSystem::evaluate("execute", command, ctx.ruleset);

    if (action == permission::PermissionAction::Deny) {
        return ToolResult::error(
            fmt::format("Bash: {}", command.substr(0, 50)),
            fmt::format("Permission denied for executing command")
        );
    }

    if (action == permission::PermissionAction::Ask) {
        if (!ctx.ask_permission) {
            return ToolResult::error(
                fmt::format("Bash: {}", command.substr(0, 50)),
                "Permission requires user confirmation but no ask_permission callback is set"
            );
        }
        permission::PermissionRequest req;
        req.id         = fmt::format("bash_{:x}", std::hash<std::string>{}(command));
        req.permission = "execute";
        req.patterns   = {command};
        req.tool       = name();

        const auto reply = ctx.ask_permission(req);
        if (reply.type == permission::PermissionReply::Type::Reject) {
            return ToolResult::error(
                fmt::format("Bash: {}", command.substr(0, 50)),
                "User rejected permission for executing command"
            );
        }
    }

    // Check abort flag before execution
    if (ctx.should_abort()) {
        return ToolResult::error("Bash aborted", "Operation was aborted");
    }

    // Escape single quotes and build the shell command
    const std::string escaped_cmd = escape_shell_arg(command);

    // Build command with timeout if available
    std::string full_command;
    const std::string& tc = timeout_command();
    if (!tc.empty()) {
        full_command = fmt::format("{} {} bash -c {} 2>&1", tc, effective_timeout, escaped_cmd);
    } else {
        full_command = fmt::format("bash -c {} 2>&1", escaped_cmd);
    }

    // Execute the command
    std::string output;
    output.reserve(4096);

    std::array<char, 256> buffer{};
    FILE* pipe = popen(full_command.c_str(), "r"); // NOLINT(cert-env33-c)
    if (!pipe) {
        return ToolResult::error(
            fmt::format("Bash: {}", command.substr(0, 50)),
            "Failed to execute command: could not open process"
        );
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
        // Check abort flag during execution
        if (ctx.should_abort()) {
            pclose(pipe);
            return ToolResult::error("Bash aborted", "Operation was aborted during execution");
        }
    }

    const int wait_status = pclose(pipe);
    int actual_exit = -1;
    if (wait_status == -1) {
        return ToolResult::error(
            fmt::format("Bash: {}", command.substr(0, 50)),
            fmt::format("pclose failed: {}", strerror(errno))  // NOLINT(concurrency-mt-unsafe)
        );
    }
    if (WIFEXITED(wait_status)) {         // NOLINT(hicpp-signed-bitwise)
        actual_exit = WEXITSTATUS(wait_status);  // NOLINT(hicpp-signed-bitwise)
    } else if (WIFSIGNALED(wait_status)) { // NOLINT(hicpp-signed-bitwise)
        actual_exit = 128 + WTERMSIG(wait_status); // NOLINT(hicpp-signed-bitwise)
    }

    nlohmann::json metadata = {
        {"command",          command},
        {"exit_code",        actual_exit},
        {"timeout",          effective_timeout},
        {"timeout_enforced", !tc.empty()}
    };

    // Exit code 124 is from GNU timeout / gtimeout indicating timeout occurred
    if (!tc.empty() && actual_exit == 124) {
        return ToolResult::error(
            fmt::format("Bash: {}", command.substr(0, 50)),
            fmt::format("Command timed out after {} seconds\n{}", effective_timeout, output),
            metadata
        );
    }

    const std::string title = fmt::format("Bash (exit {}): {}", actual_exit, command.substr(0, 50));

    if (actual_exit != 0) {
        return ToolResult{
            .title    = title,
            .output   = output,
            .metadata = metadata,
            .is_error = true
        };
    }

    return ToolResult::success(title, output, metadata);
}

} // namespace turbot::core::tool::builtin
