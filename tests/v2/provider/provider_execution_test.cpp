/**
 * @file provider_execution_test.cpp
 * @brief Provider execution tests using MockProvider
 *
 * Tests for:
 * - MockProvider chat execution
 * - MockProvider streaming execution
 * - Error handling and retry scenarios
 * - Tool call handling
 */

#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include "../mock/mock_provider.hpp"
#include <turbot/core/provider/provider.hpp>

using namespace turbot::core::provider;
using namespace turbot::test;

// ==================== MockProvider Chat Tests ====================

TEST_CASE("Provider.Mock.Chat.Basic", "[Provider][Mock][Execution]") {
    MockProvider provider;
    provider.set_api_key("test-key");
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Hello")
    };
    
    auto response = provider.chat(messages, "mock-model");
    
    REQUIRE_FALSE(response.is_error());
    REQUIRE_FALSE(response.choices.empty());
    REQUIRE_FALSE(response.choices[0].content.empty());
}

TEST_CASE("Provider.Mock.Chat.WithCustomResponse", "[Provider][Mock][Execution]") {
    MockProvider provider;
    provider.set_api_key("test-key");
    
    // Set custom response
    ChatResponse custom;
    custom.id = "custom-123";
    custom.model = "mock-model";
    custom.finish_reason = "stop";
    custom.choices.push_back(ChatMessage::assistant("Custom response"));
    custom.usage = turbot::core::TokenUsage(10, 5, 15);
    
    provider.set_next_response(custom);
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Test message")
    };
    
    auto response = provider.chat(messages, "mock-model");
    
    REQUIRE(response.id == "custom-123");
    REQUIRE(response.choices[0].content == "Custom response");
}

TEST_CASE("Provider.Mock.Chat.WithToolCalls", "[Provider][Mock][Execution]") {
    MockProvider provider;
    provider.set_api_key("test-key");
    
    // Set response with tool calls
    ChatResponse tool_response;
    tool_response.id = "tool-call-1";
    tool_response.model = "mock-model";
    tool_response.finish_reason = "tool_calls";
    
    ToolCall tc;
    tc.id = "call-123";
    tc.name = "get_weather";
    tc.arguments = R"({"city": "Beijing"})"_json;
    
    tool_response.choices.push_back(ChatMessage::assistant_with_tools("", {tc}));
    provider.set_next_response(tool_response);
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("What's the weather in Beijing?")
    };
    
    auto response = provider.chat(messages, "mock-model");
    
    REQUIRE(response.has_tool_calls());
    REQUIRE(response.choices[0].tool_calls.has_value());
    REQUIRE(response.choices[0].tool_calls->size() == 1);
    REQUIRE(response.choices[0].tool_calls->at(0).name == "get_weather");
}

TEST_CASE("Provider.Mock.Chat.ErrorSequence", "[Provider][Mock][Execution]") {
    MockProvider provider;
    provider.set_api_key("test-key");
    
    // Set error sequence: first call fails, second succeeds
    provider.set_error_sequence({MockError::RateLimitExceeded, MockError::None});
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Hello")
    };
    
    // First call should return error
    auto response1 = provider.chat(messages, "mock-model");
    REQUIRE(response1.is_error());
    REQUIRE((*response1.error)["type"] == "rate_limit_exceeded");
    
    // Second call should succeed
    auto response2 = provider.chat(messages, "mock-model");
    REQUIRE_FALSE(response2.is_error());
}

TEST_CASE("Provider.Mock.Chat.NetworkError", "[Provider][Mock][Execution]") {
    MockProvider provider;
    provider.set_api_key("test-key");
    
    provider.set_error_sequence({MockError::NetworkError});
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Hello")
    };
    
    auto response = provider.chat(messages, "mock-model");
    REQUIRE(response.is_error());
    REQUIRE((*response.error)["type"] == "network_error");
}

TEST_CASE("Provider.Mock.Chat.TimeoutError", "[Provider][Mock][Execution]") {
    MockProvider provider;
    provider.set_api_key("test-key");
    
    provider.set_error_sequence({MockError::TimeoutError});
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Hello")
    };
    
    auto response = provider.chat(messages, "mock-model");
    REQUIRE(response.is_error());
    REQUIRE((*response.error)["type"] == "timeout");
}

