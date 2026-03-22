/**
 * @file agent_execution_test.cpp
 * @brief Agent execution tests using MockProvider
 *
 * Tests for:
 * - Agent execution with mock LLM responses
 * - Tool call handling in agents
 * - Agent error handling
 * - Multi-step agent execution
 */

#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include "../mock/mock_provider.hpp"
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/agent/builtin/build_agent.hpp>
#include <turbot/core/agent/builtin/explore_agent.hpp>
#include <turbot/core/agent/builtin/plan_agent.hpp>
#include <turbot/core/agent/builtin/title_agent.hpp>
#include <turbot/core/agent/builtin/summary_agent.hpp>
#include <turbot/core/permission/permission.hpp>
#include <turbot/core/provider/provider_manager.hpp>

using namespace turbot::core::agent;
using namespace turbot::core::permission;
using namespace turbot::core::provider;
using namespace turbot::test;

// ==================== Test Fixtures ====================

class AgentExecutionFixture {
public:
    AgentExecutionFixture() {
        // Ensure valid current directory
        try {
            std::filesystem::current_path(std::filesystem::path("/tmp"));
        } catch (...) {}
        
        // Clear registry before each test
        AgentRegistry::instance().clear();
        
        // Create mock provider
        mock_provider_ = std::make_shared<MockProvider>();
        mock_provider_->set_api_key("test-key");
    }

    ~AgentExecutionFixture() {
        // Clean up after test
        AgentRegistry::instance().clear();
    }
    
    std::shared_ptr<MockProvider> mock_provider_;
};

// ==================== Agent ExecuteParams Tests ====================

TEST_CASE("Agent.ExecuteParams.Defaults", "[Agent][Execution]") {
    ExecuteParams params;
    
    REQUIRE(params.session_id.empty());
    REQUIRE(params.prompt.empty());
    REQUIRE(params.context.is_object());
    REQUIRE_FALSE(params.model_override.has_value());
}

TEST_CASE("Agent.ExecuteParams.WithValues", "[Agent][Execution]") {
    ExecuteParams params;
    params.session_id = "session-123";
    params.prompt = "Write a hello world program";
    params.context = R"({"language": "cpp"})"_json;
    params.model_override = "gpt-4";
    
    REQUIRE(params.session_id == "session-123");
    REQUIRE(params.prompt == "Write a hello world program");
    REQUIRE(params.context["language"] == "cpp");
    REQUIRE(params.model_override == "gpt-4");
}

// ==================== Agent ExecuteResult Tests ====================

TEST_CASE("Agent.ExecuteResult.Success", "[Agent][Execution]") {
    auto result = ExecuteResult::ok("Task completed successfully");
    
    REQUIRE(result.is_success);
    REQUIRE(result.output == "Task completed successfully");
    REQUIRE_FALSE(result.error_message.has_value());
}

TEST_CASE("Agent.ExecuteResult.SuccessWithMetadata", "[Agent][Execution]") {
    nlohmann::json metadata = R"({"files_created": 3, "lines_written": 100})"_json;
    auto result = ExecuteResult::ok("Done", metadata);
    
    REQUIRE(result.is_success);
    REQUIRE(result.metadata["files_created"] == 3);
    REQUIRE(result.metadata["lines_written"] == 100);
}

TEST_CASE("Agent.ExecuteResult.Error", "[Agent][Execution]") {
    auto result = ExecuteResult::error("Something went wrong");
    
    REQUIRE_FALSE(result.is_success);
    REQUIRE(result.error_message == "Something went wrong");
}

// ==================== AgentInfo Extended Tests ====================

TEST_CASE("Agent.Info.ValidateTemperature", "[Agent][Execution]") {
    AgentInfo info;
    info.name = "test-agent";
    
    // Valid temperature
    info.temperature = 0.5;
    REQUIRE(info.validate());
    
    info.temperature = 0.0;
    REQUIRE(info.validate());
    
    info.temperature = 2.0;
    REQUIRE(info.validate());
    
    // Invalid temperature would throw during from_json
    // Direct validate() doesn't throw
}

