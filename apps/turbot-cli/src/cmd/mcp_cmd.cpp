// mcp_cmd.cpp - CLI command to manage MCP servers
// Aligns with OpenCode `opencode mcp` command capability

#include <turbot/core/common/logger.hpp>
#include <turbot/core/mcp/manager.hpp>
#include <turbot/core/mcp/mcp.hpp>
#include <turbot/core/config/config_manager.hpp>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace turbot::cli {

namespace fs = std::filesystem;

/// Get the default config path
static std::string get_default_config_path() {
    return ".turbot/turbot.json";
}

/// Get status icon for MCP status
static std::string get_status_icon(core::mcp::MCPStatus status) {
    switch (status) {
        case core::mcp::MCPStatus::Connected:
            return "✓";
        case core::mcp::MCPStatus::NeedsAuth:
        case core::mcp::MCPStatus::NeedsClientRegistration:
            return "⚠";
        case core::mcp::MCPStatus::Disabled:
        case core::mcp::MCPStatus::Failed:
        default:
            return "✗";
    }
}

/// Get status text for MCP status
static std::string get_status_text(core::mcp::MCPStatus status) {
    switch (status) {
        case core::mcp::MCPStatus::Connected:
            return "connected";
        case core::mcp::MCPStatus::Disabled:
            return "disabled";
        case core::mcp::MCPStatus::Failed:
            return "failed";
        case core::mcp::MCPStatus::NeedsAuth:
            return "needs auth";
        case core::mcp::MCPStatus::NeedsClientRegistration:
            return "needs registration";
        default:
            return "unknown";
    }
}

/// List MCP servers
int list_mcp_servers() {
    auto& manager = core::mcp::MCPManager::instance();
    auto statuses = manager.status();
    
    if (statuses.empty()) {
        fmt::print("No MCP servers configured.\n\n");
        fmt::print("To add an MCP server:\n");
        fmt::print("  turbot-cli mcp add <name> --command <cmd>\n");
        fmt::print("  turbot-cli mcp add <name> --url <url>\n\n");
        fmt::print("Examples:\n");
        fmt::print("  turbot-cli mcp add filesystem --command npx -y @anthropic-ai/mcp-server-filesystem\n");
        fmt::print("  turbot-cli mcp add remote-server --url https://api.example.com/mcp\n");
        return 0;
    }
    
    fmt::print("MCP Servers:\n\n");
    fmt::print("{:<20}  {:<12}  {:<10}  {}\n", 
               "Name", "Status", "Type", "Details");
    fmt::print("{}\n", std::string(70, '-'));
    
    for (const auto& [name, status] : statuses) {
        std::string icon = get_status_icon(status);
        std::string text = get_status_text(status);
        
        // Try to get server type from config
        std::string type = "local";
        std::string details;
        
        // Get tools count if connected
        if (status == core::mcp::MCPStatus::Connected) {
            auto tools_future = manager.tools();
            auto tools = tools_future.get();
            int tool_count = 0;
            for (const auto& [tool_name, _] : tools) {
                if (tool_name.find(name) == 0) {
                    tool_count++;
                }
            }
            details = fmt::format("{} tool(s)", tool_count);
        }
        
        fmt::print("{:<20}  {} {:<10}  {:<10}  {}\n",
            name.substr(0, 19),
            icon, text.substr(0, 10),
            type,
            details);
    }
    
    fmt::print("\n{} server(s) configured.\n", statuses.size());
    return 0;
}

