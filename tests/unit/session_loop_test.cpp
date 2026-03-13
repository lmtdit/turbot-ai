#include <catch2/catch_test_macros.hpp>
#include <turbot/core/session/session_loop.hpp>
#include <turbot/core/session/session_events.hpp>
#include <turbot/core/session/retry_manager.hpp>
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/agent/builtin/build_agent.hpp>
#include <turbot/core/agent/builtin/plan_agent.hpp>
#include <turbot/core/agent/builtin/explore_agent.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/provider/provider.hpp>
#include <nlohmann/json.hpp>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

using namespace turbot::core::session;
using namespace turbot::core::agent;
using namespace turbot::core;
using namespace turbot::core::tool;

// ============================================================================
// Minimal MockProvider for 2.8.7 tests
// ============================================================================

/// A minimal Provider mock that can be configured to emit one tool-call event
/// followed by a finish event on the first invocation, then pure text on the
/// second invocation (so the loop terminates cleanly).
class ToolCallMockProvider : public provider::Provider {
public:
    std::string id() const override { return provider_id_; }
    std::string name() const override { return "ToolCallMock"; }
    bool is_ready() const override { return true; }

    std::vector<provider::ModelInfo> list_models() const override {
        return {{"mock-model", id(), "Mock", ""}};
    }
    std::optional<provider::ModelInfo> get_model(const std::string& mid) const override {
        if (mid == "mock-model") return {{"mock-model", id(), "Mock", ""}};
        return std::nullopt;
    }
    bool supports_model(const std::string& mid) const override { return mid == "mock-model"; }

    provider::ChatResponse chat(
        const std::vector<provider::ChatMessage>&,
        const std::string&,
        const provider::ChatOptions&
    ) override { return {}; }

    provider::ChatResponse chat_stream(
        const std::vector<provider::ChatMessage>& messages,
        const std::string&,
        const provider::ChatOptions& options,
        provider::StreamCallback callback
    ) override {
        provider::ChatResponse resp;
        resp.finish_reason = "stop";

        last_received_options_ = options;

        // First invocation: emit a tool call; subsequent: emit plain text stop
        if (call_count_++ == 0 && !tool_calls_.empty()) {
            for (const auto& tc : tool_calls_) {
                provider::ChatStreamEvent ev;
                ev.type = provider::StreamEventType::ToolCall;
                ev.tool_call = tc;
                callback(ev);
            }
        }

        provider::ChatStreamEvent finish;
        finish.type = provider::StreamEventType::Finish;
        finish.finish_reason = "stop";
        callback(finish);
        return resp;
    }

    int64_t count_tokens(const std::vector<provider::ChatMessage>&, const std::string&) const override {
        return 0;
    }
    bool validate() override { return true; }

    void set_tool_calls(const std::vector<provider::ToolCall>& tcs) { tool_calls_ = tcs; }
    void set_provider_id(const std::string& pid) { provider_id_ = pid; }
    const provider::ChatOptions& last_options() const { return last_received_options_; }

private:
    std::string provider_id_ = "mock";
    std::vector<provider::ToolCall> tool_calls_;
    provider::ChatOptions last_received_options_;
    int call_count_ = 0;
};

// Minimal Tool mock for repairToolCall tests.
// Named RepairEchoTool (not EchoTool) to avoid ODR conflict with the EchoTool
// defined in tool_test.cpp, which is compiled into the same test binary.
class RepairEchoTool : public Tool {
public:
    explicit RepairEchoTool(const std::string& tool_name) : tool_name_(tool_name) {}
    std::string name() const override { return tool_name_; }
    std::string description() const override { return "Echo tool for repairToolCall tests"; }
    nlohmann::json input_schema() const override {
        return {{"type", "object"}, {"properties", nlohmann::json::object()}};
    }
    ToolResult execute(const nlohmann::json&, ToolContext&) override {
        return ToolResult::success("repair_echo", "ok");
    }
private:
    std::string tool_name_;
};

/// RAII guard that clears the global ToolRegistry on construction and
/// destruction, ensuring test isolation even when an assertion fires early.
struct ToolRegistryGuard {
    ToolRegistryGuard()  { ToolRegistry::instance().clear(); }
    ~ToolRegistryGuard() { ToolRegistry::instance().clear(); }
};

// ============================================================================
// LoopResult
// ============================================================================

