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
    // Try loading from template file first
    std::string template_content = llm::SystemPrompt::load_template("summary");
    if (!template_content.empty()) {
        return template_content;
    }
    
    // Fallback embedded prompt
    return R"(Summarize what was done in this conversation. Write like a pull request description.

Rules:
- 2-3 sentences max
- Describe the changes made, not the process
- Do not mention running tests, builds, or other validation steps
- Do not explain what the user asked for
- Write in first person (I added..., I fixed...)
- Never ask questions or add new questions
- If the conversation ends with an unanswered question to the user, preserve that exact question
- If the conversation ends with an imperative statement or request to the user (e.g. "Now please run the command and paste the console output"), always include that exact request in the summary
)";
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
