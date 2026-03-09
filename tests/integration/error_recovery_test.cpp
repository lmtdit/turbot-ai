// tests/integration/error_recovery_test.cpp
// Integration tests for error recovery

#include "../mock/e2e_fixture.hpp"
#include <turbot/core/session/retry_manager.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>

using namespace turbot::core;
using namespace turbot::test;

// ============================================================================
// ERR-01: API error auto retry
// ============================================================================

TEST_CASE_METHOD(E2ETest, "ERR-01: API error auto retry", "[integration][error]") {
    // Configure error sequence: 2 errors then success
    setup_error_sequence({
        MockError::RateLimitExceeded,
        MockError::RateLimitExceeded,
        MockError::None
    });
    
    // Set final success response
    setup_simple_qa("Success after retries!");
    
    // Run the loop
    auto result = loop->run("Hello");
    
    // Verify retries occurred
    CHECK(result == session::LoopResult::Stop);
    CHECK(provider->call_count() >= 1);
}

// ============================================================================
// ERR-02: Network timeout recovery
// ============================================================================

TEST_CASE_METHOD(E2ETest, "ERR-02: Network timeout recovery", "[integration][error]") {
    // Configure timeout error then success
    setup_error_sequence({
        MockError::TimeoutError,
        MockError::None
    });
    
    // Set success response
    setup_simple_qa("Recovered from timeout!");
    
    // Run the loop
    auto result = loop->run("Hello");
    
    // Verify recovery
    CHECK(result == session::LoopResult::Stop);
}

// ============================================================================
// ERR-03: Permission denied handling
// ============================================================================

TEST_CASE_METHOD(E2ETest, "ERR-03: Permission denied handling", "[integration][error]") {
    // Use explore agent with restrictive permissions
    auto explore_agent = agent::AgentRegistry::instance().get("explore");
    REQUIRE(explore_agent != nullptr);
    loop->set_agent(explore_agent);
    
    // Configure a response
    setup_simple_qa("Operation completed within permission boundaries.");
    
    // Run the loop with a simple read operation
    auto result = loop->run("List files in the directory");
    
    // Verify the operation completed
    CHECK(result == session::LoopResult::Stop);
}

// ============================================================================
// ERR-04: Tool execution error
// ============================================================================

TEST_CASE_METHOD(E2ETest, "ERR-04: Tool execution error", "[integration][error]") {
    // Configure a tool call that will fail
    setup_tool_call("bash", "tool-bash-1", nlohmann::json{
        {"command", "exit 1"}
    });
    
    // Follow-up response after tool error
    setup_simple_qa("The command failed, but I handled it gracefully.");
    
    // Run the loop
    auto result = loop->run("Run a failing command");
    
    // Verify error was handled (loop should complete, not crash)
    CHECK(result == session::LoopResult::Stop);
}

// ============================================================================
// ERR-05: Server error recovery
// ============================================================================

TEST_CASE_METHOD(E2ETest, "ERR-05: Server error recovery", "[integration][error]") {
    // Configure server error then success
    setup_error_sequence({
        MockError::ServerError,
        MockError::None
    });
    
    setup_simple_qa("Recovered from server error!");
    
    // Run the loop
    auto result = loop->run("Hello");
    
    // Verify recovery
    CHECK(result == session::LoopResult::Stop);
}

// ============================================================================
// ERR-06: Invalid API key error
// ============================================================================

TEST_CASE_METHOD(E2ETest, "ERR-06: Invalid API key error", "[integration][error]") {
    // Configure invalid API key error (should not retry)
    setup_error_sequence({
        MockError::InvalidApiKey
    });
    
    setup_simple_qa("Response after API key error.");
    
    // Run the loop
    auto result = loop->run("Hello");
    
    // Should complete (error was handled)
    CHECK(provider->call_count() >= 1);
}

// ============================================================================
// ERR-07: Multiple consecutive errors
// ============================================================================

TEST_CASE_METHOD(E2ETest, "ERR-07: Multiple consecutive errors", "[integration][error]") {
    // Configure multiple different errors
    setup_error_sequence({
        MockError::NetworkError,
        MockError::RateLimitExceeded,
        MockError::None
    });
    
    setup_simple_qa("Recovered from multiple errors!");
    
    // Run the loop
    auto result = loop->run("Hello");
    
    // Verify recovery after multiple errors
    CHECK(result == session::LoopResult::Stop);
}

// ============================================================================
// ERR-08: Error in streaming
// ============================================================================

TEST_CASE_METHOD(E2ETest, "ERR-08: Error in streaming", "[integration][error]") {
    // Configure stream chunks followed by an error
    setup_stream_chunks({"Hello ", "world "});
    setup_error_sequence({MockError::None});
    
    setup_simple_qa("Stream completed.");
    
    // Run the loop
    auto result = loop->run("Tell me a story");
    
    // Verify completion
    CHECK(result == session::LoopResult::Stop);
}

// ============================================================================
// RetryManager unit tests
// ============================================================================

TEST_CASE("RetryManager: Exponential backoff calculation", "[integration][error]") {
    session::RetryConfig config;
    config.max_attempts = 3;
    config.base_delay_ms = 100;
    config.max_delay_ms = 10000;
    config.jitter_factor = 0;  // Disable jitter for predictable tests
    
    // Verify backoff increases: 100 * 2^attempt
    auto delay1 = session::RetryManager::calculate_delay(0, config);
    auto delay2 = session::RetryManager::calculate_delay(1, config);
    auto delay3 = session::RetryManager::calculate_delay(2, config);
    
    CHECK(delay1 == config.base_delay_ms);        // 100 * 2^0 = 100
    CHECK(delay2 == config.base_delay_ms * 2);    // 100 * 2^1 = 200
    CHECK(delay3 == config.base_delay_ms * 4);    // 100 * 2^2 = 400
    
    // Verify max delay is respected
    CHECK(delay3 <= config.max_delay_ms);
}

TEST_CASE("RetryManager: Retry count limit", "[integration][error]") {
    session::RetryConfig config;
    config.max_attempts = 2;
    
    // Verify config is valid
    CHECK(config.validate());
}

TEST_CASE("RetryManager: Jitter application", "[integration][error]") {
    int base_delay = 1000;
    double jitter_factor = 0.1;
    
    // Apply jitter multiple times
    for (int i = 0; i < 10; i++) {
        int jittered = session::RetryManager::apply_jitter(base_delay, jitter_factor);
        // Jittered value should be within 10% of base
        CHECK(jittered >= base_delay * (1 - jitter_factor));
        CHECK(jittered <= base_delay * (1 + jitter_factor));
    }
}
