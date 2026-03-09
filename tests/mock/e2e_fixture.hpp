#pragma once

#include "mock_provider.hpp"
#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_loop.hpp>
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/agent/builtin/build_agent.hpp>
#include <turbot/core/agent/builtin/plan_agent.hpp>
#include <turbot/core/agent/builtin/explore_agent.hpp>
#include <turbot/core/tool/tool.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

namespace turbot::test {

/// E2E Test Fixture
///
/// Provides a complete test environment for end-to-end testing of the
/// session loop, including mock provider, agent, and tool setup.
struct E2EFixture {
    std::unique_ptr<core::session::Session> session;
    std::shared_ptr<core::agent::Agent> agent;
    std::shared_ptr<MockProvider> provider;
    std::unique_ptr<core::session::SessionLoop> loop;
    std::filesystem::path test_dir;

    void setup() {
        // Create test directory
        test_dir = std::filesystem::temp_directory_path() / ("turbot-e2e-" + std::to_string(std::time(nullptr)));
        std::filesystem::create_directories(test_dir);

        // Create session
        core::session::CreateParams params;
        params.project_id = "e2e-test";
        params.slug = "test-session";
        params.directory = test_dir.string();
        params.title = "E2E Test Session";

        auto session_result = core::session::Session::create(params);
        REQUIRE(session_result.has_value());
        session = std::make_unique<core::session::Session>(std::move(*session_result));

        // Create default agent
        agent = core::agent::AgentRegistry::instance().get("build");
        if (!agent) {
            // Register default agents if not already registered
            core::agent::AgentRegistry::instance().register_agent(
                std::make_shared<core::agent::BuildAgent>()
            );
            core::agent::AgentRegistry::instance().register_agent(
                std::make_shared<core::agent::PlanAgent>()
            );
            core::agent::AgentRegistry::instance().register_agent(
                std::make_shared<core::agent::ExploreAgent>()
            );
            agent = core::agent::AgentRegistry::instance().get("build");
        }
        REQUIRE(agent != nullptr);

        // Create mock provider
        provider = MockProviderBuilder()
            .with_api_key("mock-api-key")
            .with_model(core::provider::ModelInfo{
                .id = "mock-model",
                .provider_id = "mock",
                .name = "Mock Model",
                .description = "A mock model for testing",
                .capabilities = {.temperature = true, .tool_call = true, .streaming = true},
                .context_window = 128000
            })
            .build();

        // Create session loop
        loop = std::make_unique<core::session::SessionLoop>(*session);
        loop->set_agent(agent);
        loop->set_provider(provider.get());
        loop->set_model("mock-model");
    }

    void teardown() {
        loop.reset();
        session.reset();
        provider.reset();

        // Clean up test directory
        if (std::filesystem::exists(test_dir)) {
            std::error_code ec;
            std::filesystem::remove_all(test_dir, ec);
        }
    }

    /// Create a test file in the test directory
    void create_test_file(const std::string& name, const std::string& content) {
        std::ofstream file(test_dir / name);
        file << content;
    }

    /// Read a test file from the test directory
    [[nodiscard]] std::string read_test_file(const std::string& name) const {
        std::ifstream file(test_dir / name);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    /// Configure the mock provider for simple Q&A
    void setup_simple_qa(const std::string& response_text) {
        provider->set_next_response(core::provider::ChatResponse{
            .id = "mock-simple",
            .model = "mock-model",
            .choices = {core::provider::ChatMessage::assistant(response_text)},
            .usage = core::TokenUsage(50, 30, 80),
            .finish_reason = "stop"
        });
    }

    /// Configure the mock provider for tool call
    void setup_tool_call(
        const std::string& tool_name,
        const std::string& tool_id,
        const nlohmann::json& arguments
    ) {
        core::provider::ToolCall tc;
        tc.id = tool_id;
        tc.name = tool_name;
        tc.arguments = arguments;

        core::provider::ChatResponse response;
        response.id = "mock-tool";
        response.model = "mock-model";
        response.finish_reason = "tool_calls";
        response.choices.push_back(
            core::provider::ChatMessage::assistant_with_tools("", {tc})
        );

        provider->set_next_response(response);
    }

    /// Configure the mock provider for multiple tool calls
    void setup_multiple_tool_calls(
        const std::vector<std::tuple<std::string, std::string, nlohmann::json>>& calls
    ) {
        std::vector<core::provider::ToolCall> tool_calls;
        for (const auto& [name, id, args] : calls) {
            tool_calls.push_back({.id = id, .name = name, .arguments = args});
        }

        core::provider::ChatResponse response;
        response.id = "mock-tools";
        response.model = "mock-model";
        response.finish_reason = "tool_calls";
        response.choices.push_back(
            core::provider::ChatMessage::assistant_with_tools("", tool_calls)
        );

        provider->set_next_response(response);
    }

    /// Configure error sequence for retry testing
    void setup_error_sequence(const std::vector<MockError>& errors) {
        provider->set_error_sequence(errors);
    }

    /// Configure stream chunks for streaming test
    void setup_stream_chunks(const std::vector<std::string>& chunks) {
        provider->set_stream_chunks(chunks);
    }
};

/// RAII wrapper for E2E fixture
struct E2ETest : public E2EFixture {
    E2ETest() { setup(); }
    ~E2ETest() { teardown(); }
};

/// Helper to create a mock tool for testing
class MockTool : public core::tool::Tool {
public:
    explicit MockTool(
        const std::string& name,
        const std::string& description = "A mock tool for testing"
    ) : name_(name), description_(description) {}

    [[nodiscard]] std::string name() const override { return name_; }
    [[nodiscard]] std::string description() const override { return description_; }

    [[nodiscard]] nlohmann::json input_schema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"input", {{"type", "string"}, {"description", "Input for the mock tool"}}}
            }},
            {"required", nlohmann::json::array({"input"})}
        };
    }

    [[nodiscard]] core::tool::ToolResult execute(
        const nlohmann::json& input,
        [[maybe_unused]] core::tool::ToolContext& context
    ) override {
        call_count_++;
        last_input_ = input;

        if (should_fail_) {
            return core::tool::ToolResult::error("MockError", "Mock tool was configured to fail");
        }

        return core::tool::ToolResult::success("MockTool Result", result_);
    }

    // Configuration methods
    void set_result(const std::string& result) { result_ = result; }
    void set_should_fail(bool fail) { should_fail_ = fail; }
    [[nodiscard]] int call_count() const { return call_count_; }
    [[nodiscard]] const nlohmann::json& last_input() const { return last_input_; }
    void reset() { call_count_ = 0; last_input_ = {}; should_fail_ = false; result_ = "mock result"; }

private:
    std::string name_;
    std::string description_;
    int call_count_ = 0;
    nlohmann::json last_input_;
    bool should_fail_ = false;
    std::string result_ = "mock result";
};

} // namespace turbot::test
