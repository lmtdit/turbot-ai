#include <turbot/core/tool/external_command_tool.hpp>
#include <turbot/core/common/logger.hpp>
#include <fstream>
#include <sstream>
#include <regex>
#include <array>
#include <cstdio>
#include <chrono>
#include <thread>
#include <filesystem>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <signal.h>
#include <cerrno>
#include <cstring>

namespace turbot::core::tool {

// ============================================================================
// CustomToolConfig Implementation
// ============================================================================

std::optional<CustomToolConfig> CustomToolConfig::from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        TURBOT_LOG_ERROR("Failed to open custom tool config: {}", path);
        return std::nullopt;
    }
    
    try {
        nlohmann::json j;
        file >> j;
        auto config = from_json(j, path);
        if (config) {
            config->source_path = path;
        }
        return config;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("Failed to parse custom tool config {}: {}", path, e.what());
        return std::nullopt;
    }
}

std::optional<CustomToolConfig> CustomToolConfig::from_json(const nlohmann::json& j, const std::string& source_path) {
    // Required fields
    if (!j.contains("name") || !j["name"].is_string()) {
        TURBOT_LOG_ERROR("Custom tool config missing required 'name' field");
        return std::nullopt;
    }
    
    if (!j.contains("command") || !j["command"].is_string()) {
        TURBOT_LOG_ERROR("Custom tool config missing required 'command' field");
        return std::nullopt;
    }
    
    CustomToolConfig config;
    config.name = j["name"].get<std::string>();
    config.command = j["command"].get<std::string>();
    config.source_path = source_path;
    
    // Optional fields
    if (j.contains("description") && j["description"].is_string()) {
        config.description = j["description"].get<std::string>();
    } else {
        config.description = "Custom tool: " + config.name;
    }
    
    if (j.contains("input_schema") && j["input_schema"].is_object()) {
        config.input_schema = j["input_schema"];
    } else {
        // Default empty schema
        config.input_schema = {
            {"type", "object"},
            {"properties", nlohmann::json::object()}
        };
    }
    
    if (j.contains("args") && j["args"].is_array()) {
        for (const auto& arg : j["args"]) {
            if (arg.is_string()) {
                config.args.push_back(arg.get<std::string>());
            }
        }
    }
    
    if (j.contains("timeout_seconds") && j["timeout_seconds"].is_number()) {
        config.timeout_seconds = j["timeout_seconds"].get<int>();
    }
    
    if (j.contains("working_dir") && j["working_dir"].is_string()) {
        config.working_dir = j["working_dir"].get<std::string>();
    }
    
    if (j.contains("enabled") && j["enabled"].is_boolean()) {
        config.enabled = j["enabled"].get<bool>();
    }
    
    return config;
}

nlohmann::json CustomToolConfig::to_json() const {
    nlohmann::json j;
    j["name"] = name;
    j["description"] = description;
    j["command"] = command;
    j["input_schema"] = input_schema;
    
    if (!args.empty()) {
        j["args"] = args;
    }
    if (timeout_seconds) {
        j["timeout_seconds"] = *timeout_seconds;
    }
    if (working_dir) {
        j["working_dir"] = *working_dir;
    }
    j["enabled"] = enabled;
    
    return j;
}

// ============================================================================
// ExternalCommandTool Implementation
// ============================================================================

ExternalCommandTool::ExternalCommandTool(const CustomToolConfig& config)
    : config_(config) {
}

std::string ExternalCommandTool::name() const {
    return config_.name;
}

std::string ExternalCommandTool::description() const {
    return config_.description;
}

nlohmann::json ExternalCommandTool::input_schema() const {
    return config_.input_schema;
}

std::vector<std::string> ExternalCommandTool::substitute_args(const nlohmann::json& input) const {
    std::vector<std::string> result;
    
    // Regex to match ${param} or ${param.nested} patterns
    static const std::regex placeholder_regex(R"(\$\{([^}]+)\})");
    
    for (const auto& arg : config_.args) {
        std::string substituted = arg;
        std::smatch match;
        
        // Find all placeholders and substitute
        std::string result_str;
        std::string::const_iterator search_start(arg.cbegin());
        
        while (std::regex_search(search_start, arg.cend(), match, placeholder_regex)) {
            result_str += match.prefix().str();
            
            std::string path = match[1].str();
            
            // Navigate the JSON to find the value
            nlohmann::json value = input;
            bool found = true;
            
            // Split path by '.' and navigate
            std::istringstream path_stream(path);
            std::string segment;
            while (std::getline(path_stream, segment, '.')) {
                if (value.is_object() && value.contains(segment)) {
                    value = value[segment];
                } else {
                    found = false;
                    break;
                }
            }
            
            if (found && value.is_string()) {
                result_str += value.get<std::string>();
            } else if (found && value.is_number()) {
                result_str += value.dump();
            } else if (found) {
                result_str += value.dump();
            } else {
                // Keep placeholder if not found
                result_str += match[0].str();
            }
            
            search_start = match.suffix().first;
        }
        result_str += std::string(search_start, arg.cend());
        
        result.push_back(result_str);
    }
    
    return result;
}

