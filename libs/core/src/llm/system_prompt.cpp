#include <turbot/core/llm/system_prompt.hpp>
#include <turbot/utils/string_utils.hpp>
#include <turbot/core/common/logger.hpp>
#include <sstream>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <mutex>

namespace turbot::core::llm {

// ===== Prompt file loading =====

namespace {

std::filesystem::path get_prompts_dir() {
    static std::filesystem::path prompts_dir = []() {
        // Try environment variable first
        if (const char* env_dir = std::getenv("TURBOT_PROMPTS_DIR")) {
            return std::filesystem::path(env_dir);
        }
        // Default: relative to executable or current directory
        return std::filesystem::current_path() / "prompts";
    }();
    return prompts_dir;
}

std::string load_prompt_file(const std::string& name) {
    static std::unordered_map<std::string, std::string> cache;
    static std::mutex cache_mutex;
    
    std::lock_guard<std::mutex> lock(cache_mutex);
    
    auto it = cache.find(name);
    if (it != cache.end()) {
        return it->second;
    }
    
    std::filesystem::path file_path = get_prompts_dir() / (name + ".md");
    
    std::ifstream file(file_path);
    if (!file.is_open()) {
        TURBOT_LOG_WARN("Prompt file not found: {}, using embedded fallback", file_path.string());
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Remove trailing newline if present
    if (!content.empty() && content.back() == '\n') {
        content.pop_back();
    }
    
    cache[name] = content;
    TURBOT_LOG_DEBUG("Loaded prompt file: {} ({} bytes)", file_path.string(), content.size());
    
    return content;
}

} // namespace

// ===== Public template loading API =====

std::string SystemPrompt::load_template(const std::string& name) {
    return load_prompt_file(name);
}

// ===== Provider type detection =====

ProviderType get_provider_type(std::string_view provider_id) noexcept {
    static const std::unordered_map<std::string_view, ProviderType> provider_map = {
        // OpenAI and compatible
        {"openai", ProviderType::OpenAI},
        {"azure", ProviderType::Azure},
        {"openrouter", ProviderType::OpenRouter},
        {"groq", ProviderType::Groq},
        {"deepinfra", ProviderType::DeepInfra},
        {"togetherai", ProviderType::Together},
        {"together", ProviderType::Together},
        {"github-copilot", ProviderType::GitHub},
        
        // Anthropic
        {"anthropic", ProviderType::Anthropic},
        
        // Google
        {"gemini", ProviderType::Gemini},
        {"google", ProviderType::Gemini},
        {"google-vertex", ProviderType::Gemini},
        
        // Amazon
        {"amazon-bedrock", ProviderType::Bedrock},
        {"bedrock", ProviderType::Bedrock},
        
        // Other providers
        {"mistral", ProviderType::Mistral},
        {"deepseek", ProviderType::DeepSeek},
        {"xai", ProviderType::XAI},
        {"cohere", ProviderType::Cohere},
        {"perplexity", ProviderType::Perplexity},
        {"cerebras", ProviderType::Cerebras},
        
        // Chinese providers
        {"bailian", ProviderType::Bailian},
        {"zhipu", ProviderType::Zhipu},
        {"kimi", ProviderType::Kimi},
        {"moonshot", ProviderType::Kimi},
        {"minimax", ProviderType::Minimax},
        
        // Legacy
        {"codex", ProviderType::Codex},
        {"trinity", ProviderType::Trinity}
    };
    
    auto it = provider_map.find(provider_id);
    return it != provider_map.end() ? it->second : ProviderType::Other;
}

// ===== Prompt templates =====

std::string SystemPrompt::prompt_codex() {
    std::string content = load_prompt_file("codex");
    if (!content.empty()) return content;
    // Fallback embedded prompt
    return R"(You are Turbot, the best coding agent on the planet.

You are an interactive CLI tool that helps users with software engineering tasks.)";
}

std::string SystemPrompt::prompt_beast() {
    std::string content = load_prompt_file("beast");
    if (!content.empty()) return content;
    // Fallback embedded prompt
    return R"(You are Turbot, an agent - please keep going until the user's query is completely resolved.

You MUST iterate and keep going until the problem is solved.)";
}

std::string SystemPrompt::prompt_anthropic() {
    std::string content = load_prompt_file("anthropic");
    if (!content.empty()) return content;
    // Fallback embedded prompt
    return R"(You are Turbot, the best coding agent on the planet.

You are an interactive CLI tool that helps users with software engineering tasks.)";
}

std::string SystemPrompt::prompt_openai() {
    std::string content = load_prompt_file("openai");
    if (!content.empty()) return content;
    // Fallback embedded prompt
    return R"(You are Turbot, a powerful coding agent - please keep going until the user's query is completely resolved.

You MUST iterate and keep going until the problem is solved.)";
}

std::string SystemPrompt::prompt_gemini() {
    std::string content = load_prompt_file("gemini");
    if (!content.empty()) return content;
    // Fallback embedded prompt
    return R"(You are Turbot, an interactive CLI agent specializing in software engineering tasks.

# Core Mandates
- Follow existing project conventions.)";
}

std::string SystemPrompt::prompt_qwen() {
    std::string content = load_prompt_file("qwen");
    if (!content.empty()) return content;
    // Fallback embedded prompt
    return R"(You are Turbot, an interactive CLI tool that helps users with software engineering tasks.

You should be concise, direct, and to the point.)";
}

std::string SystemPrompt::prompt_trinity() {
    std::string content = load_prompt_file("trinity");
    if (!content.empty()) return content;
    // Fallback embedded prompt
    return R"(You are Turbot, a powerful coding agent powered by Trinity.

You are an interactive CLI tool that helps users with software engineering tasks.)";
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
    
    // GPT and O1/O3 models use Beast-style prompt
    if (model_lower.contains("gpt-") ||
        model_lower.contains("o1") ||
        model_lower.contains("o3")) {
        return prompt_beast();
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
    
    // Qwen models
    if (model_lower.contains("qwen")) {
        return prompt_qwen();
    }
    
    // Fall back to provider type
    ProviderType type = get_provider_type(provider_id);
    switch (type) {
        // OpenAI-style prompts (OpenAI compatible providers)
        case ProviderType::OpenAI:
            return prompt_beast();  // OpenAI models use beast prompt
        case ProviderType::Azure:
        case ProviderType::OpenRouter:
        case ProviderType::Groq:
        case ProviderType::DeepInfra:
        case ProviderType::Together:
        case ProviderType::GitHub:
        case ProviderType::Cerebras:
        case ProviderType::DeepSeek:
        case ProviderType::XAI:
        case ProviderType::Perplexity:
        case ProviderType::Mistral:
            return prompt_beast();
        
        // Anthropic-style prompts
        case ProviderType::Anthropic:
        case ProviderType::Bedrock:  // Bedrock supports Claude
            return prompt_anthropic();
        
        // Google-style prompts
        case ProviderType::Gemini:
            return prompt_gemini();
        
        // Cohere uses concise style
        case ProviderType::Cohere:
            return prompt_qwen();
        
        // Chinese providers - use Qwen-style concise prompt
        case ProviderType::Bailian:
        case ProviderType::Zhipu:
        case ProviderType::Kimi:
        case ProviderType::Minimax:
            return prompt_qwen();
        
        // Legacy
        case ProviderType::Codex:
            return prompt_codex();
        case ProviderType::Trinity:
            return prompt_trinity();
        
        default:
            return prompt_qwen();  // Default to Qwen-style concise prompt
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
