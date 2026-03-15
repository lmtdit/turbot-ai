#include <turbot/core/agent/builtin/summary_agent.hpp>
#include <turbot/core/provider/provider_manager.hpp>
#include <turbot/core/llm/system_prompt.hpp>
#include <turbot/core/common/logger.hpp>
#include <fmt/format.h>

namespace turbot::core::agent {

SummaryAgent::SummaryAgent() {
    info_.name = "summary";
    info_.description = "Generates conversation summaries";
    info_.mode = AgentMode::Primary;
    info_.native = true;
    info_.hidden = true;  // Hidden from UI
    info_.prompt = get_prompt();
    
    // Deny all tools - summary agent only generates text
    using namespace turbot::core::permission;
    info_.permission.push_back(PermissionRule{"*", "*", PermissionAction::Deny});
}

std::string SummaryAgent::get_prompt() {
    // Load from template file
    std::string template_content = llm::SystemPrompt::load_template("summary");
    if (!template_content.empty()) {
        return template_content;
    }
    
    // Minimal fallback prompt
    return R"(Summarize the changes in 2-3 sentences. Write in first person. Mention key files affected.)";
}

ExecuteResult SummaryAgent::execute(const ExecuteParams& params) {
    auto& pm = provider::ProviderManager::instance();
    auto prov_opt = pm.get_default_provider();
    if (!prov_opt) {
        TURBOT_LOG_WARN("SummaryAgent::execute: no provider configured");
        return ExecuteResult::error("SummaryAgent: no provider configured");
    }

    // Get model - prefer agent's configured model, fallback to first available
    std::string model_id;
    if (info_.model) {
        model_id = info_.model->model_id;
    } else {
        auto models = (*prov_opt)->list_models();
        if (models.empty()) {
            TURBOT_LOG_WARN("SummaryAgent::execute: no model available");
            return ExecuteResult::error("SummaryAgent: no model available");
        }
        model_id = models.front().id;
    }

    // Build messages for summary generation
    std::vector<provider::ChatMessage> messages = {
        provider::ChatMessage::system(info_.prompt.value_or("")),
        provider::ChatMessage::user("Summarize the following conversation:\n\n" + params.prompt)
    };

    // Call LLM API
    provider::ChatOptions options;
    options.temperature = 0.3;  // Lower temperature for consistent summaries
    options.max_tokens = 500;   // Summaries should be concise
    // Disable all tools - summary agent only generates text
    options.tools = {};

    try {
        auto response = (*prov_opt)->chat(messages, model_id, options);
        
        if (response.is_error()) {
            TURBOT_LOG_WARN("SummaryAgent::execute: LLM error: {}", 
                response.error.has_value() ? response.error->dump() : "unknown");
            return ExecuteResult::error("SummaryAgent: LLM call failed");
        }
        
        std::string summary = response.get_text();
        
        if (summary.empty()) {
            summary = "Conversation completed.";
        }
        
        return ExecuteResult::ok(summary, {
            {"agent", "summary"},
            {"model", model_id}
        });
        
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SummaryAgent::execute: exception: {}", e.what());
        return ExecuteResult::error(fmt::format("SummaryAgent: {}", e.what()));
    }
}

} // namespace turbot::core::agent
