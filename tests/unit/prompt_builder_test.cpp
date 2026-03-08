#include <catch2/catch_test_macros.hpp>
#include <turbot/core/llm/prompt_builder.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/tool/builtin/read_file_tool.hpp>

using namespace turbot::core;
using namespace turbot::core::llm;
using namespace turbot::core::tool;
using ReadFileTool = turbot::core::tool::builtin::ReadFileTool;

// ===== ToolDefinition Tests =====

TEST_CASE("ToolDefinition::to_openai", "[llm][prompt_builder][tool_def]") {
    ToolDefinition def;
    def.name = "test_tool";
    def.description = "A test tool";
    def.parameters = {
        {"type", "object"},
        {"properties", {
            {"input", {{"type", "string"}, {"description", "Input value"}}}
        }},
        {"required", {"input"}}
    };

    auto json = def.to_openai();

    REQUIRE(json["type"] == "function");
    REQUIRE(json["function"]["name"] == "test_tool");
    REQUIRE(json["function"]["description"] == "A test tool");
    REQUIRE(json["function"]["parameters"]["type"] == "object");
}

TEST_CASE("ToolDefinition::to_anthropic", "[llm][prompt_builder][tool_def]") {
    ToolDefinition def;
    def.name = "test_tool";
    def.description = "A test tool";
    def.parameters = {
        {"type", "object"},
        {"properties", {
            {"input", {{"type", "string"}}}
        }}
    };

    auto json = def.to_anthropic();

    REQUIRE(json["name"] == "test_tool");
    REQUIRE(json["description"] == "A test tool");
    REQUIRE(json["input_schema"]["type"] == "object");
}

// ===== PromptBuildResult Tests =====

TEST_CASE("PromptBuildResult::to_json", "[llm][prompt_builder][result]") {
    PromptBuildResult result;
    result.system = "You are a helpful assistant.";
    result.messages.push_back(LlmMessage::create_user("Hello"));
    result.messages.push_back(LlmMessage::create_assistant("Hi!"));
    result.tools.push_back({"tool1", "First tool", {{"type", "object"}}});

    auto json = result.to_json();

    REQUIRE(json.contains("system"));
    REQUIRE(json["system"] == "You are a helpful assistant.");
    REQUIRE(json.contains("messages"));
    REQUIRE(json.contains("tools"));
}

TEST_CASE("PromptBuildResult::build_messages_json", "[llm][prompt_builder][result]") {
    PromptBuildResult result;
    result.messages.push_back(LlmMessage::create_system("System prompt"));
    result.messages.push_back(LlmMessage::create_user("Hello"));

    SECTION("OpenAI format") {
        auto json = result.build_messages_json(MessageFormat::OpenAI);
        REQUIRE(json.is_array());
        REQUIRE(json.size() == 2);
        REQUIRE(json[0]["role"] == "system");
        REQUIRE(json[1]["role"] == "user");
    }

    SECTION("Anthropic format") {
        auto json = result.build_messages_json(MessageFormat::Anthropic);
        REQUIRE(json.is_array());
    }
}

TEST_CASE("PromptBuildResult::build_tools_json", "[llm][prompt_builder][result]") {
    PromptBuildResult result;
    result.tools.push_back({"tool1", "First tool", {{"type", "object"}}});
    result.tools.push_back({"tool2", "Second tool", {{"type", "object"}}});

    SECTION("OpenAI format") {
        auto json = result.build_tools_json(MessageFormat::OpenAI);
        REQUIRE(json.is_array());
        REQUIRE(json.size() == 2);
        REQUIRE(json[0]["type"] == "function");
        REQUIRE(json[0]["function"]["name"] == "tool1");
    }

    SECTION("Anthropic format") {
        auto json = result.build_tools_json(MessageFormat::Anthropic);
        REQUIRE(json.is_array());
        REQUIRE(json.size() == 2);
        REQUIRE(json[0]["name"] == "tool1");
        REQUIRE(json[0].contains("input_schema"));
    }
}

// ===== PromptBuilder Tests =====