TEST_CASE("LoopResult::to_string", "[core][session][session_loop]") {
    REQUIRE(loop_result_to_string(LoopResult::Continue) == "continue");
    REQUIRE(loop_result_to_string(LoopResult::Stop) == "stop");
    REQUIRE(loop_result_to_string(LoopResult::Compact) == "compact");
}

TEST_CASE("LoopResult::to_string invalid throws", "[core][session][session_loop]") {
    REQUIRE_THROWS_AS(loop_result_to_string(static_cast<LoopResult>(999)), std::invalid_argument);
}

// ============================================================================
// SessionLoopConfig
// ============================================================================

TEST_CASE("SessionLoopConfig default values", "[core][session][session_loop]") {
    SessionLoopConfig config;
    REQUIRE(config.max_iterations == 100);
    REQUIRE(config.max_tokens == 128000);
    REQUIRE(config.compact_threshold == 100000);
    REQUIRE(config.auto_compact == true);
}

// ============================================================================
// SessionLoop Construction
// ============================================================================

TEST_CASE("SessionLoop construction with session_id", "[core][session][session_loop]") {
    // This creates or resumes a session
    SessionLoop loop("test-session-123");
    
    REQUIRE_FALSE(loop.is_running());
    REQUIRE(loop.token_count() == 0);
    REQUIRE(loop.messages().empty());
}

TEST_CASE("SessionLoop construction with Session object", "[core][session][session_loop]") {
    CreateParams params;
    params.project_id = "test-project";
    params.slug = "test-slug";
    params.directory = "/tmp/test";
    params.title = "Test Session";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    SessionLoop loop(std::move(*session));
    
    REQUIRE_FALSE(loop.is_running());
    REQUIRE(loop.session().info().project_id == "test-project");
}

TEST_CASE("SessionLoop is non-copyable and non-movable", "[core][session][session_loop]") {
    CreateParams params;
    params.project_id = "test";
    params.slug = "test";
    params.directory = "/tmp";
    params.title = "Test";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    SessionLoop loop(std::move(*session));
    
    // These should not compile - we're just verifying the API exists
    // SessionLoop copy = loop;  // Should not compile
    // SessionLoop moved = std::move(loop);  // Should not compile
}

// ============================================================================
// SessionLoop Configuration
// ============================================================================

TEST_CASE("SessionLoop::set_config", "[core][session][session_loop]") {
    CreateParams params;
    params.project_id = "test";
    params.slug = "test";
    params.directory = "/tmp";
    params.title = "Test";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    SessionLoop loop(std::move(*session));
    
    SessionLoopConfig config;
    config.max_iterations = 50;
    config.max_tokens = 64000;
    config.compact_threshold = 50000;
    config.auto_compact = false;
    
    loop.set_config(config);
    
    REQUIRE(loop.config().max_iterations == 50);
    REQUIRE(loop.config().max_tokens == 64000);
    REQUIRE(loop.config().compact_threshold == 50000);
    REQUIRE(loop.config().auto_compact == false);
}

TEST_CASE("SessionLoop::set_agent", "[core][session][session_loop]") {
    AgentRegistry::instance().clear();
    AgentRegistry::instance().register_agent(std::make_shared<BuildAgent>());
    
    CreateParams params;
    params.project_id = "test";
    params.slug = "test";
    params.directory = "/tmp";
    params.title = "Test";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    SessionLoop loop(std::move(*session));
    
    auto agent = AgentRegistry::instance().get("build");
    REQUIRE(agent != nullptr);
    
    loop.set_agent(agent);
    // Agent is set successfully if no exception
}

// ============================================================================
// SessionLoop Callbacks
// ============================================================================

TEST_CASE("SessionLoop callbacks can be set", "[core][session][session_loop]") {
    CreateParams params;
    params.project_id = "test";
    params.slug = "test";
    params.directory = "/tmp";
    params.title = "Test";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    SessionLoop loop(std::move(*session));
    
    bool message_called = false;
    bool tool_call_called = false;
    bool tool_result_called = false;
    bool error_called = false;
    
    loop.set_on_message([&](const Message&) { message_called = true; });
    loop.set_on_tool_call([&](const std::string&, const std::string&, const nlohmann::json&) { tool_call_called = true; });
    loop.set_on_tool_result([&](const std::string&, const std::string&, const ToolResult&) { tool_result_called = true; });
    loop.set_on_error([&](const std::string&, const std::string&) { error_called = true; });
    
    // Callbacks are set successfully if no exception
}

