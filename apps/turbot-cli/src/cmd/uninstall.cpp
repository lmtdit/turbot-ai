// uninstall.cpp - CLI command to uninstall turbot-cli
// Aligns with OpenCode `opencode uninstall` command capability

#include <turbot/core/common/logger.hpp>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace turbot::cli {

namespace fs = std::filesystem;

/// Get the installation directory
static std::string get_install_dir() {
    // Check common installation locations
    std::vector<std::string> candidates = {
        "/usr/local/bin",
        "/opt/homebrew/bin",
        "/usr/bin",
        fmt::format("{}/.local/bin", std::getenv("HOME") ?: "")
    };
    
    for (const auto& dir : candidates) {
        fs::path exe = fs::path(dir) / "turbot-cli";
        if (fs::exists(exe)) {
            return dir;
        }
    }
    
    return "";
}

/// Show uninstall information
int show_uninstall_info() {
    fmt::print("turbot-cli Uninstall Information\n");
    fmt::print("{}\n\n", std::string(60, '-'));
    
    std::string install_dir = get_install_dir();
    
    if (!install_dir.empty()) {
        fmt::print("Installation directory: {}\n", install_dir);
        fmt::print("Executable: {}/turbot-cli\n\n", install_dir);
    }
    
    fmt::print("Data directories:\n");
    const char* home = std::getenv("HOME");
    if (home) {
        fmt::print("  ~/.turbot/          - Configuration and cache\n");
        fmt::print("  ~/.local/share/turbot/ - Data files\n");
    }
    
    fmt::print("\nTo uninstall turbot-cli:\n");
    fmt::print("  1. Remove the executable:\n");
    fmt::print("     rm -f {}/turbot-cli\n", install_dir.empty() ? "/usr/local/bin" : install_dir);
    fmt::print("  2. Remove data directories (optional):\n");
    if (home) {
        fmt::print("     rm -rf ~/.turbot\n");
        fmt::print("     rm -rf ~/.local/share/turbot\n");
    }
    
    return 0;
}

/// Perform uninstall
int perform_uninstall(bool purge) {
    fmt::print("Uninstalling turbot-cli...\n\n");
    
    std::string install_dir = get_install_dir();
    
    if (install_dir.empty()) {
        fmt::print(stderr, "Could not find turbot-cli installation.\n");
        return 1;
    }
    
    fs::path exe = fs::path(install_dir) / "turbot-cli";
    
    fmt::print("Removing executable: {}\n", exe.string());
    
    std::error_code ec;
    if (!fs::remove(exe, ec)) {
        fmt::print(stderr, "Failed to remove executable: {}\n", ec.message());
        return 1;
    }
    
    fmt::print("Executable removed.\n");
    
    if (purge) {
        const char* home = std::getenv("HOME");
        if (home) {
            fmt::print("\nPurging data directories...\n");
            
            fs::path config_dir = fs::path(home) / ".turbot";
            if (fs::exists(config_dir)) {
                fmt::print("Removing: {}\n", config_dir.string());
                fs::remove_all(config_dir, ec);
            }
            
            fs::path data_dir = fs::path(home) / ".local" / "share" / "turbot";
            if (fs::exists(data_dir)) {
                fmt::print("Removing: {}\n", data_dir.string());
                fs::remove_all(data_dir, ec);
            }
        }
    }
    
    fmt::print("\nturbot-cli has been uninstalled.\n");
    fmt::print("Thank you for using turbot-cli!\n");
    
    return 0;
}

} // namespace turbot::cli
