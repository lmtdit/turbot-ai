#include <catch2/catch_test_macros.hpp>
#include <turbot/core/llm/system_prompt.hpp>
#include <turbot/core/agent/agent.hpp>
#include <fstream>
#include <filesystem>

using namespace turbot::core::llm;
using namespace turbot::core::agent;

// ===== ProviderType Tests =====

TEST_CASE("get_provider_type", "[llm][system_prompt][provider]") {
    SECTION("OpenAI and compatible providers") {
        REQUIRE(get_provider_type("openai") == ProviderType::OpenAI);
        REQUIRE(get_provider_type("azure") == ProviderType::Azure);
        REQUIRE(get_provider_type("openrouter") == ProviderType::OpenRouter);
        REQUIRE(get_provider_type("groq") == ProviderType::Groq);
        REQUIRE(get_provider_type("deepinfra") == ProviderType::DeepInfra);
        REQUIRE(get_provider_type("togetherai") == ProviderType::Together);
        REQUIRE(get_provider_type("together") == ProviderType::Together);
        REQUIRE(get_provider_type("github-copilot") == ProviderType::GitHub);
    }

    SECTION("Anthropic providers") {
        REQUIRE(get_provider_type("anthropic") == ProviderType::Anthropic);
    }

    SECTION("Google providers") {
        REQUIRE(get_provider_type("gemini") == ProviderType::Gemini);
        REQUIRE(get_provider_type("google") == ProviderType::Gemini);
        REQUIRE(get_provider_type("google-vertex") == ProviderType::Gemini);
    }

    SECTION("Amazon providers") {
        REQUIRE(get_provider_type("amazon-bedrock") == ProviderType::Bedrock);
        REQUIRE(get_provider_type("bedrock") == ProviderType::Bedrock);
    }

    SECTION("Other international providers") {
        REQUIRE(get_provider_type("mistral") == ProviderType::Mistral);
        REQUIRE(get_provider_type("deepseek") == ProviderType::DeepSeek);
        REQUIRE(get_provider_type("xai") == ProviderType::XAI);
        REQUIRE(get_provider_type("cohere") == ProviderType::Cohere);
        REQUIRE(get_provider_type("perplexity") == ProviderType::Perplexity);
        REQUIRE(get_provider_type("cerebras") == ProviderType::Cerebras);
    }

    SECTION("Chinese providers") {
        REQUIRE(get_provider_type("bailian") == ProviderType::Bailian);
        REQUIRE(get_provider_type("zhipu") == ProviderType::Zhipu);
        REQUIRE(get_provider_type("kimi") == ProviderType::Kimi);
        REQUIRE(get_provider_type("moonshot") == ProviderType::Kimi);
        REQUIRE(get_provider_type("minimax") == ProviderType::Minimax);
    }

    SECTION("Legacy providers") {
        REQUIRE(get_provider_type("codex") == ProviderType::Codex);
        REQUIRE(get_provider_type("trinity") == ProviderType::Trinity);
    }

    SECTION("unknown provider returns Other") {
        REQUIRE(get_provider_type("unknown_provider") == ProviderType::Other);
        REQUIRE(get_provider_type("") == ProviderType::Other);
    }
}

// ===== Prompt Template Tests =====

TEST_CASE("SystemPrompt::prompt_codex", "[llm][system_prompt][templates]") {
    auto prompt = SystemPrompt::prompt_codex();
    
    REQUIRE_FALSE(prompt.empty());
    REQUIRE(prompt.find("Turbot") != std::string::npos);
    REQUIRE(prompt.find("coding agent") != std::string::npos);
}

TEST_CASE("SystemPrompt::prompt_anthropic", "[llm][system_prompt][templates]") {
    auto prompt = SystemPrompt::prompt_anthropic();
    
    REQUIRE_FALSE(prompt.empty());
    REQUIRE(prompt.find("Turbot") != std::string::npos);
}

TEST_CASE("SystemPrompt::prompt_openai", "[llm][system_prompt][templates]") {
    auto prompt = SystemPrompt::prompt_openai();
    
    REQUIRE_FALSE(prompt.empty());
    REQUIRE(prompt.find("Turbot") != std::string::npos);
}