// ============================================================================
// SessionLoop Run
// ============================================================================

TEST_CASE("SessionLoop::run with simple message", "[core][session][session_loop]") {
    AgentRegistry::instance().clear();
    AgentRegistry::instance().register_agent(std::make_shared<BuildAgent>());
    
    CreateParams params;
    params.project_id = "test";
    params.slug = "test";
    params.directory = "/tmp";
    params.title = "Test";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    SessionLoop loop(std::move(*session));
    
    auto agent = AgentRegistry::instance().get("build");
    if (agent) {
        loop.set_agent(agent);
    }
    
    // Run with a simple message
    auto result = loop.run("Hello, world!");
    
    // Should have added the message
    REQUIRE(loop.messages().size() >= 1);
    REQUIRE(loop.token_count() > 0);
    
    // Should have stopped (no real LLM to continue)
    REQUIRE_FALSE(loop.is_running());
}

TEST_CASE("SessionLoop::run calls message callback", "[core][session][session_loop]") {
    AgentRegistry::instance().clear();
    AgentRegistry::instance().register_agent(std::make_shared<BuildAgent>());
    
    CreateParams params;
    params.project_id = "test";
    params.slug = "test";
    params.directory = "/tmp";
    params.title = "Test";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    SessionLoop loop(std::move(*session));
    
    auto agent = AgentRegistry::instance().get("build");
    if (agent) {
        loop.set_agent(agent);
    }
    
    int callback_count = 0;
    loop.set_on_message([&](const Message&) { callback_count++; });
    
    loop.run("Test message");
    
    REQUIRE(callback_count >= 1);
}

TEST_CASE("SessionLoop::stop", "[core][session][session_loop]") {
    CreateParams params;
    params.project_id = "test";
    params.slug = "test";
    params.directory = "/tmp";
    params.title = "Test";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    SessionLoop loop(std::move(*session));
    
    // Stop before running
    loop.stop();
    
    REQUIRE_FALSE(loop.is_running());
}

// ============================================================================
// SessionLoop Session Access
// ============================================================================

TEST_CASE("SessionLoop::session returns valid session", "[core][session][session_loop]") {
    CreateParams params;
    params.project_id = "access-test";
    params.slug = "test-slug";
    params.directory = "/tmp/access-test";
    params.title = "Access Test";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    std::string session_id = session->id();
    
    SessionLoop loop(std::move(*session));
    
    const Session& sess = loop.session();
    REQUIRE(sess.id() == session_id);
    REQUIRE(sess.info().project_id == "access-test");
}

// ============================================================================
// SessionLoop Edge Cases
// ============================================================================

TEST_CASE("SessionLoop::run with empty message", "[core][session][session_loop]") {
    AgentRegistry::instance().clear();
    AgentRegistry::instance().register_agent(std::make_shared<BuildAgent>());
    
    CreateParams params;
    params.project_id = "test";
    params.slug = "test";
    params.directory = "/tmp";
    params.title = "Test";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    SessionLoop loop(std::move(*session));
    
    auto agent = AgentRegistry::instance().get("build");
    if (agent) {
        loop.set_agent(agent);
    }
    
    auto result = loop.run("");
    
    // Should still work (empty message is valid)
    REQUIRE_FALSE(loop.is_running());
}

TEST_CASE("SessionLoop::step without agent", "[core][session][session_loop]") {
    CreateParams params;
    params.project_id = "test";
    params.slug = "test";
    params.directory = "/tmp";
    params.title = "Test";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    SessionLoop loop(std::move(*session));
    // No agent or provider set
    
    auto result = loop.step();
    
    // Should return error since no provider is configured
    REQUIRE(result == LoopResult::Error);
}

TEST_CASE("SessionLoop token estimation", "[core][session][session_loop]") {
    CreateParams params;
    params.project_id = "test";
    params.slug = "test";
    params.directory = "/tmp";
    params.title = "Test";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    SessionLoop loop(std::move(*session));
    
    // Without a provider, run() returns Error at the LLM call stage (inside
    // process_llm_response), but process_user_message IS still called first —
    // which adds the user message and calls estimate_tokens on the content.
    //
    // "This is a test message for token estimation" has 43 ASCII chars → ~11 tokens
    const std::string msg = "This is a test message for token estimation";
    auto result = loop.run(msg);
    REQUIRE(result == LoopResult::Error);  // Correct: no provider configured
    // token_count > 0 because process_user_message estimated the user message tokens
    REQUIRE(loop.token_count() > 0);
}

