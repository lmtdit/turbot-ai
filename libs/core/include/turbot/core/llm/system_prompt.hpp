#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/agent/agent.hpp>
#include <string>
#include <vector>

namespace turbot::core::llm {

/// Provider type enumeration for prompt selection
enum class ProviderType {
    OpenAI,      ///< OpenAI GPT models
    Anthropic,   ///< Anthropic Claude models
    Gemini,      ///< Google Gemini models
    Codex,       ///< OpenAI Codex models
    Trinity,     ///< Trinity models
    Other        ///< Other providers
};

/// Convert provider ID to ProviderType
[[nodiscard]] TURBOT_CORE_API ProviderType get_provider_type(std::string_view provider_id) noexcept;

/// System prompt build parameters
struct SystemPromptParams {
    std::string session_id;                        ///< Session identifier
    agent::AgentInfo agent;                        ///< Agent information
    std::string model_id;                          ///< Model identifier (e.g., "gpt-4", "claude-3-opus")
    std::string provider_id;                       ///< Provider identifier (e.g., "openai", "anthropic")
    std::string working_directory;                 ///< Current working directory
    bool is_git_repo = false;                      ///< Whether working directory is a git repository
    std::string platform;                          ///< Platform identifier (e.g., "darwin", "linux")
    std::string current_date;                      ///< Current date string
    std::vector<std::string> custom_prompts;       ///< Additional custom prompts
    std::vector<std::string> user_system_prompts;  ///< User-provided system prompts
};

/// System prompt builder for LLM API calls
///
/// This class constructs system prompts for different LLM providers,
/// following the OpenCode architecture pattern.
///
/// Example usage:
/// @code
/// SystemPromptParams params;
/// params.agent.name = "build";
/// params.model_id = "claude-3-opus";
/// params.provider_id = "anthropic";
///
/// std::string prompt = SystemPrompt::build(params);
/// @endcode
class TURBOT_CORE_API SystemPrompt {
public:
    /// Build complete system prompt
    /// @param params Build parameters
    /// @return Complete system prompt string
    [[nodiscard]] static std::string build(const SystemPromptParams& params);

    /// Get base instructions (codex-style header)
    /// @return Base instruction string
    [[nodiscard]] static std::string instructions();

    /// Get provider-specific prompt based on model
    /// @param provider_id Provider identifier
    /// @param model_id Model identifier
    /// @return Provider-specific prompt string
    [[nodiscard]] static std::string provider_prompt(
        std::string_view provider_id,
        std::string_view model_id
    );

    /// Get environment information prompt
    /// @param params Build parameters
    /// @return Environment information string
    [[nodiscard]] static std::string environment(const SystemPromptParams& params);

    /// Get agent-specific prompt
    /// @param agent Agent information
    /// @return Agent prompt string or empty if not defined
    [[nodiscard]] static std::string agent_prompt(const agent::AgentInfo& agent);

    /// Join multiple prompt parts with double newlines
    /// @param parts Prompt parts to join
    /// @return Joined prompt string
    /// @note May throw std::bad_alloc if memory allocation fails
    [[nodiscard]] static std::string join_prompts(const std::vector<std::string>& parts);

    // ===== Provider-specific prompt templates =====

    /// Get Anthropic Claude prompt template
    [[nodiscard]] static std::string prompt_anthropic();

    /// Get OpenAI GPT prompt template
    [[nodiscard]] static std::string prompt_openai();

    /// Get Gemini prompt template
    [[nodiscard]] static std::string prompt_gemini();

    /// Get Codex-style prompt template
    [[nodiscard]] static std::string prompt_codex();

    /// Get Trinity prompt template
    [[nodiscard]] static std::string prompt_trinity();
};

} // namespace turbot::core::llm
