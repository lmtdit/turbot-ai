#include <turbot/core/llm/llm.hpp>
#include <turbot/core/llm/provider_adapter.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <atomic>
#include <thread>

using namespace turbot::core;
using namespace turbot::core::llm;

// ===== Mock Provider for Testing =====

class MockProvider : public provider::Provider {
public:
    std::string id() const override { return "mock"; }
    std::string name() const override { return "Mock Provider"; }
    bool is_ready() const override { return ready_; }

    std::vector<provider::ModelInfo> list_models() const override {
        return {{"mock-model", "mock", "Mock Model", "A mock model for testing"}};
    }

    std::optional<provider::ModelInfo> get_model(const std::string& model_id) const override {
        if (model_id == "mock-model") {
            return {{"mock-model", "mock", "Mock Model", "A mock model for testing"}};
        }
        return std::nullopt;
    }

    bool supports_model(const std::string& model_id) const override {
        return model_id == "mock-model";
    }

    provider::ChatResponse chat(
        const std::vector<provider::ChatMessage>& messages,
        const std::string& model_id,
        const provider::ChatOptions& options
    ) override {
        provider::ChatResponse response;
        response.id = "chat-" + std::to_string(response_counter_++);
        response.model = model_id;
        response.finish_reason = "stop";

        if (should_error_) {
            response.error = {{"message", error_message_}, {"code", "test_error"}};
            return response;
        }

        // Return mock text response
        provider::ChatMessage choice;
        choice.role = provider::ChatRole::Assistant;
        choice.content = mock_text_response_;
        response.choices.push_back(choice);

        response.usage.input = 100;
        response.usage.output = 50;

        return response;
    }

    provider::ChatResponse chat_stream(
        const std::vector<provider::ChatMessage>& messages,
        const std::string& model_id,
        const provider::ChatOptions& options,
        provider::StreamCallback callback
    ) override {
        provider::ChatResponse response;
        response.id = "stream-" + std::to_string(response_counter_++);
        response.model = model_id;

        if (should_error_) {
            provider::ChatStreamEvent error_event;
            error_event.type = provider::StreamEventType::Error;
            error_event.error = {{"message", error_message_}, {"code", "test_error"}};
            callback(error_event);

            response.error = {{"message", error_message_}};
            response.finish_reason = "error";
            return response;
        }

        // Emit text-start event (implicit)
        int text_index = 0;

        // Stream text deltas
        for (const auto& delta : mock_deltas_) {
            // Check abort
            if (options.stream && abort_after_ > 0 && text_index >= abort_after_) {
                break;
            }

            provider::ChatStreamEvent event;
            event.type = provider::StreamEventType::TextDelta;
            event.content = delta;
            if (!callback(event)) {
                break;  // Stream aborted by callback
            }
            text_index++;
        }

        // Emit tool calls if any
        for (const auto& tc : mock_tool_calls_) {
            provider::ChatStreamEvent event;
            event.type = provider::StreamEventType::ToolCall;
            event.tool_call = tc;
            if (!callback(event)) {
                break;
            }
        }

        // Emit finish event
        provider::ChatStreamEvent finish_event;
        finish_event.type = provider::StreamEventType::Finish;
        finish_event.finish_reason = mock_finish_reason_;
        finish_event.usage = mock_usage_;
        callback(finish_event);

        response.finish_reason = mock_finish_reason_;
        response.usage = mock_usage_;
        return response;
    }

    int64_t count_tokens(
        const std::vector<provider::ChatMessage>& messages,
        const std::string& model_id
    ) const override {
        int64_t count = 0;
        for (const auto& msg : messages) {
            count += msg.content.size() / 4;  // Rough approximation
        }
        return count;
    }

    bool validate() override { return true; }