TEST_CASE("SystemPrompt::prompt_gemini", "[llm][system_prompt][templates]") {
    auto prompt = SystemPrompt::prompt_gemini();
    
    REQUIRE_FALSE(prompt.empty());
    REQUIRE(prompt.find("Turbot") != std::string::npos);
    REQUIRE(prompt.find("Core Mandates") != std::string::npos);
}

TEST_CASE("SystemPrompt::prompt_trinity", "[llm][system_prompt][templates]") {
    auto prompt = SystemPrompt::prompt_trinity();
    
    REQUIRE_FALSE(prompt.empty());
    REQUIRE(prompt.find("Turbot") != std::string::npos);
    REQUIRE(prompt.find("Trinity") != std::string::npos);
}

// ===== Core Methods Tests =====

TEST_CASE("SystemPrompt::instructions", "[llm][system_prompt][core]") {
    auto instr = SystemPrompt::instructions();
    
    REQUIRE_FALSE(instr.empty());
    REQUIRE(instr.find("Turbot") != std::string::npos);
    REQUIRE(instr.find("coding agent") != std::string::npos);
}

TEST_CASE("SystemPrompt::provider_prompt by model", "[llm][system_prompt][provider]") {
    SECTION("GPT-5 uses codex prompt") {
        auto prompt = SystemPrompt::provider_prompt("openai", "gpt-5-turbo");
        // codex fallback contains "coding agent"
        REQUIRE(prompt.find("coding agent") != std::string::npos);
    }

    SECTION("GPT-4 uses beast prompt") {
        auto prompt = SystemPrompt::provider_prompt("openai", "gpt-4-turbo");
        // beast fallback contains "keep going"
        REQUIRE(prompt.find("keep going") != std::string::npos);
    }

    SECTION("O1 models use beast prompt") {
        auto prompt = SystemPrompt::provider_prompt("openai", "o1-preview");
        REQUIRE(prompt.find("keep going") != std::string::npos);
    }

    SECTION("O3 models use beast prompt") {
        auto prompt = SystemPrompt::provider_prompt("openai", "o3-mini");
        REQUIRE(prompt.find("keep going") != std::string::npos);
    }

    SECTION("Claude models use Anthropic prompt") {
        auto prompt = SystemPrompt::provider_prompt("anthropic", "claude-3-opus");
        REQUIRE(prompt.find("Turbot") != std::string::npos);
    }

    SECTION("Gemini models use Gemini prompt") {
        auto prompt = SystemPrompt::provider_prompt("google", "gemini-2.0-flash");
        REQUIRE(prompt.find("Core Mandates") != std::string::npos);
    }

    SECTION("Trinity models use Trinity prompt") {
        auto prompt = SystemPrompt::provider_prompt("trinity", "trinity-1");
        REQUIRE(prompt.find("Trinity") != std::string::npos);
    }

    SECTION("Case insensitive model matching") {
        auto prompt = SystemPrompt::provider_prompt("anthropic", "CLAUDE-3-OPUS");
        REQUIRE(prompt.find("Turbot") != std::string::npos);
    }
}

TEST_CASE("SystemPrompt::provider_prompt by provider", "[llm][system_prompt][provider]") {
    SECTION("OpenAI provider") {
        auto prompt = SystemPrompt::provider_prompt("openai", "unknown-model");
        // beast fallback for OpenAI provider
        REQUIRE(prompt.find("keep going") != std::string::npos);
    }

    SECTION("Anthropic provider") {
        auto prompt = SystemPrompt::provider_prompt("anthropic", "unknown-model");
        // anthropic fallback contains "coding agent"
        REQUIRE(prompt.find("coding agent") != std::string::npos);
    }

    SECTION("Gemini provider") {
        auto prompt = SystemPrompt::provider_prompt("gemini", "unknown-model");
        REQUIRE(prompt.find("Core Mandates") != std::string::npos);
    }

    SECTION("Unknown provider defaults to Qwen-style prompt") {
        auto prompt = SystemPrompt::provider_prompt("unknown", "unknown-model");
        REQUIRE(prompt.find("concise") != std::string::npos);
    }
}