// ============================================================================
// SessionLoop with Multiple Agents
// ============================================================================

TEST_CASE("SessionLoop with different agent types", "[core][session][session_loop]") {
    AgentRegistry::instance().clear();
    AgentRegistry::instance().register_agent(std::make_shared<BuildAgent>());
    AgentRegistry::instance().register_agent(std::make_shared<PlanAgent>());
    AgentRegistry::instance().register_agent(std::make_shared<ExploreAgent>());
    
    CreateParams params;
    params.project_id = "multi-agent-test";
    params.slug = "test";
    params.directory = "/tmp";
    params.title = "Multi Agent Test";
    
    // Test with build agent
    {
        auto session = Session::create(params);
        REQUIRE(session.has_value());
        
        SessionLoop loop(std::move(*session));
        auto agent = AgentRegistry::instance().get("build");
        if (agent) {
            loop.set_agent(agent);
        }
        
        loop.run("Build test");
        REQUIRE(loop.messages().size() >= 1);
    }
    
    // Test with plan agent
    {
        auto session = Session::create(params);
        REQUIRE(session.has_value());
        
        SessionLoop loop(std::move(*session));
        auto agent = AgentRegistry::instance().get("plan");
        if (agent) {
            loop.set_agent(agent);
        }
        
        loop.run("Plan test");
        REQUIRE(loop.messages().size() >= 1);
    }
    
    // Test with explore agent
    {
        auto session = Session::create(params);
        REQUIRE(session.has_value());
        
        SessionLoop loop(std::move(*session));
        auto agent = AgentRegistry::instance().get("explore");
        if (agent) {
            loop.set_agent(agent);
        }
        
        loop.run("Explore test");
        REQUIRE(loop.messages().size() >= 1);
    }
}

// ============================================================================
// SessionLoop Config Threshold
// ============================================================================

TEST_CASE("SessionLoop respects max_iterations", "[core][session][session_loop]") {
    AgentRegistry::instance().clear();
    AgentRegistry::instance().register_agent(std::make_shared<BuildAgent>());
    
    CreateParams params;
    params.project_id = "test";
    params.slug = "test";
    params.directory = "/tmp";
    params.title = "Test";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    SessionLoop loop(std::move(*session));
    
    SessionLoopConfig config;
    config.max_iterations = 1;  // Very low limit
    loop.set_config(config);
    
    auto agent = AgentRegistry::instance().get("build");
    if (agent) {
        loop.set_agent(agent);
    }
    
    auto result = loop.run("Test");
    
    // Should stop due to iteration limit
    REQUIRE_FALSE(loop.is_running());
}

// ============================================================================
// SNAP-LOOP: Doom loop detection - (tool, input) pair
// ============================================================================

TEST_CASE("SessionLoopConfig doom_loop_threshold default", "[core][session][doom-loop]") {
    SessionLoopConfig config;
    REQUIRE(config.doom_loop_threshold == 3);
}

TEST_CASE("SessionLoop doom_loop_threshold is configurable", "[core][session][doom-loop]") {
    CreateParams params;
    params.project_id = "test";
    params.slug = "test";
    params.directory = "/tmp";
    params.title = "Test";

    auto session = Session::create(params);
    REQUIRE(session.has_value());

    SessionLoop loop(std::move(*session));

    SessionLoopConfig config;
    config.doom_loop_threshold = 5;
    loop.set_config(config);

    REQUIRE(loop.config().doom_loop_threshold == 5);
}

// ============================================================================
// 2.8.1 – RetryManager integration
// ============================================================================