TEST_CASE("Agent.Info.ValidateTopP", "[Agent][Execution]") {
    AgentInfo info;
    info.name = "test-agent";
    
    // Valid top_p
    info.top_p = 0.9;
    REQUIRE(info.validate());
    
    info.top_p = 0.0;
    REQUIRE(info.validate());
    
    info.top_p = 1.0;
    REQUIRE(info.validate());
}

TEST_CASE("Agent.Info.ValidateSteps", "[Agent][Execution]") {
    AgentInfo info;
    info.name = "test-agent";
    
    // Valid steps
    info.steps = 10;
    REQUIRE(info.validate());
    
    info.steps = 1;
    REQUIRE(info.validate());
    
    info.steps = 100;
    REQUIRE(info.validate());
}

TEST_CASE("Agent.Info.WithColor", "[Agent][Execution]") {
    AgentInfo info;
    info.name = "colored-agent";
    info.color = "#FF5500";
    
    REQUIRE(info.validate());
    REQUIRE(info.color == "#FF5500");
}

TEST_CASE("Agent.Info.WithVariant", "[Agent][Execution]") {
    AgentInfo info;
    info.name = "variant-agent";
    info.variant = "extended";
    
    REQUIRE(info.validate());
    REQUIRE(info.variant == "extended");
}

// ==================== AgentRegistry Extended Tests ====================

TEST_CASE_METHOD(AgentExecutionFixture, "Agent.Registry.Register", "[Agent][Execution]") {
    auto agent = std::make_shared<BuildAgent>();
    
    REQUIRE(AgentRegistry::instance().register_agent(agent));
    REQUIRE(AgentRegistry::instance().has("build"));
}

TEST_CASE_METHOD(AgentExecutionFixture, "Agent.Registry.Unregister", "[Agent][Execution]") {
    auto agent = std::make_shared<BuildAgent>();
    AgentRegistry::instance().register_agent(agent);
    
    REQUIRE(AgentRegistry::instance().unregister_agent("build"));
    REQUIRE_FALSE(AgentRegistry::instance().has("build"));
}

TEST_CASE_METHOD(AgentExecutionFixture, "Agent.Registry.Get", "[Agent][Execution]") {
    auto agent = std::make_shared<BuildAgent>();
    AgentRegistry::instance().register_agent(agent);
    
    auto retrieved = AgentRegistry::instance().get("build");
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->name() == "build");
}

TEST_CASE_METHOD(AgentExecutionFixture, "Agent.Registry.List", "[Agent][Execution]") {
    auto build = std::make_shared<BuildAgent>();
    auto explore = std::make_shared<ExploreAgent>();
    
    AgentRegistry::instance().register_agent(build);
    AgentRegistry::instance().register_agent(explore);
    
    auto agents = AgentRegistry::instance().list();
    REQUIRE(agents.size() >= 2);
}

TEST_CASE_METHOD(AgentExecutionFixture, "Agent.Registry.ListByMode", "[Agent][Execution]") {
    auto build = std::make_shared<BuildAgent>();
    auto explore = std::make_shared<ExploreAgent>();
    
    AgentRegistry::instance().register_agent(build);
    AgentRegistry::instance().register_agent(explore);
    
    auto primary = AgentRegistry::instance().list_by_mode(AgentMode::Primary);
    auto subagents = AgentRegistry::instance().list_by_mode(AgentMode::Subagent);
    
    // BuildAgent is Primary, ExploreAgent is Subagent
    bool has_build = false;
    for (const auto& a : primary) {
        if (a->name() == "build") has_build = true;
    }
    REQUIRE(has_build);
    
    bool has_explore = false;
    for (const auto& a : subagents) {
        if (a->name() == "explore") has_explore = true;
    }
    REQUIRE(has_explore);
}

TEST_CASE_METHOD(AgentExecutionFixture, "Agent.Registry.ListVisible", "[Agent][Execution]") {
    auto build = std::make_shared<BuildAgent>();
    auto title = std::make_shared<TitleAgent>();
    
    AgentRegistry::instance().register_agent(build);
    AgentRegistry::instance().register_agent(title);
    
    auto visible = AgentRegistry::instance().list_visible();
    
    // TitleAgent is hidden, should not appear
    bool has_title = false;
    for (const auto& a : visible) {
        if (a->name() == "title") has_title = true;
    }
    REQUIRE_FALSE(has_title);
}