TEST_CASE("Provider.Mock.Chat.ResponseSequence", "[Provider][Mock][Execution]") {
    MockProvider provider;
    provider.set_api_key("test-key");
    
    // Set sequence of responses
    std::vector<ChatResponse> responses;
    for (int i = 0; i < 3; i++) {
        ChatResponse resp;
        resp.id = "response-" + std::to_string(i);
        resp.model = "mock-model";
        resp.finish_reason = "stop";
        resp.choices.push_back(ChatMessage::assistant("Response " + std::to_string(i)));
        responses.push_back(resp);
    }
    provider.set_response_sequence(responses);
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Test")
    };
    
    for (int i = 0; i < 3; i++) {
        auto response = provider.chat(messages, "mock-model");
        REQUIRE(response.id == "response-" + std::to_string(i));
        REQUIRE(response.choices[0].content == "Response " + std::to_string(i));
    }
}

// ==================== MockProvider Streaming Tests ====================

TEST_CASE("Provider.Mock.Stream.Basic", "[Provider][Mock][Execution]") {
    MockProvider provider;
    provider.set_api_key("test-key");
    
    std::vector<std::string> received_chunks;
    auto callback = [&received_chunks](const ChatStreamEvent& event) {
        if (event.type == StreamEventType::TextDelta) {
            received_chunks.push_back(event.content);
        }
        return true;
    };
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Hello")
    };
    
    ChatOptions options;
    options.stream = true;
    
    auto response = provider.chat_stream(messages, "mock-model", options, callback);
    
    REQUIRE_FALSE(response.is_error());
    REQUIRE(received_chunks.size() > 0);
}

TEST_CASE("Provider.Mock.Stream.WithChunks", "[Provider][Mock][Execution]") {
    MockProvider provider;
    provider.set_api_key("test-key");
    
    // Set explicit stream chunks
    provider.set_stream_chunks({"Hello", " ", "world", "!"});
    
    std::vector<std::string> received_chunks;
    auto callback = [&received_chunks](const ChatStreamEvent& event) {
        if (event.type == StreamEventType::TextDelta) {
            received_chunks.push_back(event.content);
        }
        return true;
    };
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Test streaming")
    };
    
    ChatOptions options;
    options.stream = true;
    
    auto response = provider.chat_stream(messages, "mock-model", options, callback);
    
    REQUIRE(received_chunks.size() == 4);
    REQUIRE(received_chunks[0] == "Hello");
    REQUIRE(received_chunks[1] == " ");
    REQUIRE(received_chunks[2] == "world");
    REQUIRE(received_chunks[3] == "!");
}

TEST_CASE("Provider.Mock.Stream.Abort", "[Provider][Mock][Execution]") {
    MockProvider provider;
    provider.set_api_key("test-key");
    
    // Set many chunks
    provider.set_stream_chunks({"1", "2", "3", "4", "5"});
    
    std::vector<std::string> received_chunks;
    int chunk_count = 0;
    auto callback = [&received_chunks, &chunk_count](const ChatStreamEvent& event) {
        if (event.type == StreamEventType::TextDelta) {
            received_chunks.push_back(event.content);
            chunk_count++;
            // Abort after 2 chunks
            return chunk_count < 2;
        }
        return true;
    };
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Test abort")
    };
    
    ChatOptions options;
    options.stream = true;
    
    auto response = provider.chat_stream(messages, "mock-model", options, callback);
    
    // Should have received only 2 chunks before abort
    REQUIRE(received_chunks.size() == 2);
}

TEST_CASE("Provider.Mock.Stream.WithToolCalls", "[Provider][Mock][Execution]") {
    MockProvider provider;
    provider.set_api_key("test-key");
    
    // Set response with tool calls
    ChatResponse tool_response;
    tool_response.id = "stream-tool-1";
    tool_response.model = "mock-model";
    tool_response.finish_reason = "tool_calls";
    
    ToolCall tc;
    tc.id = "call-stream";
    tc.name = "search";
    tc.arguments = R"({"query": "test"})"_json;
    
    tool_response.choices.push_back(ChatMessage::assistant_with_tools("", {tc}));
    provider.set_next_response(tool_response);
    
    std::vector<ChatStreamEvent> events;
    auto callback = [&events](const ChatStreamEvent& event) {
        events.push_back(event);
        return true;
    };
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Search for test")
    };
    
    ChatOptions options;
    options.stream = true;
    
    auto response = provider.chat_stream(messages, "mock-model", options, callback);
    
    // Should have received tool call event
    bool has_tool_call = false;
    bool has_finish = false;
    for (const auto& e : events) {
        if (e.type == StreamEventType::ToolCall) {
            has_tool_call = true;
            REQUIRE(e.tool_call.has_value());
            REQUIRE(e.tool_call->name == "search");
        }
        if (e.type == StreamEventType::Finish) {
            has_finish = true;
        }
    }
    REQUIRE(has_tool_call);
    REQUIRE(has_finish);
}