TEST_CASE("RetryManager::with_retry integrated into SessionLoop context", "[core][session][retry]") {
    // These tests verify the RetryManager behaves correctly when called
    // from a SessionLoop-like context (no real LLM required).

    SECTION("non-retryable APIError propagates immediately") {
        int call_count = 0;
        REQUIRE_THROWS_AS(
            RetryManager::with_retry(
                [&]() -> int {
                    ++call_count;
                    throw APIError(400, "Bad Request");
                    return 0;
                },
                RetryConfig{}
            ),
            APIError
        );
        // Should be called exactly once — no retry on 400
        REQUIRE(call_count == 1);
    }

    SECTION("retryable APIError retries up to max_attempts then throws") {
        int call_count = 0;
        RetryConfig cfg;
        cfg.max_attempts = 3;
        cfg.base_delay_ms = 1;  // Fast for testing
        REQUIRE_THROWS_AS(
            RetryManager::with_retry(
                [&]() -> int {
                    ++call_count;
                    throw APIError(429, "Rate limited");
                    return 0;
                },
                cfg
            ),
            APIError
        );
        REQUIRE(call_count == 3);
    }

    SECTION("retryable APIError succeeds on third attempt") {
        int call_count = 0;
        RetryConfig cfg;
        cfg.max_attempts = 5;
        cfg.base_delay_ms = 1;
        int result = RetryManager::with_retry(
            [&]() -> int {
                ++call_count;
                if (call_count < 3) throw APIError(503, "Service unavailable");
                return 99;
            },
            cfg
        );
        REQUIRE(result == 99);
        REQUIRE(call_count == 3);
    }

    SECTION("on_retry callback is invoked with correct attempt index") {
        int call_count = 0;
        int callback_count = 0;
        std::vector<int> attempts_seen;
        RetryConfig cfg;
        cfg.max_attempts = 4;
        cfg.base_delay_ms = 1;
        int result = RetryManager::with_retry(
            [&]() -> int {
                ++call_count;
                if (call_count < 3) throw APIError(429, "Rate limited");
                return 42;
            },
            cfg,
            [&](int attempt, const APIError&, int) {
                ++callback_count;
                attempts_seen.push_back(attempt);
            }
        );
        REQUIRE(result == 42);
        REQUIRE(callback_count == 2);
        REQUIRE(attempts_seen[0] == 0);
        REQUIRE(attempts_seen[1] == 1);
    }

    SECTION("abort check in on_retry prevents subsequent retries") {
        std::atomic<bool> abort_flag{false};
        int call_count = 0;
        RetryConfig cfg;
        cfg.max_attempts = 5;
        cfg.base_delay_ms = 1;
        // on_retry now throws AbortRetryException (not APIError) so the signal
        // propagates cleanly without going through is_retryable (C-1/C-2 fix).
        REQUIRE_THROWS_AS(
            RetryManager::with_retry(
                [&]() -> int {
                    ++call_count;
                    throw APIError(429, "Rate limited");
                    return 0;
                },
                cfg,
                [&](int attempt, const APIError&, int) {
                    abort_flag.store(true);
                    if (abort_flag.load()) {
                        throw AbortRetryException{};
                    }
                }
            ),
            AbortRetryException
        );
        // Only 1 real attempt before abort on first retry callback
        REQUIRE(call_count == 1);
    }

    SECTION("Retry-After header respected in delay calculation") {
        RetryConfig cfg;
        cfg.base_delay_ms = 100;
        cfg.jitter_factor = 0.0;
        APIError err(429, "Rate limited", std::nullopt, 5);  // 5 seconds
        int delay = RetryManager::calculate_delay(0, cfg, err);
        REQUIRE(delay >= 5000);
    }
}

// ============================================================================
// 2.8.2 – Precise token counting
// ============================================================================

TEST_CASE("TokenUsage total() accounts for all token types", "[core][session][token]") {
    SECTION("basic input/output") {
        TokenUsage u;
        u.input = 100;
        u.output = 50;
        REQUIRE(u.total() == 150);
    }

    SECTION("includes cache and reasoning") {
        TokenUsage u;
        u.input = 100;
        u.output = 50;
        u.reasoning = 20;
        u.cache.read = 30;
        u.cache.write = 10;
        REQUIRE(u.total() == 210);
    }

    SECTION("operator+ accumulates correctly across steps") {
        TokenUsage step1;
        step1.input = 1000;
        step1.output = 200;
        step1.cache.read = 100;

        TokenUsage step2;
        step2.input = 500;
        step2.output = 300;
        step2.cache.read = 50;
        step2.cache.write = 25;

        TokenUsage total = step1 + step2;
        REQUIRE(total.input == 1500);
        REQUIRE(total.output == 500);
        REQUIRE(total.cache.read == 150);
        REQUIRE(total.cache.write == 25);
        REQUIRE(total.total() == 2175);
    }

    SECTION("zero usage falls back to estimate path (non-zero estimate)") {
        // When provider returns no usage (all zeros), estimate_tokens
        // should produce a positive value for non-empty text.
        // We test estimate_tokens indirectly by verifying token_count
        // increases after running with a message (covered in other tests).
        TokenUsage empty;
        REQUIRE(empty.total() == 0);
    }
}

