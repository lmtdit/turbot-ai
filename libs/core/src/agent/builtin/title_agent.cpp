#include <turbot/core/agent/builtin/title_agent.hpp>
#include <turbot/core/provider/provider_manager.hpp>
#include <turbot/core/llm/system_prompt.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/string_utils.hpp>
#include <fmt/format.h>
#include <regex>

namespace turbot::core::agent {

TitleAgent::TitleAgent() {
    info_.name = "title";
    info_.description = "Generates brief conversation titles";
    info_.mode = AgentMode::Primary;
    info_.native = true;
    info_.hidden = true;  // Hidden from UI
    info_.temperature = 0.5;
    info_.steps = 1;  // Single-step execution for title generation
    info_.prompt = get_prompt();
    
    // Deny all tools - title agent only generates text
    using namespace turbot::core::permission;
    info_.permission.push_back(PermissionRule{"*", "*", PermissionAction::Deny});
}

std::string TitleAgent::get_prompt() {
    // Load from template file
    std::string template_content = llm::SystemPrompt::load_template("title");
    if (!template_content.empty()) {
        return template_content;
    }
    
    // Minimal fallback prompt
    return R"(Generate a brief title (8 words or fewer) for this conversation.
Return ONLY the title text - no quotes, no punctuation, no explanation.)";
}

ExecuteResult TitleAgent::execute(const ExecuteParams& params) {
    auto& pm = provider::ProviderManager::instance();
    auto prov_opt = pm.get_default_provider();
    if (!prov_opt) {
        TURBOT_LOG_WARN("TitleAgent::execute: no provider configured");
        return ExecuteResult::error("TitleAgent: no provider configured");
    }

    // Get model - prefer agent's configured model, fallback to first available
    std::string model_id;
    if (info_.model) {
        model_id = info_.model->model_id;
    } else {
        auto models = (*prov_opt)->list_models();
        if (models.empty()) {
            TURBOT_LOG_WARN("TitleAgent::execute: no model available");
            return ExecuteResult::error("TitleAgent: no model available");
        }
        model_id = models.front().id;
    }

    // Build messages for title generation
    std::vector<provider::ChatMessage> messages = {
        provider::ChatMessage::system(info_.prompt.value_or("")),
        provider::ChatMessage::user("Generate a title for this conversation:\n" + params.prompt)
    };

    // Call LLM API
    provider::ChatOptions options;
    options.temperature = info_.temperature.value_or(0.5);
    options.max_tokens = 100;  // Titles should be short
    // Disable all tools - title agent only generates text
    options.tools = {};  

    try {
        auto response = (*prov_opt)->chat(messages, model_id, options);
        
        if (response.is_error()) {
            TURBOT_LOG_WARN("TitleAgent::execute: LLM error: {}", 
                response.error.has_value() ? response.error->dump() : "unknown");
            return ExecuteResult::error("TitleAgent: LLM call failed");
        }
        
        std::string title = response.get_text();
        
        // Clean up the response - remove think tags and get first non-empty line
        // This handles models that output reasoning in <think> tags
        static const std::regex think_pattern(R"(<think>[sS]*?</think>s*)");
        title = std::regex_replace(title, think_pattern, "");
        
        // Get first non-empty line
        auto lines = turbot::utils::split_lines(title);
        std::string cleaned_title;
        for (const auto& line : lines) {
            std::string trimmed = turbot::utils::trim(line);
            if (!trimmed.empty()) {
                cleaned_title = trimmed;
                break;
            }
        }
        
        if (cleaned_title.empty()) {
            cleaned_title = "New conversation";
        }
        
        // Truncate to max 100 characters (matching OpenCode behavior)
        if (cleaned_title.length() > 100) {
            cleaned_title = cleaned_title.substr(0, 97) + "...";
        }
        
        return ExecuteResult::ok(cleaned_title, {
            {"agent", "title"},
            {"model", model_id}
        });
        
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("TitleAgent::execute: exception: {}", e.what());
        return ExecuteResult::error(fmt::format("TitleAgent: {}", e.what()));
    }
}

} // namespace turbot::core::agent
