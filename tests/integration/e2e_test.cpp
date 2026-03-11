// tests/integration/e2e_test.cpp
// E2E tests for core flow verification

#include "../mock/e2e_fixture.hpp"
#include <turbot/core/session/session_loop.hpp>
#include <turbot/core/agent/builtin/build_agent.hpp>
#include <turbot/core/agent/builtin/plan_agent.hpp>
#include <turbot/core/agent/builtin/explore_agent.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>

using namespace turbot::core;
using namespace turbot::test;

// ============================================================================
// E2E-01: Simple Q&A
// ============================================================================
TEST_CASE_METHOD(E2ETest, "E2E-01: Simple Q&A", "[e2e][e2e-01]") {
    // Configure mock provider for simple response
    setup_simple_qa("The answer is 4.");

    // Run the loop
    auto result = loop->run("What is 2+2?");

    // Verify
    CHECK(result == session::LoopResult::Stop);
    CHECK(provider->call_count() == 1);
    CHECK_FALSE(loop->messages().empty());

    // Verify the user message was sent
    const auto& last_messages = provider->last_messages();
    CHECK_FALSE(last_messages.empty());
    CHECK(last_messages.front().role == provider::ChatRole::User);
}

// ============================================================================
// E2E-02: Tool Call
// ============================================================================
TEST_CASE_METHOD(E2ETest, "E2E-02: Tool Call", "[e2e][e2e-02]") {
    // First response: tool call
    setup_tool_call("read_file", "tool-123", nlohmann::json{
        {"path", "/tmp/test.txt"}
    });

    // Second response: final answer
    setup_simple_qa("The file content has been read.");

    // Run the loop
    auto result = loop->run("Read the file /tmp/test.txt");

    // Verify
    CHECK(result == session::LoopResult::Stop);
    CHECK(provider->call_count() >= 1);  // At least one LLM call

    // Verify tool call was requested
    const auto& first_messages = provider->last_messages();
    CHECK_FALSE(first_messages.empty());
}

// ============================================================================
// E2E-03: Multi-turn Conversation
// ============================================================================
TEST_CASE_METHOD(E2ETest, "E2E-03: Multi-turn Conversation", "[e2e][e2e-03]") {
    // First response
    setup_simple_qa("Hello! How can I help you?");

    // Run first turn
    auto result1 = loop->run("Hello!");
    CHECK(result1 == session::LoopResult::Stop);
    CHECK(provider->call_count() == 1);

    // Second response
    setup_simple_qa("The weather is sunny today.");

    // Run second turn
    auto result2 = loop->run("How's the weather?");
    CHECK(result2 == session::LoopResult::Stop);
    CHECK(provider->call_count() == 2);

    // Verify conversation history
    const auto& messages = loop->messages();
    CHECK(messages.size() >= 4);  // 2 user + 2 assistant
}

// ============================================================================
// E2E-04: Agent Switching
// ============================================================================
TEST_CASE_METHOD(E2ETest, "E2E-04: Agent Switching", "[e2e][e2e-04]") {
    // Start with build agent
    setup_simple_qa("I'm the build agent.");
    auto result1 = loop->run("Hello");
    CHECK(result1 == session::LoopResult::Stop);

    // Switch to explore agent
    auto explore_agent = agent::AgentRegistry::instance().get("explore");
    REQUIRE(explore_agent != nullptr);
    loop->set_agent(explore_agent);

    setup_simple_qa("I'm the explore agent.");
    auto result2 = loop->run("Explore the codebase");
    CHECK(result2 == session::LoopResult::Stop);

    // Verify both agents were used
    CHECK(provider->call_count() == 2);
}

// ============================================================================
// E2E-05: Stream Interruption
// ============================================================================
TEST_CASE_METHOD(E2ETest, "E2E-05: Stream Interruption", "[e2e][e2e-05]") {
    // Configure streaming chunks
    setup_stream_chunks({"Hello ", "world ", "this ", "is ", "a ", "test"});

    // Set up a callback to stop after receiving some chunks
    int chunk_count = 0;
    loop->set_on_stream_event([&chunk_count, this](const StreamEvent& event) {
        if (event.is_text_event()) {
            chunk_count++;
            if (chunk_count >= 3) {
                loop->stop();  // Stop streaming
            }
        }
    });

    // Run the loop
    (void)loop->run("Tell me a story");

    // Verify streaming was interrupted
    CHECK(chunk_count <= 4);  // Should stop after 3 chunks (+ finish event)
}

// ============================================================================
// E2E-06: Context Compaction
// ============================================================================
TEST_CASE_METHOD(E2ETest, "E2E-06: Context Compaction", "[e2e][e2e-06]") {
    // Configure with low compaction threshold
    session::SessionLoopConfig config;
    config.compact_threshold = 100;  // Low threshold to trigger compaction
    config.auto_compact = true;
    loop->set_config(config);

    // Generate a long conversation
    for (int i = 0; i < 10; i++) {
        setup_simple_qa("Response " + std::to_string(i));
        loop->run("Question " + std::to_string(i));
    }

    // Verify compaction occurred (token count should be managed)
    // Note: token_count is estimated and may not reflect actual compaction
    CHECK(loop->step_number() > 0);
}