ToolResult ExternalCommandTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    // Check for abort
    if (ctx.should_abort()) {
        return ToolResult::error(config_.name, "Execution aborted");
    }
    
    // Build command arguments
    std::vector<std::string> cmd_args = substitute_args(input);
    
    // Determine and validate working directory
    std::string working_dir = ctx.working_directory;
    if (config_.working_dir) {
        std::string wd = *config_.working_dir;
        // If relative path, make it relative to project directory
        if (!wd.empty() && wd[0] != '/') {
            wd = ctx.working_directory + "/" + wd;
        }
        // Validate and canonicalize path
        std::error_code ec;
        auto canonical = std::filesystem::canonical(wd, ec);
        if (ec) {
            return ToolResult::error(
                config_.name,
                "Invalid working directory '" + *config_.working_dir + "': " + ec.message()
            );
        }
        if (!std::filesystem::is_directory(canonical)) {
            return ToolResult::error(
                config_.name,
                "Working directory is not a directory: " + canonical.string()
            );
        }
        working_dir = canonical.string();
    }
    
    // Determine timeout
    int timeout = ctx.tool_config.default_timeout;
    if (config_.timeout_seconds) {
        timeout = *config_.timeout_seconds;
    }
    
    return run_command(cmd_args, working_dir, timeout, ctx);
}

ToolResult ExternalCommandTool::run_command(
    const std::vector<std::string>& cmd_args,
    const std::string& working_dir,
    int timeout_seconds,
    ToolContext& ctx
) {
    // Log the command being executed (without shell interpolation)
    std::string cmd_log = config_.command;
    for (const auto& arg : cmd_args) {
        cmd_log += " [" + arg + "]";
    }
    TURBOT_LOG_INFO("ExternalCommandTool '{}' executing: {}", config_.name, cmd_log);
    
    // Create pipes for stdout capture
    int stdout_pipe[2];
    if (pipe(stdout_pipe) < 0) {
        return ToolResult::error(
            config_.name,
            "Failed to create pipe: " + std::string(std::strerror(errno))
        );
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return ToolResult::error(
            config_.name,
            "Failed to fork process: " + std::string(std::strerror(errno))
        );
    }
    
    if (pid == 0) {
        // Child process
        close(stdout_pipe[0]);  // Close read end
        dup2(stdout_pipe[1], STDOUT_FILENO);  // Redirect stdout to pipe
        dup2(stdout_pipe[1], STDERR_FILENO);  // Redirect stderr to pipe
        close(stdout_pipe[1]);
        
        // Change working directory
        if (!working_dir.empty() && chdir(working_dir.c_str()) < 0) {
            std::cerr << "Failed to change directory to " << working_dir << ": " << std::strerror(errno) << std::endl;
            _exit(127);
        }
        
        // Build argv array (command + args + nullptr)
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(config_.command.c_str()));
        for (const auto& arg : cmd_args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        
        // Execute command directly (no shell interpolation)
        execvp(config_.command.c_str(), argv.data());
        
        // If execvp returns, it failed
        std::cerr << "Failed to execute '" << config_.command << "': " << std::strerror(errno) << std::endl;
        _exit(127);
    }
    
    // Parent process
    close(stdout_pipe[1]);  // Close write end
    
    // Read output with timeout checking
    std::string output;
    std::array<char, 4096> buffer;
    auto start_time = std::chrono::steady_clock::now();
    bool timed_out = false;
    
    while (true) {
        // Check for abort
        if (ctx.should_abort()) {
            close(stdout_pipe[0]);
            kill(pid, SIGTERM);
            waitpid(pid, nullptr, 0);
            return ToolResult::error(config_.name, "Execution aborted");
        }
        
        // Check timeout
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time
        ).count();
        if (elapsed >= timeout_seconds) {
            timed_out = true;
            break;
        }
        
        // Non-blocking read with timeout
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(stdout_pipe[0], &read_fds);
        
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms
        
        int ready = select(stdout_pipe[0] + 1, &read_fds, nullptr, nullptr, &tv);
        if (ready < 0) {
            if (errno == EINTR) continue;
            break;  // Error
        }
        if (ready == 0) {
            // Timeout on select, continue checking abort/timeout
            continue;
        }
        
        ssize_t bytes_read = read(stdout_pipe[0], buffer.data(), buffer.size() - 1);
        if (bytes_read <= 0) {
            break;  // EOF or error
        }
        buffer[bytes_read] = '\0';
        output += buffer.data();
    }
    
    close(stdout_pipe[0]);
    
    if (timed_out) {
        kill(pid, SIGTERM);
        waitpid(pid, nullptr, 0);
        return ToolResult::error(
            config_.name,
            "Command timed out after " + std::to_string(timeout_seconds) + " seconds"
        );
    }
    
    // Wait for child process
    int status;
    waitpid(pid, &status, 0);
    
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    
    // Trim trailing whitespace
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r' || output.back() == ' ')) {
        output.pop_back();
    }
    
    if (exit_code != 0) {
        return ToolResult::error(
            config_.name,
            "Command '" + config_.command + "' exited with code " + std::to_string(exit_code) +
            (output.empty() ? "" : ": " + output)
        );
    }
    
    nlohmann::json metadata = {
        {"command", config_.command},
        {"args", cmd_args},
        {"working_dir", working_dir},
        {"exit_code", exit_code},
        {"source", config_.source_path}
    };
    
    return ToolResult::success(config_.name + " completed", output, metadata);
}

} // namespace turbot::core::tool
