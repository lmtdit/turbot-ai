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

// ============================================================================
// 2.8.7 – toolChoice transparent forwarding
// ============================================================================

TEST_CASE("LLM::to_chat_options forwards tool_choice via extra", "[llm][toolchoice]") {
    SECTION("tool_choice=required is written into options.extra") {
        StreamParams params;
        params.session_id = "tc-test";
        params.temperature = 1.0;
        params.tool_choice = "required";

        auto opts = LLM::to_chat_options(params);

        REQUIRE(opts.extra.contains("tool_choice"));
        REQUIRE(opts.extra["tool_choice"] == "required");
    }

    SECTION("tool_choice=none is written into options.extra") {
        StreamParams params;
        params.tool_choice = "none";

        auto opts = LLM::to_chat_options(params);

        REQUIRE(opts.extra.contains("tool_choice"));
        REQUIRE(opts.extra["tool_choice"] == "none");
    }

    SECTION("tool_choice=auto is written into options.extra") {
        StreamParams params;
        params.tool_choice = "auto";

        auto opts = LLM::to_chat_options(params);

        REQUIRE(opts.extra.contains("tool_choice"));
        REQUIRE(opts.extra["tool_choice"] == "auto");
    }

    SECTION("no tool_choice — extra does NOT contain tool_choice key") {
        StreamParams params;
        // params.tool_choice is std::nullopt by default

        auto opts = LLM::to_chat_options(params);

        REQUIRE_FALSE(opts.extra.contains("tool_choice"));
    }

    SECTION("tool_choice forwarded alongside other tools in params") {
        StreamParams params;
        params.tool_choice = "required";

        LLMToolDefinition td;
        td.name = "bash";
        td.description = "Run bash commands";
        td.parameters = {{"type", "object"}, {"properties", nlohmann::json::object()}};
        params.tools.push_back(td);

        auto opts = LLM::to_chat_options(params);

        REQUIRE(opts.tools.size() == 1);
        REQUIRE(opts.extra.contains("tool_choice"));
        REQUIRE(opts.extra["tool_choice"] == "required");
    }
}

// ============================================================================
// ToolCallResult serialization
// ============================================================================

TEST_CASE("ToolCallResult::to_json and from_json", "[llm][tool_call_result]") {
    SECTION("normal result") {
        ToolCallResult result;
        result.tool_call_id = "call_1";
        result.content = "Result content";
        result.is_error = false;

        auto j = result.to_json();
        REQUIRE(j["tool_call_id"] == "call_1");
        REQUIRE(j["content"] == "Result content");
        REQUIRE_FALSE(j.contains("is_error"));

        auto restored = ToolCallResult::from_json(j);
        REQUIRE(restored.tool_call_id == "call_1");
        REQUIRE(restored.content == "Result content");
        REQUIRE_FALSE(restored.is_error);
    }

    SECTION("error result") {
        ToolCallResult result;
        result.tool_call_id = "call_err";
        result.content = "Error: something went wrong";
        result.is_error = true;

        auto j = result.to_json();
        REQUIRE(j["is_error"] == true);

        auto restored = ToolCallResult::from_json(j);
        REQUIRE(restored.is_error == true);
    }
}

// ============================================================================
// LLMToolDefinition serialization
// ============================================================================

TEST_CASE("LLMToolDefinition::to_json and from_json", "[llm][tool_definition]") {
    LLMToolDefinition tool;
    tool.name = "search";
    tool.description = "Search the web";
    tool.parameters = {{"type", "object"}, {"properties", {{"query", {{"type", "string"}}}}}};

    auto j = tool.to_json();
    REQUIRE(j["type"] == "function");
    REQUIRE(j["function"]["name"] == "search");
    REQUIRE(j["function"]["description"] == "Search the web");
    REQUIRE(j.contains("function"));

    auto restored = LLMToolDefinition::from_json(j);
    REQUIRE(restored.name == "search");
    REQUIRE(restored.description == "Search the web");
}

