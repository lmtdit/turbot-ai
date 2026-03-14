/// @file builtin_agents_llm_test.cpp
/// @brief Unit tests for P1-1: built-in Agents (BuildAgent / ExploreAgent / PlanAgent)
///        executing via a sub-SessionLoop with a mock Provider.
///
/// These tests verify the LLM integration path without a real network call:
///   - When ProviderManager has no default provider → placeholder path returns ok
///   - When ProviderManager has a mock provider → sub-loop executes and returns output
///   - Agent system prompts are set correctly
///   - Agent permission rulesets are correct

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/agent/builtin/build_agent.hpp>
#include <turbot/core/agent/builtin/explore_agent.hpp>
#include <turbot/core/agent/builtin/plan_agent.hpp>
#include <turbot/core/provider/provider_manager.hpp>
#include <turbot/core/provider/provider.hpp>
#include <turbot/core/permission/permission.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>

using namespace turbot::core::agent;
using namespace turbot::core::provider;
using namespace turbot::core::permission;

// ============================================================================
// Minimal text-only mock provider for agent tests
// ============================================================================

class AgentTestMockProvider : public Provider {
public:
    std::string id() const override { return "agent-test-mock"; }
    std::string name() const override { return "AgentTestMock"; }
    bool is_ready() const override { return true; }

    std::vector<ModelInfo> list_models() const override {
        return {{"mock-agent-model", id(), "Mock", ""}};
    }
    std::optional<ModelInfo> get_model(const std::string& mid) const override {
        if (mid == "mock-agent-model") return {{"mock-agent-model", id(), "Mock", ""}};
        return std::nullopt;
    }
    bool supports_model(const std::string&) const override { return true; }

    ChatResponse chat(const std::vector<ChatMessage>&, const std::string&,
                      const ChatOptions&) override {
        return {};
    }

    ChatResponse chat_stream(
        const std::vector<ChatMessage>&,
        const std::string&,
        const ChatOptions&,
        StreamCallback callback
    ) override {
        // Emit a text delta + finish
        ChatStreamEvent text_ev;
        text_ev.type    = StreamEventType::TextDelta;
        text_ev.content = response_text_;
        callback(text_ev);

        ChatStreamEvent finish;
        finish.type          = StreamEventType::Finish;
        finish.finish_reason = "stop";
        callback(finish);

        ChatResponse resp;
        resp.finish_reason = "stop";
        return resp;
    }

    int64_t count_tokens(const std::vector<ChatMessage>&, const std::string&) const override {
        return 0;
    }
    bool validate() override { return true; }

    void set_response(const std::string& text) { response_text_ = text; }
private:
    std::string response_text_{"Mock LLM response"};
};

// ============================================================================
// Test fixture helpers
// ============================================================================

static std::shared_ptr<AgentTestMockProvider> register_mock_provider() {
    auto mock = std::make_shared<AgentTestMockProvider>();
    auto& pm = ProviderManager::instance();
    if (!pm.has_provider("agent-test-mock")) {
        pm.register_provider(mock);
    }
    pm.set_default_provider("agent-test-mock");
    return mock;
}

static void clear_mock_provider() {
    ProviderManager::instance().unregister_provider("agent-test-mock");
}

// ============================================================================
// Section 1 – Agent metadata
// ============================================================================

TEST_CASE("BuildAgent: metadata is set correctly", "[agent][builtin][build]") {
    BuildAgent agent;
    CHECK(agent.name() == "build");
    CHECK_FALSE(agent.description().empty());
    CHECK(agent.info().mode == AgentMode::Primary);
    CHECK(agent.info().native == true);
    // System prompt should be non-empty after P1-1
    REQUIRE(agent.info().prompt.has_value());
    CHECK_FALSE(agent.info().prompt->empty());
}

TEST_CASE("ExploreAgent: metadata is set correctly", "[agent][builtin][explore]") {
    ExploreAgent agent;
    CHECK(agent.name() == "explore");
    CHECK_FALSE(agent.description().empty());
    CHECK(agent.info().mode == AgentMode::Subagent);
    REQUIRE(agent.info().prompt.has_value());
    CHECK_FALSE(agent.info().prompt->empty());
    // Verify prompt mentions read-only
    CHECK(agent.info().prompt->find("read-only") != std::string::npos);
}

TEST_CASE("PlanAgent: metadata is set correctly", "[agent][builtin][plan]") {
    PlanAgent agent;
    CHECK(agent.name() == "plan");
    CHECK_FALSE(agent.description().empty());
    CHECK(agent.info().mode == AgentMode::Primary);
    REQUIRE(agent.info().prompt.has_value());
    CHECK_FALSE(agent.info().prompt->empty());
}

// ============================================================================
// Section 2 – Permission rulesets
// ============================================================================

TEST_CASE("BuildAgent: permission ruleset contains allow-all rule", "[agent][builtin][build][permission]") {
    BuildAgent agent;
    const auto& rules = agent.info().permission;
    bool has_allow_all = false;
    for (const auto& rule : rules) {
        if (rule.action == PermissionAction::Allow && rule.permission == "*") {
            has_allow_all = true;
        }
    }
    CHECK(has_allow_all);
}

