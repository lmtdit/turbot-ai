#include <turbot/core/common/version.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/string_utils.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string_view>

// Forward declarations from commands.cpp
namespace turbot::cli {
int run_session(const std::string& session_id);
int list_sessions();
int run_acp(const std::string& cwd);
// Forward declarations from cmd/*.cpp
int list_models();
int show_model(const std::string& model_id);
int list_providers();
int add_provider(const std::string& name, const std::string& type,
                 const std::string& api_key, const std::string& base_url,
                 bool set_default);
int remove_provider(const std::string& name);
int set_default_provider(const std::string& name);
int show_provider(const std::string& name);
int list_config();
int get_config(const std::string& key);
int set_config(const std::string& key, const std::string& value);
int delete_config(const std::string& key);
int show_config_path();
// Session commands
int list_sessions_cmd(int limit, bool json_format);
int show_session(const std::string& session_id);
int delete_session(const std::string& session_id);
int archive_session(const std::string& session_id);
int restore_session(const std::string& session_id);
// MCP commands
int list_mcp_servers();
int add_mcp_server(const std::string& name, const std::string& command_str,
                   const std::string& url, bool enabled);
int remove_mcp_server(const std::string& name);
int show_mcp_server(const std::string& name);
}

using namespace turbot;

void print_usage(std::string_view program_name) {
    fmt::print("Usage: {} <command> [options]\n", program_name);
    fmt::print("\nCommands:\n");
    fmt::print("  run [--session <id>]   Run interactive session\n");
    fmt::print("  list                   List sessions\n");
    fmt::print("  acp [--cwd <dir>]      Start ACP server for IDE integration\n");
    fmt::print("\nProvider Commands:\n");
    fmt::print("  models [model-id]      List available models or show model details\n");
    fmt::print("  providers              List configured providers\n");
    fmt::print("  providers add <name>   Add a provider (use --help for options)\n");
    fmt::print("  providers remove <name> Remove a provider\n");
    fmt::print("  providers default <name> Set default provider\n");
    fmt::print("  providers show <name>  Show provider details\n");
    fmt::print("\nSession Commands:\n");
    fmt::print("  session                List all sessions\n");
    fmt::print("  session show <id>      Show session details\n");
    fmt::print("  session delete <id>    Delete a session\n");
    fmt::print("  session archive <id>   Archive a session\n");
    fmt::print("  session restore <id>   Restore an archived session\n");
    fmt::print("\nMCP Commands:\n");
    fmt::print("  mcp                    List MCP servers\n");
    fmt::print("  mcp add <name>         Add an MCP server (--command or --url)\n");
    fmt::print("  mcp remove <name>      Remove an MCP server\n");
    fmt::print("  mcp show <name>        Show MCP server details\n");
    fmt::print("\nConfig Commands:\n");
    fmt::print("  config                 Show all configuration\n");
    fmt::print("  config get <key>       Get a config value\n");
    fmt::print("  config set <key> <val> Set a config value\n");
    fmt::print("  config delete <key>    Delete a config key\n");
    fmt::print("  config path            Show config file path\n");
    fmt::print("\nOther:\n");
    fmt::print("  version                Show version information\n");
    fmt::print("  help                   Show this help message\n");
    fmt::print("\nExamples:\n");
    fmt::print("  {} run                  # Start new interactive session\n", program_name);
    fmt::print("  {} list                 # List all sessions\n", program_name);
    fmt::print("  {} models               # List available models\n", program_name);
    fmt::print("  {} providers add myai --type bailian --api-key xxx\n", program_name);
    fmt::print("  {} mcp add filesystem --command \"npx -y @anthropic-ai/mcp-server-filesystem\"\n", program_name);
    fmt::print("  {} config set default_model qwen-max\n", program_name);
}