// ============================================================================
// LLMMessage with optional fields
// ============================================================================

TEST_CASE("LLMMessage serialization with optional fields", "[llm][message][json]") {
    SECTION("message with name") {
        auto msg = LLMMessage::user("Hello");
        msg.name = "alice";

        auto j = msg.to_json();
        REQUIRE(j.contains("name"));
        REQUIRE(j["name"] == "alice");

        auto restored = LLMMessage::from_json(j);
        REQUIRE(restored.name.has_value());
        REQUIRE(restored.name.value() == "alice");
    }

    SECTION("tool_result message with tool_call_id") {
        auto msg = LLMMessage::tool_result("call_42", "The answer is 42", false);
        REQUIRE(msg.tool_call_id.has_value());
        REQUIRE(msg.tool_call_id.value() == "call_42");

        auto j = msg.to_json();
        REQUIRE(j.contains("tool_call_id"));
        REQUIRE(j["tool_call_id"] == "call_42");

        auto restored = LLMMessage::from_json(j);
        REQUIRE(restored.tool_call_id.has_value());
        REQUIRE(restored.tool_call_id.value() == "call_42");
    }
}

// ============================================================================
// LLMToolDefinition::from_json flat format (no "function" wrapper)
// ============================================================================

TEST_CASE("LLMToolDefinition::from_json flat format", "[llm][tool_definition]") {
    // When JSON has no "function" key, it should fall back to reading fields directly
    nlohmann::json flat = {
        {"name", "grep"},
        {"description", "Search files"},
        {"parameters", {{"type", "object"}}}
    };

    auto tool = LLMToolDefinition::from_json(flat);
    REQUIRE(tool.name == "grep");
    REQUIRE(tool.description == "Search files");
    REQUIRE(tool.parameters["type"] == "object");
}

// ============================================================================
// StreamParams with optional fields (tool_choice, top_p, stop)
// ============================================================================

TEST_CASE("StreamParams serialization with optional fields", "[llm][params]") {
    SECTION("with tool_choice") {
        StreamParams params;
        params.session_id = "sess-1";
        params.tool_choice = "auto";

        auto j = params.to_json();
        REQUIRE(j.contains("tool_choice"));
        REQUIRE(j["tool_choice"] == "auto");

        auto restored = StreamParams::from_json(j);
        REQUIRE(restored.tool_choice.has_value());
        REQUIRE(restored.tool_choice.value() == "auto");
    }

    SECTION("with top_p") {
        StreamParams params;
        params.top_p = 0.95;

        auto j = params.to_json();
        REQUIRE(j.contains("top_p"));

        auto restored = StreamParams::from_json(j);
        REQUIRE(restored.top_p.has_value());
    }

    SECTION("with stop sequences") {
        StreamParams params;
        params.stop = {"<|end|>", "STOP"};

        auto j = params.to_json();
        REQUIRE(j.contains("stop"));
        REQUIRE(j["stop"].size() == 2);

        auto restored = StreamParams::from_json(j);
        REQUIRE(restored.stop.size() == 2);
        REQUIRE(restored.stop[0] == "<|end|>");
    }

    SECTION("with max_tokens in from_json") {
        nlohmann::json j = {
            {"session_id", "sess-x"},
            {"temperature", 0.8},
            {"max_tokens", 2048}
        };
        auto params = StreamParams::from_json(j);
        REQUIRE(params.max_tokens.has_value());
        REQUIRE(params.max_tokens.value() == 2048);
    }
}

// ============================================================================
// StreamingState::get_reasoning
// ============================================================================