TEST_CASE("PromptBuilder::build_system", "[llm][prompt_builder][system]") {
    PromptBuildParams params;
    params.session_id = "test-session";
    params.agent.name = "build";
    params.agent.prompt = "You are a coding assistant.";
    params.model_id = "claude-3-opus";
    params.provider_id = "anthropic";
    params.working_directory = "/home/user/project";
    params.is_git_repo = true;
    params.platform = "linux";
    params.current_date = "Mon Jan 15 2026";

    auto system = PromptBuilder::build_system(params);

    REQUIRE_FALSE(system.empty());
    REQUIRE(system.find("coding assistant") != std::string::npos);
    REQUIRE(system.find("claude-3-opus") != std::string::npos);
    REQUIRE(system.find("/home/user/project") != std::string::npos);
}

TEST_CASE("PromptBuilder::build_messages", "[llm][prompt_builder][messages]") {
    PromptBuilder builder;
    PromptBuildParams params;
    params.user_message = "Write a hello world program";

    SECTION("OpenAI format") {
        auto messages = builder.build_messages(params, MessageFormat::OpenAI);
        REQUIRE_FALSE(messages.empty());
        REQUIRE(messages[0].role == LlmRole::User);
        REQUIRE(messages[0].content.has_value());
        REQUIRE(*messages[0].content == "Write a hello world program");
    }

    SECTION("Anthropic format") {
        auto messages = builder.build_messages(params, MessageFormat::Anthropic);
        REQUIRE_FALSE(messages.empty());
    }

    SECTION("Empty user message") {
        params.user_message = "";
        auto messages = builder.build_messages(params, MessageFormat::OpenAI);
        REQUIRE(messages.empty());
    }
}

TEST_CASE("PromptBuilder::build_tools", "[llm][prompt_builder][tools]") {
    PromptBuilder builder;

    ToolRegistry::instance().register_tool(std::make_unique<ReadFileTool>());

    SECTION("All tools") {
        auto tools = builder.build_tools({});
        REQUIRE_FALSE(tools.empty());
        
        bool has_read = false;
        for (const auto& tool : tools) {
            if (tool.name == "read") {
                has_read = true;
                REQUIRE_FALSE(tool.description.empty());
                REQUIRE(tool.parameters.contains("type"));
            }
        }
        REQUIRE(has_read);
    }

    SECTION("Filtered tools") {
        auto tools = builder.build_tools({"read"});
        REQUIRE(tools.size() == 1);
        REQUIRE(tools[0].name == "read");
    }

    SECTION("Non-existent tool filter") {
        auto tools = builder.build_tools({"non_existent_tool"});
        REQUIRE(tools.empty());
    }

    ToolRegistry::instance().clear();
}

TEST_CASE("PromptBuilder::build complete", "[llm][prompt_builder][build]") {
    PromptBuilder builder;
    
    ToolRegistry::instance().register_tool(std::make_unique<ReadFileTool>());

    PromptBuildParams params;
    params.session_id = "test-session";
    params.agent.name = "build";
    params.agent.prompt = "You are a coding assistant.";
    params.model_id = "gpt-4";
    params.provider_id = "openai";
    params.working_directory = "/home/user/project";
    params.is_git_repo = true;
    params.platform = "linux";
    params.current_date = "Mon Jan 15 2026";
    params.user_message = "Write a hello world program";
    params.format = MessageFormat::OpenAI;

    auto result = builder.build(params);

    REQUIRE_FALSE(result.system.empty());
    REQUIRE(result.system.find("coding assistant") != std::string::npos);
    REQUIRE_FALSE(result.messages.empty());
    REQUIRE(result.messages[0].role == LlmRole::User);
    REQUIRE_FALSE(result.tools.empty());

    ToolRegistry::instance().clear();
}

TEST_CASE("PromptBuilder::build with custom prompts", "[llm][prompt_builder][build]") {
    PromptBuilder builder;
    PromptBuildParams params;
    params.session_id = "test-session";
    params.agent.name = "build";
    params.model_id = "claude-3-opus";
    params.provider_id = "anthropic";
    params.working_directory = "/tmp";
    params.platform = "darwin";
    params.current_date = "Mon Jan 15 2026";
    params.user_message = "Hello";
    params.custom_prompts = {"Custom instruction 1", "Custom instruction 2"};
    params.user_system_prompts = {"User system prompt"};

    auto result = builder.build(params);

    REQUIRE(result.system.find("Custom instruction 1") != std::string::npos);
    REQUIRE(result.system.find("Custom instruction 2") != std::string::npos);
    REQUIRE(result.system.find("User system prompt") != std::string::npos);
}