TEST_CASE("SystemPrompt::provider_prompt OpenAI-compatible providers", "[llm][system_prompt][provider]") {
    // Azure, OpenRouter, Groq etc. should use beast prompt
    SECTION("Azure provider") {
        auto prompt = SystemPrompt::provider_prompt("azure", "unknown-model");
        REQUIRE(prompt.find("keep going") != std::string::npos);
    }
    SECTION("OpenRouter provider") {
        auto prompt = SystemPrompt::provider_prompt("openrouter", "unknown-model");
        REQUIRE(prompt.find("keep going") != std::string::npos);
    }
    SECTION("Groq provider") {
        auto prompt = SystemPrompt::provider_prompt("groq", "unknown-model");
        REQUIRE(prompt.find("keep going") != std::string::npos);
    }
    SECTION("DeepSeek provider") {
        auto prompt = SystemPrompt::provider_prompt("deepseek", "unknown-model");
        REQUIRE(prompt.find("keep going") != std::string::npos);
    }
    SECTION("XAI provider") {
        auto prompt = SystemPrompt::provider_prompt("xai", "unknown-model");
        REQUIRE(prompt.find("keep going") != std::string::npos);
    }
}

TEST_CASE("SystemPrompt::provider_prompt Chinese/other providers", "[llm][system_prompt][provider]") {
    SECTION("Bailian provider") {
        auto prompt = SystemPrompt::provider_prompt("bailian", "unknown-model");
        REQUIRE(prompt.find("concise") != std::string::npos);
    }
    SECTION("Kimi provider") {
        auto prompt = SystemPrompt::provider_prompt("kimi", "unknown-model");
        REQUIRE(prompt.find("concise") != std::string::npos);
    }
    SECTION("Zhipu provider") {
        auto prompt = SystemPrompt::provider_prompt("zhipu", "unknown-model");
        REQUIRE(prompt.find("concise") != std::string::npos);
    }
    SECTION("Cohere provider") {
        auto prompt = SystemPrompt::provider_prompt("cohere", "unknown-model");
        REQUIRE(prompt.find("concise") != std::string::npos);
    }
    SECTION("Mistral provider") {
        auto prompt = SystemPrompt::provider_prompt("mistral", "unknown-model");
        REQUIRE(prompt.find("keep going") != std::string::npos);
    }
}

TEST_CASE("SystemPrompt::provider_prompt Bedrock provider", "[llm][system_prompt][provider]") {
    auto prompt = SystemPrompt::provider_prompt("bedrock", "unknown-model");
    // Bedrock uses Anthropic prompt
    REQUIRE(prompt.find("Turbot") != std::string::npos);
}

TEST_CASE("SystemPrompt::provider_prompt Qwen model", "[llm][system_prompt][provider]") {
    auto prompt = SystemPrompt::provider_prompt("openai", "qwen-turbo");
    REQUIRE(prompt.find("concise") != std::string::npos);
}

TEST_CASE("SystemPrompt::prompt file caching via TURBOT_PROMPTS_DIR", "[llm][system_prompt][file]") {
    // Create a temp dir with a prompt file
    namespace fs = std::filesystem;
    auto temp_dir = fs::temp_directory_path() / "turbot_prompt_test";
    fs::create_directories(temp_dir);

    // Write a test prompt file
    std::string prompt_content = "Test prompt for caching verification";
    std::ofstream f(temp_dir / "openai.md");
    f << prompt_content << "\n";
    f.close();

    // Set the env var to point to our temp dir
    setenv("TURBOT_PROMPTS_DIR", temp_dir.c_str(), 1);

    // Note: the prompt cache is a static singleton, so we can't easily clear it
    // in tests. We just verify the env var path is respected on first call.
    // For this test we just verify it compiles and runs without crash.
    auto prompt = SystemPrompt::prompt_openai();
    REQUIRE_FALSE(prompt.empty());

    unsetenv("TURBOT_PROMPTS_DIR");
    fs::remove_all(temp_dir);
}