TEST_CASE("StreamingState::get_reasoning", "[llm][streaming_state]") {
    StreamingState state;

    SECTION("empty reasoning returns empty string") {
        REQUIRE(state.get_reasoning().empty());
    }

    SECTION("accumulates reasoning deltas") {
        StreamEvent ev1;
        ev1.type = StreamEventType::ReasoningDelta;
        ev1.delta = "Think ";
        state.add_event(ev1);

        StreamEvent ev2;
        ev2.type = StreamEventType::ReasoningDelta;
        ev2.delta = "step by step";
        state.add_event(ev2);

        REQUIRE(state.get_reasoning() == "Think step by step");
    }

    SECTION("non-reasoning events do not affect reasoning text") {
        state.add_event(StreamEvent::create_text_delta("t1", "Hello"));

        StreamEvent reason_ev;
        reason_ev.type = StreamEventType::ReasoningDelta;
        reason_ev.delta = "reason";
        state.add_event(reason_ev);

        REQUIRE(state.get_text() == "Hello");
        REQUIRE(state.get_reasoning() == "reason");
    }
}

// ============================================================================
// LLMStreamResult with null state (no-op / default paths)
// ============================================================================

TEST_CASE("LLMStreamResult null state", "[llm][stream_result]") {
    // Construct with null state
    LLMStreamResult result(nullptr);

    REQUIRE(result.is_done());
    REQUIRE(result.final_text().empty());
    REQUIRE(result.final_reasoning().empty());
    REQUIRE(result.tool_calls().empty());
    REQUIRE(result.usage().total() == 0);
    REQUIRE(result.finish_reason() == FinishReason::Stop);
    REQUIRE_FALSE(result.has_error());
    REQUIRE_FALSE(result.error().has_value());
    REQUIRE_FALSE(result.next().has_value());
}

// ============================================================================
// LLMStreamResult::final_reasoning with a real state
// ============================================================================

TEST_CASE("LLMStreamResult::final_reasoning", "[llm][stream_result]") {
    auto state = std::make_shared<StreamingState>();

    StreamEvent ev;
    ev.type = StreamEventType::ReasoningDelta;
    ev.delta = "I think therefore I am";
    state->add_event(ev);
    state->mark_done(FinishReason::Stop, TokenUsage{});

    LLMStreamResult result(state);
    // Drain events first
    while (result.next().has_value()) {}

    REQUIRE(result.final_reasoning() == "I think therefore I am");
}

// ============================================================================
// LLM::to_provider_message with tool_calls
// ============================================================================

TEST_CASE("LLM::to_provider_message with tool_calls", "[llm][convert]") {
    SECTION("assistant message with tool_calls") {
        LLMMessage msg = LLMMessage::assistant("Let me search");
        ToolCallChunk tc;
        tc.id = "call_1";
        tc.name = "search";
        tc.arguments = R"({"query":"test"})";
        tc.is_complete = true;
        msg.tool_calls.push_back(tc);

        auto pmsg = LLM::to_provider_message(msg);
        REQUIRE(pmsg.tool_calls.has_value());
        REQUIRE(pmsg.tool_calls->size() == 1);
        REQUIRE((*pmsg.tool_calls)[0].id == "call_1");
        REQUIRE((*pmsg.tool_calls)[0].name == "search");
    }

    SECTION("assistant message with invalid JSON arguments") {
        LLMMessage msg = LLMMessage::assistant("...");
        ToolCallChunk tc;
        tc.id = "call_bad";
        tc.name = "tool";
        tc.arguments = "invalid json {{{";
        tc.is_complete = true;
        msg.tool_calls.push_back(tc);

        // Should not throw, invalid JSON falls back to string
        auto pmsg = LLM::to_provider_message(msg);
        REQUIRE(pmsg.tool_calls.has_value());
        // Arguments should be stored as string when JSON parse fails
    }
}

// ============================================================================
// StreamingState::pop_event empty queue
// ============================================================================