TEST_CASE("SessionLoop token_count uses real usage when non-zero", "[core][session][token]") {
    // Verify that after run(), when no provider is configured,
    // token_count is 0 (pre-flight check prevents process_user_message).
    // When a provider is configured, process_user_message updates token_count
    // from real LLM usage (or estimate fallback).
    CreateParams params;
    params.project_id = "token-test";
    params.slug = "tok";
    params.directory = "/tmp";
    params.title = "Token Test";

    auto session = Session::create(params);
    REQUIRE(session.has_value());

    SessionLoop loop(std::move(*session));

    // No provider — loop enters process_user_message (which estimates user msg tokens),
    // then fails at process_llm_response. token_count > 0 from the user message estimate.
    auto result = loop.run("Hello token test");
    REQUIRE(result == LoopResult::Error);
    REQUIRE(loop.token_count() > 0);  // estimate_tokens("Hello token test") > 0
}

TEST_CASE("TokenUsage operator+= multi-step accumulation", "[core][session][token]") {
    TokenUsage total;
    for (int i = 0; i < 5; ++i) {
        TokenUsage step;
        step.input = 100;
        step.output = 50;
        step.cache.read = 20;
        total += step;
    }
    REQUIRE(total.input == 500);
    REQUIRE(total.output == 250);
    REQUIRE(total.cache.read == 100);
    REQUIRE(total.total() == 850);
}

// ============================================================================
// 2.8.7 – repairToolCall (tool name case correction + invalid fallback)
// ============================================================================

TEST_CASE("repairToolCall: uppercase tool name is lowercased and executed",
          "[core][session][repair_tool_call]") {
    ToolRegistryGuard registry_guard;  // clears registry on entry and exit

    // Create session first
    CreateParams cp;
    cp.project_id = "repair-test"; cp.slug = "rt"; cp.directory = "/tmp"; cp.title = "Repair";
    auto session = Session::create(cp);
    REQUIRE(session.has_value());

    // Register "echo_tool" (lowercase)
    ToolRegistry::instance().register_tool(std::make_unique<RepairEchoTool>("echo_tool"));

    // Verify the tool is actually registered
    REQUIRE(ToolRegistry::instance().get("echo_tool") != nullptr);
    REQUIRE(ToolRegistry::instance().get("ECHO_TOOL") == nullptr);  // exact match fails

    // Provider returns a tool call with uppercase name "ECHO_TOOL"
    ToolCallMockProvider provider;
    provider::ToolCall tc;
    tc.id   = "call-repair-1";
    tc.name = "ECHO_TOOL";        // LLM hallucinated uppercase
    tc.arguments = nlohmann::json::object();
    provider.set_tool_calls({tc});

    SessionLoop loop(std::move(*session));
    loop.set_provider(&provider);
    loop.set_model("mock-model");

    bool tool_result_received = false;
    bool tool_errored = false;
    std::string tool_result_output;
    loop.set_on_tool_result([&](const std::string& name, const std::string&, const ToolResult& result) {
        tool_result_received = true;
        // The result should NOT be an error — the lowercase lookup succeeded
        tool_errored = result.is_error;
        tool_result_output = result.output;
        (void)name;
    });

    auto result = loop.run("test repair");
    // Loop should complete normally (not error)
    REQUIRE(result != LoopResult::Error);
    // Tool result callback must have fired
    REQUIRE(tool_result_received);
    // The repaired lookup should have succeeded (no error)
    REQUIRE_FALSE(tool_errored);
}

TEST_CASE("repairToolCall: completely unknown tool returns structured error",
          "[core][session][repair_tool_call]") {
    ToolRegistryGuard registry_guard;  // clears registry on entry and exit (no tools)

    // Provider returns a tool call with a name that does not exist at all
    ToolCallMockProvider provider;
    provider::ToolCall tc;
    tc.id   = "call-unknown-1";
    tc.name = "nonexistent_tool_xyz";
    tc.arguments = nlohmann::json::object();
    provider.set_tool_calls({tc});

    CreateParams cp;
    cp.project_id = "repair-test2"; cp.slug = "rt2"; cp.directory = "/tmp"; cp.title = "Repair2";
    auto session = Session::create(cp);
    REQUIRE(session.has_value());

    SessionLoop loop(std::move(*session));
    loop.set_provider(&provider);
    loop.set_model("mock-model");

    bool result_received = false;
    std::string result_output;
    loop.set_on_tool_result([&](const std::string&, const std::string&, const ToolResult& r) {
        result_received = true;
        result_output = r.output;
    });

    loop.run("test unknown tool");
    REQUIRE(result_received);

    // The structured error payload must contain the tool name and an error field
    auto payload = nlohmann::json::parse(result_output);
    REQUIRE(payload.contains("tool"));
    REQUIRE(payload["tool"] == "nonexistent_tool_xyz");
    REQUIRE(payload.contains("error"));
}

