// config_path.cpp - Configuration path utilities
// Provides centralized config path access to avoid duplication across CLI commands

#include <turbot/core/config/config_path.hpp>
#include <filesystem>

namespace turbot::core::config {

std::string get_default_config_path() {
    return ".turbot/turbot.json";
}

std::string get_default_config_dir() {
    return ".turbot";
}

std::string get_config_path_for_dir(const std::string& directory) {
    std::filesystem::path dir_path(directory);
    return (dir_path / ".turbot" / "turbot.json").string();
}

} // namespace turbot::core::config
