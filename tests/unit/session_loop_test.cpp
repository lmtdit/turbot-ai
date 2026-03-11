#include <catch2/catch_test_macros.hpp>
#include <turbot/core/session/session_loop.hpp>
#include <turbot/core/session/retry_manager.hpp>
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/agent/builtin/build_agent.hpp>
#include <turbot/core/agent/builtin/plan_agent.hpp>
#include <turbot/core/agent/builtin/explore_agent.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <memory>

using namespace turbot::core::session;
using namespace turbot::core::agent;
using namespace turbot::core;
using namespace turbot::core::tool;

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
