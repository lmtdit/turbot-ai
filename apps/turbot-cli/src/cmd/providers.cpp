// providers.cpp - CLI command to manage providers
// Aligns with OpenCode `opencode providers` command capability

#include <turbot/core/common/logger.hpp>
#include <turbot/core/provider/provider_manager.hpp>
#include <turbot/core/provider/impl/bailian_provider.hpp>
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

/// List all registered providers
int list_providers() {
    auto& pm = core::provider::ProviderManager::instance();
    auto providers = pm.list_providers();
    
    if (providers.empty()) {
        fmt::print("No providers configured.\n\n");
        fmt::print("To add a provider:\n");
        fmt::print("  turbot-cli providers add <name> --type <type> --api-key <key>\n\n");
        fmt::print("Supported types: bailian, openai, anthropic, azure, ollama\n");
        return 0;
    }
    
    auto default_provider = pm.get_default_provider();
    std::string default_id;
    if (default_provider) {
        default_id = (*default_provider)->id();
    }
    
    fmt::print("Configured Providers:\n\n");
    fmt::print("{:<20}  {:<15}  {:<10}  {}\n", 
               "Name", "Type", "Default", "Models");
    fmt::print("{}\n", std::string(60, '-'));
    
    for (const auto& provider : providers) {
        auto models = provider->list_models();
        bool is_default = (provider->id() == default_id);
        
        fmt::print("{:<20}  {:<15}  {:<10}  {} model(s)\n",
            provider->id().substr(0, 19),
            provider->id().substr(0, 14),  // Use id as type indicator
            is_default ? "yes" : "",
            models.size());
    }
    
    fmt::print("\n{} provider(s) configured.\n", providers.size());
    if (!default_id.empty()) {
        fmt::print("Default provider: {}\n", default_id);
    }
    
    return 0;
}

/// Add a new provider to the config file
int add_provider(const std::string& name, const std::string& type, 
                 const std::string& api_key, const std::string& base_url,
                 bool set_default) {
    std::string config_path = get_default_config_path();
    nlohmann::json config;
    
    // Load existing config if exists
    if (fs::exists(config_path)) {
        std::ifstream f(config_path);
        if (f.is_open()) {
            try {
                config = nlohmann::json::parse(f);
            } catch (const std::exception& e) {
                fmt::print(stderr, "Failed to parse existing config: {}\n", e.what());
                return 1;
            }
        }
    }
    
    // Ensure providers array exists
    if (!config.contains("providers")) {
        config["providers"] = nlohmann::json::array();
    }
    
    // Check if provider already exists
    for (const auto& p : config["providers"]) {
        if (p.value("name", "") == name) {
            fmt::print(stderr, "Provider '{}' already exists in config.\n", name);
            return 1;
        }
    }
    
    // Add new provider
    nlohmann::json new_provider;
    new_provider["name"] = name;
    new_provider["type"] = type;
    new_provider["api_key"] = api_key;
    if (!base_url.empty()) {
        new_provider["base_url"] = base_url;
    }
    
    config["providers"].push_back(new_provider);
    
    // Create directory if needed
    fs::path p(config_path);
    if (p.has_parent_path() && !fs::exists(p.parent_path())) {
        std::error_code ec;
        if (!fs::create_directories(p.parent_path(), ec)) {
            fmt::print(stderr, "Failed to create directory: {}\n", ec.message());
            return 1;
        }
    }
    
    // Save config
    std::ofstream f(config_path);
    if (!f.is_open()) {
        fmt::print(stderr, "Failed to open config file for writing: {}\n", config_path);
        return 1;
    }
    
    f << config.dump(2) << "\n";
    f.close();
    
    fmt::print("Provider '{}' added to {}.\n", name, config_path);
    
    // Register provider immediately
    auto& pm = core::provider::ProviderManager::instance();
    
    core::provider::ProviderConfig provider_config;
    provider_config.api_key = api_key;
    provider_config.base_url = base_url;
    
    if (type == "bailian") {
        auto provider = std::make_shared<core::provider::BailianProvider>(provider_config);
        // Note: BailianProvider uses the config-provided id, registration sets it
        pm.register_provider(provider);
        if (set_default) {
            pm.set_default_provider(name);
        }
        fmt::print("Provider '{}' registered and ready to use.\n", name);
    } else {
        fmt::print("Note: Provider type '{}' requires restart to take effect.\n", type);
    }
    
    return 0;
}

/// Remove a provider from the config file
int remove_provider(const std::string& name) {
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
    
    if (!config.contains("providers") || !config["providers"].is_array()) {
        fmt::print(stderr, "No providers found in config.\n");
        return 1;
    }
    
    auto& providers = config["providers"];
    bool found = false;
    
    for (auto it = providers.begin(); it != providers.end(); ++it) {
        if (it->value("name", "") == name) {
            providers.erase(it);
            found = true;
            break;
        }
    }
    
    if (!found) {
        fmt::print(stderr, "Provider '{}' not found in config.\n", name);
        return 1;
    }
    
    // Save updated config
    std::ofstream f(config_path);
    if (!f.is_open()) {
        fmt::print(stderr, "Failed to open config file for writing: {}\n", config_path);
        return 1;
    }
    
    f << config.dump(2) << "\n";
    f.close();
    
    // Unregister from memory
    auto& pm = core::provider::ProviderManager::instance();
    pm.unregister_provider(name);
    
    fmt::print("Provider '{}' removed.\n", name);
    return 0;
}

/// Set the default provider
int set_default_provider(const std::string& name) {
    auto& pm = core::provider::ProviderManager::instance();
    
    if (!pm.has_provider(name)) {
        fmt::print(stderr, "Provider '{}' not found.\n", name);
        fmt::print("Use 'turbot-cli providers list' to see available providers.\n");
        return 1;
    }
    
    pm.set_default_provider(name);
    fmt::print("Default provider set to '{}'.\n", name);
    return 0;
}

/// Show provider details
int show_provider(const std::string& name) {
    auto& pm = core::provider::ProviderManager::instance();
    auto provider_opt = pm.get_provider(name);
    
    if (!provider_opt) {
        fmt::print(stderr, "Provider '{}' not found.\n", name);
        return 1;
    }
    
    const auto& provider = *provider_opt;
    auto models = provider->list_models();
    auto default_provider = pm.get_default_provider();
    bool is_default = default_provider && (*default_provider)->id() == name;
    
    fmt::print("Provider: {}\n", name);
    fmt::print("{}\n", std::string(60, '-'));
    fmt::print("  ID:       {}\n", provider->id());
    fmt::print("  Name:     {}\n", provider->name());
    fmt::print("  Default:  {}\n", is_default ? "yes" : "no");
    fmt::print("  Models:   {} available\n", models.size());
    
    if (!models.empty()) {
        fmt::print("\n  Available Models:\n");
        for (const auto& model : models) {
            fmt::print("    - {} (context: {}K)\n", 
                model.id, model.context_window / 1024);
        }
    }
    
    return 0;
}

} // namespace turbot::cli