TEST_CASE("Provider.Mock.Stream.FinishEvent", "[Provider][Mock][Execution]") {
    MockProvider provider;
    provider.set_api_key("test-key");
    
    std::optional<ChatStreamEvent> finish_event;
    auto callback = [&finish_event](const ChatStreamEvent& event) {
        if (event.type == StreamEventType::Finish) {
            finish_event = event;
        }
        return true;
    };
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Test finish")
    };
    
    ChatOptions options;
    options.stream = true;
    
    auto response = provider.chat_stream(messages, "mock-model", options, callback);
    
    REQUIRE(finish_event.has_value());
    REQUIRE(finish_event->finish_reason == "stop");
    REQUIRE(finish_event->usage.total() > 0);
}

// ==================== MockProvider Builder Tests ====================

TEST_CASE("Provider.Mock.Builder.Basic", "[Provider][Mock][Execution]") {
    auto provider = MockProviderBuilder()
        .with_api_key("test-key")
        .with_ready(true)
        .build();
    
    REQUIRE(provider->is_ready());
    REQUIRE(provider->id() == "mock");
}

TEST_CASE("Provider.Mock.Builder.WithModel", "[Provider][Mock][Execution]") {
    ModelInfo model;
    model.id = "test-model";
    model.name = "Test Model";
    model.context_window = 8192;
    
    auto provider = MockProviderBuilder()
        .with_api_key("test-key")
        .with_model(model)
        .build();
    
    auto models = provider->list_models();
    REQUIRE_FALSE(models.empty());
    REQUIRE(models[0].id == "test-model");
}

TEST_CASE("Provider.Mock.Builder.WithTextResponse", "[Provider][Mock][Execution]") {
    auto provider = MockProviderBuilder()
        .with_api_key("test-key")
        .with_text_response("Hello from builder!")
        .build();
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Test")
    };
    
    auto response = provider->chat(messages, "mock-model");
    REQUIRE(response.choices[0].content == "Hello from builder!");
}

TEST_CASE("Provider.Mock.Builder.WithToolCallResponse", "[Provider][Mock][Execution]") {
    auto provider = MockProviderBuilder()
        .with_api_key("test-key")
        .with_tool_call_response("calculator", "calc-1", R"({"expr": "1+1"})"_json)
        .build();
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Calculate 1+1")
    };
    
    auto response = provider->chat(messages, "mock-model");
    
    REQUIRE(response.has_tool_calls());
    REQUIRE(response.choices[0].tool_calls->at(0).name == "calculator");
}

TEST_CASE("Provider.Mock.Builder.WithStreamChunks", "[Provider][Mock][Execution]") {
    auto provider = MockProviderBuilder()
        .with_api_key("test-key")
        .with_stream_chunks({"A", "B", "C"})
        .build();
    
    std::vector<std::string> chunks;
    auto callback = [&chunks](const ChatStreamEvent& event) {
        if (event.type == StreamEventType::TextDelta) {
            chunks.push_back(event.content);
        }
        return true;
    };
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Test")
    };
    
    ChatOptions options;
    options.stream = true;
    
    (void)provider->chat_stream(messages, "mock-model", options, callback);
    
    REQUIRE(chunks.size() == 3);
    REQUIRE(chunks[0] == "A");
    REQUIRE(chunks[1] == "B");
    REQUIRE(chunks[2] == "C");
}

TEST_CASE("Provider.Mock.Builder.WithErrorSequence", "[Provider][Mock][Execution]") {
    auto provider = MockProviderBuilder()
        .with_api_key("test-key")
        .with_error_sequence({MockError::ServerError, MockError::None})
        .build();
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Test")
    };
    
    // First call fails
    auto response1 = provider->chat(messages, "mock-model");
    REQUIRE(response1.is_error());
    
    // Second call succeeds
    auto response2 = provider->chat(messages, "mock-model");
    REQUIRE_FALSE(response2.is_error());
}