void print_version() {
    fmt::print("{} version {}\n", core::Version::name(), core::Version::string());
    fmt::print("C++ Standard: {}\n", __cplusplus);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string_view command = argv[1];

    if (command == "version" || command == "-v" || command == "--version") {
        print_version();
        return 0;
    }

    if (command == "help" || command == "-h" || command == "--help") {
        print_usage(argv[0]);
        return 0;
    }

    if (command == "run") {
        std::string session_id;
        
        // Parse options
        for (int i = 2; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "--session" && i + 1 < argc) {
                session_id = argv[++i];
            }
        }
        
        return cli::run_session(session_id);
    }

    if (command == "list") {
        return cli::list_sessions();
    }

    if (command == "acp") {
        std::string cwd = ".";
        for (int i = 2; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "--cwd" && i + 1 < argc) {
                cwd = argv[++i];
            }
        }
        return cli::run_acp(cwd);
    }

    // Models command
    if (command == "models") {
        if (argc > 2) {
            return cli::show_model(argv[2]);
        }
        return cli::list_models();
    }

    // Providers command
    if (command == "providers") {
        if (argc < 3) {
            return cli::list_providers();
        }
        std::string_view subcommand = argv[2];
        
        if (subcommand == "add" && argc >= 5) {
            std::string name = argv[3];
            std::string type = "bailian";
            std::string api_key;
            std::string base_url;
            bool set_default = false;
            
            for (int i = 4; i < argc; ++i) {
                std::string_view arg = argv[i];
                if (arg == "--type" && i + 1 < argc) {
                    type = argv[++i];
                } else if (arg == "--api-key" && i + 1 < argc) {
                    api_key = argv[++i];
                } else if (arg == "--base-url" && i + 1 < argc) {
                    base_url = argv[++i];
                } else if (arg == "--default") {
                    set_default = true;
                }
            }
            
            if (api_key.empty()) {
                fmt::print(stderr, "Error: --api-key is required\n");
                return 1;
            }
            return cli::add_provider(name, type, api_key, base_url, set_default);
        }
        
        if (subcommand == "remove" && argc >= 4) {
            return cli::remove_provider(argv[3]);
        }
        
        if (subcommand == "default" && argc >= 4) {
            return cli::set_default_provider(argv[3]);
        }
        
        if (subcommand == "show" && argc >= 4) {
            return cli::show_provider(argv[3]);
        }
        
        fmt::print(stderr, "Unknown providers subcommand: {}\n", subcommand);
        return 1;
    }

    // Config command
    if (command == "config") {
        if (argc < 3) {
            return cli::list_config();
        }
        std::string_view subcommand = argv[2];
        
        if (subcommand == "get" && argc >= 4) {
            return cli::get_config(argv[3]);
        }
        
        if (subcommand == "set" && argc >= 5) {
            return cli::set_config(argv[3], argv[4]);
        }
        
        if (subcommand == "delete" && argc >= 4) {
            return cli::delete_config(argv[3]);
        }
        
        if (subcommand == "path") {
            return cli::show_config_path();
        }
        
        fmt::print(stderr, "Unknown config subcommand: {}\n", subcommand);
        return 1;
    }

    // Session command
    if (command == "session") {
        if (argc < 3) {
            return cli::list_sessions_cmd(0, false);
        }
        std::string_view subcommand = argv[2];
        
        if (subcommand == "show" && argc >= 4) {
            return cli::show_session(argv[3]);
        }
        
        if (subcommand == "delete" && argc >= 4) {
            return cli::delete_session(argv[3]);
        }
        
        if (subcommand == "archive" && argc >= 4) {
            return cli::archive_session(argv[3]);
        }
        
        if (subcommand == "restore" && argc >= 4) {
            return cli::restore_session(argv[3]);
        }
        
        if (subcommand == "list") {
            int limit = 0;
            bool json_format = false;
            for (int i = 3; i < argc; ++i) {
                std::string_view arg = argv[i];
                if ((arg == "-n" || arg == "--limit") && i + 1 < argc) {
                    limit = std::stoi(argv[++i]);
                } else if (arg == "--json") {
                    json_format = true;
                }
            }
            return cli::list_sessions_cmd(limit, json_format);
        }
        
        fmt::print(stderr, "Unknown session subcommand: {}\n", subcommand);
        return 1;
    }

    // MCP command
    if (command == "mcp") {
        if (argc < 3) {
            return cli::list_mcp_servers();
        }
        std::string_view subcommand = argv[2];
        
        if (subcommand == "add" && argc >= 4) {
            std::string name = argv[3];
            std::string command_str;
            std::string url;
            bool enabled = true;
            
            for (int i = 4; i < argc; ++i) {
                std::string_view arg = argv[i];
                if (arg == "--command" && i + 1 < argc) {
                    command_str = argv[++i];
                } else if (arg == "--url" && i + 1 < argc) {
                    url = argv[++i];
                } else if (arg == "--disabled") {
                    enabled = false;
                }
            }
            
            return cli::add_mcp_server(name, command_str, url, enabled);
        }
        
        if (subcommand == "remove" && argc >= 4) {
            return cli::remove_mcp_server(argv[3]);
        }
        
        if (subcommand == "show" && argc >= 4) {
            return cli::show_mcp_server(argv[3]);
        }
        
        if (subcommand == "list" || subcommand == "ls") {
            return cli::list_mcp_servers();
        }
        
        fmt::print(stderr, "Unknown mcp subcommand: {}\n", subcommand);
        return 1;
    }

    fmt::print(stderr, "Unknown command: {}\n", command);
    print_usage(argv[0]);
    return 1;
}