    // Mock configuration
    void set_mock_text_response(const std::string& text) { mock_text_response_ = text; }
    void set_mock_deltas(const std::vector<std::string>& deltas) { mock_deltas_ = deltas; }
    void set_mock_tool_calls(const std::vector<provider::ToolCall>& tcs) { mock_tool_calls_ = tcs; }
    void set_mock_usage(const TokenUsage& usage) { mock_usage_ = usage; }
    void set_mock_finish_reason(const std::string& reason) { mock_finish_reason_ = reason; }
    void set_should_error(bool should, const std::string& msg = "Test error") {
        should_error_ = should;
        error_message_ = msg;
    }
    void set_abort_after(int count) { abort_after_ = count; }
    void set_ready(bool ready) { ready_ = ready; }

private:
    bool ready_ = true;
    bool should_error_ = false;
    std::string error_message_;
    std::string mock_text_response_ = "Hello, world!";
    std::vector<std::string> mock_deltas_ = {"Hello", ", ", "world", "!"};
    std::vector<provider::ToolCall> mock_tool_calls_;
    TokenUsage mock_usage_{100, 50, 0};  // input=100, output=50, reasoning=0
    std::string mock_finish_reason_ = "stop";
    int abort_after_ = 0;
    std::atomic<int> response_counter_{0};
};

// ===== LLMMessage Tests =====

TEST_CASE("LLMMessage factory methods", "[llm][message]") {
    SECTION("system message") {
        auto msg = LLMMessage::system("You are helpful");
        REQUIRE(msg.role == provider::ChatRole::System);
        REQUIRE(msg.content == "You are helpful");
        REQUIRE_FALSE(msg.name.has_value());
        REQUIRE_FALSE(msg.tool_call_id.has_value());
        REQUIRE(msg.tool_calls.empty());
    }

    SECTION("user message") {
        auto msg = LLMMessage::user("Hello");
        REQUIRE(msg.role == provider::ChatRole::User);
        REQUIRE(msg.content == "Hello");
    }

    SECTION("assistant message") {
        auto msg = LLMMessage::assistant("Hi there!");
        REQUIRE(msg.role == provider::ChatRole::Assistant);
        REQUIRE(msg.content == "Hi there!");
    }

    SECTION("assistant with tools") {
        std::vector<ToolCallChunk> tools = {{"call-1", "read", R"({"path": "/tmp"})", true}};
        auto msg = LLMMessage::assistant_with_tools("Done", tools);
        REQUIRE(msg.role == provider::ChatRole::Assistant);
        REQUIRE(msg.content == "Done");
        REQUIRE(msg.tool_calls.size() == 1);
        REQUIRE(msg.tool_calls[0].name == "read");
    }

    SECTION("tool result") {
        auto msg = LLMMessage::tool_result("call-1", "file contents", false);
        REQUIRE(msg.role == provider::ChatRole::Tool);
        REQUIRE(msg.tool_call_id == "call-1");
        REQUIRE(msg.content == "file contents");
    }

    SECTION("tool result with error") {
        auto msg = LLMMessage::tool_result("call-1", "Error: file not found", true);
        REQUIRE(msg.role == provider::ChatRole::Tool);
        REQUIRE(msg.name == "error");
    }
}

TEST_CASE("LLMMessage serialization", "[llm][message][json]") {
    SECTION("to_json and from_json") {
        auto original = LLMMessage::user("Test message");
        auto j = original.to_json();
        auto restored = LLMMessage::from_json(j);

        REQUIRE(restored.role == original.role);
        REQUIRE(restored.content == original.content);
    }

    SECTION("tool calls serialization") {
        std::vector<ToolCallChunk> tools = {{"call-1", "read", R"({"path": "/tmp"})", true}};
        auto original = LLMMessage::assistant_with_tools("", tools);
        auto j = original.to_json();
        auto restored = LLMMessage::from_json(j);

        REQUIRE(restored.tool_calls.size() == 1);
        REQUIRE(restored.tool_calls[0].id == "call-1");
        REQUIRE(restored.tool_calls[0].name == "read");
    }
}

// ===== StreamParams Tests =====