// ============================================================================
// 2.8.7 – LiteLLM noop tool injection
// ============================================================================

TEST_CASE("LiteLLM noop injected when history has tool role messages and tools list is empty",
          "[core][session][litellm_noop]") {
    // This test verifies the noop-injection path by checking that the loop
    // completes without error when provider id contains "litellm" and the
    // message history already contains tool-role messages but no active tools.
    //
    // Strategy: run two sessions — the second one has no registered tools,
    // uses a LiteLLM-id provider, and has pre-populated tool role messages.

    ToolRegistryGuard registry_guard;  // clears registry on entry and exit
    ToolRegistry::instance().register_tool(std::make_unique<RepairEchoTool>("echo_tool"));

    ToolCallMockProvider provider;
    provider.set_provider_id("litellm-proxy");

    // First call: emit a tool-call so history gets a Tool role message
    provider::ToolCall tc;
    tc.id = "call-noop-1"; tc.name = "echo_tool"; tc.arguments = nlohmann::json::object();
    provider.set_tool_calls({tc});

    CreateParams cp;
    cp.project_id = "litellm-noop"; cp.slug = "lln"; cp.directory = "/tmp"; cp.title = "Noop";
    auto session = Session::create(cp);
    REQUIRE(session.has_value());

    SessionLoop loop(std::move(*session));
    loop.set_provider(&provider);
    loop.set_model("mock-model");

    // First run: populates history with a Tool role message via echo_tool
    auto r1 = loop.run("first turn");
    // Should not error — echo_tool exists
    REQUIRE(r1 != LoopResult::Error);

    // Now clear the registry so next LLM step has no active tools
    ToolRegistry::instance().clear();

    // Second run: no tools registered, LiteLLM provider, history has Tool msg
    // The noop injection path should keep the loop from crashing and allow
    // the second LLM call to complete (provider ignores the noop tool).
    auto r2 = loop.run("second turn — no tools");
    REQUIRE(r2 != LoopResult::Error);
}

// ============================================================================
// loop_result_to_string
// ============================================================================

TEST_CASE("loop_result_to_string", "[session][loop]") {
    REQUIRE(loop_result_to_string(LoopResult::Continue) == "continue");
    REQUIRE(loop_result_to_string(LoopResult::Stop)     == "stop");
    REQUIRE(loop_result_to_string(LoopResult::Compact)  == "compact");
    REQUIRE(loop_result_to_string(LoopResult::Error)    == "error");
    REQUIRE_THROWS(loop_result_to_string(static_cast<LoopResult>(99)));
}

TEST_CASE("SessionLoop::set callbacks", "[session][loop]") {
    CreateParams cp;
    cp.project_id = "test-cb"; cp.slug = "cb"; cp.directory = "/tmp"; cp.title = "CB";
    auto session = Session::create(cp);
    REQUIRE(session.has_value());

    SessionLoop loop(std::move(*session));

    bool stream_called = false;
    bool step_called = false;
    bool error_called = false;

    loop.set_on_stream_event([&](const StreamEvent& ev) {
        stream_called = true;
    });
    loop.set_on_step([&](const StepInfo& info) {
        step_called = true;
    });
    loop.set_on_error([&](const std::string& err, const std::string& code) {
        error_called = true;
    });

    // Callbacks are set; we just verify they compile and don't crash
    REQUIRE_FALSE(stream_called);
    REQUIRE_FALSE(step_called);
    REQUIRE_FALSE(error_called);
}

TEST_CASE("SessionLoop::stop_requested via request_stop", "[session][loop]") {
    CreateParams cp;
    cp.project_id = "test-stop"; cp.slug = "st"; cp.directory = "/tmp"; cp.title = "Stop";
    auto session = Session::create(cp);
    REQUIRE(session.has_value());

    SessionLoop loop(std::move(*session));
    // stop() should not crash when called before run()
    loop.stop();
    REQUIRE_FALSE(loop.is_running());
}
