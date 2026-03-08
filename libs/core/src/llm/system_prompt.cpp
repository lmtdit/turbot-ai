#include <turbot/core/llm/system_prompt.hpp>
#include <turbot/utils/string_utils.hpp>
#include <sstream>
#include <unordered_map>

namespace turbot::core::llm {

// ===== Provider type detection =====

ProviderType get_provider_type(std::string_view provider_id) noexcept {
    static const std::unordered_map<std::string_view, ProviderType> provider_map = {
        {"openai", ProviderType::OpenAI},
        {"anthropic", ProviderType::Anthropic},
        {"gemini", ProviderType::Gemini},
        {"google", ProviderType::Gemini},
        {"codex", ProviderType::Codex},
        {"trinity", ProviderType::Trinity}
    };
    
    auto it = provider_map.find(provider_id);
    return it != provider_map.end() ? it->second : ProviderType::Other;
}

// ===== Prompt templates =====

std::string SystemPrompt::prompt_codex() {
    return R"(You are Turbot, a powerful coding agent.

You are an interactive CLI tool that helps users with software engineering tasks. Use the instructions below and the tools available to you to assist the user.

## Editing constraints
- Default to ASCII when editing or creating files. Only introduce non-ASCII or other Unicode characters when there is a clear justification and the file already uses them.
- Only add comments if they are necessary to make a non-obvious block easier to understand.
- Try to use apply_patch for single file edits, but it is fine to explore other options to make the edit if it does not work well.

## Tool usage
- Prefer specialized tools over shell for file operations:
  - Use Read to view files, Edit to modify files, and Write only when needed.
  - Use Glob to find files by name and Grep to search file contents.
- Use Bash for terminal operations (git, cmake, builds, tests, running scripts).
- Run tool calls in parallel when neither call needs the other's output; otherwise run sequentially.

## Git and workspace hygiene
- You may be in a dirty git worktree.
    * NEVER revert existing changes you did not make unless explicitly requested.
    * If asked to make a commit or code edits and there are unrelated changes, don't revert those changes.
    * If the changes are in files you've touched recently, read carefully and understand how you can work with the changes.
- Do not amend commits unless explicitly requested.
- **NEVER** use destructive commands like `git reset --hard` unless specifically requested.

## Tone and style
- Only use emojis if the user explicitly requests it.
- Your output will be displayed on a command line interface. Keep responses short and concise.
- Output text to communicate with the user; all text outside tool use is displayed to the user.
- NEVER create files unless they're absolutely necessary. ALWAYS prefer editing an existing file.

## Professional objectivity
Prioritize technical accuracy and truthfulness over validating the user's beliefs. Focus on facts and problem-solving, providing direct, objective technical info without unnecessary superlatives.
)";
}

std::string SystemPrompt::prompt_anthropic() {
    return R"(You are Turbot, a powerful coding agent.

You are an interactive CLI tool that helps users with software engineering tasks. Use the instructions below and the tools available to assist the user.

IMPORTANT: You must NEVER generate or guess URLs for the user unless you are confident that the URLs are for helping with programming.

# Tone and style
- Only use emojis if the user explicitly requests it. Avoid using emojis in all communication unless asked.
- Your output will be displayed on a command line interface. Keep responses short and concise.
- Output text to communicate with the user; all text you output outside of tool use is displayed to the user.
- NEVER create files unless they're absolutely necessary. ALWAYS prefer editing an existing file.

# Professional objectivity
Prioritize technical accuracy and truthfulness over validating the user's beliefs. Focus on facts and problem-solving, providing direct, objective technical info without any unnecessary superlatives, praise, or emotional validation.

# Task Management
You have access to tools to help you manage and plan tasks. Use these tools VERY frequently to ensure that you are tracking your tasks and giving the user visibility into your progress.

It is critical that you mark tasks as completed as soon as you are done with a task. Do not batch up multiple tasks before marking them as completed.

# Tool usage policy
- When doing file search, prefer to use specialized tools in order to reduce context usage.
- You can call multiple tools in a single response. Make all independent calls in parallel.
- Use specialized tools instead of bash commands when possible.
)";
}

std::string SystemPrompt::prompt_openai() {
    return R"(You are Turbot, a powerful coding agent - please keep going until the user's query is completely resolved, before ending your turn.

Your thinking should be thorough and so it's fine if it's very long. However, avoid unnecessary repetition and verbosity. You should be concise, but thorough.

You MUST iterate and keep going until the problem is solved.

You have everything you need to resolve this problem. Fully solve it autonomously before coming back to the user.

Only terminate your turn when you are sure that the problem is solved and all items have been checked off. Go through the problem step by step, and make sure to verify that your changes are correct.

# Workflow
1. Understand the problem deeply. Carefully read the issue and think critically about what is required.
2. Investigate the codebase. Explore relevant files, search for key functions, and gather context.
3. Develop a clear, step-by-step plan. Break down the fix into manageable, incremental steps.
4. Implement the fix incrementally. Make small, testable code changes.
5. Debug as needed. Use debugging techniques to isolate and resolve issues.
6. Test frequently. Run tests after each change to verify correctness.
7. Iterate until the root cause is fixed and all tests pass.

# Communication Guidelines
Always communicate clearly and concisely in a casual, friendly yet professional tone.
- Respond with clear, direct answers. Use bullet points and code blocks for structure.
- Always write code directly to the correct files.
- Do not display code to the user unless they specifically ask for it.
)";
}

