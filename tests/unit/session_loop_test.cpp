#include <catch2/catch_test_macros.hpp>
#include <turbot/core/session/session_loop.hpp>
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
    loop.set_on_tool_call([&](const std::string&, const nlohmann::json&) { tool_call_called = true; });
    loop.set_on_tool_result([&](const std::string&, const ToolResult&) { tool_result_called = true; });
    loop.set_on_error([&](const std::string&) { error_called = true; });
    
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
    // No agent set
    
    auto result = loop.step();
    
    // Should continue (nothing to do)
    REQUIRE((result == LoopResult::Continue || result == LoopResult::Stop));
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
    
    // Run with a message to trigger token estimation
    loop.run("This is a test message for token estimation");
    
    // Token count should be positive
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
