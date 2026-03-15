#include <turbot/core/agent/builtin/title_agent.hpp>
#include <turbot/core/provider/provider_manager.hpp>
#include <turbot/core/llm/system_prompt.hpp>
#include <turbot/core/common/logger.hpp>
#include <fmt/format.h>

namespace turbot::core::agent {

TitleAgent::TitleAgent() {
    info_.name = "title";
    info_.description = "Generates brief conversation titles";
    info_.mode = AgentMode::Primary;
    info_.native = true;
    info_.hidden = true;  // Hidden from UI
    info_.temperature = 0.5;
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

    // Get model
    std::string model_id;
    if (info_.model) {
        model_id = info_.model->model_id;
    } else {
        auto models = (*prov_opt)->list_models();
        model_id = models.empty() ? "" : models.front().id;
    }

    if (model_id.empty()) {
        return ExecuteResult::error("TitleAgent: no model available");
    }

    // For title generation, we need to call the LLM directly
    // This is a simplified implementation - in practice you'd use the provider's chat API
    // to generate a completion with the system prompt
    
    // Generate a simple title from the first line of the prompt
    std::string title = params.prompt;
    
    // Take first line and truncate
    size_t newline_pos = title.find('\n');
    if (newline_pos != std::string::npos) {
        title = title.substr(0, newline_pos);
    }
    
    // Truncate to 50 characters
    if (title.length() > 50) {
        title = title.substr(0, 47) + "...";
    }
    
    return ExecuteResult::ok(title, {
        {"agent", "title"},
        {"model", model_id}
    });
}

} // namespace turbot::core::agent
