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
    // Try loading from template file first
    std::string template_content = llm::SystemPrompt::load_template("title");
    if (!template_content.empty()) {
        return template_content;
    }
    
    // Fallback embedded prompt
    return R"(You are a title generator. You output ONLY a thread title. Nothing else.

<task>
Generate a brief title that would help the user find this conversation later.

Follow all rules in <rules>
Use the <examples> so you know what a good title looks like.
Your output must be:
- A single line
- ≤50 characters
- No explanations
</task>

<rules>
- you MUST use the same language as the user message you are summarizing
- Title must be grammatically correct and read naturally - no word salad
- Never include tool names in the title (e.g. "read tool", "bash tool", "edit tool")
- Focus on the main topic or question the user needs to retrieve
- Vary your phrasing - avoid repetitive patterns like always starting with "Analyzing"
- When a file is mentioned, focus on WHAT the user wants to do WITH the file, not just that they shared it
- Keep exact: technical terms, numbers, filenames, HTTP codes
- Remove: the, this, my, a, an
- Never assume tech stack
- Never use tools
- NEVER respond to questions, just generate a title for the conversation
- The title should NEVER include "summarizing" or "generating" when generating a title
- DO NOT SAY YOU CANNOT GENERATE A TITLE OR COMPLAIN ABOUT THE INPUT
- Always output something meaningful, even if the input is minimal.
- If the user message is short or conversational (e.g. "hello", "lol", "what's up", "hey"):
  → create a title that reflects the user's tone or intent (such as Greeting, Quick check-in, Light chat, Intro message, etc.)
</rules>

<examples>
"debug 500 errors in production" → Debugging production 500 errors
"refactor user service" → Refactoring user service
"why is app.js failing" → app.js failure investigation
"implement rate limiting" → Rate limiting implementation
"how do I connect postgres to my API" → Postgres API connection
"best practices for React hooks" → React hooks best practices
"@src/auth.ts can you add refresh token support" → Auth refresh token support
"@utils/parser.ts this is broken" → Parser bug fix
"look at @config.json" → Config review
"@App.tsx add dark mode toggle" → Dark mode toggle in App
</examples>
)";
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