std::string SystemPrompt::prompt_gemini() {
    return R"(You are Turbot, a powerful coding agent.

You are an interactive CLI tool that helps users with software engineering tasks. Use the instructions below and the tools available to assist the user.

## Tool usage
- Prefer specialized tools over shell for file operations.
- Use Bash for terminal operations (git, cmake, builds, tests).
- Run tool calls in parallel when neither call needs the other's output.

## Tone and style
- Only use emojis if the user explicitly requests it.
- Keep responses short and concise.
- NEVER create files unless they're absolutely necessary. ALWAYS prefer editing an existing file.

## Professional objectivity
Prioritize technical accuracy and truthfulness. Focus on facts and problem-solving.
)";
}

std::string SystemPrompt::prompt_trinity() {
    return R"(You are Turbot, a powerful coding agent powered by Trinity.

You are an interactive CLI tool that helps users with software engineering tasks. Use the instructions below and the tools available to assist the user.

## Tone and style
- Only use emojis if the user explicitly requests it.
- Keep responses short and concise.
- NEVER create files unless they're absolutely necessary. ALWAYS prefer editing an existing file.

## Professional objectivity
Prioritize technical accuracy and truthfulness. Focus on facts and problem-solving.
)";
}

// ===== Core methods =====

std::string SystemPrompt::instructions() {
    return prompt_codex();
}

std::string SystemPrompt::provider_prompt(
    std::string_view provider_id,
    std::string_view model_id
) {
    // Check model ID first for specific model prompts
    std::string model_lower = utils::to_lower(model_id);
    
    // GPT-5 uses codex prompt
    if (model_lower.contains("gpt-5")) {
        return prompt_codex();
    }
    
    // GPT and O1/O3 models use OpenAI-style prompt
    if (model_lower.contains("gpt-") ||
        model_lower.contains("o1") ||
        model_lower.contains("o3")) {
        return prompt_openai();
    }
    
    // Gemini models
    if (model_lower.contains("gemini-")) {
        return prompt_gemini();
    }
    
    // Claude models
    if (model_lower.contains("claude")) {
        return prompt_anthropic();
    }
    
    // Trinity models
    if (model_lower.contains("trinity")) {
        return prompt_trinity();
    }
    
    // Fall back to provider type
    ProviderType type = get_provider_type(provider_id);
    switch (type) {
        case ProviderType::OpenAI:
            return prompt_openai();
        case ProviderType::Anthropic:
            return prompt_anthropic();
        case ProviderType::Gemini:
            return prompt_gemini();
        case ProviderType::Codex:
            return prompt_codex();
        case ProviderType::Trinity:
            return prompt_trinity();
        default:
            return prompt_anthropic();  // Default to Anthropic-style prompt
    }
}

std::string SystemPrompt::environment(const SystemPromptParams& params) {
    std::ostringstream oss;
    
    oss << "You are powered by the model named " << params.model_id << ".\n";
    oss << "The exact model ID is " << params.provider_id << "/" << params.model_id << ".\n";
    oss << "\nHere is some useful information about the environment you are running in:\n";
    oss << "<env>\n";
    if (!params.session_id.empty()) {
        oss << "  Session ID: " << params.session_id << "\n";
    }
    oss << "  Working directory: " << params.working_directory << "\n";
    oss << "  Is directory a git repo: " << (params.is_git_repo ? "yes" : "no") << "\n";
    oss << "  Platform: " << params.platform << "\n";
    oss << "  Today's date: " << params.current_date << "\n";
    oss << "</env>";
    
    return oss.str();
}

std::string SystemPrompt::agent_prompt(const agent::AgentInfo& agent) {
    return agent.prompt.value_or("");
}

std::string SystemPrompt::join_prompts(const std::vector<std::string>& parts) {
    // Pre-calculate total size for memory efficiency
    size_t total_size = 0;
    size_t non_empty_count = 0;
    for (const auto& part : parts) {
        if (!part.empty()) {
            total_size += part.size();
            ++non_empty_count;
        }
    }
    
    // Account for separators (double newlines between parts)
    if (non_empty_count > 1) {
        total_size += (non_empty_count - 1) * 2;  // "\n\n" between parts
    }
    
    std::string result;
    result.reserve(total_size);
    bool first = true;
    
    for (const auto& part : parts) {
        if (part.empty()) continue;
        
        if (!first) {
            result += "\n\n";
        }
        result += part;
        first = false;
    }
    
    return result;
}

std::string SystemPrompt::build(const SystemPromptParams& params) {
    std::vector<std::string> parts;
    
    // 1. Provider-specific prompt (or agent prompt if defined)
    // If agent has a custom prompt, use it instead of provider prompt
    if (params.agent.prompt.has_value() && !params.agent.prompt->empty()) {
        parts.push_back(*params.agent.prompt);
    } else {
        parts.push_back(provider_prompt(params.provider_id, params.model_id));
    }
    
    // 2. Custom prompts from caller
    for (const auto& custom : params.custom_prompts) {
        if (!custom.empty()) {
            parts.push_back(custom);
        }
    }
    
    // 3. User-provided system prompts
    for (const auto& user_sys : params.user_system_prompts) {
        if (!user_sys.empty()) {
            parts.push_back(user_sys);
        }
    }
    
    // 4. Environment information
    parts.push_back(environment(params));
    
    return join_prompts(parts);
}

} // namespace turbot::core::llm