TEST_CASE_METHOD(AgentExecutionFixture, "Agent.Registry.DefaultAgent", "[Agent][Execution]") {
    auto build = std::make_shared<BuildAgent>();
    AgentRegistry::instance().register_agent(build);
    
    auto default_agent = AgentRegistry::instance().default_agent();
    REQUIRE_FALSE(default_agent.empty());
}

TEST_CASE_METHOD(AgentExecutionFixture, "Agent.Registry.Size", "[Agent][Execution]") {
    AgentRegistry::instance().clear();
    
    REQUIRE(AgentRegistry::instance().size() == 0);
    
    auto build = std::make_shared<BuildAgent>();
    AgentRegistry::instance().register_agent(build);
    
    REQUIRE(AgentRegistry::instance().size() == 1);
    
    AgentRegistry::instance().clear();
    REQUIRE(AgentRegistry::instance().size() == 0);
}

// ==================== Agent Tool Definition Tests ====================

TEST_CASE_METHOD(AgentExecutionFixture, "Agent.ToToolDefinition", "[Agent][Execution]") {
    BuildAgent agent;
    
    auto def = agent.to_tool_definition();
    
    REQUIRE(def.contains("type"));
    REQUIRE(def["type"] == "function");
    REQUIRE(def["function"].contains("name"));
    REQUIRE(def["function"]["name"] == "build");
    REQUIRE(def["function"].contains("description"));
    REQUIRE(def["function"].contains("parameters"));
}

// ==================== MockProvider Agent Integration Tests ====================

TEST_CASE_METHOD(AgentExecutionFixture, "Agent.MockProvider.Chat", "[Agent][Execution]") {
    // Configure mock provider with a response
    ChatResponse response;
    response.id = "agent-response-1";
    response.model = "mock-model";
    response.finish_reason = "stop";
    response.choices.push_back(ChatMessage::assistant("I have completed the task."));
    response.usage = turbot::core::TokenUsage(100, 50, 150);
    
    mock_provider_->set_next_response(response);
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Build a hello world program")
    };
    
    auto result = mock_provider_->chat(messages, "mock-model");
    
    REQUIRE_FALSE(result.is_error());
    REQUIRE(result.choices[0].content == "I have completed the task.");
    REQUIRE(result.usage.total() == 150);
}

TEST_CASE_METHOD(AgentExecutionFixture, "Agent.MockProvider.WithToolCalls", "[Agent][Execution]") {
    // Configure mock provider with tool calls
    ChatResponse response;
    response.id = "tool-response-1";
    response.model = "mock-model";
    response.finish_reason = "tool_calls";
    
    ToolCall tc;
    tc.id = "call-1";
    tc.name = "write_file";
    tc.arguments = R"({"path": "/tmp/hello.cpp", "content": "#include <iostream>\nint main() { std::cout << \"Hello\"; return 0; }"})"_json;
    
    response.choices.push_back(ChatMessage::assistant_with_tools("Creating file...", {tc}));
    
    mock_provider_->set_next_response(response);
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Create a hello world program")
    };
    
    auto result = mock_provider_->chat(messages, "mock-model");
    
    REQUIRE(result.has_tool_calls());
    REQUIRE(result.choices[0].tool_calls->size() == 1);
    REQUIRE(result.choices[0].tool_calls->at(0).name == "write_file");
}

