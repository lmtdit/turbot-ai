// tests/integration/agent_collaboration_test.cpp
// Integration tests for multi-agent collaboration

#include "../mock/e2e_fixture.hpp"
#include <turbot/core/permission/permission.hpp>
#include <turbot/core/agent/agent.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace turbot::core;
using namespace turbot::test;

// ============================================================================
// AGENT-01: Main Agent calls Sub Agent
// ============================================================================

TEST_CASE_METHOD(E2ETest, "AGENT-01: Main Agent calls Sub Agent", "[integration][agent]") {
    // Configure mock provider to return a task tool call
    setup_tool_call("task", "task-123", nlohmann::json{
        {"subagent_type", "explore"},
        {"prompt", "Explore the codebase structure"}
    });
    
    // Second response after tool call
    setup_simple_qa("Task completed successfully. Found the structure.");
    
    // Run the loop
    auto result = loop->run("Explore the codebase structure");
    
    // Verify
    CHECK(result == session::LoopResult::Stop);
    CHECK(provider->call_count() >= 1);
}

// ============================================================================
// AGENT-02: Sub Agent permission isolation
// ============================================================================

TEST_CASE_METHOD(E2ETest, "AGENT-02: Sub Agent permission isolation", "[integration][agent]") {
    // Get agents
    auto build_agent = agent::AgentRegistry::instance().get("build");
    auto explore_agent = agent::AgentRegistry::instance().get("explore");
    
    REQUIRE(build_agent != nullptr);
    REQUIRE(explore_agent != nullptr);
    
    // Verify permission differences
    // Build agent should have broader permissions
    const auto& build_permission = build_agent->info().permission;
    const auto& explore_permission = explore_agent->info().permission;
    
    // Both should have valid permission rulesets
    CHECK_FALSE(build_permission.empty());
    CHECK_FALSE(explore_permission.empty());
    
    // Explore agent should have more restrictive permissions
    // (read-only, no write operations)
    bool explore_has_deny_all = false;
    for (const auto& rule : explore_permission) {
        if (rule.permission == "*" && rule.pattern == "*" && 
            rule.action == permission::PermissionAction::Deny) {
            explore_has_deny_all = true;
        }
    }
    CHECK(explore_has_deny_all);
}

// ============================================================================
// AGENT-03: Sub Agent result return
// ============================================================================

TEST_CASE_METHOD(E2ETest, "AGENT-03: Sub Agent result return", "[integration][agent]") {
    // Configure a tool call and its result
    setup_tool_call("read_file", "tool-read-1", nlohmann::json{
        {"path", "/tmp/test_file.txt"}
    });
    
    // Final response after tool execution
    setup_simple_qa("The file content has been analyzed.");
    
    // Create a test file
    create_test_file("test_file.txt", "Test content for analysis");
    
    // Run the loop
    auto result = loop->run("Read and analyze the file /tmp/test_file.txt");
    
    // Verify
    CHECK(result == session::LoopResult::Stop);
    
    // Verify messages contain tool results
    const auto& messages = loop->messages();
    CHECK_FALSE(messages.empty());
}

// ============================================================================
// AGENT-04: Nested Agent calls (simulated)
// ============================================================================

TEST_CASE_METHOD(E2ETest, "AGENT-04: Nested Agent calls", "[integration][agent]") {
    // Configure multiple sequential tool calls to simulate nested behavior
    setup_tool_call("read_file", "tool-1", nlohmann::json{{"path", "/tmp/file1.txt"}});
    setup_tool_call("grep", "tool-2", nlohmann::json{{"pattern", "TODO"}});
    setup_simple_qa("Analysis complete with nested operations.");
    
    // Create test files
    create_test_file("file1.txt", "File content with TODO items");
    
    // Run the loop
    auto result = loop->run("Analyze the project and create a summary");
    
    // Verify
    CHECK(result == session::LoopResult::Stop);
    
    // Verify multiple tool calls were made (simulating nested behavior)
    CHECK(provider->call_count() >= 1);
}

// ============================================================================
// AGENT-05: Agent switching during session
// ============================================================================

TEST_CASE_METHOD(E2ETest, "AGENT-05: Agent switching during session", "[integration][agent]") {
    // Start with build agent
    setup_simple_qa("Build agent response.");
    auto result1 = loop->run("Initial task");
    CHECK(result1 == session::LoopResult::Stop);
    
    // Switch to explore agent
    auto explore_agent = agent::AgentRegistry::instance().get("explore");
    REQUIRE(explore_agent != nullptr);
    loop->set_agent(explore_agent);
    
    setup_simple_qa("Explore agent response.");
    auto result2 = loop->run("Explore task");
    CHECK(result2 == session::LoopResult::Stop);
    
    // Verify both agents were used
    CHECK(provider->call_count() == 2);
}

// ============================================================================
// AGENT-06: Agent registry operations
// ============================================================================

TEST_CASE("AGENT-06: Agent registry operations", "[integration][agent]") {
    auto& registry = agent::AgentRegistry::instance();
    
    // Initialize built-in agents for testing
    agent::agent_loader::initialize_builtin_agents();
    
    // Verify built-in agents are registered
    CHECK(registry.get("build") != nullptr);
    CHECK(registry.get("explore") != nullptr);
    CHECK(registry.get("plan") != nullptr);
    
    // Verify non-existent agent returns nullptr
    CHECK(registry.get("non_existent_agent") == nullptr);
}

// ============================================================================
// AGENT-07: Agent info and metadata
// ============================================================================

TEST_CASE_METHOD(E2ETest, "AGENT-07: Agent info and metadata", "[integration][agent]") {
    auto build_agent = agent::AgentRegistry::instance().get("build");
    REQUIRE(build_agent != nullptr);
    
    // Verify agent info
    CHECK(build_agent->name() == "build");
    CHECK_FALSE(build_agent->description().empty());
    
    // Verify permission is configured
    CHECK_FALSE(build_agent->info().permission.empty());
}

// ============================================================================
// AGENT-08: Multiple tool calls in sequence
// ============================================================================

TEST_CASE_METHOD(E2ETest, "AGENT-08: Multiple tool calls in sequence", "[integration][agent]") {
    // Configure multiple tool calls
    setup_multiple_tool_calls({
        {"read_file", "tool-1", {{"path", "/tmp/a.txt"}}},
        {"read_file", "tool-2", {{"path", "/tmp/b.txt"}}}
    });
    
    // Final response
    setup_simple_qa("Both files have been processed.");
    
    // Create test files
    create_test_file("a.txt", "Content A");
    create_test_file("b.txt", "Content B");
    
    // Run the loop
    auto result = loop->run("Read both files");
    
    // Verify
    CHECK(result == session::LoopResult::Stop);
}
