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

    // Get model
    std::string model_id;
    if (info_.model) {
        model_id = info_.model->model_id;
    } else {
        auto models = (*prov_opt)->list_models();
        model_id = models.empty() ? "" : models.front().id;
    }

    if (model_id.empty()) {
        return ExecuteResult::error("SummaryAgent: no model available");
    }

    // For summary generation, we need to call the LLM directly
    // This is a simplified implementation - in practice you'd use the provider's chat API
    // to generate a completion with the system prompt
    
    // Generate a simple summary
    std::string summary = "Conversation completed.";
    
    // Check if there's context with actual work done
    if (params.context.contains("changes") && !params.context["changes"].empty()) {
        summary = "I made changes to the codebase as requested.";
    }
    
    return ExecuteResult::ok(summary, {
        {"agent", "summary"},
        {"model", model_id}
    });
}

} // namespace turbot::core::agent
