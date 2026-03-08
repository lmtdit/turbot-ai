#include <turbot/core/llm/prompt_builder.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/utils/string_utils.hpp>

namespace turbot::core::llm {

// ===== ToolDefinition =====

nlohmann::json ToolDefinition::to_openai() const {
    return {
        {"type", "function"},
        {"function", {
            {"name", name},
            {"description", description},
            {"parameters", parameters}
        }}
    };
}

nlohmann::json ToolDefinition::to_anthropic() const {
    return {
        {"name", name},
        {"description", description},
        {"input_schema", parameters}
    };
}

// ===== PromptBuildResult =====

nlohmann::json PromptBuildResult::to_json() const {
    return {
        {"system", system},
        {"messages", build_messages_json(MessageFormat::OpenAI)},
        {"tools", build_tools_json(MessageFormat::OpenAI)}
    };
}

nlohmann::json PromptBuildResult::build_messages_json(MessageFormat format) const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& msg : messages) {
        arr.push_back(msg.to_format(format));
    }
    return arr;
}

nlohmann::json PromptBuildResult::build_tools_json(MessageFormat format) const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& tool : tools) {
        if (format == MessageFormat::Anthropic) {
            arr.push_back(tool.to_anthropic());
        } else {
            arr.push_back(tool.to_openai());
        }
    }
    return arr;
}

// ===== PromptBuilder =====

std::string PromptBuilder::build_system(const PromptBuildParams& params) {
    SystemPromptParams sys_params;
    sys_params.session_id = params.session_id;
    sys_params.agent = params.agent;
    sys_params.model_id = params.model_id;
    sys_params.provider_id = params.provider_id;
    sys_params.working_directory = params.working_directory;
    sys_params.is_git_repo = params.is_git_repo;
    sys_params.platform = params.platform;
    sys_params.current_date = params.current_date;
    sys_params.custom_prompts = params.custom_prompts;
    sys_params.user_system_prompts = params.user_system_prompts;
    
    return SystemPrompt::build(sys_params);
}

std::vector<LlmMessage> PromptBuilder::build_messages(
    const PromptBuildParams& params,
    MessageFormat format
) {
    message_builder_.clear();
    message_builder_.set_format(format);
    
    // Add user message
    if (!params.user_message.empty()) {
        message_builder_.add_user(params.user_message);
    }
    
    return message_builder_.get_messages();
}

ToolDefinition PromptBuilder::tool_to_definition(const tool::ToolPtr& tool) {
    if (!tool) {
        throw std::invalid_argument("tool cannot be null");
    }
    ToolDefinition def;
    def.name = tool->name();
    def.description = tool->description();
    def.parameters = tool->input_schema();
    return def;
}

std::vector<ToolDefinition> PromptBuilder::build_tools(
    const std::vector<std::string>& allowed_tools
) {
    std::vector<ToolDefinition> definitions;
    
    auto& registry = tool::ToolRegistry::instance();
    std::vector<tool::ToolPtr> tools;
    
    if (allowed_tools.empty()) {
        // Get all tools
        tools = registry.list();
    } else {
        // Filter by allowed names
        tools = registry.filter(allowed_tools);
    }
    
    definitions.reserve(tools.size());
    for (const auto& tool : tools) {
        definitions.push_back(tool_to_definition(tool));
    }
    
    return definitions;
}

PromptBuildResult PromptBuilder::build(const PromptBuildParams& params) {
    PromptBuildResult result;
    
    // 1. Build system prompt
    result.system = build_system(params);
    
    // 2. Build messages
    result.messages = build_messages(params, params.format);
    
    // 3. Build tool definitions
    result.tools = build_tools(params.allowed_tools);
    
    return result;
}

} // namespace turbot::core::llm