TEST_CASE_METHOD(AgentExecutionFixture, "Agent.MockProvider.MultiStep", "[Agent][Execution]") {
    // Configure a sequence of responses for multi-step execution
    std::vector<ChatResponse> responses;
    
    // Step 1: Agent decides to read a file
    ChatResponse step1;
    step1.id = "step-1";
    step1.model = "mock-model";
    step1.finish_reason = "tool_calls";
    ToolCall tc1;
    tc1.id = "call-1";
    tc1.name = "read_file";
    tc1.arguments = R"({"path": "/tmp/existing.cpp"})"_json;
    step1.choices.push_back(ChatMessage::assistant_with_tools("", {tc1}));
    responses.push_back(step1);
    
    // Step 2: Agent decides to write a file
    ChatResponse step2;
    step2.id = "step-2";
    step2.model = "mock-model";
    step2.finish_reason = "tool_calls";
    ToolCall tc2;
    tc2.id = "call-2";
    tc2.name = "write_file";
    tc2.arguments = R"({"path": "/tmp/new.cpp", "content": "new content"})"_json;
    step2.choices.push_back(ChatMessage::assistant_with_tools("", {tc2}));
    responses.push_back(step2);
    
    // Step 3: Agent completes
    ChatResponse step3;
    step3.id = "step-3";
    step3.model = "mock-model";
    step3.finish_reason = "stop";
    step3.choices.push_back(ChatMessage::assistant("Task completed successfully!"));
    responses.push_back(step3);
    
    mock_provider_->set_response_sequence(responses);
    
    // Simulate multi-step execution
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Read existing.cpp and create new.cpp")
    };
    
    // Step 1
    auto result1 = mock_provider_->chat(messages, "mock-model");
    REQUIRE(result1.has_tool_calls());
    REQUIRE(result1.choices[0].tool_calls->at(0).name == "read_file");
    
    // Add tool result
    messages.push_back(ChatMessage::assistant_with_tools("", *result1.choices[0].tool_calls));
    messages.push_back(ChatMessage::tool_result("call-1", "existing file content"));
    
    // Step 2
    auto result2 = mock_provider_->chat(messages, "mock-model");
    REQUIRE(result2.has_tool_calls());
    REQUIRE(result2.choices[0].tool_calls->at(0).name == "write_file");
    
    // Add tool result
    messages.push_back(ChatMessage::assistant_with_tools("", *result2.choices[0].tool_calls));
    messages.push_back(ChatMessage::tool_result("call-2", "File written successfully"));
    
    // Step 3
    auto result3 = mock_provider_->chat(messages, "mock-model");
    REQUIRE_FALSE(result3.has_tool_calls());
    REQUIRE(result3.choices[0].content == "Task completed successfully!");
}

TEST_CASE_METHOD(AgentExecutionFixture, "Agent.MockProvider.Streaming", "[Agent][Execution]") {
    // Configure streaming response
    mock_provider_->set_stream_chunks({"I", " am", " thinking", "...", " Done!"});
    
    std::vector<std::string> chunks;
    auto callback = [&chunks](const ChatStreamEvent& event) {
        if (event.type == StreamEventType::TextDelta) {
            chunks.push_back(event.content);
        }
        return true;
    };
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Think about the problem")
    };
    
    ChatOptions options;
    options.stream = true;
    
    auto result = mock_provider_->chat_stream(messages, "mock-model", options, callback);
    
    REQUIRE(chunks.size() == 5);
    REQUIRE(chunks[0] == "I");
    REQUIRE(chunks[4] == " Done!");
}

TEST_CASE_METHOD(AgentExecutionFixture, "Agent.MockProvider.Error", "[Agent][Execution]") {
    // Configure error response
    mock_provider_->set_error_sequence({MockError::ServerError});
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("This will fail")
    };
    
    auto result = mock_provider_->chat(messages, "mock-model");
    
    REQUIRE(result.is_error());
    REQUIRE(result.error->contains("server_error"));
}

// ==================== Agent Permission Tests ====================

TEST_CASE_METHOD(AgentExecutionFixture, "Agent.Permission.BuildAgent", "[Agent][Execution]") {
    BuildAgent agent;
    auto info = agent.info();
    
    // BuildAgent should have comprehensive permissions
    REQUIRE_FALSE(info.permission.empty());
    
    // Check for common permissions
    std::vector<std::string> expected_permissions = {"read", "write", "execute"};
    
    int found_count = 0;
    for (const auto& expected : expected_permissions) {
        for (const auto& rule : info.permission) {
            if (rule.permission == expected || rule.permission == "*") {
                found_count++;
                break;
            }
        }
    }
    // At least some permissions should be present
    REQUIRE(found_count > 0);
}