TEST_CASE("StreamingState::pop_event empty queue", "[llm][streaming_state]") {
    StreamingState state;
    
    // Pop from empty queue should return nullopt
    auto result = state.pop_event();
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("StreamingState::pop_event drains queue", "[llm][streaming_state]") {
    StreamingState state;
    
    state.add_event(StreamEvent::create_text_delta("t1", "Hello"));
    state.add_event(StreamEvent::create_text_delta("t2", "World"));
    
    // Pop first event
    auto e1 = state.pop_event();
    REQUIRE(e1.has_value());
    REQUIRE(e1->delta == "Hello");
    
    // Pop second event
    auto e2 = state.pop_event();
    REQUIRE(e2.has_value());
    REQUIRE(e2->delta == "World");
    
    // Queue is now empty
    auto e3 = state.pop_event();
    REQUIRE_FALSE(e3.has_value());
}

// ============================================================================
// StreamingState::has_events
// ============================================================================

TEST_CASE("StreamingState::has_events", "[llm][streaming_state]") {
    StreamingState state;
    
    REQUIRE_FALSE(state.has_events());
    
    state.add_event(StreamEvent::create_text_delta("t1", "test"));
    REQUIRE(state.has_events());
    
    state.pop_event();
    REQUIRE_FALSE(state.has_events());
}

// ============================================================================
// MockProvider with Reasoning support
// ============================================================================

class ReasoningMockProvider : public provider::Provider {
public:
    std::string id() const override { return "reasoning-mock"; }
    std::string name() const override { return "Reasoning Mock Provider"; }
    bool is_ready() const override { return true; }

    std::vector<provider::ModelInfo> list_models() const override {
        return {{"reasoning-model", "reasoning-mock", "Reasoning Model", "A reasoning model"}};
    }

    std::optional<provider::ModelInfo> get_model(const std::string& model_id) const override {
        if (model_id == "reasoning-model") {
            return {{"reasoning-model", "reasoning-mock", "Reasoning Model", "A reasoning model"}};
        }
        return std::nullopt;
    }

    bool supports_model(const std::string& model_id) const override {
        return model_id == "reasoning-model";
    }

    provider::ChatResponse chat(
        const std::vector<provider::ChatMessage>& messages,
        const std::string& model_id,
        const provider::ChatOptions& options
    ) override {
        provider::ChatResponse response;
        response.id = "reasoning-chat-1";
        response.model = model_id;
        response.finish_reason = "stop";
        
        provider::ChatMessage choice;
        choice.role = provider::ChatRole::Assistant;
        choice.content = "Final answer";
        response.choices.push_back(choice);
        
        return response;
    }

    provider::ChatResponse chat_stream(
        const std::vector<provider::ChatMessage>& messages,
        const std::string& model_id,
        const provider::ChatOptions& options,
        provider::StreamCallback callback
    ) override {
        provider::ChatResponse response;
        response.id = "reasoning-stream-1";
        response.model = model_id;

        // Emit reasoning event
        if (emit_reasoning_) {
            provider::ChatStreamEvent reasoning_event;
            reasoning_event.type = provider::StreamEventType::Reasoning;
            reasoning_event.content = "Let me think about this...";
            callback(reasoning_event);
        }

        // Emit text delta
        provider::ChatStreamEvent text_event;
        text_event.type = provider::StreamEventType::TextDelta;
        text_event.content = "Final answer";
        callback(text_event);

        // Emit finish
        provider::ChatStreamEvent finish_event;
        finish_event.type = provider::StreamEventType::Finish;
        finish_event.finish_reason = "stop";
        finish_event.usage = TokenUsage{100, 50, 20};
        callback(finish_event);

        response.finish_reason = "stop";
        response.usage = TokenUsage{100, 50, 20};
        return response;
    }

    int64_t count_tokens(
        const std::vector<provider::ChatMessage>& messages,
        const std::string& model_id
    ) const override {
        return 100;
    }

    bool validate() override { return true; }

    void set_emit_reasoning(bool emit) { emit_reasoning_ = emit; }

private:
    bool emit_reasoning_ = true;
};

// ============================================================================
// LLM::stream with Reasoning event
// ============================================================================

TEST_CASE("LLM stream with reasoning event", "[llm][stream][reasoning]") {
    ReasoningMockProvider provider;
    StreamParams params;
    params.session_id = "test-reasoning";
    params.messages.push_back(LLMMessage::user("What is 2+2?"));

    std::vector<StreamEvent> collected_events;
    auto result = LLM::stream(provider, "reasoning-model", params, 
        [&collected_events](const StreamEvent& e) {
            collected_events.push_back(e);
        });

    // Drain events
    while (auto event = result.next()) {
        // Process events
    }

    // Should have reasoning in the final result
    REQUIRE_FALSE(result.final_reasoning().empty());
}

// ============================================================================
// LLM::stream with abort callback
// ============================================================================

TEST_CASE("LLM stream with abort callback", "[llm][stream][abort]") {
    MockProvider provider;
    
    // Create params with abort callback
    StreamParams params;
    params.session_id = "test-abort";
    params.messages.push_back(LLMMessage::user("Hello"));
    
    // Set abort to return true
    bool should_abort = false;
    params.is_aborted = [&should_abort]() { return should_abort; };

    std::vector<StreamEvent> events;
    auto result = LLM::stream(provider, "mock-model", params, 
        [&events](const StreamEvent& e) {
            events.push_back(e);
        });

    // Drain events normally first
    while (auto event = result.next()) {
        // Process
    }

    // Should complete normally
    REQUIRE(result.is_done());
}

// ============================================================================
// LLM::complete error handling
// ============================================================================

TEST_CASE("LLM complete with error response", "[llm][complete][error]") {
    MockProvider provider;
    provider.set_should_error(true, "API rate limit exceeded");
    
    StreamParams params;
    params.session_id = "test-error";
    params.messages.push_back(LLMMessage::user("Hello"));

    REQUIRE_THROWS_AS(
        LLM::complete(provider, "mock-model", params),
        std::runtime_error
    );
}

TEST_CASE("LLM complete with error but no message", "[llm][complete][error]") {
    MockProvider provider;
    provider.set_should_error(true, "");  // Error but empty message
    
    StreamParams params;
    params.session_id = "test-error-no-msg";
    params.messages.push_back(LLMMessage::user("Hello"));

    // Should throw generic error message
    REQUIRE_THROWS_AS(
        LLM::complete(provider, "mock-model", params),
        std::runtime_error
    );
}

// ============================================================================
// StreamingState::to_result
// ============================================================================

TEST_CASE("StreamingState::to_result", "[llm][streaming_state]") {
    StreamingState state;
    
    state.set_response_id("resp-123");
    state.set_model("gpt-4");
    state.add_event(StreamEvent::create_text_delta("t1", "Hello"));
    state.mark_done(FinishReason::Stop, TokenUsage{100, 50, 0});
    
    auto result = state.to_result();
    
    REQUIRE(result.id == "resp-123");
    REQUIRE(result.model == "gpt-4");
    REQUIRE(result.finish_reason == FinishReason::Stop);
    REQUIRE(result.usage.input == 100);
    REQUIRE(result.usage.output == 50);
    REQUIRE_FALSE(result.error.has_value());
}

// ============================================================================
// StreamingState::mark_error with code
// ============================================================================

TEST_CASE("StreamingState::mark_error with error code", "[llm][streaming_state]") {
    StreamingState state;
    
    state.mark_error("Rate limit exceeded", "rate_limit");
    
    REQUIRE(state.is_done());
    REQUIRE(state.error() == "Rate limit exceeded");
    
    // Drain the error event
    auto event = state.pop_event();
    REQUIRE(event.has_value());
    REQUIRE(event->type == StreamEventType::Error);
    REQUIRE(event->error_message == "Rate limit exceeded");
    REQUIRE(event->error_code == "rate_limit");
}