TEST_CASE("ExploreAgent: permission denies write operations", "[agent][builtin][explore][permission]") {
    ExploreAgent agent;
    const auto& rules = agent.info().permission;
    // Last rule should be Deny *:*
    REQUIRE_FALSE(rules.empty());
    const auto& last = rules.back();
    CHECK(last.action == PermissionAction::Deny);
    CHECK(last.permission == "*");
}

TEST_CASE("PlanAgent: permission ruleset is empty (no tool access)", "[agent][builtin][plan][permission]") {
    PlanAgent agent;
    // PlanAgent doesn't need any tool permission rules
    CHECK(agent.info().permission.empty());
}

// ============================================================================
// Section 3 – Placeholder path (no provider configured)
// ============================================================================

TEST_CASE("BuildAgent::execute: returns placeholder when no provider", "[agent][builtin][build][placeholder]") {
    // Ensure no default provider
    ProviderManager::instance().unregister_provider("agent-test-mock");
    ProviderManager::instance().set_default_provider("");

    auto agent = std::make_shared<BuildAgent>();
    ExecuteParams params;
    params.session_id = "sess-placeholder";
    params.prompt     = "Build the project";

    auto result = agent->execute(params);
    CHECK(result.is_success);
    CHECK_FALSE(result.output.empty());
}

TEST_CASE("ExploreAgent::execute: returns placeholder when no provider", "[agent][builtin][explore][placeholder]") {
    ProviderManager::instance().set_default_provider("");

    auto agent = std::make_shared<ExploreAgent>();
    ExecuteParams params;
    params.session_id = "sess-placeholder-explore";
    params.prompt     = "Explore the codebase";

    auto result = agent->execute(params);
    CHECK(result.is_success);
}

TEST_CASE("PlanAgent::execute: returns placeholder when no provider", "[agent][builtin][plan][placeholder]") {
    ProviderManager::instance().set_default_provider("");

    auto agent = std::make_shared<PlanAgent>();
    ExecuteParams params;
    params.session_id = "sess-placeholder-plan";
    params.prompt     = "Plan the feature";

    auto result = agent->execute(params);
    CHECK(result.is_success);
}

// ============================================================================
// Section 4 – LLM integration path (with mock provider)
// ============================================================================

TEST_CASE("BuildAgent::execute: calls sub-SessionLoop with mock provider", "[agent][builtin][build][llm]") {
    auto mock = register_mock_provider();
    mock->set_response("Here is how I would build it...");

    auto agent = std::make_shared<BuildAgent>();
    ExecuteParams params;
    params.session_id = "sess-build-mock";
    params.prompt     = "Compile and fix any errors";

    auto result = agent->execute(params);

    CHECK(result.is_success);
    // The mock provider emits "Here is how I would build it..."
    CHECK(result.output == "Here is how I would build it...");
    CHECK(result.metadata.contains("agent"));
    CHECK(result.metadata["agent"] == "build");

    clear_mock_provider();
}

TEST_CASE("ExploreAgent::execute: calls sub-SessionLoop with mock provider", "[agent][builtin][explore][llm]") {
    auto mock = register_mock_provider();
    mock->set_response("The codebase contains 10 modules...");

    auto agent = std::make_shared<ExploreAgent>();
    ExecuteParams params;
    params.session_id = "sess-explore-mock";
    params.prompt     = "Summarise the architecture";

    auto result = agent->execute(params);

    CHECK(result.is_success);
    CHECK(result.output == "The codebase contains 10 modules...");
    CHECK(result.metadata["agent"] == "explore");

    clear_mock_provider();
}

TEST_CASE("PlanAgent::execute: calls sub-SessionLoop with mock provider", "[agent][builtin][plan][llm]") {
    auto mock = register_mock_provider();
    mock->set_response("Step 1: Analyse requirements\nStep 2: ...");

    auto agent = std::make_shared<PlanAgent>();
    ExecuteParams params;
    params.session_id = "sess-plan-mock";
    params.prompt     = "Plan the refactor";

    auto result = agent->execute(params);

    CHECK(result.is_success);
    CHECK_FALSE(result.output.empty());
    CHECK(result.metadata["agent"] == "plan");

    clear_mock_provider();
}

TEST_CASE("BuildAgent::execute: respects model_override param", "[agent][builtin][build][llm]") {
    auto mock = register_mock_provider();
    mock->set_response("Done with override model");

    auto agent = std::make_shared<BuildAgent>();
    ExecuteParams params;
    params.session_id    = "sess-override";
    params.prompt        = "Build fast";
    params.model_override = "mock-agent-model";

    auto result = agent->execute(params);
    CHECK(result.is_success);
    CHECK(result.metadata.contains("model"));
    CHECK(result.metadata["model"] == "mock-agent-model");

    clear_mock_provider();
}
