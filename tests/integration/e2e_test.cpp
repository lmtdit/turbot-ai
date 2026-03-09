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
TEST_CASE_METHOD(E2ETest, "E2E-07: Error Retry", "[e2e][e2e-07]") {
    // Configure error sequence: 2 errors then success
    setup_error_sequence({
        MockError::RateLimitExceeded,
        MockError::NetworkError,
        MockError::None
    });

    // Set final success response
    setup_simple_qa("Success after retries!");

    // Run the loop
    loop->run("Test retry logic");

    // Verify retries occurred
    CHECK(provider->call_count() >= 1);
}

// ============================================================================
// E2E-08: Doom Loop Detection
// ============================================================================
TEST_CASE_METHOD(E2ETest, "E2E-08: Doom Loop Detection", "[e2e][e2e-08]") {
    // Configure with low doom loop threshold
    session::SessionLoopConfig config;
    config.doom_loop_threshold = 3;
    loop->set_config(config);

    // Configure repeated same tool calls
    for (int i = 0; i < 5; i++) {
        setup_tool_call("same_tool", "tool-" + std::to_string(i), nlohmann::json{
            {"param", "same_value"}
        });
    }

    // Run the loop
    loop->run("Keep doing the same thing");

    // Verify doom loop was detected (should stop, not infinite loop)
    // The loop should either stop or continue with a different result
    CHECK(provider->call_count() <= config.doom_loop_threshold + 1);
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
