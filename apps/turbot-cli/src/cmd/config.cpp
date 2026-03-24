// config.cpp - CLI command to manage configuration
// Provides config get/set/list capabilities for turbot.json

#include <turbot/core/common/logger.hpp>
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

/// Load config from file
static bool load_config(const std::string& path, nlohmann::json& config) {
    std::ifstream f(path);
    if (!f.is_open()) {
        return false;
    }
    try {
        config = nlohmann::json::parse(f);
        return true;
    } catch (const std::exception& e) {
        fmt::print(stderr, "Failed to parse config: {}\n", e.what());
        return false;
    }
}

/// Save config to file
static bool save_config(const std::string& path, const nlohmann::json& config) {
    // Create directory if needed
    fs::path p(path);
    if (p.has_parent_path() && !fs::exists(p.parent_path())) {
        std::error_code ec;
        if (!fs::create_directories(p.parent_path(), ec)) {
            fmt::print(stderr, "Failed to create directory: {}\n", ec.message());
            return false;
        }
    }
    
    std::ofstream f(path);
    if (!f.is_open()) {
        fmt::print(stderr, "Failed to open config file for writing: {}\n", path);
        return false;
    }
    
    f << config.dump(2) << "\n";
    return true;
}

/// List all configuration
int list_config() {
    std::string config_path = get_default_config_path();
    nlohmann::json config;
    
    if (!fs::exists(config_path)) {
        fmt::print("No configuration file found at: {}\n", config_path);
        fmt::print("\nTo create a configuration:\n");
        fmt::print("  turbot-cli config set <key> <value>\n");
        fmt::print("  turbot-cli providers add <name> --type <type> --api-key <key>\n");
        return 0;
    }
    
    if (!load_config(config_path, config)) {
        return 1;
    }
    
    fmt::print("Configuration ({}):\n\n", config_path);
    fmt::print("{}\n", config.dump(2));
    
    return 0;
}

/// Get a configuration value
int get_config(const std::string& key) {
    std::string config_path = get_default_config_path();
    nlohmann::json config;
    
    if (!fs::exists(config_path)) {
        fmt::print(stderr, "No configuration file found.\n");
        return 1;
    }
    
    if (!load_config(config_path, config)) {
        return 1;
    }
    
    // Support nested key access with dot notation (e.g., "providers.0.name")
    std::vector<std::string> parts;
    size_t start = 0;
    size_t end = key.find('.');
    while (end != std::string::npos) {
        parts.push_back(key.substr(start, end - start));
        start = end + 1;
        end = key.find('.', start);
    }
    parts.push_back(key.substr(start));
    
    nlohmann::json* current = &config;
    for (const auto& part : parts) {
        if (current->is_object()) {
            if (!current->contains(part)) {
                fmt::print(stderr, "Key '{}' not found.\n", key);
                return 1;
            }
            current = &(*current)[part];
        } else if (current->is_array()) {
            try {
                size_t index = std::stoul(part);
                if (index >= current->size()) {
                    fmt::print(stderr, "Index {} out of range.\n", index);
                    return 1;
                }
                current = &(*current)[index];
            } catch (const std::exception&) {
                fmt::print(stderr, "Invalid array index: {}\n", part);
                return 1;
            }
        } else {
            fmt::print(stderr, "Cannot access '{}' on non-object/array value.\n", part);
            return 1;
        }
    }
    
    fmt::print("{}\n", current->dump(2));
    return 0;
}