TEST_CASE("StreamParams serialization", "[llm][params]") {
    SECTION("basic params") {
        StreamParams params;
        params.session_id = "test-session";
        params.temperature = 0.7;
        params.max_tokens = 1000;

        auto j = params.to_json();
        REQUIRE(j["session_id"] == "test-session");
        REQUIRE(j["temperature"] == 0.7);
        REQUIRE(j["max_tokens"] == 1000);
    }

    SECTION("with messages") {
        StreamParams params;
        params.messages.push_back(LLMMessage::system("Be helpful"));
        params.messages.push_back(LLMMessage::user("Hi"));

        auto j = params.to_json();
        REQUIRE(j["messages"].size() == 2);

        auto restored = StreamParams::from_json(j);
        REQUIRE(restored.messages.size() == 2);
        REQUIRE(restored.messages[0].role == provider::ChatRole::System);
        REQUIRE(restored.messages[1].role == provider::ChatRole::User);
    }

    SECTION("with tools") {
        StreamParams params;
        LLMToolDefinition tool;
        tool.name = "read";
        tool.description = "Read a file";
        tool.parameters = {{"type", "object"}, {"properties", {{"path", {{"type", "string"}}}}}};
        params.tools.push_back(tool);

        auto j = params.to_json();
        REQUIRE(j["tools"].size() == 1);

        auto restored = StreamParams::from_json(j);
        REQUIRE(restored.tools.size() == 1);
        REQUIRE(restored.tools[0].name == "read");
    }
}

// ===== StreamingState Tests =====

TEST_CASE("StreamingState basic operations", "[llm][streaming_state]") {
    StreamingState state;

    SECTION("initial state") {
        REQUIRE_FALSE(state.is_done());
        REQUIRE_FALSE(state.has_events());
        REQUIRE(state.get_text().empty());
        REQUIRE(state.usage().total() == 0);
    }

    SECTION("add and pop events") {
        state.add_event(StreamEvent::create_text_start("t1"));
        state.add_event(StreamEvent::create_text_delta("t1", "Hello"));

        REQUIRE(state.has_events());

        auto event1 = state.pop_event();
        REQUIRE(event1.has_value());
        REQUIRE(event1->type == StreamEventType::TextStart);

        auto event2 = state.pop_event();
        REQUIRE(event2.has_value());
        REQUIRE(event2->type == StreamEventType::TextDelta);
        REQUIRE(event2->delta == "Hello");

        REQUIRE_FALSE(state.has_events());
    }

    SECTION("mark done") {
        TokenUsage usage{100, 50, 0};  // input=100, output=50, reasoning=0
        state.mark_done(FinishReason::Stop, usage);

        REQUIRE(state.is_done());
        REQUIRE(state.finish_reason() == FinishReason::Stop);
        REQUIRE(state.usage().total() == 150);
    }

    SECTION("mark error") {
        state.mark_error("Test error", "test_code");

        REQUIRE(state.is_done());
        REQUIRE(state.finish_reason() == FinishReason::Error);
        REQUIRE(state.error() == "Test error");
    }

    SECTION("text accumulation") {
        state.add_event(StreamEvent::create_text_start("t1"));
        state.add_event(StreamEvent::create_text_delta("t1", "Hello"));
        state.add_event(StreamEvent::create_text_delta("t1", " "));
        state.add_event(StreamEvent::create_text_delta("t1", "World"));
        state.add_event(StreamEvent::create_text_end("t1"));

        REQUIRE(state.get_text() == "Hello World");
    }

    SECTION("tool call accumulation") {
        state.add_event(StreamEvent::create_tool_input_start("tc1", "read"));
        state.add_event(StreamEvent::create_tool_input_delta("tc1", R"({"path")"));
        state.add_event(StreamEvent::create_tool_input_delta("tc1", R"(: "/tmp")"));
        state.add_event(StreamEvent::create_tool_input_end("tc1"));

        auto tool_calls = state.get_tool_calls();
        REQUIRE(tool_calls.size() == 1);
        REQUIRE(tool_calls[0].name == "read");
        REQUIRE(tool_calls[0].arguments == R"({"path": "/tmp")");
        REQUIRE(tool_calls[0].is_complete);
    }
}