TEST_CASE("SystemPrompt::environment", "[llm][system_prompt][environment]") {
    SystemPromptParams params;
    params.session_id = "test-session-123";
    params.model_id = "claude-3-opus";
    params.provider_id = "anthropic";
    params.working_directory = "/home/user/project";
    params.is_git_repo = true;
    params.platform = "linux";
    params.current_date = "Mon Jan 15 2026";

    auto env = SystemPrompt::environment(params);

    REQUIRE_FALSE(env.empty());
    REQUIRE(env.find("claude-3-opus") != std::string::npos);
    REQUIRE(env.find("anthropic/claude-3-opus") != std::string::npos);
    REQUIRE(env.find("/home/user/project") != std::string::npos);
    REQUIRE(env.find("yes") != std::string::npos);  // is_git_repo
    REQUIRE(env.find("linux") != std::string::npos);
    REQUIRE(env.find("Mon Jan 15 2026") != std::string::npos);
    REQUIRE(env.find("<env>") != std::string::npos);
    REQUIRE(env.find("</env>") != std::string::npos);
    REQUIRE(env.find("test-session-123") != std::string::npos);  // session_id
}

TEST_CASE("SystemPrompt::environment empty session_id", "[llm][system_prompt][environment]") {
    SystemPromptParams params;
    params.session_id = "";  // empty session_id should not be included
    params.model_id = "gpt-4";
    params.provider_id = "openai";
    params.working_directory = "/tmp/test";
    params.is_git_repo = false;
    params.platform = "darwin";
    params.current_date = "Tue Jan 16 2026";

    auto env = SystemPrompt::environment(params);

    REQUIRE(env.find("Session ID") == std::string::npos);  // should not include session_id line
    REQUIRE(env.find("no") != std::string::npos);  // not a git repo
    REQUIRE(env.find("darwin") != std::string::npos);
}

TEST_CASE("SystemPrompt::environment non-git", "[llm][system_prompt][environment]") {
    SystemPromptParams params;
    params.model_id = "gpt-4";
    params.provider_id = "openai";
    params.working_directory = "/tmp/test";
    params.is_git_repo = false;
    params.platform = "darwin";
    params.current_date = "Tue Jan 16 2026";

    auto env = SystemPrompt::environment(params);

    // Check for "no" as git repo status (not session_id which is empty)
    REQUIRE(env.find("Is directory a git repo: no") != std::string::npos);
    REQUIRE(env.find("darwin") != std::string::npos);
}

TEST_CASE("SystemPrompt::agent_prompt", "[llm][system_prompt][agent]") {
    SECTION("agent with prompt") {
        AgentInfo agent;
        agent.name = "build";
        agent.prompt = "You are a specialized coding assistant.";

        auto prompt = SystemPrompt::agent_prompt(agent);
        REQUIRE(prompt == "You are a specialized coding assistant.");
    }

    SECTION("agent without prompt") {
        AgentInfo agent;
        agent.name = "test";

        auto prompt = SystemPrompt::agent_prompt(agent);
        REQUIRE(prompt.empty());
    }
}

TEST_CASE("SystemPrompt::join_prompts", "[llm][system_prompt][utils]") {
    SECTION("multiple parts") {
        std::vector<std::string> parts = {"Part 1", "Part 2", "Part 3"};
        auto result = SystemPrompt::join_prompts(parts);
        
        REQUIRE(result == "Part 1\n\nPart 2\n\nPart 3");
    }

    SECTION("empty parts are filtered") {
        std::vector<std::string> parts = {"Part 1", "", "Part 2", ""};
        auto result = SystemPrompt::join_prompts(parts);
        
        REQUIRE(result == "Part 1\n\nPart 2");
    }

    SECTION("single part") {
        std::vector<std::string> parts = {"Only part"};
        auto result = SystemPrompt::join_prompts(parts);
        
        REQUIRE(result == "Only part");
    }

    SECTION("all empty parts") {
        std::vector<std::string> parts = {"", "", ""};
        auto result = SystemPrompt::join_prompts(parts);
        
        REQUIRE(result.empty());
    }

    SECTION("empty vector") {
        std::vector<std::string> parts;
        auto result = SystemPrompt::join_prompts(parts);
        
        REQUIRE(result.empty());
    }
}

// ===== Full Build Tests =====

TEST_CASE("SystemPrompt::build basic", "[llm][system_prompt][build]") {
    SystemPromptParams params;
    params.session_id = "test-session";
    params.agent.name = "build";
    params.model_id = "claude-3-opus";
    params.provider_id = "anthropic";
    params.working_directory = "/home/user/project";
    params.is_git_repo = true;
    params.platform = "linux";
    params.current_date = "Mon Jan 15 2026";

    auto prompt = SystemPrompt::build(params);

    REQUIRE_FALSE(prompt.empty());
    // Should contain provider prompt (anthropic fallback contains "coding agent")
    REQUIRE(prompt.find("Turbot") != std::string::npos);
    // Should contain environment
    REQUIRE(prompt.find("/home/user/project") != std::string::npos);
    REQUIRE(prompt.find("claude-3-opus") != std::string::npos);
}