/// Set a configuration value
int set_config(const std::string& key, const std::string& value) {
    std::string config_path = get_default_config_path();
    nlohmann::json config;
    
    // Load existing config or create empty
    if (fs::exists(config_path)) {
        if (!load_config(config_path, config)) {
            return 1;
        }
    } else {
        config = nlohmann::json::object();
    }
    
    // Support nested key access with dot notation
    std::vector<std::string> parts;
    size_t start = 0;
    size_t end = key.find('.');
    while (end != std::string::npos) {
        parts.push_back(key.substr(start, end - start));
        start = end + 1;
        end = key.find('.', start);
    }
    parts.push_back(key.substr(start));
    
    // Navigate to parent and set value
    nlohmann::json* current = &config;
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        const auto& part = parts[i];
        if (current->is_object()) {
            if (!current->contains(part)) {
                (*current)[part] = nlohmann::json::object();
            }
            current = &(*current)[part];
        } else if (current->is_array()) {
            try {
                size_t index = std::stoul(part);
                if (index >= current->size()) {
                    fmt::print(stderr, "Index {} out of range.\n", index);
                    return 1;
                }
                current = &(*current)[index];
            } catch (const std::exception&) {
                fmt::print(stderr, "Invalid array index: {}\n", part);
                return 1;
            }
        } else {
            fmt::print(stderr, "Cannot access '{}' on non-object/array value.\n", part);
            return 1;
        }
    }
    
    // Try to parse value as JSON, otherwise treat as string
    const std::string& last_key = parts.back();
    try {
        nlohmann::json parsed = nlohmann::json::parse(value);
        (*current)[last_key] = parsed;
    } catch (const std::exception&) {
        (*current)[last_key] = value;
    }
    
    if (!save_config(config_path, config)) {
        return 1;
    }
    
    fmt::print("Set {} = {}\n", key, value);
    return 0;
}

/// Delete a configuration key
int delete_config(const std::string& key) {
    std::string config_path = get_default_config_path();
    nlohmann::json config;
    
    if (!fs::exists(config_path)) {
        fmt::print(stderr, "No configuration file found.\n");
        return 1;
    }
    
    if (!load_config(config_path, config)) {
        return 1;
    }
    
    // Support nested key access with dot notation
    std::vector<std::string> parts;
    size_t start = 0;
    size_t end = key.find('.');
    while (end != std::string::npos) {
        parts.push_back(key.substr(start, end - start));
        start = end + 1;
        end = key.find('.', start);
    }
    parts.push_back(key.substr(start));
    
    // Navigate to parent and delete key
    nlohmann::json* current = &config;
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        const auto& part = parts[i];
        if (current->is_object()) {
            if (!current->contains(part)) {
                fmt::print(stderr, "Key '{}' not found.\n", key);
                return 1;
            }
            current = &(*current)[part];
        } else if (current->is_array()) {
            try {
                size_t index = std::stoul(part);
                if (index >= current->size()) {
                    fmt::print(stderr, "Index {} out of range.\n", index);
                    return 1;
                }
                current = &(*current)[index];
            } catch (const std::exception&) {
                fmt::print(stderr, "Invalid array index: {}\n", part);
                return 1;
            }
        } else {
            fmt::print(stderr, "Cannot access '{}' on non-object/array value.\n", part);
            return 1;
        }
    }
    
    const std::string& last_key = parts.back();
    if (current->is_object()) {
        if (!current->contains(last_key)) {
            fmt::print(stderr, "Key '{}' not found.\n", key);
            return 1;
        }
        current->erase(last_key);
    } else if (current->is_array()) {
        try {
            size_t index = std::stoul(last_key);
            if (index >= current->size()) {
                fmt::print(stderr, "Index {} out of range.\n", index);
                return 1;
            }
            current->erase(current->begin() + static_cast<ptrdiff_t>(index));
        } catch (const std::exception&) {
            fmt::print(stderr, "Invalid array index: {}\n", last_key);
            return 1;
        }
    } else {
        fmt::print(stderr, "Cannot delete from non-object/array value.\n");
        return 1;
    }
    
    if (!save_config(config_path, config)) {
        return 1;
    }
    
    fmt::print("Deleted key: {}\n", key);
    return 0;
}

/// Show config file path
int show_config_path() {
    std::string config_path = get_default_config_path();
    fs::path abs_path = fs::absolute(config_path);
    
    fmt::print("Config path: {}\n", abs_path.string());
    
    if (fs::exists(config_path)) {
        fmt::print("Status: exists\n");
    } else {
        fmt::print("Status: not found\n");
    }
    
    return 0;
}

} // namespace turbot::cli