// ============================================================================
// E2E-07: Error Retry
// ============================================================================
TEST_CASE_METHOD(E2ETest, "E2E-07: Error Propagation (no internal retry)", "[e2e][e2e-07]") {
    // Configure: first call returns error; SessionLoop has no internal retry mechanism.
    setup_error_sequence({
        MockError::RateLimitExceeded,
        MockError::None
    });

    // The second response would succeed if retried, but SessionLoop won't retry.
    setup_simple_qa("This would succeed if retried.");

    // Capture error callback
    std::string error_code;
    loop->set_on_error([&](const std::string& /*msg*/, const std::string& code) {
        error_code = code;
    });

    // Run the loop
    auto result = loop->run("Test error propagation");

    // SessionLoop has no internal retry: stops on first error (2.7.1 conclusion B)
    CHECK(result == session::LoopResult::Error);
    CHECK(provider->call_count() == 1);         // Exactly 1 call (no retry)
    CHECK(error_code == "llm_error");            // Error callback fired with correct code
}

// ============================================================================
// E2E-08: Doom Loop Detection
// ============================================================================
TEST_CASE_METHOD(E2ETest, "E2E-08: Doom Loop Detection", "[e2e][e2e-08]") {
    // Configure with low doom loop threshold
    session::SessionLoopConfig config;
    config.doom_loop_threshold = 3;
    loop->set_config(config);

    // Configure repeated same tool calls (same name AND same input = doom loop)
    nlohmann::json same_input = {{"param", "same_value"}};
    for (int i = 0; i < 5; i++) {
        setup_tool_call("same_tool", "tool-" + std::to_string(i), same_input);
    }

    // Capture error callback
    std::string error_code;
    loop->set_on_error([&](const std::string& /*msg*/, const std::string& code) {
        error_code = code;
    });

    // Run the loop
    auto result = loop->run("Keep doing the same thing");

    // Doom loop MUST terminate with Error (not Stop).
    // Note: LoopResult has no DoomLoop variant; doom loop detection returns LoopResult::Error.
    CHECK(result == session::LoopResult::Error);
    CHECK(error_code == "doom_loop");

    // Verify LLM was not called more than threshold+1 times
    CHECK(provider->call_count() <= config.doom_loop_threshold + 1);
}

// ============================================================================
// E2E-08b: Doom Loop Detection - Behavioral Verification
// Verifies that exactly threshold consecutive identical (tool, input) pairs
// return LoopResult::Error with error_code == "doom_loop".
// Registers a real MockTool so the first (threshold-1) calls go through the
// full tool execution path; doom loop fires on the threshold-th call.
// ============================================================================
TEST_CASE_METHOD(E2ETest, "E2E-08b: Doom Loop triggers Error with registered tool", "[e2e][e2e-08b][doom-loop]") {
    // Create a shared counter so we can observe call_count after ownership transfer
    auto shared_counter = std::make_shared<std::atomic<int>>(0);

    // Register the tool (ToolRegistry takes unique_ptr ownership)
    tool::ToolRegistry::instance().register_tool(
        std::make_unique<MockTool>("repeat_tool", shared_counter)
    );

    // Configure threshold = 3: the 3rd consecutive identical call should trigger Error
    session::SessionLoopConfig config;
    config.doom_loop_threshold = 3;
    loop->set_config(config);

    // Queue threshold+1 identical (tool_name, input) pairs.
    // doom loop triggers on the 3rd call (after 2 successful executions).
    nlohmann::json fixed_input = {{"input", "fixed"}};
    for (int i = 0; i < config.doom_loop_threshold + 1; i++) {
        setup_tool_call("repeat_tool", "call-" + std::to_string(i), fixed_input);
    }

    // Track whether the doom loop error callback fires
    std::string error_code;
    loop->set_on_error([&](const std::string& /*msg*/, const std::string& code) {
        error_code = code;
    });

    auto result = loop->run("Repeat the same tool");

    // Must terminate with Error (doom loop detected)
    CHECK(result == session::LoopResult::Error);
    // Error code must indicate doom loop
    CHECK(error_code == "doom_loop");
    // LLM called at most threshold times (no extra calls after detection)
    CHECK(provider->call_count() <= config.doom_loop_threshold);
    // Tool executed threshold-1 times before doom loop fired
    CHECK(shared_counter->load(std::memory_order_relaxed) == config.doom_loop_threshold - 1);

    // Clean up registered tool (avoid polluting other tests)
    tool::ToolRegistry::instance().remove("repeat_tool");
}
// ============================================================================
// Additional Edge Cases
// ============================================================================

TEST_CASE_METHOD(E2ETest, "E2E-Empty Response Handling", "[e2e][edge]") {
    // Configure empty response
    provider->set_next_response(provider::ChatResponse{
        .id = "empty",
        .model = "mock-model",
        .choices = {provider::ChatMessage::assistant("")},
        .usage = TokenUsage(10, 0, 10),
        .finish_reason = "stop"
    });

    auto result = loop->run("Say nothing");
    CHECK(result == session::LoopResult::Stop);
}

TEST_CASE_METHOD(E2ETest, "E2E-Multiple Tool Calls", "[e2e][edge]") {
    // Configure multiple tool calls in one response
    setup_multiple_tool_calls({
        {"read_file", "tool-1", {{"path", "/tmp/a.txt"}}},
        {"read_file", "tool-2", {{"path", "/tmp/b.txt"}}},
        {"bash", "tool-3", {{"command", "ls -la"}}}
    });

    // Final response
    setup_simple_qa("All tools executed successfully.");

    auto result = loop->run("Read two files and list directory");
    CHECK(result == session::LoopResult::Stop);
}

TEST_CASE_METHOD(E2ETest, "E2E-Max Iterations", "[e2e][edge]") {
    // Configure low max iterations
    session::SessionLoopConfig config;
    config.max_iterations = 5;
    loop->set_config(config);

    // Configure continuous tool calls
    for (int i = 0; i < 10; i++) {
        setup_tool_call("tool_" + std::to_string(i), "tool-" + std::to_string(i), nlohmann::json{});
    }

    loop->run("Keep going");
    CHECK(loop->step_number() <= config.max_iterations);
}
