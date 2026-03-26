// models.cpp - CLI command to list available models
// Aligns with OpenCode `opencode providers` models listing capability

#include <turbot/core/common/logger.hpp>
#include <turbot/core/provider/provider_manager.hpp>
#include <fmt/format.h>
#include <iostream>
#include <string>

namespace turbot::cli {

/// List all available models from all registered providers
int list_models() {
    auto& pm = core::provider::ProviderManager::instance();
    auto models = pm.list_all_models();
    
    if (models.empty()) {
        fmt::print("No models available.\n");
        fmt::print("\nTo configure a provider, create a .turbot/turbot.json file:\n");
        fmt::print("  {{\n");
        fmt::print("    \"providers\": [\n");
        fmt::print("      {{\n");
        fmt::print("        \"name\": \"bailian\",\n");
        fmt::print("        \"type\": \"bailian\",\n");
        fmt::print("        \"api_key\": \"your-api-key\"\n");
        fmt::print("      }}\n");
        fmt::print("    ]\n");
        fmt::print("  }}\n");
        return 0;
    }
    
    fmt::print("Available Models:\n\n");
    fmt::print("{:<40}  {:<15}  {:<10}  {}\n", 
               "Model ID", "Provider", "Context", "Features");
    fmt::print("{}\n", std::string(90, '-'));
    
    for (const auto& model : models) {
        std::string features;
        if (model.capabilities.tool_call) features += "tool ";
        if (model.capabilities.input.image) features += "vision ";
        if (model.capabilities.streaming) features += "stream ";
        if (model.capabilities.reasoning) features += "reason ";
        
        fmt::print("{:<40}  {:<15}  {:<10}  {}\n",
            model.id.substr(0, 39),
            model.provider_id.substr(0, 14),
            fmt::format("{}K", model.context_window / 1024),
            features);
    }
    
    fmt::print("\n{} model(s) found.\n", models.size());
    
    // Show default provider if set
    auto default_provider = pm.get_default_provider();
    if (default_provider) {
        fmt::print("Default provider: {}\n", (*default_provider)->id());
    }
    
    return 0;
}

/// Show details for a specific model
int show_model(const std::string& model_id) {
    auto& pm = core::provider::ProviderManager::instance();
    auto model_opt = pm.find_model(model_id);
    
    if (!model_opt) {
        fmt::print(stderr, "Model not found: {}\n", model_id);
        return 1;
    }
    
    const auto& model = *model_opt;
    
    fmt::print("Model: {}\n", model.id);
    fmt::print("{}\n", std::string(60, '-'));
    fmt::print("  Provider:     {}\n", model.provider_id);
    fmt::print("  Name:         {}\n", model.name);
    fmt::print("  Description:  {}\n", model.description);
    fmt::print("  Context:      {} tokens\n", model.context_window);
    
    fmt::print("\nCapabilities:\n");
    fmt::print("  Temperature:  {}\n", model.capabilities.temperature ? "yes" : "no");
    fmt::print("  Tool Call:    {}\n", model.capabilities.tool_call ? "yes" : "no");
    fmt::print("  Streaming:    {}\n", model.capabilities.streaming ? "yes" : "no");
    fmt::print("  Vision:       {}\n", model.capabilities.input.image ? "yes" : "no");
    fmt::print("  Reasoning:    {}\n", model.capabilities.reasoning ? "yes" : "no");
    fmt::print("  Audio:        {}\n", model.capabilities.input.audio ? "yes" : "no");
    
    if (!model.pricing.empty()) {
        fmt::print("\nPricing (per 1M tokens):\n");
        if (model.pricing.contains("input")) {
            fmt::print("  Input:        ${}\n", model.pricing["input"].get<double>());
        }
        if (model.pricing.contains("output")) {
            fmt::print("  Output:       ${}\n", model.pricing["output"].get<double>());
        }
    }
    
    return 0;
}

} // namespace turbot::cli