// ==================== MockProvider Request Recording Tests ====================

TEST_CASE("Provider.Mock.Recording.Messages", "[Provider][Mock][Execution]") {
    MockProvider provider;
    provider.set_api_key("test-key");
    
    std::vector<ChatMessage> messages = {
        ChatMessage::system("You are helpful"),
        ChatMessage::user("Hello")
    };
    
    (void)provider.chat(messages, "test-model");
    
    auto last_messages = provider.last_messages();
    REQUIRE(last_messages.size() == 2);
    REQUIRE(last_messages[0].role == ChatRole::System);
    REQUIRE(last_messages[1].role == ChatRole::User);
    REQUIRE(last_messages[1].content == "Hello");
}

TEST_CASE("Provider.Mock.Recording.ModelId", "[Provider][Mock][Execution]") {
    MockProvider provider;
    provider.set_api_key("test-key");
    
    (void)provider.chat({}, "gpt-4");
    
    REQUIRE(provider.last_model_id() == "gpt-4");
}

TEST_CASE("Provider.Mock.Recording.Options", "[Provider][Mock][Execution]") {
    MockProvider provider;
    provider.set_api_key("test-key");
    
    ChatOptions options;
    options.temperature = 0.5;
    options.max_tokens = 1000;
    
    (void)provider.chat({}, "test-model", options);
    
    auto last_options = provider.last_options();
    REQUIRE(last_options.temperature == 0.5);
    REQUIRE(last_options.max_tokens == 1000);
}

TEST_CASE("Provider.Mock.Recording.CallCount", "[Provider][Mock][Execution]") {
    MockProvider provider;
    provider.set_api_key("test-key");
    
    REQUIRE(provider.call_count() == 0);
    
    (void)provider.chat({}, "test-model");
    REQUIRE(provider.call_count() == 1);
    
    (void)provider.chat({}, "test-model");
    REQUIRE(provider.call_count() == 2);
    
    provider.reset();
    REQUIRE(provider.call_count() == 0);
}

// ==================== MockProvider State Management Tests ====================

TEST_CASE("Provider.Mock.State.Ready", "[Provider][Mock][Execution]") {
    MockProvider provider;
    
    provider.set_ready(true);
    REQUIRE(provider.is_ready());
    
    provider.set_ready(false);
    REQUIRE_FALSE(provider.is_ready());
}

TEST_CASE("Provider.Mock.State.Validate", "[Provider][Mock][Execution]") {
    MockProvider provider;
    
    REQUIRE_FALSE(provider.validate());  // No API key
    
    provider.set_api_key("test-key");
    REQUIRE(provider.validate());
}

TEST_CASE("Provider.Mock.State.Reset", "[Provider][Mock][Execution]") {
    MockProvider provider;
    provider.set_api_key("test-key");
    
    // Make some calls
    (void)provider.chat({}, "test-model");
    (void)provider.chat({}, "test-model");
    
    REQUIRE(provider.call_count() == 2);
    
    // Reset
    provider.reset();
    
    REQUIRE(provider.call_count() == 0);
    REQUIRE(provider.last_messages().empty());
    REQUIRE(provider.last_model_id().empty());
}

// ==================== MockProvider Token Counting Tests ====================

TEST_CASE("Provider.Mock.CountTokens.Basic", "[Provider][Mock][Execution]") {
    MockProvider provider;
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Hello, world!")
    };
    
    auto count = provider.count_tokens(messages, "mock-model");
    REQUIRE(count > 0);
}

TEST_CASE("Provider.Mock.CountTokens.MultipleMessages", "[Provider][Mock][Execution]") {
    MockProvider provider;
    
    std::vector<ChatMessage> messages = {
        ChatMessage::system("You are helpful"),
        ChatMessage::user("Hello"),
        ChatMessage::assistant("Hi there!"),
        ChatMessage::user("How are you?")
    };
    
    auto count = provider.count_tokens(messages, "mock-model");
    REQUIRE(count > 0);
}

TEST_CASE("Provider.Mock.CountTokens.Empty", "[Provider][Mock][Execution]") {
    MockProvider provider;
    
    std::vector<ChatMessage> messages;
    
    auto count = provider.count_tokens(messages, "mock-model");
    REQUIRE(count == 0);
}