TEST_CASE("SystemPrompt::build with agent prompt", "[llm][system_prompt][build]") {
    SystemPromptParams params;
    params.session_id = "test-session";
    params.agent.name = "custom";
    params.agent.prompt = "You are a specialized testing assistant.";
    params.model_id = "gpt-4";
    params.provider_id = "openai";
    params.working_directory = "/tmp";
    params.platform = "darwin";
    params.current_date = "Tue Jan 16 2026";

    auto prompt = SystemPrompt::build(params);

    REQUIRE_FALSE(prompt.empty());
    // Should use agent prompt instead of provider prompt
    REQUIRE(prompt.find("specialized testing assistant") != std::string::npos);
    // Should still contain environment
    REQUIRE(prompt.find("/tmp") != std::string::npos);
}

TEST_CASE("SystemPrompt::build with custom prompts", "[llm][system_prompt][build]") {
    SystemPromptParams params;
    params.session_id = "test-session";
    params.agent.name = "build";
    params.model_id = "gpt-4";
    params.provider_id = "openai";
    params.working_directory = "/home/user/project";
    params.platform = "linux";
    params.current_date = "Mon Jan 15 2026";
    params.custom_prompts = {"Custom instruction 1", "Custom instruction 2"};

    auto prompt = SystemPrompt::build(params);

    REQUIRE(prompt.find("Custom instruction 1") != std::string::npos);
    REQUIRE(prompt.find("Custom instruction 2") != std::string::npos);
}

TEST_CASE("SystemPrompt::build with user system prompts", "[llm][system_prompt][build]") {
    SystemPromptParams params;
    params.session_id = "test-session";
    params.agent.name = "build";
    params.model_id = "claude-3-opus";
    params.provider_id = "anthropic";
    params.working_directory = "/home/user/project";
    params.platform = "linux";
    params.current_date = "Mon Jan 15 2026";
    params.user_system_prompts = {"User system prompt 1", "User system prompt 2"};

    auto prompt = SystemPrompt::build(params);

    REQUIRE(prompt.find("User system prompt 1") != std::string::npos);
    REQUIRE(prompt.find("User system prompt 2") != std::string::npos);
}

TEST_CASE("SystemPrompt::build complete", "[llm][system_prompt][build]") {
    SystemPromptParams params;
    params.session_id = "complete-test-session";
    params.agent.name = "build";
    params.agent.prompt = "Agent-specific instructions.";
    params.model_id = "claude-3-opus";
    params.provider_id = "anthropic";
    params.working_directory = "/home/user/project";
    params.is_git_repo = true;
    params.platform = "linux";
    params.current_date = "Mon Jan 15 2026";
    params.custom_prompts = {"Custom system prompt"};
    params.user_system_prompts = {"User override prompt"};

    auto prompt = SystemPrompt::build(params);

    // Verify all parts are included
    REQUIRE_FALSE(prompt.empty());
    REQUIRE(prompt.find("Agent-specific instructions") != std::string::npos);
    REQUIRE(prompt.find("Custom system prompt") != std::string::npos);
    REQUIRE(prompt.find("User override prompt") != std::string::npos);
    REQUIRE(prompt.find("/home/user/project") != std::string::npos);
    REQUIRE(prompt.find("claude-3-opus") != std::string::npos);
    REQUIRE(prompt.find("Mon Jan 15 2026") != std::string::npos);
}

// ===== Edge Cases =====

TEST_CASE("SystemPrompt::build empty fields", "[llm][system_prompt][edge]") {
    SystemPromptParams params;
    // All default/empty values
    params.model_id = "unknown-model";
    params.provider_id = "unknown-provider";
    params.platform = "unknown";
    params.current_date = "Unknown Date";

    auto prompt = SystemPrompt::build(params);

    REQUIRE_FALSE(prompt.empty());
    // Should still have a valid prompt with defaults
    REQUIRE(prompt.find("Turbot") != std::string::npos);
}