TEST_CASE_METHOD(AgentExecutionFixture, "Agent.Permission.ExploreAgent", "[Agent][Execution]") {
    ExploreAgent agent;
    auto info = agent.info();
    
    // ExploreAgent should have read-only permissions
    REQUIRE_FALSE(info.permission.empty());
}

TEST_CASE_METHOD(AgentExecutionFixture, "Agent.Permission.PlanAgent", "[Agent][Execution]") {
    PlanAgent agent;
    auto info = agent.info();
    
    // PlanAgent should have limited permissions
    REQUIRE(info.validate());
}

// ==================== Agent Mode Tests ====================

TEST_CASE("Agent.Mode.Primary", "[Agent][Execution]") {
    BuildAgent agent;
    REQUIRE(agent.mode() == AgentMode::Primary);
}

TEST_CASE("Agent.Mode.Subagent", "[Agent][Execution]") {
    ExploreAgent agent;
    REQUIRE(agent.mode() == AgentMode::Subagent);
}

TEST_CASE("Agent.Mode.Conversion", "[Agent][Execution]") {
    REQUIRE(agent_mode_to_string(AgentMode::Primary) == "primary");
    REQUIRE(agent_mode_to_string(AgentMode::Subagent) == "subagent");
    REQUIRE(agent_mode_to_string(AgentMode::All) == "all");
    
    REQUIRE(string_to_agent_mode("primary") == AgentMode::Primary);
    REQUIRE(string_to_agent_mode("subagent") == AgentMode::Subagent);
    REQUIRE(string_to_agent_mode("all") == AgentMode::All);
}

// ==================== Agent Hidden Tests ====================

TEST_CASE("Agent.Hidden.TitleAgent", "[Agent][Execution]") {
    TitleAgent agent;
    REQUIRE(agent.is_hidden());
}

TEST_CASE("Agent.Hidden.SummaryAgent", "[Agent][Execution]") {
    SummaryAgent agent;
    REQUIRE(agent.is_hidden());
}

TEST_CASE("Agent.NotHidden.BuildAgent", "[Agent][Execution]") {
    BuildAgent agent;
    REQUIRE_FALSE(agent.is_hidden());
}

TEST_CASE("Agent.NotHidden.ExploreAgent", "[Agent][Execution]") {
    ExploreAgent agent;
    REQUIRE_FALSE(agent.is_hidden());
}

// ==================== Agent ModelRef Tests ====================

TEST_CASE("Agent.ModelRef.JsonRoundTrip", "[Agent][Execution]") {
    ModelRef ref;
    ref.model_id = "claude-3-opus";
    ref.provider_id = "anthropic";
    
    auto j = ref.to_json();
    auto restored = ModelRef::from_json(j);
    
    REQUIRE(restored.model_id == ref.model_id);
    REQUIRE(restored.provider_id == ref.provider_id);
}

TEST_CASE("Agent.ModelRef.Equality", "[Agent][Execution]") {
    ModelRef ref1{"gpt-4", "openai"};
    ModelRef ref2{"gpt-4", "openai"};
    ModelRef ref3{"gpt-4", "azure"};
    ModelRef ref4{"gpt-3.5", "openai"};
    
    REQUIRE(ref1 == ref2);
    REQUIRE_FALSE(ref1 == ref3);
    REQUIRE_FALSE(ref1 == ref4);
}

// ==================== Agent Builder Tests ====================

TEST_CASE("Agent.Builder.WithProvider", "[Agent][Execution]") {
    auto provider = MockProviderBuilder()
        .with_api_key("test-key")
        .with_text_response("Agent response")
        .build();
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Test")
    };
    
    auto response = provider->chat(messages, "mock-model");
    REQUIRE(response.choices[0].content == "Agent response");
}

TEST_CASE("Agent.Builder.WithToolCalls", "[Agent][Execution]") {
    auto provider = MockProviderBuilder()
        .with_api_key("test-key")
        .with_tool_call_response("bash", "bash-1", R"({"command": "ls"})"_json)
        .build();
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("List files")
    };
    
    auto response = provider->chat(messages, "mock-model");
    
    REQUIRE(response.has_tool_calls());
    REQUIRE(response.choices[0].tool_calls->at(0).name == "bash");
}