TEST_CASE("PromptBuilder::build with tool filtering", "[llm][prompt_builder][build]") {
    PromptBuilder builder;
    
    ToolRegistry::instance().register_tool(std::make_unique<ReadFileTool>());

    PromptBuildParams params;
    params.session_id = "test-session";
    params.agent.name = "explore";
    params.model_id = "claude-3-opus";
    params.provider_id = "anthropic";
    params.working_directory = "/tmp";
    params.platform = "linux";
    params.current_date = "Mon Jan 15 2026";
    params.user_message = "Explore the codebase";
    params.allowed_tools = {"read"};
    params.format = MessageFormat::Anthropic;

    auto result = builder.build(params);

    REQUIRE(result.tools.size() == 1);
    REQUIRE(result.tools[0].name == "read");

    ToolRegistry::instance().clear();
}

TEST_CASE("PromptBuilder::tool_to_definition", "[llm][prompt_builder][tool]") {
    auto read_tool = std::make_shared<ReadFileTool>();
    auto def = PromptBuilder::tool_to_definition(read_tool);

    REQUIRE(def.name == "read");
    REQUIRE_FALSE(def.description.empty());
    REQUIRE(def.parameters.contains("type"));
    REQUIRE(def.parameters["type"] == "object");
}

TEST_CASE("PromptBuilder::tool_to_definition null", "[llm][prompt_builder][tool]") {
    tool::ToolPtr null_tool;
    REQUIRE_THROWS_AS(PromptBuilder::tool_to_definition(null_tool), std::invalid_argument);
}

// ===== Edge Cases =====

TEST_CASE("PromptBuilder::build empty params", "[llm][prompt_builder][edge]") {
    PromptBuilder builder;
    PromptBuildParams params;

    auto result = builder.build(params);

    REQUIRE_FALSE(result.system.empty());
    REQUIRE(result.messages.empty());
}

TEST_CASE("PromptBuilder::build different providers", "[llm][prompt_builder][provider]") {
    PromptBuilder builder;
    PromptBuildParams params;
    params.session_id = "test";
    params.agent.name = "build";
    params.working_directory = "/tmp";
    params.platform = "linux";
    params.current_date = "Mon Jan 15 2026";
    params.user_message = "Hello";

    SECTION("OpenAI provider") {
        params.provider_id = "openai";
        params.model_id = "gpt-4";

        auto result = builder.build(params);
        REQUIRE(result.system.find("Workflow") != std::string::npos);
    }

    SECTION("Anthropic provider") {
        params.provider_id = "anthropic";
        params.model_id = "claude-3-opus";

        auto result = builder.build(params);
        REQUIRE(result.system.find("Task Management") != std::string::npos);
    }

    SECTION("Gemini provider") {
        params.provider_id = "gemini";
        params.model_id = "gemini-2.0-flash";

        auto result = builder.build(params);
        REQUIRE(result.system.find("Core Mandates") != std::string::npos);
    }
}

TEST_CASE("PromptBuilder::build different agents", "[llm][prompt_builder][agent]") {
    PromptBuilder builder;
    PromptBuildParams params;
    params.session_id = "test";
    params.model_id = "gpt-4";
    params.provider_id = "openai";
    params.working_directory = "/tmp";
    params.platform = "linux";
    params.current_date = "Mon Jan 15 2026";
    params.user_message = "Hello";

    SECTION("Build agent") {
        params.agent.name = "build";
        params.agent.prompt = "You are a build agent.";

        auto result = builder.build(params);
        REQUIRE(result.system.find("build agent") != std::string::npos);
    }

    SECTION("Plan agent") {
        params.agent.name = "plan";
        params.agent.prompt = "You are a planning agent.";

        auto result = builder.build(params);
        REQUIRE(result.system.find("planning agent") != std::string::npos);
    }

    SECTION("Explore agent") {
        params.agent.name = "explore";
        params.agent.prompt = "You are an exploration agent.";

        auto result = builder.build(params);
        REQUIRE(result.system.find("exploration agent") != std::string::npos);
    }
}
