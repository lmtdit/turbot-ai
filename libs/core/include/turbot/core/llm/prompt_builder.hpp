#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/llm/system_prompt.hpp>
#include <turbot/core/llm/tool_schema.hpp>
#include <turbot/core/llm/message_builder.hpp>
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/tool/tool.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace turbot::core::llm {

/// Prompt build parameters
struct PromptBuildParams {
    std::string session_id;                        ///< Session identifier
    agent::AgentInfo agent;                        ///< Agent information
    std::string model_id;                          ///< Model identifier
    std::string provider_id;                       ///< Provider identifier
    std::string working_directory;                 ///< Current working directory
    bool is_git_repo = false;                      ///< Whether working directory is a git repository
    std::string platform;                          ///< Platform identifier
    std::string current_date;                      ///< Current date string
    std::string user_message;                      ///< User's input message
    std::vector<std::string> custom_prompts;       ///< Additional custom prompts
    std::vector<std::string> user_system_prompts;  ///< User-provided system prompts
    std::vector<std::string> allowed_tools;        ///< List of allowed tool names (empty = all)
    MessageFormat format = MessageFormat::OpenAI;  ///< Output message format
};

/// Tool definition for LLM API
struct ToolDefinition {
    std::string name;            ///< Tool name
    std::string description;     ///< Tool description
    nlohmann::json parameters;   ///< JSON Schema for parameters

    /// Convert to OpenAI tool format
    [[nodiscard]] nlohmann::json to_openai() const;
    
    /// Convert to Anthropic tool format
    [[nodiscard]] nlohmann::json to_anthropic() const;
};

/// Prompt build result
struct PromptBuildResult {
    std::string system;                         ///< System prompt
    std::vector<LlmMessage> messages;           ///< Message list for LLM API
    std::vector<ToolDefinition> tools;          ///< Tool definitions

    /// Convert to JSON representation
    [[nodiscard]] nlohmann::json to_json() const;
    
    /// Get messages in specified format
    [[nodiscard]] nlohmann::json build_messages_json(MessageFormat format) const;
    
    /// Get tools in specified format
    [[nodiscard]] nlohmann::json build_tools_json(MessageFormat format) const;
};

/// PromptBuilder - Integrates all components to build complete LLM prompts
///
/// This class combines:
/// - SystemPrompt (2.2) for system prompt generation
/// - ToolSchema (2.3) for tool definitions
/// - MessageBuilder (2.4) for message formatting
///
/// Example usage:
/// @code
/// PromptBuildParams params;
/// params.session_id = "session-123";
/// params.agent.name = "build";
/// params.model_id = "claude-3-opus";
/// params.provider_id = "anthropic";
/// params.user_message = "Write a hello world program";
///
/// PromptBuilder builder;
/// auto result = builder.build(params);
///
/// // Use result.system, result.messages, result.tools
/// @endcode
class TURBOT_CORE_API PromptBuilder {
public:
    /// Build complete prompt for LLM API call
    /// @param params Build parameters
    /// @return Complete prompt with system, messages, and tools
    [[nodiscard]] PromptBuildResult build(const PromptBuildParams& params);

    /// Build system prompt only
    /// @param params Build parameters
    /// @return System prompt string
    [[nodiscard]] static std::string build_system(const PromptBuildParams& params);

    /// Build messages from conversation history
    /// @param params Build parameters
    /// @param format Output format
    /// @return Message list
    [[nodiscard]] std::vector<LlmMessage> build_messages(
        const PromptBuildParams& params,
        MessageFormat format
    );

    /// Build tool definitions
    /// @param allowed_tools List of allowed tool names (empty = all)
    /// @return Tool definitions
    [[nodiscard]] std::vector<ToolDefinition> build_tools(
        const std::vector<std::string>& allowed_tools
    );

    /// Convert Tool to ToolDefinition
    /// @param tool Tool pointer
    /// @return ToolDefinition struct
    [[nodiscard]] static ToolDefinition tool_to_definition(const tool::ToolPtr& tool);

private:
    MessageBuilder message_builder_;
};

} // namespace turbot::core::llm