TEST_CASE("StreamingState to_result", "[llm][streaming_state]") {
    StreamingState state;
    state.set_response_id("resp-123");
    state.set_model("mock-model");
    state.add_event(StreamEvent::create_text_delta("t1", "Hello"));
    state.add_event(StreamEvent::create_finish(FinishReason::Stop, TokenUsage{100, 50, 0}));
    state.mark_done(FinishReason::Stop, TokenUsage{100, 50, 0});

    auto result = state.to_result();

    REQUIRE(result.id == "resp-123");
    REQUIRE(result.model == "mock-model");
    REQUIRE(result.events.size() == 2);  // text-delta + finish
    REQUIRE(result.finish_reason == FinishReason::Stop);
    REQUIRE(result.usage.total() == 150);
}

// ===== LLM::stream Tests =====

TEST_CASE("LLM stream with mock provider", "[llm][stream]") {
    MockProvider provider;

    SECTION("stream text content") {
        provider.set_mock_deltas({"Hello", ", ", "world", "!"});

        StreamParams params;
        params.session_id = "test-session";
        params.messages.push_back(LLMMessage::user("Say hello"));

        auto result = LLM::stream(provider, "mock-model", params);
        auto events = result.collect();

        REQUIRE(result.is_done());
        REQUIRE(events.size() >= 4);  // At least 4 text deltas + finish
        REQUIRE(result.final_text() == "Hello, world!");
        REQUIRE_FALSE(result.has_error());
    }

    SECTION("stream with tool calls") {
        provider.set_mock_deltas({});
        provider.set_mock_tool_calls({
            {"call-1", "function", "read", R"json({"path": "/tmp/test.txt"})json"}
        });

        StreamParams params;
        params.session_id = "test-session";

        auto result = LLM::stream(provider, "mock-model", params);
        auto events = result.collect();

        auto tool_calls = result.tool_calls();
        REQUIRE(tool_calls.size() == 1);
        REQUIRE(tool_calls[0].name == "read");
        REQUIRE(tool_calls[0].is_complete);
    }

    SECTION("stream with error") {
        provider.set_should_error(true, "API rate limit exceeded");

        StreamParams params;
        params.session_id = "test-session";

        auto result = LLM::stream(provider, "mock-model", params);
        auto events = result.collect();

        REQUIRE(result.has_error());
        REQUIRE(result.error() == "API rate limit exceeded");
    }

    SECTION("stream with event handler") {
        provider.set_mock_deltas({"Test", " ", "content"});

        std::vector<StreamEvent> captured_events;
        auto handler = [&captured_events](const StreamEvent& event) {
            captured_events.push_back(event);
        };

        StreamParams params;
        params.session_id = "test-session";

        auto result = LLM::stream(provider, "mock-model", params, handler);

        REQUIRE(captured_events.size() >= 3);  // At least 3 text deltas
    }
}

TEST_CASE("LLM complete with mock provider", "[llm][complete]") {
    MockProvider provider;

    SECTION("complete returns text") {
        provider.set_mock_text_response("This is the response.");

        StreamParams params;
        params.session_id = "test-session";
        params.messages.push_back(LLMMessage::user("Hello"));

        auto result = LLM::complete(provider, "mock-model", params);

        REQUIRE(result == "This is the response.");
    }

    SECTION("complete with error throws") {
        provider.set_should_error(true, "Model not found");

        StreamParams params;
        params.session_id = "test-session";

        REQUIRE_THROWS_AS(
            LLM::complete(provider, "mock-model", params),
            std::runtime_error
        );
    }
}

// ===== ProviderAdapter Tests =====