TEST_CASE("SystemPrompt::build special characters in paths", "[llm][system_prompt][edge]") {
    SystemPromptParams params;
    params.model_id = "gpt-4";
    params.provider_id = "openai";
    params.working_directory = "/home/user/my project with spaces";
    params.platform = "linux";
    params.current_date = "Mon Jan 15 2026";

    auto prompt = SystemPrompt::build(params);

    REQUIRE(prompt.find("/home/user/my project with spaces") != std::string::npos);
}

TEST_CASE("SystemPrompt::build unicode content", "[llm][system_prompt][edge]") {
    SystemPromptParams params;
    params.model_id = "claude-3-opus";
    params.provider_id = "anthropic";
    params.agent.prompt = "你好，这是一个测试。Привет! 🌍";
    params.working_directory = "/home/user/项目";
    params.platform = "darwin";
    params.current_date = "Mon Jan 15 2026";

    auto prompt = SystemPrompt::build(params);

    REQUIRE(prompt.find("你好，这是一个测试") != std::string::npos);
    REQUIRE(prompt.find("Привет") != std::string::npos);
    REQUIRE(prompt.find("/home/user/项目") != std::string::npos);
}

// ============================================================================
// SystemPrompt provider-specific template methods (direct fallback coverage)
// ============================================================================

TEST_CASE("SystemPrompt::prompt_codex returns non-empty", "[llm][system_prompt][template]") {
    auto s = SystemPrompt::prompt_codex();
    REQUIRE_FALSE(s.empty());
}

TEST_CASE("SystemPrompt::prompt_beast returns non-empty", "[llm][system_prompt][template]") {
    auto s = SystemPrompt::prompt_beast();
    REQUIRE_FALSE(s.empty());
}

TEST_CASE("SystemPrompt::prompt_anthropic returns non-empty", "[llm][system_prompt][template]") {
    auto s = SystemPrompt::prompt_anthropic();
    REQUIRE_FALSE(s.empty());
}

TEST_CASE("SystemPrompt::prompt_openai returns non-empty", "[llm][system_prompt][template]") {
    auto s = SystemPrompt::prompt_openai();
    REQUIRE_FALSE(s.empty());
}

TEST_CASE("SystemPrompt::prompt_gemini returns non-empty", "[llm][system_prompt][template]") {
    auto s = SystemPrompt::prompt_gemini();
    REQUIRE_FALSE(s.empty());
}

TEST_CASE("SystemPrompt::prompt_qwen returns non-empty", "[llm][system_prompt][template]") {
    auto s = SystemPrompt::prompt_qwen();
    REQUIRE_FALSE(s.empty());
}

TEST_CASE("SystemPrompt::prompt_trinity returns non-empty", "[llm][system_prompt][template]") {
    auto s = SystemPrompt::prompt_trinity();
    REQUIRE_FALSE(s.empty());
}

TEST_CASE("SystemPrompt::provider_prompt for various providers", "[llm][system_prompt][template]") {
    SECTION("openai provider returns non-empty prompt") {
        auto s = SystemPrompt::provider_prompt("openai", "gpt-4");
        REQUIRE_FALSE(s.empty());
    }

    SECTION("anthropic provider returns non-empty prompt") {
        auto s = SystemPrompt::provider_prompt("anthropic", "claude-3-opus");
        REQUIRE_FALSE(s.empty());
    }

    SECTION("gemini provider returns non-empty prompt") {
        auto s = SystemPrompt::provider_prompt("gemini", "gemini-pro");
        REQUIRE_FALSE(s.empty());
    }

    SECTION("deepseek provider returns non-empty prompt") {
        auto s = SystemPrompt::provider_prompt("deepseek", "deepseek-r1");
        REQUIRE_FALSE(s.empty());
    }

    SECTION("ollama provider returns non-empty prompt") {
        auto s = SystemPrompt::provider_prompt("ollama", "llama3");
        REQUIRE_FALSE(s.empty());
    }

    SECTION("unknown provider returns non-empty prompt (default)") {
        auto s = SystemPrompt::provider_prompt("unknown-provider", "some-model");
        REQUIRE_FALSE(s.empty());
    }
}

TEST_CASE("SystemPrompt::instructions returns non-empty", "[llm][system_prompt]") {
    auto s = SystemPrompt::instructions();
    REQUIRE_FALSE(s.empty());
}
