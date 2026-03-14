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
}

using namespace turbot;

void print_usage(std::string_view program_name) {
    fmt::print("Usage: {} <command> [options]\n", program_name);
    fmt::print("\nCommands:\n");
    fmt::print("  run [--session <id>]   Run interactive session\n");
    fmt::print("  list                   List sessions\n");
    fmt::print("  acp [--cwd <dir>]      Start ACP server for IDE integration\n");
    fmt::print("  version                Show version information\n");
    fmt::print("  help                   Show this help message\n");
    fmt::print("\nExamples:\n");
    fmt::print("  {} run                  # Start new interactive session\n", program_name);
    fmt::print("  {} list                 # List all sessions\n", program_name);
    fmt::print("  {} run --session sess_xxx  # Resume existing session\n", program_name);
    fmt::print("  {} acp                  # Start ACP server (for Zed/Cursor IDE)\n", program_name);
    fmt::print("  {} acp --cwd /my/proj   # Start ACP server with working directory\n", program_name);
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

    fmt::print(stderr, "Unknown command: {}\n", command);
    print_usage(argv[0]);
    return 1;
}