TEST_CASE("ProviderAdapter message conversion", "[llm][adapter]") {
    SECTION("to_provider_message") {
        auto llm_msg = LLMMessage::user("Hello");
        auto provider_msg = ProviderAdapter::to_provider_message(llm_msg);

        REQUIRE(provider_msg.role == provider::ChatRole::User);
        REQUIRE(provider_msg.content == "Hello");
    }

    SECTION("from_provider_message") {
        provider::ChatMessage pmsg = provider::ChatMessage::assistant("Hi!");
        auto llm_msg = ProviderAdapter::from_provider_message(pmsg);

        REQUIRE(llm_msg.role == provider::ChatRole::Assistant);
        REQUIRE(llm_msg.content == "Hi!");
    }

    SECTION("round-trip conversion") {
        auto original = LLMMessage::user("Test message");
        auto provider_msg = ProviderAdapter::to_provider_message(original);
        auto restored = ProviderAdapter::from_provider_message(provider_msg);

        REQUIRE(restored.role == original.role);
        REQUIRE(restored.content == original.content);
    }
}

TEST_CASE("ProviderAdapter tool conversion", "[llm][adapter]") {
    SECTION("to_provider_tool") {
        LLMToolDefinition tool;
        tool.name = "read";
        tool.description = "Read a file";
        tool.parameters = {{"type", "object"}};

        auto ptool = ProviderAdapter::to_provider_tool(tool);

        REQUIRE(ptool.name == "read");
        REQUIRE(ptool.description == "Read a file");
    }

    SECTION("to_tool_call_chunk") {
        provider::ToolCall tc;
        tc.id = "call-1";
        tc.name = "read";
        tc.arguments = {{"path", "/tmp"}};

        auto chunk = ProviderAdapter::to_tool_call_chunk(tc);

        REQUIRE(chunk.id == "call-1");
        REQUIRE(chunk.name == "read");
        REQUIRE(chunk.is_complete);
    }
}

TEST_CASE("ProviderAdapter format detection", "[llm][adapter]") {
    SECTION("detect OpenAI format") {
        REQUIRE(ProviderAdapter::detect_format("openai") == MessageFormat::OpenAI);
        REQUIRE(ProviderAdapter::detect_format("azure") == MessageFormat::OpenAI);
        REQUIRE(ProviderAdapter::detect_format("ollama") == MessageFormat::OpenAI);
    }

    SECTION("detect Anthropic format") {
        REQUIRE(ProviderAdapter::detect_format("anthropic") == MessageFormat::Anthropic);
        REQUIRE(ProviderAdapter::detect_format("claude") == MessageFormat::Anthropic);
    }

    SECTION("detect OpenAICompat format") {
        REQUIRE(ProviderAdapter::detect_format("unknown") == MessageFormat::OpenAICompat);
        REQUIRE(ProviderAdapter::detect_format("custom-provider") == MessageFormat::OpenAICompat);
    }
}

TEST_CASE("ProviderAdapter capability detection", "[llm][adapter]") {
    SECTION("streaming support") {
        REQUIRE(ProviderAdapter::supports_streaming("openai"));
        REQUIRE(ProviderAdapter::supports_streaming("anthropic"));
    }

    SECTION("tool call support") {
        REQUIRE(ProviderAdapter::supports_tool_calls("openai"));
        REQUIRE(ProviderAdapter::supports_tool_calls("anthropic"));
    }

    SECTION("reasoning support") {
        REQUIRE(ProviderAdapter::supports_reasoning("openai"));
        REQUIRE(ProviderAdapter::supports_reasoning("anthropic"));
    }
}

// ===== Integration Tests =====

TEST_CASE("LLM full conversation flow", "[llm][integration]") {
    MockProvider provider;
    provider.set_mock_deltas({"I", " ", "can", " ", "help", "!"});

    StreamParams params;
    params.session_id = "conv-1";
    params.temperature = 0.7;
    params.messages.push_back(LLMMessage::system("Be helpful"));
    params.messages.push_back(LLMMessage::user("Hello"));

    auto result = LLM::stream(provider, "mock-model", params);

    // Collect all events
    auto events = result.collect();

    // Verify
    REQUIRE(result.is_done());
    REQUIRE(result.final_text() == "I can help!");
    REQUIRE(result.finish_reason() == FinishReason::Stop);
    REQUIRE(result.usage().total() == 150);  // input=100, output=50
}