/// Add an MCP server (local or remote)
int add_mcp_server(const std::string& name, const std::string& command_str,
                   const std::string& url, bool enabled) {
    // Load config
    std::string config_path = get_default_config_path();
    nlohmann::json config;
    
    if (fs::exists(config_path)) {
        std::ifstream f(config_path);
        if (f.is_open()) {
            try {
                config = nlohmann::json::parse(f);
            } catch (const std::exception& e) {
                fmt::print(stderr, "Failed to parse config: {}\n", e.what());
                return 1;
            }
        }
    } else {
        config = nlohmann::json::object();
    }
    
    // Ensure mcp object exists
    if (!config.contains("mcp")) {
        config["mcp"] = nlohmann::json::object();
    }
    
    // Check if already exists
    if (config["mcp"].contains(name)) {
        fmt::print(stderr, "MCP server '{}' already exists. Use 'mcp remove {}' first.\n", name, name);
        return 1;
    }
    
    // Create MCP config
    nlohmann::json mcp_config;
    
    if (!command_str.empty()) {
        // Local MCP server
        mcp_config["type"] = "local";
        
        // Parse command string into array
        std::vector<std::string> args;
        std::string current;
        bool in_quote = false;
        for (char c : command_str) {
            if (c == '"') {
                in_quote = !in_quote;
            } else if (c == ' ' && !in_quote) {
                if (!current.empty()) {
                    args.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            args.push_back(current);
        }
        
        mcp_config["command"] = args;
    } else if (!url.empty()) {
        // Remote MCP server
        mcp_config["type"] = "remote";
        mcp_config["url"] = url;
    } else {
        fmt::print(stderr, "Error: Must specify either --command or --url\n");
        return 1;
    }
    
    mcp_config["enabled"] = enabled;
    
    // Save to config
    config["mcp"][name] = mcp_config;
    
    // Create directory if needed
    fs::path p(config_path);
    if (p.has_parent_path() && !fs::exists(p.parent_path())) {
        std::error_code ec;
        if (!fs::create_directories(p.parent_path(), ec)) {
            fmt::print(stderr, "Failed to create directory: {}\n", ec.message());
            return 1;
        }
    }
    
    std::ofstream f(config_path);
    if (!f.is_open()) {
        fmt::print(stderr, "Failed to open config file for writing: {}\n", config_path);
        return 1;
    }
    
    f << config.dump(2) << "\n";
    f.close();
    
    fmt::print("MCP server '{}' added to config.\n", name);
    fmt::print("Run 'turbot-cli run' to connect and use the server.\n");
    
    return 0;
}

/// Remove an MCP server
int remove_mcp_server(const std::string& name) {
    std::string config_path = get_default_config_path();
    
    if (!fs::exists(config_path)) {
        fmt::print(stderr, "Config file not found: {}\n", config_path);
        return 1;
    }
    
    nlohmann::json config;
    {
        std::ifstream f(config_path);
        if (!f.is_open()) {
            fmt::print(stderr, "Failed to open config file: {}\n", config_path);
            return 1;
        }
        try {
            config = nlohmann::json::parse(f);
        } catch (const std::exception& e) {
            fmt::print(stderr, "Failed to parse config: {}\n", e.what());
            return 1;
        }
    }
    
    if (!config.contains("mcp") || !config["mcp"].contains(name)) {
        fmt::print(stderr, "MCP server '{}' not found in config.\n", name);
        return 1;
    }
    
    config["mcp"].erase(name);
    
    std::ofstream f(config_path);
    if (!f.is_open()) {
        fmt::print(stderr, "Failed to open config file for writing: {}\n", config_path);
        return 1;
    }
    
    f << config.dump(2) << "\n";
    f.close();
    
    // Also remove from manager
    auto& manager = core::mcp::MCPManager::instance();
    manager.remove(name);
    
    fmt::print("MCP server '{}' removed.\n", name);
    return 0;
}

/// Show MCP server details
int show_mcp_server(const std::string& name) {
    auto& manager = core::mcp::MCPManager::instance();
    auto statuses = manager.status();
    
    if (statuses.find(name) == statuses.end()) {
        fmt::print(stderr, "MCP server '{}' not found.\n", name);
        return 1;
    }
    
    auto status = statuses[name];
    
    fmt::print("MCP Server: {}\n", name);
    fmt::print("{}\n", std::string(60, '-'));
    fmt::print("  Status:  {} {}\n", get_status_icon(status), get_status_text(status));
    
    // Load config for more details
    std::string config_path = get_default_config_path();
    if (fs::exists(config_path)) {
        std::ifstream f(config_path);
        if (f.is_open()) {
            try {
                nlohmann::json config = nlohmann::json::parse(f);
                if (config.contains("mcp") && config["mcp"].contains(name)) {
                    auto& cfg = config["mcp"][name];
                    fmt::print("  Type:    {}\n", cfg.value("type", "unknown"));
                    
                    if (cfg.contains("command")) {
                        std::string cmd_str;
                        for (const auto& arg : cfg["command"]) {
                            if (!cmd_str.empty()) cmd_str += " ";
                            cmd_str += arg.get<std::string>();
                        }
                        fmt::print("  Command: {}\n", cmd_str);
                    }
                    
                    if (cfg.contains("url")) {
                        fmt::print("  URL:     {}\n", cfg["url"].get<std::string>());
                    }
                    
                    fmt::print("  Enabled: {}\n", cfg.value("enabled", true) ? "yes" : "no");
                }
            } catch (...) {
                // Ignore config errors
            }
        }
    }
    
    // Show tools if connected
    if (status == core::mcp::MCPStatus::Connected) {
        auto tools_future = manager.tools();
        auto tools = tools_future.get();
        
        fmt::print("\n  Available Tools:\n");
        int count = 0;
        for (const auto& [tool_name, tool_info] : tools) {
            if (tool_name.find(name) == 0) {
                fmt::print("    - {}\n", tool_name);
                count++;
            }
        }
        if (count == 0) {
            fmt::print("    (none)\n");
        }
    }
    
    return 0;
}

} // namespace turbot::cli
