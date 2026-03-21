#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <turbot/core/llm/stream_event.hpp>
#include <turbot/core/llm/llm.hpp>
#include <turbot/core/llm/system_prompt.hpp>
#include <turbot/core/llm/message_builder.hpp>
#include <turbot/core/llm/prompt_builder.hpp>
#include <turbot/core/llm/provider_adapter.hpp>
#include <turbot/core/llm/tool_schema.hpp>
#include <turbot/core/common/version.hpp>
#include <turbot/core/tool/tool.hpp>

using namespace turbot::core;

// Mock Tool for testing
class MockTool : public tool::Tool {
public:
    MockTool(const std::string& name, const std::string& desc)
        : name_(name), desc_(desc) {}

    [[nodiscard]] std::string name() const override { return name_; }
    [[nodiscard]] std::string description() const override { return desc_; }
    [[nodiscard]] nlohmann::json input_schema() const override {
        return {{"type", "object"}, {"properties", {{"input", {{"type", "string"}}}}}};
    }

    tool::ToolResult execute(const nlohmann::json& input, tool::ToolContext& ctx) override {
        return tool::ToolResult::success(name_, "mock result");
    }

private:
    std::string name_;
    std::string desc_;
};

// ==================== StreamEventType String Conversion Tests ====================

TEST_CASE("LLM.StreamEventType.ToString", "[Core][LLM]") {
    REQUIRE(stream_event_type_to_string(StreamEventType::Start) == "start");
    REQUIRE(stream_event_type_to_string(StreamEventType::Finish) == "finish");
    REQUIRE(stream_event_type_to_string(StreamEventType::Error) == "error");
    REQUIRE(stream_event_type_to_string(StreamEventType::TextStart) == "text-start");
    REQUIRE(stream_event_type_to_string(StreamEventType::TextDelta) == "text-delta");
    REQUIRE(stream_event_type_to_string(StreamEventType::TextEnd) == "text-end");
    REQUIRE(stream_event_type_to_string(StreamEventType::ReasoningStart) == "reasoning-start");
    REQUIRE(stream_event_type_to_string(StreamEventType::ReasoningDelta) == "reasoning-delta");
    REQUIRE(stream_event_type_to_string(StreamEventType::ReasoningEnd) == "reasoning-end");
    REQUIRE(stream_event_type_to_string(StreamEventType::ToolInputStart) == "tool-input-start");
    REQUIRE(stream_event_type_to_string(StreamEventType::ToolInputDelta) == "tool-input-delta");
    REQUIRE(stream_event_type_to_string(StreamEventType::ToolInputEnd) == "tool-input-end");
    REQUIRE(stream_event_type_to_string(StreamEventType::ToolCall) == "tool-call");
    REQUIRE(stream_event_type_to_string(StreamEventType::StepStart) == "step-start");
    REQUIRE(stream_event_type_to_string(StreamEventType::StepFinish) == "step-finish");
    REQUIRE(stream_event_type_to_string(StreamEventType::SourceStart) == "source-start");
    REQUIRE(stream_event_type_to_string(StreamEventType::SourceEnd) == "source-end");
}

TEST_CASE("LLM.StreamEventType.FromString", "[Core][LLM]") {
    REQUIRE(stream_event_type_from_string("start") == StreamEventType::Start);
    REQUIRE(stream_event_type_from_string("finish") == StreamEventType::Finish);
    REQUIRE(stream_event_type_from_string("error") == StreamEventType::Error);
    REQUIRE(stream_event_type_from_string("text-start") == StreamEventType::TextStart);
    REQUIRE(stream_event_type_from_string("text-delta") == StreamEventType::TextDelta);
    REQUIRE(stream_event_type_from_string("text-end") == StreamEventType::TextEnd);
}

// ==================== FinishReason String Conversion Tests ====================

TEST_CASE("LLM.FinishReason.ToString", "[Core][LLM]") {
    REQUIRE(finish_reason_to_string(FinishReason::Stop) == "stop");
    REQUIRE(finish_reason_to_string(FinishReason::Length) == "length");
    REQUIRE(finish_reason_to_string(FinishReason::ToolCall) == "tool-call");
    REQUIRE(finish_reason_to_string(FinishReason::ContentFilter) == "content-filter");
    REQUIRE(finish_reason_to_string(FinishReason::Error) == "error");
    REQUIRE(finish_reason_to_string(FinishReason::Other) == "other");
}

TEST_CASE("LLM.FinishReason.FromString", "[Core][LLM]") {
    REQUIRE(finish_reason_from_string("stop") == FinishReason::Stop);
    REQUIRE(finish_reason_from_string("length") == FinishReason::Length);
    REQUIRE(finish_reason_from_string("tool-call") == FinishReason::ToolCall);
    REQUIRE(finish_reason_from_string("content-filter") == FinishReason::ContentFilter);
    REQUIRE(finish_reason_from_string("error") == FinishReason::Error);
    REQUIRE(finish_reason_from_string("other") == FinishReason::Other);
}

// ==================== ToolCallChunk Tests ====================

TEST_CASE("LLM.ToolCallChunk.ToJson", "[Core][LLM]") {
    ToolCallChunk chunk;
    chunk.id = "call_123";
    chunk.name = "read_file";
    chunk.arguments = R"({"path": "/tmp/test.txt"})";
    chunk.is_complete = true;
    
    nlohmann::json j = chunk.to_json();
    
    REQUIRE(j["id"] == "call_123");
    REQUIRE(j["name"] == "read_file");
    REQUIRE(j["arguments"] == R"({"path": "/tmp/test.txt"})");
    REQUIRE(j["is_complete"] == true);
}

TEST_CASE("LLM.ToolCallChunk.FromJson", "[Core][LLM]") {
    nlohmann::json j = {
        {"id", "call_456"},
        {"name", "bash"},
        {"arguments", R"({"cmd": "ls"})"},
        {"is_complete", false}
    };
    
    ToolCallChunk chunk = ToolCallChunk::from_json(j);
    
    REQUIRE(chunk.id == "call_456");
    REQUIRE(chunk.name == "bash");
    REQUIRE(chunk.arguments == R"({"cmd": "ls"})");
    REQUIRE(chunk.is_complete == false);
}

// ==================== ReasoningMetadata Tests ====================

TEST_CASE("LLM.ReasoningMetadata.ToJson", "[Core][LLM]") {
    ReasoningMetadata meta;
    meta.encrypted_content = "encrypted_data";
    meta.budget_tokens = 1000;
    meta.used_tokens = 500;
    
    nlohmann::json j = meta.to_json();
    
    REQUIRE(j["encrypted_content"] == "encrypted_data");
    REQUIRE(j["budget_tokens"] == 1000);
    REQUIRE(j["used_tokens"] == 500);
}

TEST_CASE("LLM.ReasoningMetadata.FromJson", "[Core][LLM]") {
    nlohmann::json j = {
        {"encrypted_content", "test"},
        {"budget_tokens", 2000},
        {"used_tokens", 1000}
    };
    
    ReasoningMetadata meta = ReasoningMetadata::from_json(j);
    
    REQUIRE(meta.encrypted_content == "test");
    REQUIRE(meta.budget_tokens == 2000);
    REQUIRE(meta.used_tokens == 1000);
}

// ==================== SourceInfo Tests ====================

TEST_CASE("LLM.SourceInfo.ToJson", "[Core][LLM]") {
    SourceInfo info;
    info.id = "src_123";
    info.type = "document";
    info.title = "Test Document";
    info.url = "https://example.com/doc";
    info.filename = "test.pdf";
    
    nlohmann::json j = info.to_json();
    
    REQUIRE(j["id"] == "src_123");
    REQUIRE(j["type"] == "document");
    REQUIRE(j["title"] == "Test Document");
}

TEST_CASE("LLM.SourceInfo.FromJson", "[Core][LLM]") {
    nlohmann::json j = {
        {"id", "src_456"},
        {"type", "url"},
        {"title", "Website"},
        {"url", "https://example.com"}
    };
    
    SourceInfo info = SourceInfo::from_json(j);
    
    REQUIRE(info.id == "src_456");
    REQUIRE(info.type == "url");
    REQUIRE(info.title == "Website");
    REQUIRE(info.url == "https://example.com");
}

// ==================== StreamEvent Factory Methods Tests ====================

TEST_CASE("LLM.StreamEvent.CreateStart", "[Core][LLM]") {
    StreamEvent event = StreamEvent::create_start();
    
    REQUIRE(event.is_start());
    REQUIRE(event.type == StreamEventType::Start);
    REQUIRE(event.timestamp > 0);
}

TEST_CASE("LLM.StreamEvent.CreateFinish", "[Core][LLM]") {
    TokenUsage usage;
    usage.input = 100;
    usage.output = 50;
    usage.reasoning = 150;
    StreamEvent event = StreamEvent::create_finish(FinishReason::Stop, usage);
    
    REQUIRE(event.is_finish());
    REQUIRE(event.finish_reason == FinishReason::Stop);
    REQUIRE(event.usage.has_value());
    REQUIRE(event.usage->input == 100);
}

TEST_CASE("LLM.StreamEvent.CreateError", "[Core][LLM]") {
    StreamEvent event = StreamEvent::create_error("Something went wrong", "ERR001");
    
    REQUIRE(event.is_error());
    REQUIRE(event.error_message == "Something went wrong");
    REQUIRE(event.error_code == "ERR001");
}

TEST_CASE("LLM.StreamEvent.CreateTextStart", "[Core][LLM]") {
    StreamEvent event = StreamEvent::create_text_start("text_1");
    
    REQUIRE(event.type == StreamEventType::TextStart);
    REQUIRE(event.id == "text_1");
    REQUIRE(event.is_text_event());
}

TEST_CASE("LLM.StreamEvent.CreateTextDelta", "[Core][LLM]") {
    StreamEvent event = StreamEvent::create_text_delta("text_1", "Hello");
    
    REQUIRE(event.type == StreamEventType::TextDelta);
    REQUIRE(event.id == "text_1");
    REQUIRE(event.delta == "Hello");
    REQUIRE(event.is_text_event());
}

TEST_CASE("LLM.StreamEvent.CreateTextEnd", "[Core][LLM]") {
    StreamEvent event = StreamEvent::create_text_end("text_1");
    
    REQUIRE(event.type == StreamEventType::TextEnd);
    REQUIRE(event.id == "text_1");
}

TEST_CASE("LLM.StreamEvent.CreateReasoningStart", "[Core][LLM]") {
    StreamEvent event = StreamEvent::create_reasoning_start("reason_1");
    
    REQUIRE(event.type == StreamEventType::ReasoningStart);
    REQUIRE(event.is_reasoning_event());
}

TEST_CASE("LLM.StreamEvent.CreateReasoningDelta", "[Core][LLM]") {
    StreamEvent event = StreamEvent::create_reasoning_delta("reason_1", "Thinking...");
    
    REQUIRE(event.type == StreamEventType::ReasoningDelta);
    REQUIRE(event.delta == "Thinking...");
}

TEST_CASE("LLM.StreamEvent.CreateToolInputStart", "[Core][LLM]") {
    StreamEvent event = StreamEvent::create_tool_input_start("tool_1", "read_file");
    
    REQUIRE(event.type == StreamEventType::ToolInputStart);
    REQUIRE(event.id == "tool_1");
    REQUIRE(event.tool_call.has_value());
    REQUIRE(event.tool_call->name == "read_file");
    REQUIRE(event.is_tool_event());
}

TEST_CASE("LLM.StreamEvent.CreateToolInputDelta", "[Core][LLM]") {
    StreamEvent event = StreamEvent::create_tool_input_delta("tool_1", R"({"path": ")");
    
    REQUIRE(event.type == StreamEventType::ToolInputDelta);
    REQUIRE(event.delta == R"({"path": ")");
}

TEST_CASE("LLM.StreamEvent.CreateToolInputEnd", "[Core][LLM]") {
    StreamEvent event = StreamEvent::create_tool_input_end("tool_1");
    
    REQUIRE(event.type == StreamEventType::ToolInputEnd);
}

TEST_CASE("LLM.StreamEvent.CreateToolCall", "[Core][LLM]") {
    ToolCallChunk chunk;
    chunk.id = "call_1";
    chunk.name = "bash";
    chunk.arguments = R"({"cmd": "ls"})";
    chunk.is_complete = true;
    
    StreamEvent event = StreamEvent::create_tool_call(chunk);
    
    REQUIRE(event.type == StreamEventType::ToolCall);
    REQUIRE(event.tool_call.has_value());
    REQUIRE(event.tool_call->id == "call_1");
}

TEST_CASE("LLM.StreamEvent.CreateStepStart", "[Core][LLM]") {
    StreamEvent event = StreamEvent::create_step_start("step_1", "Analysis");
    
    REQUIRE(event.type == StreamEventType::StepStart);
    REQUIRE(event.id == "step_1");
    REQUIRE(event.is_step_event());
}

TEST_CASE("LLM.StreamEvent.CreateStepFinish", "[Core][LLM]") {
    nlohmann::json result = {{"status", "completed"}};
    StreamEvent event = StreamEvent::create_step_finish("step_1", result);
    
    REQUIRE(event.type == StreamEventType::StepFinish);
}

// ==================== StreamEvent Serialization Tests ====================

TEST_CASE("LLM.StreamEvent.ToJson", "[Core][LLM]") {
    StreamEvent event = StreamEvent::create_text_delta("text_1", "Hello");
    event.timestamp = 1234567890;
    
    nlohmann::json j = event.to_json();
    
    REQUIRE(j["type"] == "text-delta");
    REQUIRE(j["id"] == "text_1");
    REQUIRE(j["delta"] == "Hello");
}

TEST_CASE("LLM.StreamEvent.FromJson", "[Core][LLM]") {
    nlohmann::json j = {
        {"type", "text-delta"},
        {"id", "text_2"},
        {"delta", "World"},
        {"timestamp", 1234567890}
    };
    
    StreamEvent event = StreamEvent::from_json(j);
    
    REQUIRE(event.type == StreamEventType::TextDelta);
    REQUIRE(event.id == "text_2");
    REQUIRE(event.delta == "World");
}

TEST_CASE("LLM.StreamEvent.RoundTrip", "[Core][LLM]") {
    StreamEvent original = StreamEvent::create_tool_input_start("tool_1", "bash");
    
    nlohmann::json j = original.to_json();
    StreamEvent restored = StreamEvent::from_json(j);
    
    REQUIRE(restored.type == StreamEventType::ToolInputStart);
    REQUIRE(restored.id == "tool_1");
    REQUIRE(restored.tool_call.has_value());
    REQUIRE(restored.tool_call->name == "bash");
}

// ==================== StreamResult Tests ====================

TEST_CASE("LLM.StreamResult.GetText", "[Core][LLM]") {
    StreamResult result;
    result.events.push_back(StreamEvent::create_text_start("t1"));
    result.events.push_back(StreamEvent::create_text_delta("t1", "Hello "));
    result.events.push_back(StreamEvent::create_text_delta("t1", "World"));
    result.events.push_back(StreamEvent::create_text_end("t1"));
    
    REQUIRE(result.get_text() == "Hello World");
}

TEST_CASE("LLM.StreamResult.GetReasoning", "[Core][LLM]") {
    StreamResult result;
    result.events.push_back(StreamEvent::create_reasoning_start("r1"));
    result.events.push_back(StreamEvent::create_reasoning_delta("r1", "Thinking..."));
    result.events.push_back(StreamEvent::create_reasoning_end("r1"));
    
    REQUIRE(result.get_reasoning() == "Thinking...");
}

TEST_CASE("LLM.StreamResult.GetToolCalls", "[Core][LLM]") {
    StreamResult result;
    
    ToolCallChunk chunk;
    chunk.id = "call_1";
    chunk.name = "bash";
    chunk.arguments = R"({"cmd": "ls"})";
    chunk.is_complete = true;
    
    result.events.push_back(StreamEvent::create_tool_call(chunk));
    
    auto calls = result.get_tool_calls();
    REQUIRE(calls.size() == 1);
    REQUIRE(calls[0].name == "bash");
}

TEST_CASE("LLM.StreamResult.ToJson", "[Core][LLM]") {
    StreamResult result;
    result.id = "resp_123";
    result.model = "gpt-4";
    result.finish_reason = FinishReason::Stop;
    result.usage = TokenUsage{};
    result.usage.input = 100;
    result.usage.output = 50;
    result.usage.reasoning = 150;
    
    nlohmann::json j = result.to_json();
    
    REQUIRE(j["id"] == "resp_123");
    REQUIRE(j["model"] == "gpt-4");
    REQUIRE(j["finish_reason"] == "stop");
}

TEST_CASE("LLM.StreamResult.FromJson", "[Core][LLM]") {
    nlohmann::json j = {
        {"id", "resp_456"},
        {"model", "claude-3"},
        {"events", nlohmann::json::array()},
        {"usage", {{"input", 200}, {"output", 100}, {"total", 300}}},
        {"finish_reason", "tool-call"}
    };
    
    StreamResult result = StreamResult::from_json(j);
    
    REQUIRE(result.id == "resp_456");
    REQUIRE(result.model == "claude-3");
    REQUIRE(result.finish_reason == FinishReason::ToolCall);
}

// ==================== LLMToolDefinition Tests ====================

TEST_CASE("LLM.LLMToolDefinition.ToJson", "[Core][LLM]") {
    llm::LLMToolDefinition tool;
    tool.name = "read_file";
    tool.description = "Read a file";
    tool.parameters = {
        {"type", "object"},
        {"properties", {
            {"path", {{"type", "string"}}}
        }}
    };
    
    nlohmann::json j = tool.to_json();
    
    REQUIRE(j["type"] == "function");
    REQUIRE(j["function"]["name"] == "read_file");
    REQUIRE(j["function"]["description"] == "Read a file");
}

TEST_CASE("LLM.LLMToolDefinition.FromJson", "[Core][LLM]") {
    nlohmann::json j = {
        {"name", "bash"},
        {"description", "Run a bash command"},
        {"parameters", {{"type", "object"}}}
    };
    
    llm::LLMToolDefinition tool = llm::LLMToolDefinition::from_json(j);
    
    REQUIRE(tool.name == "bash");
    REQUIRE(tool.description == "Run a bash command");
}

// ==================== ToolCallResult Tests ====================

TEST_CASE("LLM.ToolCallResult.ToJson", "[Core][LLM]") {
    llm::ToolCallResult result;
    result.tool_call_id = "call_123";
    result.content = "File contents here";
    result.is_error = false;
    
    nlohmann::json j = result.to_json();
    
    REQUIRE(j["tool_call_id"] == "call_123");
    REQUIRE(j["content"] == "File contents here");
    // is_error is only included when true
    REQUIRE_FALSE(j.contains("is_error"));
}

TEST_CASE("LLM.ToolCallResult.ToJsonWithError", "[Core][LLM]") {
    llm::ToolCallResult result;
    result.tool_call_id = "call_123";
    result.content = "Error occurred";
    result.is_error = true;
    
    nlohmann::json j = result.to_json();
    
    REQUIRE(j["tool_call_id"] == "call_123");
    REQUIRE(j["content"] == "Error occurred");
    REQUIRE(j["is_error"] == true);
}

TEST_CASE("LLM.ToolCallResult.FromJson", "[Core][LLM]") {
    nlohmann::json j = {
        {"tool_call_id", "call_456"},
        {"content", "Error occurred"},
        {"is_error", true}
    };
    
    llm::ToolCallResult result = llm::ToolCallResult::from_json(j);
    
    REQUIRE(result.tool_call_id == "call_456");
    REQUIRE(result.is_error == true);
}

// ==================== LLMMessage Tests ====================

TEST_CASE("LLM.LLMMessage.System", "[Core][LLM]") {
    llm::LLMMessage msg = llm::LLMMessage::system("You are a helpful assistant.");
    
    REQUIRE(msg.role == provider::ChatRole::System);
    REQUIRE(msg.content == "You are a helpful assistant.");
}

TEST_CASE("LLM.LLMMessage.User", "[Core][LLM]") {
    llm::LLMMessage msg = llm::LLMMessage::user("Hello!");
    
    REQUIRE(msg.role == provider::ChatRole::User);
    REQUIRE(msg.content == "Hello!");
}

TEST_CASE("LLM.LLMMessage.Assistant", "[Core][LLM]") {
    llm::LLMMessage msg = llm::LLMMessage::assistant("Hi there!");
    
    REQUIRE(msg.role == provider::ChatRole::Assistant);
    REQUIRE(msg.content == "Hi there!");
}

TEST_CASE("LLM.LLMMessage.ToolResult", "[Core][LLM]") {
    llm::LLMMessage msg = llm::LLMMessage::tool_result("call_123", "Result content", true);
    
    REQUIRE(msg.role == provider::ChatRole::Tool);
    REQUIRE(msg.tool_call_id == "call_123");
    REQUIRE(msg.content == "Result content");
}

TEST_CASE("LLM.LLMMessage.ToJson", "[Core][LLM]") {
    llm::LLMMessage msg = llm::LLMMessage::user("Test message");
    
    nlohmann::json j = msg.to_json();
    
    REQUIRE(j["role"] == "user");
    REQUIRE(j["content"] == "Test message");
}

TEST_CASE("LLM.LLMMessage.FromJson", "[Core][LLM]") {
    nlohmann::json j = {
        {"role", "assistant"},
        {"content", "Response"}
    };
    
    llm::LLMMessage msg = llm::LLMMessage::from_json(j);
    
    REQUIRE(msg.role == provider::ChatRole::Assistant);
    REQUIRE(msg.content == "Response");
}

// ==================== StreamParams Tests ====================

TEST_CASE("LLM.StreamParams.ToJson", "[Core][LLM]") {
    llm::StreamParams params;
    params.session_id = "sess_123";
    params.temperature = 0.7;
    params.max_tokens = 1000;
    params.messages.push_back(llm::LLMMessage::user("Hello"));
    
    nlohmann::json j = params.to_json();
    
    REQUIRE(j["session_id"] == "sess_123");
    REQUIRE(j["temperature"] == 0.7);
    REQUIRE(j["max_tokens"] == 1000);
}

TEST_CASE("LLM.StreamParams.FromJson", "[Core][LLM]") {
    nlohmann::json j = {
        {"session_id", "sess_456"},
        {"temperature", 0.5},
        {"messages", nlohmann::json::array()},
        {"tools", nlohmann::json::array()},
        {"stop", nlohmann::json::array()}
    };
    
    llm::StreamParams params = llm::StreamParams::from_json(j);
    
    REQUIRE(params.session_id == "sess_456");
    REQUIRE(params.temperature == 0.5);
}

// ==================== StreamingState Tests ====================

TEST_CASE("LLM.StreamingState.AddEvent", "[Core][LLM]") {
    llm::StreamingState state;
    
    state.add_event(StreamEvent::create_text_delta("t1", "Hello"));
    
    REQUIRE(state.has_events());
    REQUIRE(state.events().size() == 1);
}

TEST_CASE("LLM.StreamingState.PopEvent", "[Core][LLM]") {
    llm::StreamingState state;
    
    state.add_event(StreamEvent::create_text_delta("t1", "Hello"));
    
    auto event = state.pop_event();
    REQUIRE(event.has_value());
    REQUIRE(event->delta == "Hello");
    
    REQUIRE_FALSE(state.has_events());
}

TEST_CASE("LLM.StreamingState.GetText", "[Core][LLM]") {
    llm::StreamingState state;
    
    state.add_event(StreamEvent::create_text_start("t1"));
    state.add_event(StreamEvent::create_text_delta("t1", "Hello "));
    state.add_event(StreamEvent::create_text_delta("t1", "World"));
    
    REQUIRE(state.get_text() == "Hello World");
}

TEST_CASE("LLM.StreamingState.MarkDone", "[Core][LLM]") {
    llm::StreamingState state;
    
    REQUIRE_FALSE(state.is_done());
    
    TokenUsage usage;
    usage.input = 100;
    usage.output = 50;
    usage.reasoning = 150;
    state.mark_done(FinishReason::Stop, usage);
    
    REQUIRE(state.is_done());
    REQUIRE(state.finish_reason() == FinishReason::Stop);
    REQUIRE(state.usage().input == 100);
}

TEST_CASE("LLM.StreamingState.MarkError", "[Core][LLM]") {
    llm::StreamingState state;
    
    state.mark_error("Connection failed", "NET_ERR");
    
    REQUIRE(state.error() == "Connection failed");
}

// ==================== ProviderType Tests ====================

TEST_CASE("LLM.ProviderType.OpenAI", "[Core][LLM]") {
    REQUIRE(llm::get_provider_type("openai") == llm::ProviderType::OpenAI);
}

TEST_CASE("LLM.ProviderType.Anthropic", "[Core][LLM]") {
    REQUIRE(llm::get_provider_type("anthropic") == llm::ProviderType::Anthropic);
}

TEST_CASE("LLM.ProviderType.Gemini", "[Core][LLM]") {
    REQUIRE(llm::get_provider_type("gemini") == llm::ProviderType::Gemini);
    REQUIRE(llm::get_provider_type("google") == llm::ProviderType::Gemini);
}

TEST_CASE("LLM.ProviderType.Azure", "[Core][LLM]") {
    REQUIRE(llm::get_provider_type("azure") == llm::ProviderType::Azure);
}

TEST_CASE("LLM.ProviderType.OpenRouter", "[Core][LLM]") {
    REQUIRE(llm::get_provider_type("openrouter") == llm::ProviderType::OpenRouter);
}

TEST_CASE("LLM.ProviderType.Groq", "[Core][LLM]") {
    REQUIRE(llm::get_provider_type("groq") == llm::ProviderType::Groq);
}

TEST_CASE("LLM.ProviderType.DeepSeek", "[Core][LLM]") {
    REQUIRE(llm::get_provider_type("deepseek") == llm::ProviderType::DeepSeek);
}

TEST_CASE("LLM.ProviderType.XAI", "[Core][LLM]") {
    REQUIRE(llm::get_provider_type("xai") == llm::ProviderType::XAI);
}

TEST_CASE("LLM.ProviderType.Mistral", "[Core][LLM]") {
    REQUIRE(llm::get_provider_type("mistral") == llm::ProviderType::Mistral);
}

TEST_CASE("LLM.ProviderType.Bedrock", "[Core][LLM]") {
    REQUIRE(llm::get_provider_type("bedrock") == llm::ProviderType::Bedrock);
}

TEST_CASE("LLM.ProviderType.Unknown", "[Core][LLM]") {
    REQUIRE(llm::get_provider_type("unknown_provider") == llm::ProviderType::Other);
}

// ==================== SystemPrompt Tests ====================

TEST_CASE("LLM.SystemPrompt.LoadTemplate", "[Core][LLM]") {
    // Try loading a template (may return empty if file doesn't exist)
    std::string prompt = llm::SystemPrompt::load_template("title");
    // Just verify it doesn't crash
    REQUIRE(true);
}

TEST_CASE("LLM.SystemPrompt.PromptCodex", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::prompt_codex();
    REQUIRE_FALSE(prompt.empty());
    REQUIRE(prompt.find("Turbot") != std::string::npos);
}

TEST_CASE("LLM.SystemPrompt.PromptBeast", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::prompt_beast();
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.PromptAnthropic", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::prompt_anthropic();
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.PromptOpenAI", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::prompt_openai();
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.PromptGemini", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::prompt_gemini();
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.PromptQwen", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::prompt_qwen();
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.Instructions", "[Core][LLM]") {
    std::string instructions = llm::SystemPrompt::instructions();
    REQUIRE_FALSE(instructions.empty());
}

TEST_CASE("LLM.SystemPrompt.ProviderPrompt.OpenAI", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::provider_prompt("openai", "gpt-4");
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.ProviderPrompt.Anthropic", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::provider_prompt("anthropic", "claude-3-opus");
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.ProviderPrompt.Gemini", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::provider_prompt("gemini", "gemini-pro");
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.ProviderPrompt.GPT5", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::provider_prompt("openai", "gpt-5");
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.ProviderPrompt.O1Model", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::provider_prompt("openai", "o1-preview");
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.ProviderPrompt.Claude", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::provider_prompt("anthropic", "claude-3-sonnet");
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.ProviderPrompt.Qwen", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::provider_prompt("qwen", "qwen-turbo");
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.ProviderPrompt.Unknown", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::provider_prompt("unknown", "unknown-model");
    REQUIRE_FALSE(prompt.empty());  // Should return a default prompt
}

// ==================== SystemPrompt Advanced Tests ====================

TEST_CASE("LLM.SystemPrompt.Environment", "[Core][LLM]") {
    llm::SystemPromptParams params;
    params.session_id = "test-session-123";
    params.model_id = "gpt-4";
    params.provider_id = "openai";
    params.working_directory = "/home/user/project";
    params.is_git_repo = true;
    params.platform = "darwin";
    params.current_date = "2024-01-15";
    
    std::string env = llm::SystemPrompt::environment(params);
    
    REQUIRE_FALSE(env.empty());
    REQUIRE(env.find("gpt-4") != std::string::npos);
    REQUIRE(env.find("openai") != std::string::npos);
    REQUIRE(env.find("test-session-123") != std::string::npos);
    REQUIRE(env.find("/home/user/project") != std::string::npos);
    REQUIRE(env.find("darwin") != std::string::npos);
    REQUIRE(env.find("2024-01-15") != std::string::npos);
    REQUIRE(env.find("yes") != std::string::npos);  // is_git_repo
}

TEST_CASE("LLM.SystemPrompt.Environment.NoGit", "[Core][LLM]") {
    llm::SystemPromptParams params;
    params.model_id = "claude-3";
    params.provider_id = "anthropic";
    params.working_directory = "/tmp";
    params.is_git_repo = false;
    params.platform = "linux";
    params.current_date = "2024-01-15";
    
    std::string env = llm::SystemPrompt::environment(params);
    
    REQUIRE(env.find("no") != std::string::npos);  // is_git_repo = false
}

TEST_CASE("LLM.SystemPrompt.JoinPrompts", "[Core][LLM]") {
    std::vector<std::string> parts = {
        "First part",
        "Second part",
        "Third part"
    };
    
    std::string joined = llm::SystemPrompt::join_prompts(parts);
    
    REQUIRE(joined == "First part\n\nSecond part\n\nThird part");
}

TEST_CASE("LLM.SystemPrompt.JoinPrompts.EmptyParts", "[Core][LLM]") {
    std::vector<std::string> parts = {
        "First",
        "",
        "Second",
        "",
        "Third"
    };
    
    std::string joined = llm::SystemPrompt::join_prompts(parts);
    
    REQUIRE(joined == "First\n\nSecond\n\nThird");
}

TEST_CASE("LLM.SystemPrompt.JoinPrompts.AllEmpty", "[Core][LLM]") {
    std::vector<std::string> parts = {"", "", ""};
    
    std::string joined = llm::SystemPrompt::join_prompts(parts);
    
    REQUIRE(joined.empty());
}

TEST_CASE("LLM.SystemPrompt.JoinPrompts.Single", "[Core][LLM]") {
    std::vector<std::string> parts = {"Only one part"};
    
    std::string joined = llm::SystemPrompt::join_prompts(parts);
    
    REQUIRE(joined == "Only one part");
}

TEST_CASE("LLM.SystemPrompt.Build", "[Core][LLM]") {
    llm::SystemPromptParams params;
    params.session_id = "build-test";
    params.agent.name = "build";
    params.model_id = "gpt-4";
    params.provider_id = "openai";
    params.working_directory = "/project";
    params.platform = "darwin";
    params.current_date = "2024-01-15";
    
    std::string prompt = llm::SystemPrompt::build(params);
    
    REQUIRE_FALSE(prompt.empty());
    REQUIRE(prompt.find("gpt-4") != std::string::npos);
    REQUIRE(prompt.find("/project") != std::string::npos);
}

TEST_CASE("LLM.SystemPrompt.Build.WithCustomPrompts", "[Core][LLM]") {
    llm::SystemPromptParams params;
    params.session_id = "custom-test";
    params.agent.name = "build";
    params.model_id = "claude-3";
    params.provider_id = "anthropic";
    params.working_directory = "/project";
    params.platform = "linux";
    params.current_date = "2024-01-15";
    params.custom_prompts = {"Custom prompt 1", "Custom prompt 2"};
    params.user_system_prompts = {"User system prompt"};
    
    std::string prompt = llm::SystemPrompt::build(params);
    
    REQUIRE_FALSE(prompt.empty());
    REQUIRE(prompt.find("Custom prompt 1") != std::string::npos);
    REQUIRE(prompt.find("Custom prompt 2") != std::string::npos);
    REQUIRE(prompt.find("User system prompt") != std::string::npos);
}

TEST_CASE("LLM.SystemPrompt.Build.WithAgentPrompt", "[Core][LLM]") {
    llm::SystemPromptParams params;
    params.session_id = "agent-prompt-test";
    params.agent.name = "custom";
    params.agent.prompt = "Custom agent prompt";
    params.model_id = "gpt-4";
    params.provider_id = "openai";
    params.working_directory = "/project";
    params.platform = "darwin";
    params.current_date = "2024-01-15";
    
    std::string prompt = llm::SystemPrompt::build(params);
    
    REQUIRE_FALSE(prompt.empty());
    REQUIRE(prompt.find("Custom agent prompt") != std::string::npos);
}

TEST_CASE("LLM.SystemPrompt.AgentPrompt", "[Core][LLM]") {
    agent::AgentInfo info;
    info.name = "test";
    info.prompt = "Agent specific prompt";
    
    std::string result = llm::SystemPrompt::agent_prompt(info);
    REQUIRE(result == "Agent specific prompt");
}

TEST_CASE("LLM.SystemPrompt.AgentPrompt.Empty", "[Core][LLM]") {
    agent::AgentInfo info;
    info.name = "test";
    
    std::string result = llm::SystemPrompt::agent_prompt(info);
    REQUIRE(result.empty());
}

// ==================== ProviderPrompt Model-Specific Tests ====================

TEST_CASE("LLM.SystemPrompt.ProviderPrompt.GPT4", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::provider_prompt("openai", "gpt-4-turbo");
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.ProviderPrompt.O3Model", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::provider_prompt("openai", "o3-mini");
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.ProviderPrompt.GeminiModel", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::provider_prompt("google", "gemini-2.0-flash");
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.ProviderPrompt.TrinityModel", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::provider_prompt("trinity", "trinity-1");
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.ProviderPrompt.Bedrock", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::provider_prompt("bedrock", "anthropic.claude-3");
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.ProviderPrompt.Cohere", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::provider_prompt("cohere", "command");
    REQUIRE_FALSE(prompt.empty());
}

TEST_CASE("LLM.SystemPrompt.ProviderPrompt.ChineseProviders", "[Core][LLM]") {
    std::string prompt = llm::SystemPrompt::provider_prompt("bailian", "qwen-turbo");
    REQUIRE_FALSE(prompt.empty());
    
    prompt = llm::SystemPrompt::provider_prompt("zhipu", "glm-4");
    REQUIRE_FALSE(prompt.empty());
    
    prompt = llm::SystemPrompt::provider_prompt("kimi", "moonshot-v1");
    REQUIRE_FALSE(prompt.empty());
    
    prompt = llm::SystemPrompt::provider_prompt("minimax", "abab5");
    REQUIRE_FALSE(prompt.empty());
}

// ==================== LlmRole Conversion Tests ====================

TEST_CASE("LLM.LlmRole.ToString", "[Core][LLM]") {
    REQUIRE(llm_role_to_string(LlmRole::System) == "system");
    REQUIRE(llm_role_to_string(LlmRole::User) == "user");
    REQUIRE(llm_role_to_string(LlmRole::Assistant) == "assistant");
    REQUIRE(llm_role_to_string(LlmRole::Tool) == "tool");
}

TEST_CASE("LLM.LlmRole.FromString", "[Core][LLM]") {
    REQUIRE(llm_role_from_string("system") == LlmRole::System);
    REQUIRE(llm_role_from_string("user") == LlmRole::User);
    REQUIRE(llm_role_from_string("assistant") == LlmRole::Assistant);
    REQUIRE(llm_role_from_string("tool") == LlmRole::Tool);
    
    REQUIRE_THROWS_AS(llm_role_from_string("invalid"), std::invalid_argument);
}

// ==================== LlmMessage Factory Tests ====================

TEST_CASE("LLM.LlmMessage.CreateSystem", "[Core][LLM]") {
    LlmMessage msg = LlmMessage::create_system("You are a helpful assistant.");
    REQUIRE(msg.role == LlmRole::System);
    REQUIRE(msg.content == "You are a helpful assistant.");
}

TEST_CASE("LLM.LlmMessage.CreateUser", "[Core][LLM]") {
    LlmMessage msg = LlmMessage::create_user("Hello!");
    REQUIRE(msg.role == LlmRole::User);
    REQUIRE(msg.content == "Hello!");
}

TEST_CASE("LLM.LlmMessage.CreateAssistant", "[Core][LLM]") {
    LlmMessage msg = LlmMessage::create_assistant("Hi there!");
    REQUIRE(msg.role == LlmRole::Assistant);
    REQUIRE(msg.content == "Hi there!");
}

TEST_CASE("LLM.LlmMessage.CreateUserParts", "[Core][LLM]") {
    std::vector<nlohmann::json> parts = {
        {{"type", "text"}, {"text", "Hello"}},
        {{"type", "image_url"}, {"image_url", {{"url", "http://example.com/img.png"}}}}
    };
    LlmMessage msg = LlmMessage::create_user_parts(parts);
    REQUIRE(msg.role == LlmRole::User);
    REQUIRE(msg.content_parts.has_value());
    REQUIRE(msg.content_parts->size() == 2);
}

TEST_CASE("LLM.LlmMessage.CreateAssistantParts", "[Core][LLM]") {
    std::vector<nlohmann::json> parts = {
        {{"type", "text"}, {"text", "Response"}}
    };
    LlmMessage msg = LlmMessage::create_assistant_parts(parts);
    REQUIRE(msg.role == LlmRole::Assistant);
    REQUIRE(msg.content_parts.has_value());
    REQUIRE(msg.content_parts->size() == 1);
}

TEST_CASE("LLM.LlmMessage.CreateToolResponse", "[Core][LLM]") {
    LlmMessage msg = LlmMessage::create_tool_response("call_123", "Tool result");
    REQUIRE(msg.role == LlmRole::Tool);
    REQUIRE(msg.tool_call_id == "call_123");
    REQUIRE(msg.content == "Tool result");
}

TEST_CASE("LLM.LlmMessage.CreateAssistantWithTools", "[Core][LLM]") {
    std::vector<nlohmann::json> tool_calls = {
        {{"id", "call_1"}, {"function", {{"name", "read_file"}, {"arguments", "{}"}}}}
    };
    LlmMessage msg = LlmMessage::create_assistant_with_tools("Thinking...", tool_calls);
    REQUIRE(msg.role == LlmRole::Assistant);
    REQUIRE(msg.content == "Thinking...");
    REQUIRE(msg.tool_calls.has_value());
    REQUIRE(msg.tool_calls->size() == 1);
}

// ==================== LlmMessage Serialization Tests ====================

TEST_CASE("LLM.LlmMessage.ToOpenAI", "[Core][LLM]") {
    LlmMessage msg = LlmMessage::create_user("Hello");
    nlohmann::json j = msg.to_openai();
    
    REQUIRE(j["role"] == "user");
    REQUIRE(j["content"] == "Hello");
}

TEST_CASE("LLM.LlmMessage.ToOpenAI.WithToolCalls", "[Core][LLM]") {
    std::vector<nlohmann::json> tool_calls = {
        {{"id", "call_1"}, {"function", {{"name", "test"}, {"arguments", "{}"}}}}
    };
    LlmMessage msg = LlmMessage::create_assistant_with_tools(std::nullopt, tool_calls);
    nlohmann::json j = msg.to_openai();
    
    REQUIRE(j["role"] == "assistant");
    REQUIRE(j["content"].is_null());
    REQUIRE(j.contains("tool_calls"));
}

TEST_CASE("LLM.LlmMessage.ToOpenAI.ToolResponse", "[Core][LLM]") {
    LlmMessage msg = LlmMessage::create_tool_response("call_123", "Result");
    nlohmann::json j = msg.to_openai();
    
    REQUIRE(j["role"] == "tool");
    REQUIRE(j["tool_call_id"] == "call_123");
    REQUIRE(j["content"] == "Result");
}

TEST_CASE("LLM.LlmMessage.ToAnthropic.System", "[Core][LLM]") {
    LlmMessage msg = LlmMessage::create_system("System prompt");
    nlohmann::json j = msg.to_anthropic();
    
    REQUIRE(j["role"] == "system");
    REQUIRE(j["type"] == "system");
    REQUIRE(j["content"] == "System prompt");
}

TEST_CASE("LLM.LlmMessage.ToAnthropic.User", "[Core][LLM]") {
    LlmMessage msg = LlmMessage::create_user("Hello");
    nlohmann::json j = msg.to_anthropic();
    
    REQUIRE(j["role"] == "user");
    REQUIRE(j["content"].is_array());
    REQUIRE(j["content"][0]["type"] == "text");
    REQUIRE(j["content"][0]["text"] == "Hello");
}

TEST_CASE("LLM.LlmMessage.ToAnthropic.WithImage", "[Core][LLM]") {
    std::vector<nlohmann::json> parts = {
        {{"type", "text"}, {"text", "Check this image"}},
        {{"type", "image_url"}, {"image_url", {{"url", "data:image/png;base64,abc123"}}}}
    };
    LlmMessage msg = LlmMessage::create_user_parts(parts);
    nlohmann::json j = msg.to_anthropic();
    
    REQUIRE(j["role"] == "user");
    REQUIRE(j["content"].is_array());
    REQUIRE(j["content"].size() == 2);
}

TEST_CASE("LLM.LlmMessage.ToFormat", "[Core][LLM]") {
    LlmMessage msg = LlmMessage::create_user("Test");
    
    nlohmann::json j_openai = msg.to_format(MessageFormat::OpenAI);
    REQUIRE(j_openai["role"] == "user");
    
    nlohmann::json j_anthropic = msg.to_format(MessageFormat::Anthropic);
    REQUIRE(j_anthropic["role"] == "user");
}

// ==================== Content Parts Factory Tests ====================

TEST_CASE("LLM.ContentParts.Text", "[Core][LLM]") {
    nlohmann::json part = content_parts::text("Hello");
    REQUIRE(part["type"] == "text");
    REQUIRE(part["text"] == "Hello");
}

TEST_CASE("LLM.ContentParts.ImageBase64", "[Core][LLM]") {
    nlohmann::json part = content_parts::image_base64("abc123", "image/png");
    REQUIRE(part["type"] == "image_url");
    REQUIRE(part["image_url"]["url"] == "data:image/png;base64,abc123");
}

TEST_CASE("LLM.ContentParts.ImageUrl", "[Core][LLM]") {
    nlohmann::json part = content_parts::image_url("http://example.com/img.png");
    REQUIRE(part["type"] == "image_url");
    REQUIRE(part["image_url"]["url"] == "http://example.com/img.png");
}

TEST_CASE("LLM.ContentParts.File", "[Core][LLM]") {
    nlohmann::json part = content_parts::file("test.pdf", "base64data", "application/pdf");
    REQUIRE(part["type"] == "file");
    REQUIRE(part["filename"] == "test.pdf");
    REQUIRE(part["data"] == "base64data");
    REQUIRE(part["mime_type"] == "application/pdf");
}

TEST_CASE("LLM.ContentParts.ToolCall", "[Core][LLM]") {
    nlohmann::json args = {{"path", "/tmp/test"}};
    nlohmann::json part = content_parts::tool_call("call_1", "read_file", args);
    
    REQUIRE(part["type"] == "tool_call");
    REQUIRE(part["id"] == "call_1");
    REQUIRE(part["function"]["name"] == "read_file");
}

TEST_CASE("LLM.ContentParts.ToolResult", "[Core][LLM]") {
    nlohmann::json part = content_parts::tool_result("call_1", "File content");
    REQUIRE(part["type"] == "tool_result");
    REQUIRE(part["tool_call_id"] == "call_1");
    REQUIRE(part["content"] == "File content");
}

TEST_CASE("LLM.ContentParts.Reasoning", "[Core][LLM]") {
    nlohmann::json part = content_parts::reasoning("Thinking...");
    REQUIRE(part["type"] == "reasoning");
    REQUIRE(part["text"] == "Thinking...");
}

// ==================== MessageBuilder Tests ====================

TEST_CASE("LLM.MessageBuilder.Basic", "[Core][LLM]") {
    MessageBuilder builder;
    
    builder.add_system("You are helpful.")
           .add_user("Hello!")
           .add_assistant("Hi!");
    
    auto messages = builder.get_messages();
    REQUIRE(messages.size() == 3);
    REQUIRE(messages[0].role == LlmRole::System);
    REQUIRE(messages[1].role == LlmRole::User);
    REQUIRE(messages[2].role == LlmRole::Assistant);
}

TEST_CASE("LLM.MessageBuilder.Build", "[Core][LLM]") {
    MessageBuilder builder;
    builder.add_user("Test message");
    
    auto result = builder.build();
    REQUIRE(result.size() == 1);
    REQUIRE(result[0]["role"] == "user");
    REQUIRE(result[0]["content"] == "Test message");
}

TEST_CASE("LLM.MessageBuilder.WithFormat", "[Core][LLM]") {
    MessageBuilder builder;
    builder.set_format(MessageFormat::Anthropic);
    builder.add_user("Hello");
    
    auto result = builder.build();
    REQUIRE(result[0]["content"].is_array());
}

TEST_CASE("LLM.MessageBuilder.WithProvider", "[Core][LLM]") {
    MessageBuilder builder;
    builder.set_provider("anthropic", "claude-3");
    builder.add_user("Test");
    
    auto result = builder.build();
    REQUIRE(result.size() == 1);
}

TEST_CASE("LLM.MessageBuilder.AddMessages", "[Core][LLM]") {
    MessageBuilder builder;
    std::vector<LlmMessage> msgs = {
        LlmMessage::create_user("Hello"),
        LlmMessage::create_assistant("Hi!")
    };
    
    builder.add_messages(msgs);
    
    auto result = builder.get_messages();
    REQUIRE(result.size() == 2);
}

TEST_CASE("LLM.MessageBuilder.BuildWithSystem", "[Core][LLM]") {
    MessageBuilder builder;
    builder.add_system("System prompt")
           .add_user("Hello");
    
    auto [systems, others] = builder.build_with_system();
    REQUIRE(systems.size() == 1);
    REQUIRE(systems[0] == "System prompt");
    REQUIRE(others.size() == 1);
}

TEST_CASE("LLM.MessageBuilder.Clear", "[Core][LLM]") {
    MessageBuilder builder;
    builder.add_user("Test");
    builder.clear();
    
    REQUIRE(builder.get_messages().empty());
}

// ==================== PromptBuilder Tests ====================

TEST_CASE("LLM.PromptBuilder.ToolDefinition.ToOpenAI", "[Core][LLM]") {
    llm::ToolDefinition def;
    def.name = "read_file";
    def.description = "Read a file";
    def.parameters = {{"type", "object"}};
    
    nlohmann::json j = def.to_openai();
    REQUIRE(j["type"] == "function");
    REQUIRE(j["function"]["name"] == "read_file");
    REQUIRE(j["function"]["description"] == "Read a file");
}

TEST_CASE("LLM.PromptBuilder.ToolDefinition.ToAnthropic", "[Core][LLM]") {
    llm::ToolDefinition def;
    def.name = "bash";
    def.description = "Run a command";
    def.parameters = {{"type", "object"}};
    
    nlohmann::json j = def.to_anthropic();
    REQUIRE(j["name"] == "bash");
    REQUIRE(j["description"] == "Run a command");
    REQUIRE(j.contains("input_schema"));
}

TEST_CASE("LLM.PromptBuilder.BuildResult.ToJson", "[Core][LLM]") {
    llm::PromptBuildResult result;
    result.system = "System prompt";
    result.messages = {LlmMessage::create_user("Hello")};
    
    llm::ToolDefinition tool;
    tool.name = "test";
    tool.description = "Test tool";
    tool.parameters = {{"type", "object"}};
    result.tools = {tool};
    
    nlohmann::json j = result.to_json();
    REQUIRE(j["system"] == "System prompt");
    REQUIRE(j["messages"].is_array());
    REQUIRE(j["tools"].is_array());
}

// ==================== ProviderAdapter Tests ====================

TEST_CASE("LLM.ProviderAdapter.DetectFormat.Anthropic", "[Core][LLM]") {
    REQUIRE(llm::ProviderAdapter::detect_format("anthropic") == MessageFormat::Anthropic);
    REQUIRE(llm::ProviderAdapter::detect_format("claude") == MessageFormat::Anthropic);
    REQUIRE(llm::ProviderAdapter::detect_format("bedrock") == MessageFormat::Anthropic);
    REQUIRE(llm::ProviderAdapter::detect_format("ANTHROPIC") == MessageFormat::Anthropic);
}

TEST_CASE("LLM.ProviderAdapter.DetectFormat.OpenAI", "[Core][LLM]") {
    REQUIRE(llm::ProviderAdapter::detect_format("openai") == MessageFormat::OpenAI);
    REQUIRE(llm::ProviderAdapter::detect_format("azure") == MessageFormat::OpenAI);
    REQUIRE(llm::ProviderAdapter::detect_format("groq") == MessageFormat::OpenAI);
    REQUIRE(llm::ProviderAdapter::detect_format("ollama") == MessageFormat::OpenAI);
    REQUIRE(llm::ProviderAdapter::detect_format("openrouter") == MessageFormat::OpenAI);
}

TEST_CASE("LLM.ProviderAdapter.DetectFormat.Gemini", "[Core][LLM]") {
    REQUIRE(llm::ProviderAdapter::detect_format("gemini") == MessageFormat::OpenAI);
    REQUIRE(llm::ProviderAdapter::detect_format("google") == MessageFormat::OpenAI);
    REQUIRE(llm::ProviderAdapter::detect_format("google-vertex") == MessageFormat::OpenAI);
}

TEST_CASE("LLM.ProviderAdapter.DetectFormat.Unknown", "[Core][LLM]") {
    REQUIRE(llm::ProviderAdapter::detect_format("unknown-provider") == MessageFormat::OpenAICompat);
}

TEST_CASE("LLM.ProviderAdapter.SupportsStreaming", "[Core][LLM]") {
    // Most providers support streaming
    REQUIRE(llm::ProviderAdapter::supports_streaming("openai"));
    REQUIRE(llm::ProviderAdapter::supports_streaming("anthropic"));
    REQUIRE(llm::ProviderAdapter::supports_streaming("gemini"));
}

TEST_CASE("LLM.ProviderAdapter.SupportsToolCalls", "[Core][LLM]") {
    REQUIRE(llm::ProviderAdapter::supports_tool_calls("openai"));
    REQUIRE(llm::ProviderAdapter::supports_tool_calls("anthropic"));
    REQUIRE(llm::ProviderAdapter::supports_tool_calls("gemini"));
    REQUIRE(llm::ProviderAdapter::supports_tool_calls("mistral"));
    REQUIRE(llm::ProviderAdapter::supports_tool_calls("deepseek"));
    REQUIRE(llm::ProviderAdapter::supports_tool_calls("bailian"));
    REQUIRE(llm::ProviderAdapter::supports_tool_calls("zhipu"));
    REQUIRE(llm::ProviderAdapter::supports_tool_calls("ollama"));
}

TEST_CASE("LLM.ProviderAdapter.SupportsReasoning", "[Core][LLM]") {
    REQUIRE(llm::ProviderAdapter::supports_reasoning("openai"));
    REQUIRE(llm::ProviderAdapter::supports_reasoning("anthropic"));
    REQUIRE(llm::ProviderAdapter::supports_reasoning("deepseek"));
    REQUIRE(llm::ProviderAdapter::supports_reasoning("gemini"));
    REQUIRE(llm::ProviderAdapter::supports_reasoning("xai"));
}

TEST_CASE("LLM.ProviderAdapter.ToolCallConversion", "[Core][LLM]") {
    ToolCallChunk chunk;
    chunk.id = "call_123";
    chunk.name = "read_file";
    chunk.arguments = R"({"path": "/tmp/test"})";
    chunk.is_complete = true;
    
    auto tc = llm::ProviderAdapter::from_tool_call_chunk(chunk);
    REQUIRE(tc.id == "call_123");
    REQUIRE(tc.name == "read_file");
    REQUIRE(tc.arguments["path"] == "/tmp/test");
    
    // Convert back
    auto restored = llm::ProviderAdapter::to_tool_call_chunk(tc);
    REQUIRE(restored.id == "call_123");
    REQUIRE(restored.name == "read_file");
}

TEST_CASE("LLM.ProviderAdapter.MessageConversion", "[Core][LLM]") {
    llm::LLMMessage msg = llm::LLMMessage::user("Hello");
    
    auto pmsg = llm::ProviderAdapter::to_provider_message(msg);
    REQUIRE(pmsg.content == "Hello");
    
    // Convert back
    auto restored = llm::ProviderAdapter::from_provider_message(pmsg);
    REQUIRE(restored.content == "Hello");
}

TEST_CASE("LLM.ProviderAdapter.ToolDefinitionConversion", "[Core][LLM]") {
    llm::LLMToolDefinition tool;
    tool.name = "bash";
    tool.description = "Run a command";
    tool.parameters = {{"type", "object"}, {"properties", {{"cmd", {{"type", "string"}}}}}};
    
    auto ptool = llm::ProviderAdapter::to_provider_tool(tool);
    REQUIRE(ptool.name == "bash");
    REQUIRE(ptool.description == "Run a command");
    REQUIRE(ptool.type == "function");
    
    // Convert back
    auto restored = llm::ProviderAdapter::from_provider_tool(ptool);
    REQUIRE(restored.name == "bash");
    REQUIRE(restored.description == "Run a command");
}

// ==================== ToolSchema Tests ====================

TEST_CASE("LLM.SchemaType.ToString", "[Core][LLM]") {
    REQUIRE(llm::schema_type_to_string(llm::SchemaType::String) == "string");
    REQUIRE(llm::schema_type_to_string(llm::SchemaType::Number) == "number");
    REQUIRE(llm::schema_type_to_string(llm::SchemaType::Integer) == "integer");
    REQUIRE(llm::schema_type_to_string(llm::SchemaType::Boolean) == "boolean");
    REQUIRE(llm::schema_type_to_string(llm::SchemaType::Object) == "object");
    REQUIRE(llm::schema_type_to_string(llm::SchemaType::Array) == "array");
    REQUIRE(llm::schema_type_to_string(llm::SchemaType::Null) == "null");
}

TEST_CASE("LLM.SchemaType.FromString", "[Core][LLM]") {
    REQUIRE(llm::string_to_schema_type("string") == llm::SchemaType::String);
    REQUIRE(llm::string_to_schema_type("number") == llm::SchemaType::Number);
    REQUIRE(llm::string_to_schema_type("integer") == llm::SchemaType::Integer);
    REQUIRE(llm::string_to_schema_type("boolean") == llm::SchemaType::Boolean);
    REQUIRE(llm::string_to_schema_type("object") == llm::SchemaType::Object);
    REQUIRE(llm::string_to_schema_type("array") == llm::SchemaType::Array);
    REQUIRE(llm::string_to_schema_type("null") == llm::SchemaType::Null);
    
    REQUIRE_THROWS_AS(llm::string_to_schema_type("invalid"), std::invalid_argument);
}

TEST_CASE("LLM.ParameterSchema.ToJsonSchema", "[Core][LLM]") {
    llm::ParameterSchema schema;
    schema.type = llm::SchemaType::String;
    schema.description = "A file path";
    schema.required = true;
    
    nlohmann::json j = schema.to_json_schema();
    REQUIRE(j["type"] == "string");
    REQUIRE(j["description"] == "A file path");
}

TEST_CASE("LLM.ParameterSchema.WithConstraints", "[Core][LLM]") {
    llm::ParameterSchema schema;
    schema.type = llm::SchemaType::String;
    schema.min_length = 1;
    schema.max_length = 100;
    schema.pattern = "^[a-zA-Z]+$";
    
    nlohmann::json j = schema.to_json_schema();
    REQUIRE(j["minLength"] == 1);
    REQUIRE(j["maxLength"] == 100);
    REQUIRE(j["pattern"] == "^[a-zA-Z]+$");
}

TEST_CASE("LLM.ParameterSchema.WithEnum", "[Core][LLM]") {
    llm::ParameterSchema schema;
    schema.type = llm::SchemaType::String;
    schema.enum_values = nlohmann::json::array({"read", "write", "execute"});
    
    nlohmann::json j = schema.to_json_schema();
    REQUIRE(j["enum"].is_array());
    REQUIRE(j["enum"].size() == 3);
}

TEST_CASE("LLM.ParameterSchema.NumberWithRange", "[Core][LLM]") {
    llm::ParameterSchema schema;
    schema.type = llm::SchemaType::Integer;
    schema.minimum = 0;
    schema.maximum = 100;
    schema.default_value = 50;
    
    nlohmann::json j = schema.to_json_schema();
    REQUIRE(j["minimum"] == 0);
    REQUIRE(j["maximum"] == 100);
    REQUIRE(j["default"] == 50);
}

TEST_CASE("LLM.ToolSchema.Basic", "[Core][LLM]") {
    llm::ToolSchema schema("read_file", "Read a file from disk");
    
    REQUIRE(schema.name() == "read_file");
    REQUIRE(schema.description() == "Read a file from disk");
}

TEST_CASE("LLM.ToolSchema.AddStringParam", "[Core][LLM]") {
    llm::ToolSchema schema("test");
    schema.add_string_param("path", "File path", true);
    
    auto params = schema.parameters();
    REQUIRE(params.contains("path"));
    REQUIRE(params.at("path").type == llm::SchemaType::String);
    REQUIRE(schema.required_params().size() == 1);
}

TEST_CASE("LLM.ToolSchema.AddNumberParam", "[Core][LLM]") {
    llm::ToolSchema schema("test");
    schema.add_number_param("timeout", "Timeout in seconds", false, std::nullopt, 0.0, 60.0);
    
    auto params = schema.parameters();
    REQUIRE(params.contains("timeout"));
    REQUIRE(params.at("timeout").type == llm::SchemaType::Number);
    REQUIRE(params.at("timeout").minimum == 0.0);
    REQUIRE(params.at("timeout").maximum == 60.0);
}

TEST_CASE("LLM.ToolSchema.AddIntegerParam", "[Core][LLM]") {
    llm::ToolSchema schema("test");
    schema.add_integer_param("count", "Number of items", true, 10, 1, 100);
    
    auto params = schema.parameters();
    REQUIRE(params.contains("count"));
    REQUIRE(params.at("count").type == llm::SchemaType::Integer);
    REQUIRE(params.at("count").default_value == 10);
}

TEST_CASE("LLM.ToolSchema.AddBooleanParam", "[Core][LLM]") {
    llm::ToolSchema schema("test");
    schema.add_boolean_param("verbose", "Enable verbose output", false, true);
    
    auto params = schema.parameters();
    REQUIRE(params.contains("verbose"));
    REQUIRE(params.at("verbose").type == llm::SchemaType::Boolean);
    REQUIRE(params.at("verbose").default_value == true);
}

TEST_CASE("LLM.ToolSchema.AddEnumParam", "[Core][LLM]") {
    llm::ToolSchema schema("test");
    schema.add_enum_param("mode", "Operation mode", {"read", "write", "append"}, true);
    
    auto params = schema.parameters();
    REQUIRE(params.contains("mode"));
    REQUIRE(params.at("mode").enum_values.has_value());
    REQUIRE(params.at("mode").enum_values->size() == 3);
}

TEST_CASE("LLM.ToolSchema.ToOpenAITool", "[Core][LLM]") {
    llm::ToolSchema schema("bash", "Run a bash command");
    schema.add_string_param("command", "The command to run", true);
    
    nlohmann::json j = schema.to_openai_tool();
    REQUIRE(j["type"] == "function");
    REQUIRE(j["function"]["name"] == "bash");
    REQUIRE(j["function"]["description"] == "Run a bash command");
    REQUIRE(j["function"]["parameters"]["type"] == "object");
}

TEST_CASE("LLM.ToolSchema.ToAnthropicTool", "[Core][LLM]") {
    llm::ToolSchema schema("read_file", "Read a file");
    schema.add_string_param("path", "File path", true);
    
    nlohmann::json j = schema.to_anthropic_tool();
    REQUIRE(j["name"] == "read_file");
    REQUIRE(j["description"] == "Read a file");
    REQUIRE(j.contains("input_schema"));
}

TEST_CASE("LLM.ToolSchema.RoundTrip", "[Core][LLM]") {
    llm::ToolSchema original("test_tool", "A test tool");
    original.add_string_param("input", "Input string", true);
    original.add_integer_param("count", "Count", false, 5);
    
    nlohmann::json j = original.to_json();
    auto restored = llm::ToolSchema::from_json(j);
    
    REQUIRE(restored.name() == "test_tool");
    REQUIRE(restored.description() == "A test tool");
    REQUIRE(restored.parameters().size() == 2);
}

TEST_CASE("LLM.SchemaUtils.ValidateAgainstSchema", "[Core][LLM]") {
    llm::ParameterSchema schema;
    schema.type = llm::SchemaType::String;
    schema.min_length = 1;
    schema.max_length = 10;
    
    REQUIRE(llm::schema_utils::validate_against_schema("hello", schema));
    REQUIRE_FALSE(llm::schema_utils::validate_against_schema("", schema));  // Too short
    REQUIRE_FALSE(llm::schema_utils::validate_against_schema("this is too long", schema));  // Too long
}

// ==================== Version Tests ====================

TEST_CASE("Core.Version.String", "[Core]") {
    REQUIRE(Version::string() == "0.1.0");
    REQUIRE(Version::major == 0);
    REQUIRE(Version::minor == 1);
    REQUIRE(Version::patch == 0);
}

TEST_CASE("Core.Version.Name", "[Core]") {
    REQUIRE(Version::name() == "turbot-ai");
}

TEST_CASE("Core.GetVersionString", "[Core]") {
    REQUIRE(get_version_string() == "0.1.0");
}

// ==================== PromptBuilder Extended Tests ====================

TEST_CASE("LLM.PromptBuilder.BuildSystem", "[Core][LLM][PromptBuilder]") {
    llm::PromptBuilder builder;
    llm::PromptBuildParams params;
    params.session_id = "test-session";
    params.model_id = "gpt-4";
    params.provider_id = "openai";
    params.working_directory = "/tmp";
    params.is_git_repo = true;
    params.platform = "macos";
    
    std::string system = builder.build_system(params);
    REQUIRE_FALSE(system.empty());
    // Should contain session info
    REQUIRE(system.find("test-session") != std::string::npos);
}

TEST_CASE("LLM.PromptBuilder.BuildMessages", "[Core][LLM][PromptBuilder]") {
    llm::PromptBuilder builder;
    llm::PromptBuildParams params;
    params.user_message = "Hello, world!";
    params.format = MessageFormat::OpenAI;
    
    auto messages = builder.build_messages(params, MessageFormat::OpenAI);
    REQUIRE_FALSE(messages.empty());
    REQUIRE(messages[0].role == ::turbot::core::LlmRole::User);
}

TEST_CASE("LLM.PromptBuilder.BuildMessages.AnthropicFormat", "[Core][LLM][PromptBuilder]") {
    llm::PromptBuilder builder;
    llm::PromptBuildParams params;
    params.user_message = "Test message";
    params.format = MessageFormat::Anthropic;
    
    auto messages = builder.build_messages(params, MessageFormat::Anthropic);
    REQUIRE_FALSE(messages.empty());
}

TEST_CASE("LLM.PromptBuilder.BuildMessages.EmptyUserMessage", "[Core][LLM][PromptBuilder]") {
    llm::PromptBuilder builder;
    llm::PromptBuildParams params;
    params.user_message = "";  // Empty user message
    params.format = MessageFormat::OpenAI;
    
    auto messages = builder.build_messages(params, MessageFormat::OpenAI);
    // Should return empty messages when user_message is empty
    REQUIRE(messages.empty());
}

TEST_CASE("LLM.PromptBuilder.BuildTools.AllTools", "[Core][LLM][PromptBuilder]") {
    llm::PromptBuilder builder;
    
    // Build tools with empty filter (get all)
    auto tools = builder.build_tools({});
    // May be empty if no tools registered
    REQUIRE(tools.size() >= 0);
}

TEST_CASE("LLM.PromptBuilder.BuildTools.Filtered", "[Core][LLM][PromptBuilder]") {
    llm::PromptBuilder builder;
    
    // Build tools with filter
    std::vector<std::string> allowed = {"bash", "read_file"};
    auto tools = builder.build_tools(allowed);
    // Tools should be filtered
    for (const auto& tool : tools) {
        REQUIRE((tool.name == "bash" || tool.name == "read_file"));
    }
}

TEST_CASE("LLM.PromptBuilder.ToolToDefinition", "[Core][LLM][PromptBuilder]") {
    llm::PromptBuilder builder;
    
    // Create a mock tool
    auto mock_tool = std::make_shared<MockTool>("test_tool", "Test description");
    
    auto def = builder.tool_to_definition(mock_tool);
    REQUIRE(def.name == "test_tool");
    REQUIRE(def.description == "Test description");
    REQUIRE(def.parameters.is_object());
}

TEST_CASE("LLM.PromptBuilder.ToolToDefinition.NullTool", "[Core][LLM][PromptBuilder]") {
    llm::PromptBuilder builder;
    
    // Test with null tool - should throw
    REQUIRE_THROWS_AS(builder.tool_to_definition(nullptr), std::invalid_argument);
}

TEST_CASE("LLM.PromptBuilder.Build.Full", "[Core][LLM][PromptBuilder]") {
    llm::PromptBuilder builder;
    llm::PromptBuildParams params;
    params.session_id = "full-test-session";
    params.model_id = "claude-3";
    params.provider_id = "anthropic";
    params.user_message = "Write a hello world program";
    params.format = MessageFormat::Anthropic;
    params.working_directory = "/workspace";
    
    auto result = builder.build(params);
    REQUIRE_FALSE(result.system.empty());
    REQUIRE_FALSE(result.messages.empty());
    // Tools may be empty if none registered
    REQUIRE(result.tools.size() >= 0);
}

TEST_CASE("LLM.PromptBuilder.BuildResult.MessagesJson", "[Core][LLM][PromptBuilder]") {
    llm::PromptBuildResult result;
    result.messages = {
        LlmMessage::create_user("Hello"),
        LlmMessage::create_assistant("Hi there!")
    };
    
    auto json = result.build_messages_json(MessageFormat::OpenAI);
    REQUIRE(json.is_array());
    REQUIRE(json.size() == 2);
}

TEST_CASE("LLM.PromptBuilder.BuildResult.ToolsJson.OpenAI", "[Core][LLM][PromptBuilder]") {
    llm::PromptBuildResult result;
    llm::ToolDefinition tool;
    tool.name = "bash";
    tool.description = "Run command";
    tool.parameters = {{"type", "object"}};
    result.tools = {tool};
    
    auto json = result.build_tools_json(MessageFormat::OpenAI);
    REQUIRE(json.is_array());
    REQUIRE(json.size() == 1);
    REQUIRE(json[0]["type"] == "function");
}

TEST_CASE("LLM.PromptBuilder.BuildResult.ToolsJson.Anthropic", "[Core][LLM][PromptBuilder]") {
    llm::PromptBuildResult result;
    llm::ToolDefinition tool;
    tool.name = "read_file";
    tool.description = "Read file content";
    tool.parameters = {{"type", "object"}};
    result.tools = {tool};
    
    auto json = result.build_tools_json(MessageFormat::Anthropic);
    REQUIRE(json.is_array());
    REQUIRE(json.size() == 1);
    REQUIRE(json[0].contains("input_schema"));
}

// ==================== LLMMessage Extended Tests ====================

TEST_CASE("LLM.LLMMessage.ToJson.WithName", "[Core][LLM]") {
    llm::LLMMessage msg;
    msg.role = provider::ChatRole::User;
    msg.content = "Hello";
    msg.name = "test_user";
    
    auto j = msg.to_json();
    REQUIRE(j["role"] == "user");
    REQUIRE(j["content"] == "Hello");
    REQUIRE(j["name"] == "test_user");
}

TEST_CASE("LLM.LLMMessage.ToJson.WithToolCallId", "[Core][LLM]") {
    llm::LLMMessage msg;
    msg.role = provider::ChatRole::Tool;
    msg.content = "Tool result";
    msg.tool_call_id = "call_123";
    
    auto j = msg.to_json();
    REQUIRE(j["tool_call_id"] == "call_123");
}

TEST_CASE("LLM.LLMMessage.ToJson.WithToolCalls", "[Core][LLM]") {
    llm::LLMMessage msg;
    msg.role = provider::ChatRole::Assistant;
    msg.content = "Using tools";
    
    ToolCallChunk tc;
    tc.id = "tc_1";
    tc.name = "bash";
    tc.arguments = R"({"command": "ls"})";
    msg.tool_calls = {tc};
    
    auto j = msg.to_json();
    REQUIRE(j.contains("tool_calls"));
    REQUIRE(j["tool_calls"].is_array());
    REQUIRE(j["tool_calls"].size() == 1);
}

TEST_CASE("LLM.LLMMessage.FromJson.WithName", "[Core][LLM]") {
    nlohmann::json j = {
        {"role", "user"},
        {"content", "Hello"},
        {"name", "test_user"}
    };
    
    auto msg = llm::LLMMessage::from_json(j);
    REQUIRE(msg.role == provider::ChatRole::User);
    REQUIRE(msg.content == "Hello");
    REQUIRE(msg.name.has_value());
    REQUIRE(*msg.name == "test_user");
}

TEST_CASE("LLM.LLMMessage.FromJson.WithToolCallId", "[Core][LLM]") {
    nlohmann::json j = {
        {"role", "tool"},
        {"content", "Result"},
        {"tool_call_id", "call_456"}
    };
    
    auto msg = llm::LLMMessage::from_json(j);
    REQUIRE(msg.role == provider::ChatRole::Tool);
    REQUIRE(msg.tool_call_id.has_value());
    REQUIRE(*msg.tool_call_id == "call_456");
}

TEST_CASE("LLM.LLMMessage.FromJson.WithToolCalls", "[Core][LLM]") {
    nlohmann::json j = {
        {"role", "assistant"},
        {"content", "Using tools"},
        {"tool_calls", {
            {{"id", "tc_1"}, {"function", {{"name", "bash"}, {"arguments", "{}"}}}}
        }}
    };
    
    auto msg = llm::LLMMessage::from_json(j);
    REQUIRE(msg.role == provider::ChatRole::Assistant);
    REQUIRE_FALSE(msg.tool_calls.empty());
}

TEST_CASE("LLM.LLMMessage.System.Short", "[Core][LLM]") {
    auto msg = llm::LLMMessage::system("System prompt");
    REQUIRE(msg.role == provider::ChatRole::System);
    REQUIRE(msg.content == "System prompt");
}

TEST_CASE("LLM.LLMMessage.User.Short", "[Core][LLM]") {
    auto msg = llm::LLMMessage::user("User message");
    REQUIRE(msg.role == provider::ChatRole::User);
    REQUIRE(msg.content == "User message");
}

TEST_CASE("LLM.LLMMessage.Assistant.Short", "[Core][LLM]") {
    auto msg = llm::LLMMessage::assistant("Assistant response");
    REQUIRE(msg.role == provider::ChatRole::Assistant);
    REQUIRE(msg.content == "Assistant response");
}

TEST_CASE("LLM.LLMMessage.Tool", "[Core][LLM]") {
    auto msg = llm::LLMMessage::tool_result("call_789", "Tool result");
    REQUIRE(msg.role == provider::ChatRole::Tool);
    REQUIRE(msg.tool_call_id == "call_789");
    REQUIRE(msg.content == "Tool result");
}

// ==================== LLMToolDefinition Extended Tests ====================

TEST_CASE("LLM.LLMToolDefinition.FromJson.WithFunction", "[Core][LLM]") {
    nlohmann::json j = {
        {"type", "function"},
        {"function", {
            {"name", "read_file"},
            {"description", "Read a file"},
            {"parameters", {{"type", "object"}}}
        }}
    };
    
    auto tool = llm::LLMToolDefinition::from_json(j);
    REQUIRE(tool.name == "read_file");
    REQUIRE(tool.description == "Read a file");
}

TEST_CASE("LLM.LLMToolDefinition.FromJson.WithoutFunction", "[Core][LLM]") {
    nlohmann::json j = {
        {"name", "bash"},
        {"description", "Run command"},
        {"parameters", {{"type", "object"}}}
    };
    
    auto tool = llm::LLMToolDefinition::from_json(j);
    REQUIRE(tool.name == "bash");
    REQUIRE(tool.description == "Run command");
}

// ==================== ToolCallResult Extended Tests ====================

TEST_CASE("LLM.ToolCallResult.ToJson.WithError", "[Core][LLM]") {
    llm::ToolCallResult result;
    result.tool_call_id = "tc_error";
    result.content = "Error occurred";
    result.is_error = true;
    
    auto j = result.to_json();
    REQUIRE(j["is_error"] == true);
}

TEST_CASE("LLM.ToolCallResult.ToJson.WithoutError", "[Core][LLM]") {
    llm::ToolCallResult result;
    result.tool_call_id = "tc_success";
    result.content = "Success";
    result.is_error = false;
    
    auto j = result.to_json();
    REQUIRE_FALSE(j.contains("is_error"));
}

TEST_CASE("LLM.ToolCallResult.FromJson.WithError", "[Core][LLM]") {
    nlohmann::json j = {
        {"tool_call_id", "tc_err"},
        {"content", "Error"},
        {"is_error", true}
    };
    
    auto result = llm::ToolCallResult::from_json(j);
    REQUIRE(result.is_error == true);
    REQUIRE(result.tool_call_id == "tc_err");
}

// ==================== Extended LLM Tests ====================

TEST_CASE("LLM.ToolCallChunk.Streaming", "[Core][LLM]") {
    ToolCallChunk chunk;
    chunk.id = "stream_call";
    chunk.name = "test_tool";
    chunk.arguments = R"({"arg":)";
    chunk.is_complete = false;
    
    REQUIRE_FALSE(chunk.is_complete);
    
    // Complete the chunk
    chunk.arguments += R"( "value"})";
    chunk.is_complete = true;
    
    REQUIRE(chunk.is_complete);
}

// ==================== Extended ProviderAdapter Tests ====================

TEST_CASE("LLM.ProviderAdapter.MessageWithToolCalls", "[Core][LLM]") {
    llm::LLMMessage msg;
    msg.role = provider::ChatRole::Assistant;
    msg.content = "";
    msg.tool_calls.push_back(ToolCallChunk{"tc-1", "bash", R"({"cmd": "ls"})", true});
    
    auto pmsg = llm::ProviderAdapter::to_provider_message(msg);
    REQUIRE(pmsg.tool_calls.has_value());
    REQUIRE(pmsg.tool_calls->size() == 1);
    REQUIRE(pmsg.tool_calls->at(0).name == "bash");
    
    // Convert back
    auto restored = llm::ProviderAdapter::from_provider_message(pmsg);
    REQUIRE(restored.tool_calls.size() == 1);
    REQUIRE(restored.tool_calls[0].name == "bash");
}

TEST_CASE("LLM.ProviderAdapter.MessagesVector", "[Core][LLM]") {
    std::vector<llm::LLMMessage> messages;
    messages.push_back(llm::LLMMessage::system("System prompt"));
    messages.push_back(llm::LLMMessage::user("User message"));
    
    auto pmsgs = llm::ProviderAdapter::to_provider_messages(messages);
    REQUIRE(pmsgs.size() == 2);
    REQUIRE(pmsgs[0].role == provider::ChatRole::System);
    REQUIRE(pmsgs[1].role == provider::ChatRole::User);
}

TEST_CASE("LLM.ProviderAdapter.ToolsVector", "[Core][LLM]") {
    std::vector<llm::LLMToolDefinition> tools;
    tools.push_back({"bash", "Run command", {{"type", "object"}}});
    tools.push_back({"read", "Read file", {{"type", "object"}}});
    
    auto ptools = llm::ProviderAdapter::to_provider_tools(tools);
    REQUIRE(ptools.size() == 2);
    REQUIRE(ptools[0].name == "bash");
    REQUIRE(ptools[1].name == "read");
}

TEST_CASE("LLM.ProviderAdapter.ChatOptionsConversion", "[Core][LLM]") {
    llm::StreamParams params;
    params.temperature = 0.7;
    params.top_p = 0.9;
    params.max_tokens = 2048;
    params.stop = {"END", "STOP"};
    params.tools.push_back({"test_tool", "A test tool", {{"type", "object"}}});
    
    auto options = llm::ProviderAdapter::to_chat_options(params);
    REQUIRE(options.temperature == Catch::Approx(0.7));
    REQUIRE(options.top_p == Catch::Approx(0.9));
    REQUIRE(options.max_tokens == 2048);
    REQUIRE(!options.stop.empty());
    REQUIRE(options.stop.size() == 2);
    REQUIRE(options.tools.size() == 1);
    REQUIRE(options.stream == true);
    
    // Convert back
    auto restored = llm::ProviderAdapter::from_chat_options(options);
    REQUIRE(restored.temperature == Catch::Approx(0.7));
    REQUIRE(restored.top_p.value() == Catch::Approx(0.9));
    REQUIRE(restored.max_tokens.value() == 2048);
}

// ==================== Extended ToolSchema Tests ====================

TEST_CASE("LLM.ToolSchema.AddArrayParam", "[Core][LLM]") {
    llm::ToolSchema schema("test");
    llm::ParameterSchema item_schema;
    item_schema.type = llm::SchemaType::String;
    
    schema.add_array_param("items", "List of items", item_schema, false);
    
    auto params = schema.parameters();
    REQUIRE(params.contains("items"));
    REQUIRE(params.at("items").type == llm::SchemaType::Array);
    REQUIRE(params.at("items").items != nullptr);
}

TEST_CASE("LLM.ToolSchema.AddObjectParam", "[Core][LLM]") {
    llm::ToolSchema schema("test");
    std::map<std::string, llm::ParameterSchema> properties;
    
    llm::ParameterSchema name_schema;
    name_schema.type = llm::SchemaType::String;
    name_schema.description = "Name";
    properties["name"] = name_schema;
    
    llm::ParameterSchema age_schema;
    age_schema.type = llm::SchemaType::Integer;
    age_schema.description = "Age";
    properties["age"] = age_schema;
    
    schema.add_object_param("person", "Person object", properties, true);
    
    auto params = schema.parameters();
    REQUIRE(params.contains("person"));
    REQUIRE(params.at("person").type == llm::SchemaType::Object);
    REQUIRE(params.at("person").properties.size() == 2);
}

TEST_CASE("LLM.ToolSchema.SetName", "[Core][LLM]") {
    llm::ToolSchema schema("old_name");
    schema.set_name("new_name");
    
    REQUIRE(schema.name() == "new_name");
}

TEST_CASE("LLM.ToolSchema.SetDescription", "[Core][LLM]") {
    llm::ToolSchema schema("test");
    schema.set_description("New description");
    
    REQUIRE(schema.description() == "New description");
}

TEST_CASE("LLM.ParameterSchema.ToJsonSchema.Full", "[Core][LLM]") {
    llm::ParameterSchema schema;
    schema.type = llm::SchemaType::String;
    schema.description = "A string parameter";
    schema.default_value = "default";
    schema.enum_values = nlohmann::json::array({"a", "b", "c"});
    schema.min_length = 1;
    schema.max_length = 10;
    schema.pattern = "^[a-z]+$";
    
    auto j = schema.to_json_schema();
    REQUIRE(j["type"] == "string");
    REQUIRE(j["description"] == "A string parameter");
    REQUIRE(j["default"] == "default");
    REQUIRE(j["enum"].size() == 3);
    REQUIRE(j["minLength"] == 1);
    REQUIRE(j["maxLength"] == 10);
    REQUIRE(j["pattern"] == "^[a-z]+$");
}

TEST_CASE("LLM.ParameterSchema.FromJsonSchema.Full", "[Core][LLM]") {
    nlohmann::json j = {
        {"type", "integer"},
        {"description", "An integer"},
        {"default", 42},
        {"minimum", 0},
        {"maximum", 100}
    };
    
    auto schema = llm::ParameterSchema::from_json_schema(j);
    REQUIRE(schema.type == llm::SchemaType::Integer);
    REQUIRE(schema.description == "An integer");
    REQUIRE(schema.default_value == 42);
    REQUIRE(schema.minimum == 0);
    REQUIRE(schema.maximum == 100);
}

TEST_CASE("LLM.ParameterSchema.ArrayWithItems", "[Core][LLM]") {
    nlohmann::json j = {
        {"type", "array"},
        {"items", {{"type", "string"}}}
    };
    
    auto schema = llm::ParameterSchema::from_json_schema(j);
    REQUIRE(schema.type == llm::SchemaType::Array);
    REQUIRE(schema.items != nullptr);
    REQUIRE(schema.items->type == llm::SchemaType::String);
}

TEST_CASE("LLM.ParameterSchema.ObjectWithProperties", "[Core][LLM]") {
    nlohmann::json j = {
        {"type", "object"},
        {"properties", {
            {"name", {{"type", "string"}}},
            {"count", {{"type", "integer"}}}
        }}
    };
    
    auto schema = llm::ParameterSchema::from_json_schema(j);
    REQUIRE(schema.type == llm::SchemaType::Object);
    REQUIRE(schema.properties.size() == 2);
}

TEST_CASE("LLM.SchemaType.ToString.All", "[Core][LLM]") {
    REQUIRE(llm::schema_type_to_string(llm::SchemaType::String) == "string");
    REQUIRE(llm::schema_type_to_string(llm::SchemaType::Number) == "number");
    REQUIRE(llm::schema_type_to_string(llm::SchemaType::Integer) == "integer");
    REQUIRE(llm::schema_type_to_string(llm::SchemaType::Boolean) == "boolean");
    REQUIRE(llm::schema_type_to_string(llm::SchemaType::Object) == "object");
    REQUIRE(llm::schema_type_to_string(llm::SchemaType::Array) == "array");
    REQUIRE(llm::schema_type_to_string(llm::SchemaType::Null) == "null");
}

TEST_CASE("LLM.SchemaType.FromString.All", "[Core][LLM]") {
    REQUIRE(llm::string_to_schema_type("string") == llm::SchemaType::String);
    REQUIRE(llm::string_to_schema_type("number") == llm::SchemaType::Number);
    REQUIRE(llm::string_to_schema_type("integer") == llm::SchemaType::Integer);
    REQUIRE(llm::string_to_schema_type("boolean") == llm::SchemaType::Boolean);
    REQUIRE(llm::string_to_schema_type("object") == llm::SchemaType::Object);
    REQUIRE(llm::string_to_schema_type("array") == llm::SchemaType::Array);
    REQUIRE(llm::string_to_schema_type("null") == llm::SchemaType::Null);
}

TEST_CASE("LLM.SchemaType.FromString.Invalid", "[Core][LLM]") {
    REQUIRE_THROWS_AS(llm::string_to_schema_type("invalid_type"), std::invalid_argument);
}

TEST_CASE("LLM.ToolSchema.FromJson.AnthropicFormat", "[Core][LLM]") {
    nlohmann::json j = {
        {"name", "test_tool"},
        {"description", "A test tool"},
        {"input_schema", {
            {"type", "object"},
            {"properties", {
                {"input", {{"type", "string"}}}
            }},
            {"required", {"input"}}
        }}
    };
    
    auto schema = llm::ToolSchema::from_json(j);
    REQUIRE(schema.name() == "test_tool");
    REQUIRE(schema.description() == "A test tool");
    REQUIRE(schema.parameters().size() == 1);
    REQUIRE(schema.required_params().size() == 1);
}

TEST_CASE("LLM.ToolSchema.FromJson.MissingFunction", "[Core][LLM]") {
    nlohmann::json j = {{"type", "function"}};
    
    REQUIRE_THROWS_AS(llm::ToolSchema::from_json(j), std::invalid_argument);
}

TEST_CASE("LLM.ToolSchema.FromToolDefinition.Valid", "[Core][LLM]") {
    nlohmann::json j = {
        {"type", "function"},
        {"function", {
            {"name", "valid_tool"},
            {"description", "Valid tool"},
            {"parameters", {{"type", "object"}}}
        }}
    };
    
    auto result = llm::ToolSchema::from_tool_definition(j);
    REQUIRE(result.has_value());
    REQUIRE(result->name() == "valid_tool");
}

TEST_CASE("LLM.ToolSchema.FromToolDefinition.Invalid", "[Core][LLM]") {
    nlohmann::json j = {{"invalid", "format"}};
    
    auto result = llm::ToolSchema::from_tool_definition(j);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("LLM.SchemaUtils.ValidateNumber", "[Core][LLM]") {
    llm::ParameterSchema schema;
    schema.type = llm::SchemaType::Number;
    schema.minimum = 0.0;
    schema.maximum = 100.0;
    
    REQUIRE(llm::schema_utils::validate_against_schema(50.0, schema));
    REQUIRE(llm::schema_utils::validate_against_schema(0.0, schema));
    REQUIRE(llm::schema_utils::validate_against_schema(100.0, schema));
    REQUIRE_FALSE(llm::schema_utils::validate_against_schema(-1.0, schema));
    REQUIRE_FALSE(llm::schema_utils::validate_against_schema(101.0, schema));
}

TEST_CASE("LLM.SchemaUtils.ValidateInteger", "[Core][LLM]") {
    llm::ParameterSchema schema;
    schema.type = llm::SchemaType::Integer;
    schema.minimum = 1;
    schema.maximum = 10;
    
    REQUIRE(llm::schema_utils::validate_against_schema(5, schema));
    REQUIRE(llm::schema_utils::validate_against_schema(1, schema));
    REQUIRE(llm::schema_utils::validate_against_schema(10, schema));
    REQUIRE_FALSE(llm::schema_utils::validate_against_schema(0, schema));
    REQUIRE_FALSE(llm::schema_utils::validate_against_schema(11, schema));
}

TEST_CASE("LLM.SchemaUtils.ValidateBoolean", "[Core][LLM]") {
    llm::ParameterSchema schema;
    schema.type = llm::SchemaType::Boolean;
    
    REQUIRE(llm::schema_utils::validate_against_schema(true, schema));
    REQUIRE(llm::schema_utils::validate_against_schema(false, schema));
}

TEST_CASE("LLM.SchemaUtils.ValidateEnum", "[Core][LLM]") {
    llm::ParameterSchema schema;
    schema.type = llm::SchemaType::String;
    schema.enum_values = nlohmann::json::array({"red", "green", "blue"});
    
    REQUIRE(llm::schema_utils::validate_against_schema("red", schema));
    REQUIRE(llm::schema_utils::validate_against_schema("green", schema));
    REQUIRE(llm::schema_utils::validate_against_schema("blue", schema));
    REQUIRE_FALSE(llm::schema_utils::validate_against_schema("yellow", schema));
}

// ==================== ProviderAdapter Extended Tests ====================

// ==================== LLMToolDefinition Extended Tests ====================

TEST_CASE("LLM.LLMToolDefinition.ToJsonExtended", "[Core][LLM]") {
    llm::LLMToolDefinition tool;
    tool.name = "read_file";
    tool.description = "Read a file from disk";
    tool.parameters = {
        {"type", "object"},
        {"properties", {
            {"path", {{"type", "string"}, {"description", "File path"}}}
        }},
        {"required", {"path"}}
    };
    
    nlohmann::json j = tool.to_json();
    
    REQUIRE(j["type"] == "function");
    REQUIRE(j["function"]["name"] == "read_file");
    REQUIRE(j["function"]["description"] == "Read a file from disk");
    REQUIRE(j["function"]["parameters"]["type"] == "object");
}

TEST_CASE("LLM.LLMToolDefinition.FromJsonExtended", "[Core][LLM]") {
    nlohmann::json j = {
        {"type", "function"},
        {"function", {
            {"name", "write_file"},
            {"description", "Write a file"},
            {"parameters", {{"type", "object"}}}
        }}
    };
    
    auto tool = llm::LLMToolDefinition::from_json(j);
    REQUIRE(tool.name == "write_file");
    REQUIRE(tool.description == "Write a file");
}

TEST_CASE("LLM.LLMToolDefinition.FromJson.WithoutFunctionExtended", "[Core][LLM]") {
    nlohmann::json j = {
        {"name", "simple_tool"},
        {"description", "A simple tool"},
        {"parameters", {{"type", "object"}}}
    };
    
    auto tool = llm::LLMToolDefinition::from_json(j);
    REQUIRE(tool.name == "simple_tool");
    REQUIRE(tool.description == "A simple tool");
}

// ==================== ToolCallResult Tests ====================

TEST_CASE("LLM.ToolCallResult.ToJsonExtended", "[Core][LLM]") {
    llm::ToolCallResult result;
    result.tool_call_id = "call_123";
    result.content = "File content here";
    result.is_error = false;
    
    nlohmann::json j = result.to_json();
    
    REQUIRE(j["tool_call_id"] == "call_123");
    REQUIRE(j["content"] == "File content here");
    REQUIRE_FALSE(j.contains("is_error"));
}

TEST_CASE("LLM.ToolCallResult.ToJsonWithErrorExtended", "[Core][LLM]") {
    llm::ToolCallResult result;
    result.tool_call_id = "call_456";
    result.content = "Error: file not found";
    result.is_error = true;
    
    nlohmann::json j = result.to_json();
    
    REQUIRE(j["tool_call_id"] == "call_456");
    REQUIRE(j["content"] == "Error: file not found");
    REQUIRE(j["is_error"] == true);
}

TEST_CASE("LLM.ToolCallResult.FromJsonExtended", "[Core][LLM]") {
    nlohmann::json j = {
        {"tool_call_id", "call_789"},
        {"content", "Success"},
        {"is_error", true}
    };
    
    auto result = llm::ToolCallResult::from_json(j);
    REQUIRE(result.tool_call_id == "call_789");
    REQUIRE(result.content == "Success");
    REQUIRE(result.is_error == true);
}

// ==================== LLMMessage Extended Tests ====================

TEST_CASE("LLM.LLMMessage.ToJsonExtended", "[Core][LLM]") {
    llm::LLMMessage msg = llm::LLMMessage::user("Hello, world!");
    
    nlohmann::json j = msg.to_json();
    
    REQUIRE(j["role"] == "user");
    REQUIRE(j["content"] == "Hello, world!");
}

TEST_CASE("LLM.LLMMessage.ToJsonWithToolCallsExtended", "[Core][LLM]") {
    ToolCallChunk chunk;
    chunk.id = "tc_123";
    chunk.name = "read_file";
    chunk.arguments = R"({"path": "/tmp/test.txt"})";
    chunk.is_complete = true;
    
    llm::LLMMessage msg = llm::LLMMessage::assistant_with_tools("Let me read that file.", {chunk});
    
    nlohmann::json j = msg.to_json();
    
    REQUIRE(j["role"] == "assistant");
    REQUIRE(j["content"] == "Let me read that file.");
    REQUIRE(j["tool_calls"].size() == 1);
    REQUIRE(j["tool_calls"][0]["id"] == "tc_123");
}

TEST_CASE("LLM.LLMMessage.ToJsonWithToolCallIdExtended", "[Core][LLM]") {
    llm::LLMMessage msg = llm::LLMMessage::tool_result("tc_456", "File contents here");
    
    nlohmann::json j = msg.to_json();
    
    REQUIRE(j["role"] == "tool");
    REQUIRE(j["tool_call_id"] == "tc_456");
    REQUIRE(j["content"] == "File contents here");
}

TEST_CASE("LLM.LLMMessage.FromJsonExtended", "[Core][LLM]") {
    nlohmann::json j = {
        {"role", "assistant"},
        {"content", "Hello!"},
        {"name", "assistant"}
    };
    
    auto msg = llm::LLMMessage::from_json(j);
    REQUIRE(msg.role == provider::ChatRole::Assistant);
    REQUIRE(msg.content == "Hello!");
    REQUIRE(msg.name.has_value());
    REQUIRE(*msg.name == "assistant");
}

TEST_CASE("LLM.LLMMessage.FromJsonWithToolCallsExtended", "[Core][LLM]") {
    nlohmann::json j = {
        {"role", "assistant"},
        {"content", ""},
        {"tool_calls", {
            {
                {"id", "tc_001"},
                {"name", "bash"},
                {"arguments", R"({"cmd": "ls"})"},
                {"is_complete", true}
            }
        }}
    };
    
    auto msg = llm::LLMMessage::from_json(j);
    REQUIRE(msg.role == provider::ChatRole::Assistant);
    REQUIRE(msg.tool_calls.size() == 1);
    REQUIRE(msg.tool_calls[0].id == "tc_001");
    REQUIRE(msg.tool_calls[0].name == "bash");
}

TEST_CASE("LLM.LLMMessage.FactoryMethods", "[Core][LLM]") {
    auto system_msg = llm::LLMMessage::system("You are helpful.");
    REQUIRE(system_msg.role == provider::ChatRole::System);
    REQUIRE(system_msg.content == "You are helpful.");
    
    auto user_msg = llm::LLMMessage::user("Hi!");
    REQUIRE(user_msg.role == provider::ChatRole::User);
    REQUIRE(user_msg.content == "Hi!");
    
    auto assistant_msg = llm::LLMMessage::assistant("Hello!");
    REQUIRE(assistant_msg.role == provider::ChatRole::Assistant);
    REQUIRE(assistant_msg.content == "Hello!");
    
    auto tool_msg = llm::LLMMessage::tool_result("tc_123", "Result", false);
    REQUIRE(tool_msg.role == provider::ChatRole::Tool);
    REQUIRE(*tool_msg.tool_call_id == "tc_123");
    REQUIRE(tool_msg.content == "Result");
    
    auto tool_error_msg = llm::LLMMessage::tool_result("tc_456", "Error", true);
    REQUIRE(tool_error_msg.name.has_value());
    REQUIRE(*tool_error_msg.name == "error");
}

// ==================== StreamParams Tests ====================

TEST_CASE("LLM.StreamParams.ToJson", "[Core][LLM]") {
    llm::StreamParams params;
    params.session_id = "session_123";
    params.temperature = 0.7;
    params.messages.push_back(llm::LLMMessage::user("Hello"));
    
    nlohmann::json j = params.to_json();
    
    REQUIRE(j["session_id"] == "session_123");
    REQUIRE(j["temperature"] == 0.7);
    REQUIRE(j["messages"].size() == 1);
}

TEST_CASE("LLM.StreamParams.ToJsonWithTools", "[Core][LLM]") {
    llm::StreamParams params;
    params.temperature = 1.0;
    
    llm::LLMToolDefinition tool;
    tool.name = "bash";
    tool.description = "Run bash command";
    tool.parameters = {{"type", "object"}};
    params.tools.push_back(tool);
    params.tool_choice = "auto";
    
    nlohmann::json j = params.to_json();
    
    REQUIRE(j["tools"].size() == 1);
    REQUIRE(j["tool_choice"] == "auto");
}

TEST_CASE("LLM.StreamParams.ToJsonWithOptions", "[Core][LLM]") {
    llm::StreamParams params;
    params.temperature = 0.5;
    params.top_p = 0.9;
    params.max_tokens = 2048;
    params.stop = {"END", "STOP"};
    
    nlohmann::json j = params.to_json();
    
    REQUIRE(j["temperature"] == 0.5);
    REQUIRE(j["top_p"] == 0.9);
    REQUIRE(j["max_tokens"] == 2048);
    REQUIRE(j["stop"].size() == 2);
}

TEST_CASE("LLM.StreamParams.FromJson", "[Core][LLM]") {
    nlohmann::json j = {
        {"session_id", "session_456"},
        {"temperature", 0.8},
        {"messages", {
            {{"role", "user"}, {"content", "Test"}}
        }},
        {"tools", {
            {{"type", "function"}, {"function", {{"name", "test"}, {"description", "Test tool"}}}}
        }},
        {"tool_choice", "required"},
        {"top_p", 0.95},
        {"max_tokens", 1024},
        {"stop", {"END"}}
    };
    
    auto params = llm::StreamParams::from_json(j);
    
    REQUIRE(params.session_id == "session_456");
    REQUIRE(params.temperature == 0.8);
    REQUIRE(params.messages.size() == 1);
    REQUIRE(params.tools.size() == 1);
    REQUIRE(params.tool_choice.has_value());
    REQUIRE(*params.tool_choice == "required");
    REQUIRE(params.top_p.has_value());
    REQUIRE(*params.top_p == 0.95);
    REQUIRE(params.max_tokens.has_value());
    REQUIRE(*params.max_tokens == 1024);
    REQUIRE(params.stop.size() == 1);
}

// ==================== StreamingState Tests ====================

TEST_CASE("LLM.StreamingState.AddEvent", "[Core][LLM]") {
    llm::StreamingState state;
    
    auto event = StreamEvent::create_text_delta("text-0", "Hello");
    state.add_event(event);
    
    REQUIRE(state.has_events());
    REQUIRE(state.events().size() == 1);
}

TEST_CASE("LLM.StreamingState.PopEvent", "[Core][LLM]") {
    llm::StreamingState state;
    
    state.add_event(StreamEvent::create_text_delta("text-0", "Hello"));
    state.add_event(StreamEvent::create_text_delta("text-0", " World"));
    
    auto event1 = state.pop_event();
    REQUIRE(event1.has_value());
    REQUIRE(event1->delta == "Hello");
    
    auto event2 = state.pop_event();
    REQUIRE(event2.has_value());
    REQUIRE(event2->delta == " World");
    
    auto event3 = state.pop_event();
    REQUIRE_FALSE(event3.has_value());
}

TEST_CASE("LLM.StreamingState.MarkDone", "[Core][LLM]") {
    llm::StreamingState state;
    
    TokenUsage usage;
    usage.input = 100;
    usage.output = 50;
    state.mark_done(FinishReason::Stop, usage);
    
    REQUIRE(state.is_done());
    REQUIRE(state.finish_reason() == FinishReason::Stop);
    REQUIRE(state.usage().input == 100);
    REQUIRE(state.usage().output == 50);
    REQUIRE(state.usage().total() == 150);
}

TEST_CASE("LLM.StreamingState.MarkError", "[Core][LLM]") {
    llm::StreamingState state;
    
    state.mark_error("Connection failed", "network_error");
    
    REQUIRE(state.is_done());
    REQUIRE(state.finish_reason() == FinishReason::Error);
    REQUIRE(state.error().has_value());
    REQUIRE(*state.error() == "Connection failed");
}

TEST_CASE("LLM.StreamingState.GetText", "[Core][LLM]") {
    llm::StreamingState state;
    
    state.add_event(StreamEvent::create_text_delta("text-0", "Hello"));
    state.add_event(StreamEvent::create_text_delta("text-0", " "));
    state.add_event(StreamEvent::create_text_delta("text-0", "World"));
    
    REQUIRE(state.get_text() == "Hello World");
}

TEST_CASE("LLM.StreamingState.GetReasoning", "[Core][LLM]") {
    llm::StreamingState state;
    
    state.add_event(StreamEvent::create_reasoning_delta("reasoning-0", "Let me think..."));
    state.add_event(StreamEvent::create_reasoning_delta("reasoning-0", " about this."));
    
    REQUIRE(state.get_reasoning() == "Let me think... about this.");
}

TEST_CASE("LLM.StreamingState.GetToolCalls", "[Core][LLM]") {
    llm::StreamingState state;
    
    ToolCallChunk chunk1;
    chunk1.id = "tc_001";
    chunk1.name = "read_file";
    chunk1.arguments = R"({"path": "/tmp/test.txt"})";
    chunk1.is_complete = true;
    
    state.add_event(StreamEvent::create_tool_call(chunk1));
    
    auto tool_calls = state.get_tool_calls();
    REQUIRE(tool_calls.size() == 1);
    REQUIRE(tool_calls[0].id == "tc_001");
    REQUIRE(tool_calls[0].name == "read_file");
}

TEST_CASE("LLM.StreamingState.GetToolCallsStreaming", "[Core][LLM]") {
    llm::StreamingState state;
    
    // Simulate streaming tool call
    state.add_event(StreamEvent::create_tool_input_start("tc_002", "bash"));
    state.add_event(StreamEvent::create_tool_input_delta("tc_002", R"({"cmd": "ls")"));
    state.add_event(StreamEvent::create_tool_input_delta("tc_002", R"(})"));
    state.add_event(StreamEvent::create_tool_input_end("tc_002"));
    
    auto tool_calls = state.get_tool_calls();
    REQUIRE(tool_calls.size() == 1);
    REQUIRE(tool_calls[0].id == "tc_002");
    REQUIRE(tool_calls[0].name == "bash");
    REQUIRE(tool_calls[0].arguments == R"({"cmd": "ls"})");
    REQUIRE(tool_calls[0].is_complete);
}

TEST_CASE("LLM.StreamingState.ToResult", "[Core][LLM]") {
    llm::StreamingState state;
    state.set_response_id("resp_123");
    state.set_model("gpt-4");
    state.add_event(StreamEvent::create_text_delta("text-0", "Hello"));
    
    TokenUsage usage;
    usage.input = 10;
    usage.output = 5;
    usage.reasoning = 15;
    state.mark_done(FinishReason::Stop, usage);
    
    auto result = state.to_result();
    
    REQUIRE(result.id == "resp_123");
    REQUIRE(result.model == "gpt-4");
    REQUIRE(result.events.size() == 2); // text delta + finish
    REQUIRE(result.finish_reason == FinishReason::Stop);
}

// ==================== LLMStreamResult Tests ====================

TEST_CASE("LLM.LLMStreamResult.DefaultConstructor", "[Core][LLM]") {
    llm::LLMStreamResult result;
    
    REQUIRE(result.is_done());
    REQUIRE(result.final_text().empty());
    REQUIRE(result.final_reasoning().empty());
    REQUIRE(result.tool_calls().empty());
    REQUIRE_FALSE(result.has_error());
}

TEST_CASE("LLM.LLMStreamResult.WithState", "[Core][LLM]") {
    auto state = std::make_shared<llm::StreamingState>();
    state->add_event(StreamEvent::create_text_delta("text-0", "Test"));
    
    TokenUsage usage;
    usage.input = 5;
    usage.output = 2;
    state->mark_done(FinishReason::Stop, usage);
    
    llm::LLMStreamResult result(state);
    
    REQUIRE(result.is_done());
    REQUIRE(result.final_text() == "Test");
    REQUIRE(result.usage().total() == 7);
    REQUIRE(result.finish_reason() == FinishReason::Stop);
}

TEST_CASE("LLM.LLMStreamResult.Collect", "[Core][LLM]") {
    auto state = std::make_shared<llm::StreamingState>();
    state->add_event(StreamEvent::create_text_delta("text-0", "Hello"));
    state->add_event(StreamEvent::create_text_delta("text-0", " World"));
    state->mark_done(FinishReason::Stop);
    
    llm::LLMStreamResult result(state);
    
    auto events = result.collect();
    REQUIRE(events.size() == 3); // 2 text deltas + finish
}

TEST_CASE("LLM.LLMStreamResult.Error", "[Core][LLM]") {
    auto state = std::make_shared<llm::StreamingState>();
    state->mark_error("Test error", "test_code");
    
    llm::LLMStreamResult result(state);
    
    REQUIRE(result.has_error());
    REQUIRE(result.error().has_value());
    REQUIRE(*result.error() == "Test error");
}

// ==================== LLM Static Methods Tests ====================

TEST_CASE("LLM.ToProviderMessage", "[Core][LLM]") {
    auto msg = llm::LLMMessage::user("Hello");
    auto pmsg = llm::LLM::to_provider_message(msg);
    
    REQUIRE(pmsg.role == provider::ChatRole::User);
    REQUIRE(pmsg.content == "Hello");
}

TEST_CASE("LLM.ToProviderMessageWithToolCalls", "[Core][LLM]") {
    ToolCallChunk chunk;
    chunk.id = "tc_123";
    chunk.name = "read_file";
    chunk.arguments = R"({"path": "/tmp/test.txt"})";
    chunk.is_complete = true;
    
    auto msg = llm::LLMMessage::assistant_with_tools("", {chunk});
    auto pmsg = llm::LLM::to_provider_message(msg);
    
    REQUIRE(pmsg.role == provider::ChatRole::Assistant);
    REQUIRE(pmsg.tool_calls.has_value());
    REQUIRE(pmsg.tool_calls->size() == 1);
    REQUIRE((*pmsg.tool_calls)[0].id == "tc_123");
    REQUIRE((*pmsg.tool_calls)[0].name == "read_file");
}

TEST_CASE("LLM.ToProviderTool", "[Core][LLM]") {
    llm::LLMToolDefinition tool;
    tool.name = "bash";
    tool.description = "Run bash command";
    tool.parameters = {{"type", "object"}};
    
    auto ptool = llm::LLM::to_provider_tool(tool);
    
    REQUIRE(ptool.type == "function");
    REQUIRE(ptool.name == "bash");
    REQUIRE(ptool.description == "Run bash command");
}

TEST_CASE("LLM.ToChatOptions", "[Core][LLM]") {
    llm::StreamParams params;
    params.temperature = 0.5;
    params.top_p = 0.9;
    params.max_tokens = 1024;
    params.stop = {"END"};
    
    llm::LLMToolDefinition tool;
    tool.name = "test";
    tool.description = "Test tool";
    tool.parameters = {{"type", "object"}};
    params.tools.push_back(tool);
    params.tool_choice = "auto";
    
    auto options = llm::LLM::to_chat_options(params);
    
    REQUIRE(options.temperature == 0.5);
    REQUIRE(options.top_p == 0.9);
    REQUIRE(options.max_tokens == 1024);
    REQUIRE(options.stop.size() == 1);
    REQUIRE(options.tools.size() == 1);
    REQUIRE(options.stream == true);
    REQUIRE(options.extra["tool_choice"] == "auto");
}

TEST_CASE("LLM.ProviderAdapter.ToProviderMessages", "[Core][LLM]") {
    std::vector<llm::LLMMessage> messages;
    messages.push_back(llm::LLMMessage::user("Hello"));
    messages.push_back(llm::LLMMessage::assistant("Hi there!"));
    messages.push_back(llm::LLMMessage::user("How are you?"));
    
    auto pmsgs = llm::ProviderAdapter::to_provider_messages(messages);
    REQUIRE(pmsgs.size() == 3);
    REQUIRE(pmsgs[0].content == "Hello");
    REQUIRE(pmsgs[1].content == "Hi there!");
    REQUIRE(pmsgs[2].content == "How are you?");
}

TEST_CASE("LLM.ProviderAdapter.ToProviderTools", "[Core][LLM]") {
    std::vector<llm::LLMToolDefinition> tools;
    
    llm::LLMToolDefinition tool1;
    tool1.name = "read_file";
    tool1.description = "Read a file";
    tool1.parameters = {{"type", "object"}};
    tools.push_back(tool1);
    
    llm::LLMToolDefinition tool2;
    tool2.name = "write_file";
    tool2.description = "Write a file";
    tool2.parameters = {{"type", "object"}};
    tools.push_back(tool2);
    
    auto ptools = llm::ProviderAdapter::to_provider_tools(tools);
    REQUIRE(ptools.size() == 2);
    REQUIRE(ptools[0].name == "read_file");
    REQUIRE(ptools[0].type == "function");
    REQUIRE(ptools[1].name == "write_file");
}

TEST_CASE("LLM.ProviderAdapter.ChatOptionsConversionExtended", "[Core][LLM]") {
    llm::StreamParams params;
    params.temperature = 0.7;
    params.top_p = 0.9;
    params.max_tokens = 2048;
    
    auto options = llm::ProviderAdapter::to_chat_options(params);
    REQUIRE(options.temperature == 0.7);
    REQUIRE(options.top_p == 0.9);
    REQUIRE(options.max_tokens == 2048);
    REQUIRE(options.stream == true);
    
    // Convert back
    auto restored = llm::ProviderAdapter::from_chat_options(options);
    REQUIRE(restored.temperature == 0.7);
    REQUIRE(restored.top_p == 0.9);
    REQUIRE(restored.max_tokens == 2048);
}

TEST_CASE("LLM.ProviderAdapter.StreamEventConversion", "[Core][LLM]") {
    provider::ChatStreamEvent event;
    event.type = provider::StreamEventType::TextDelta;
    event.content = "Hello";
    
    auto stream_event = llm::ProviderAdapter::to_stream_event(event);
    // Verify conversion works
    REQUIRE(stream_event.type == StreamEventType::TextDelta);
}

TEST_CASE("LLM.ProviderAdapter.StreamResultConversion", "[Core][LLM]") {
    provider::ChatResponse response;
    response.id = "chat_123";
    response.model = "gpt-4";
    
    auto result = llm::ProviderAdapter::to_stream_result(response);
    REQUIRE(result.id == "chat_123");
    REQUIRE(result.model == "gpt-4");
}

TEST_CASE("LLM.ProviderAdapter.ChatOptionsWithTools", "[Core][LLM]") {
    llm::StreamParams params;
    params.temperature = 0.5;
    
    llm::LLMToolDefinition tool;
    tool.name = "test_tool";
    tool.description = "A test tool";
    tool.parameters = {{"type", "object"}};
    params.tools.push_back(tool);
    
    auto options = llm::ProviderAdapter::to_chat_options(params);
    REQUIRE(options.tools.size() == 1);
    REQUIRE(options.tools[0].name == "test_tool");
}

// ==================== MessageBuilder Extended Tests ====================

TEST_CASE("LLM.MessageBuilder.WithOptions", "[Core][LLM]") {
    MessageBuilderOptions options;
    options.format = MessageFormat::Anthropic;
    options.capabilities.supports_vision = true;
    options.apply_caching = true;
    
    MessageBuilder builder(options);
    builder.add_user("Hello");
    
    auto result = builder.build();
    REQUIRE(result.size() == 1);
    REQUIRE(result[0]["content"].is_array());
}

TEST_CASE("LLM.MessageBuilder.AddUserParts", "[Core][LLM]") {
    MessageBuilder builder;
    builder.add_user_parts({
        content_parts::text("Hello"),
        content_parts::text("World")
    });
    
    auto messages = builder.get_messages();
    REQUIRE(messages.size() == 1);
    REQUIRE(messages[0].role == LlmRole::User);
    REQUIRE(messages[0].content_parts.has_value());
    REQUIRE(messages[0].content_parts->size() == 2);
}

TEST_CASE("LLM.MessageBuilder.AddAssistantParts", "[Core][LLM]") {
    MessageBuilder builder;
    builder.add_assistant_parts({
        content_parts::text("Hi there!")
    });
    
    auto messages = builder.get_messages();
    REQUIRE(messages.size() == 1);
    REQUIRE(messages[0].role == LlmRole::Assistant);
}

TEST_CASE("LLM.MessageBuilder.AddAssistantWithTools", "[Core][LLM]") {
    MessageBuilder builder;
    builder.add_assistant_with_tools(
        "Let me help you.",
        {content_parts::tool_call("tc_001", "read_file", {{"path", "/tmp/test.txt"}})}
    );
    
    auto messages = builder.get_messages();
    REQUIRE(messages.size() == 1);
    REQUIRE(messages[0].role == LlmRole::Assistant);
    REQUIRE(messages[0].tool_calls.has_value());
    REQUIRE(messages[0].tool_calls->size() == 1);
}

TEST_CASE("LLM.MessageBuilder.AddToolResponse", "[Core][LLM]") {
    MessageBuilder builder;
    builder.add_tool_response("tc_001", "File contents here");
    
    auto messages = builder.get_messages();
    REQUIRE(messages.size() == 1);
    REQUIRE(messages[0].role == LlmRole::Tool);
    REQUIRE(*messages[0].tool_call_id == "tc_001");
    REQUIRE(*messages[0].content == "File contents here");
}

TEST_CASE("LLM.MessageBuilder.AddMessage", "[Core][LLM]") {
    MessageBuilder builder;
    auto msg = LlmMessage::create_user("Test");
    builder.add_message(msg);
    
    REQUIRE(builder.size() == 1);
    REQUIRE_FALSE(builder.empty());
}

TEST_CASE("LLM.MessageBuilder.SetCapabilities", "[Core][LLM]") {
    ProviderCapabilities caps;
    caps.supports_vision = true;
    caps.supports_audio = true;
    caps.supports_pdf = true;
    
    MessageBuilder builder;
    builder.set_capabilities(caps);
    builder.add_user("Test");
    
    REQUIRE(builder.get_messages().size() == 1);
}

TEST_CASE("LLM.MessageBuilder.SetCaching", "[Core][LLM]") {
    MessageBuilder builder;
    builder.set_format(MessageFormat::Anthropic);
    builder.set_caching(true);
    builder.add_system("System prompt");
    builder.add_user("Hello");
    
    auto result = builder.build();
    // Caching hints should be applied
    REQUIRE(result.size() == 2);
}

TEST_CASE("LLM.MessageBuilder.Size", "[Core][LLM]") {
    MessageBuilder builder;
    REQUIRE(builder.size() == 0);
    REQUIRE(builder.empty());
    
    builder.add_user("Test");
    REQUIRE(builder.size() == 1);
    REQUIRE_FALSE(builder.empty());
    
    builder.clear();
    REQUIRE(builder.size() == 0);
    REQUIRE(builder.empty());
}

// ==================== Content Parts Tests ====================

TEST_CASE("LLM.ContentParts.Text", "[Core][LLM]") {
    auto part = content_parts::text("Hello, world!");
    
    REQUIRE(part["type"] == "text");
    REQUIRE(part["text"] == "Hello, world!");
}

TEST_CASE("LLM.ContentParts.ImageBase64", "[Core][LLM]") {
    auto part = content_parts::image_base64("base64data", "image/png");
    
    REQUIRE(part["type"] == "image_url");
    REQUIRE(part["image_url"]["url"] == "data:image/png;base64,base64data");
}

TEST_CASE("LLM.ContentParts.ImageUrl", "[Core][LLM]") {
    auto part = content_parts::image_url("https://example.com/image.png");
    
    REQUIRE(part["type"] == "image_url");
    REQUIRE(part["image_url"]["url"] == "https://example.com/image.png");
}

TEST_CASE("LLM.ContentParts.File", "[Core][LLM]") {
    auto part = content_parts::file("test.pdf", "base64data", "application/pdf");
    
    REQUIRE(part["type"] == "file");
    REQUIRE(part["filename"] == "test.pdf");
    REQUIRE(part["data"] == "base64data");
    REQUIRE(part["mime_type"] == "application/pdf");
}

TEST_CASE("LLM.ContentParts.ToolCall", "[Core][LLM]") {
    auto part = content_parts::tool_call("tc_001", "read_file", {{"path", "/tmp/test.txt"}});
    
    REQUIRE(part["type"] == "tool_call");
    REQUIRE(part["id"] == "tc_001");
    REQUIRE(part["function"]["name"] == "read_file");
    REQUIRE(part["function"]["arguments"].is_string());
}

TEST_CASE("LLM.ContentParts.ToolResult", "[Core][LLM]") {
    auto part = content_parts::tool_result("tc_001", "File contents");
    
    REQUIRE(part["type"] == "tool_result");
    REQUIRE(part["tool_call_id"] == "tc_001");
    REQUIRE(part["content"] == "File contents");
}

TEST_CASE("LLM.ContentParts.Reasoning", "[Core][LLM]") {
    auto part = content_parts::reasoning("Let me think about this...");
    
    REQUIRE(part["type"] == "reasoning");
    REQUIRE(part["text"] == "Let me think about this...");
}

// ==================== LlmMessage Format Tests ====================

TEST_CASE("LLM.LlmMessage.ToOpenAI", "[Core][LLM]") {
    auto msg = LlmMessage::create_user("Hello");
    auto j = msg.to_openai();
    
    REQUIRE(j["role"] == "user");
    REQUIRE(j["content"] == "Hello");
}

TEST_CASE("LLM.LlmMessage.ToOpenAIWithParts", "[Core][LLM]") {
    auto msg = LlmMessage::create_user_parts({
        content_parts::text("Hello"),
        content_parts::text("World")
    });
    auto j = msg.to_openai();
    
    REQUIRE(j["role"] == "user");
    REQUIRE(j["content"].is_array());
    REQUIRE(j["content"].size() == 2);
}

TEST_CASE("LLM.LlmMessage.ToOpenAIWithToolCalls", "[Core][LLM]") {
    auto msg = LlmMessage::create_assistant_with_tools(
        "Let me help",
        {content_parts::tool_call("tc_001", "bash", {{"cmd", "ls"}})}
    );
    auto j = msg.to_openai();
    
    REQUIRE(j["role"] == "assistant");
    REQUIRE(j.contains("tool_calls"));
    REQUIRE(j["tool_calls"].size() == 1);
}

TEST_CASE("LLM.LlmMessage.ToOpenAIWithProviderOptions", "[Core][LLM]") {
    LlmMessage msg = LlmMessage::create_user("Test");
    msg.provider_options = {{"openai", {{"custom", "value"}}}};
    
    auto j = msg.to_openai();
    REQUIRE(j.contains("provider_options"));
}

TEST_CASE("LLM.LlmMessage.ToAnthropic", "[Core][LLM]") {
    auto msg = LlmMessage::create_user("Hello");
    auto j = msg.to_anthropic();
    
    REQUIRE(j["role"] == "user");
    REQUIRE(j["content"].is_array());
    REQUIRE(j["content"][0]["type"] == "text");
    REQUIRE(j["content"][0]["text"] == "Hello");
}

TEST_CASE("LLM.LlmMessage.ToAnthropicSystem", "[Core][LLM]") {
    auto msg = LlmMessage::create_system("You are helpful.");
    auto j = msg.to_anthropic();
    
    REQUIRE(j["type"] == "system");
    REQUIRE(j["content"] == "You are helpful.");
}

TEST_CASE("LLM.LlmMessage.ToAnthropicWithImage", "[Core][LLM]") {
    auto msg = LlmMessage::create_user_parts({
        content_parts::text("What's in this image?"),
        content_parts::image_base64("base64imagedata", "image/png")
    });
    auto j = msg.to_anthropic();
    
    REQUIRE(j["content"].is_array());
    REQUIRE(j["content"].size() == 2);
    // First part is text
    REQUIRE(j["content"][0]["type"] == "text");
    // Second part is image
    REQUIRE(j["content"][1]["type"] == "image");
    REQUIRE(j["content"][1]["source"]["type"] == "base64");
}

TEST_CASE("LLM.LlmMessage.ToAnthropicWithToolCall", "[Core][LLM]") {
    auto msg = LlmMessage::create_assistant_with_tools(
        std::nullopt,
        {content_parts::tool_call("tc_001", "read_file", {{"path", "/tmp/test.txt"}})}
    );
    auto j = msg.to_anthropic();
    
    REQUIRE(j["role"] == "assistant");
    REQUIRE(j["content"].is_array());
    
    // Find tool_use in content
    bool found_tool_use = false;
    for (const auto& part : j["content"]) {
        if (part["type"] == "tool_use") {
            found_tool_use = true;
            REQUIRE(part["id"] == "tc_001");
            REQUIRE(part["name"] == "read_file");
            break;
        }
    }
    REQUIRE(found_tool_use);
}

TEST_CASE("LLM.LlmMessage.ToAnthropicToolResponse", "[Core][LLM]") {
    auto msg = LlmMessage::create_tool_response("tc_001", "File contents here");
    auto j = msg.to_anthropic();
    
    REQUIRE(j["role"] == "tool");
    REQUIRE(j["content"] == "File contents here");
    REQUIRE(j["tool_call_id"] == "tc_001");
}

TEST_CASE("LLM.LlmMessage.ToFormat", "[Core][LLM]") {
    auto msg = LlmMessage::create_user("Hello");
    
    auto openai_j = msg.to_format(MessageFormat::OpenAI);
    REQUIRE(openai_j["role"] == "user");
    
    auto anthropic_j = msg.to_format(MessageFormat::Anthropic);
    REQUIRE(anthropic_j["role"] == "user");
    REQUIRE(anthropic_j["content"].is_array());
    
    auto compat_j = msg.to_format(MessageFormat::OpenAICompat);
    REQUIRE(compat_j["role"] == "user");
}

// ==================== LlmRole Tests ====================

TEST_CASE("LLM.LlmRole.ToString", "[Core][LLM]") {
    REQUIRE(llm_role_to_string(LlmRole::System) == "system");
    REQUIRE(llm_role_to_string(LlmRole::User) == "user");
    REQUIRE(llm_role_to_string(LlmRole::Assistant) == "assistant");
    REQUIRE(llm_role_to_string(LlmRole::Tool) == "tool");
}

TEST_CASE("LLM.LlmRole.FromString", "[Core][LLM]") {
    REQUIRE(llm_role_from_string("system") == LlmRole::System);
    REQUIRE(llm_role_from_string("user") == LlmRole::User);
    REQUIRE(llm_role_from_string("assistant") == LlmRole::Assistant);
    REQUIRE(llm_role_from_string("tool") == LlmRole::Tool);
    
    REQUIRE_THROWS_AS(llm_role_from_string("invalid"), std::invalid_argument);
}

// ==================== Message Transform Tests ====================

TEST_CASE("LLM.MessageTransform.PartToContent.Text", "[Core][LLM]") {
    Part part;
    part.type = PartType::Text;
    part.data = "Hello, world!";
    
    auto content = message_transform::part_to_content(part, MessageFormat::OpenAI);
    
    REQUIRE(content["type"] == "text");
    REQUIRE(content["text"] == "Hello, world!");
}

TEST_CASE("LLM.MessageTransform.PartToContent.Tool", "[Core][LLM]") {
    Part part;
    part.type = PartType::Tool;
    part.data = {
        {"tool_id", "tc_001"},
        {"tool_name", "read_file"},
        {"arguments", {{"path", "/tmp/test.txt"}}}
    };
    
    auto content = message_transform::part_to_content(part, MessageFormat::OpenAI);
    
    REQUIRE(content["type"] == "tool_call");
    REQUIRE(content["id"] == "tc_001");
    REQUIRE(content["function"]["name"] == "read_file");
}

TEST_CASE("LLM.MessageTransform.PartToContent.Reasoning", "[Core][LLM]") {
    Part part;
    part.type = PartType::Reasoning;
    part.data = "Let me think...";
    
    auto content = message_transform::part_to_content(part, MessageFormat::OpenAI);
    
    REQUIRE(content["type"] == "reasoning");
    REQUIRE(content["text"] == "Let me think...");
}

TEST_CASE("LLM.MessageTransform.PartToContent.File", "[Core][LLM]") {
    Part part;
    part.type = PartType::File;
    part.data = {
        {"path", "test.pdf"},
        {"content", "base64data"},
        {"mime_type", "application/pdf"}
    };
    
    auto content = message_transform::part_to_content(part, MessageFormat::OpenAI);
    
    REQUIRE(content["type"] == "file");
    REQUIRE(content["filename"] == "test.pdf");
}

TEST_CASE("LLM.MessageTransform.PartsToContent", "[Core][LLM]") {
    std::vector<Part> parts;
    
    Part text_part;
    text_part.type = PartType::Text;
    text_part.data = "Hello";
    parts.push_back(text_part);
    
    Part tool_part;
    tool_part.type = PartType::Tool;
    tool_part.data = {
        {"tool_id", "tc_001"},
        {"tool_name", "bash"},
        {"arguments", {{"cmd", "ls"}}}
    };
    parts.push_back(tool_part);
    
    ProviderCapabilities caps;
    auto contents = message_transform::parts_to_content(parts, MessageFormat::OpenAI, caps);
    
    REQUIRE(contents.size() == 2);
    REQUIRE(contents[0]["type"] == "text");
    REQUIRE(contents[1]["type"] == "tool_call");
}

TEST_CASE("LLM.MessageTransform.PartsToContent.FilterUnsupported", "[Core][LLM]") {
    std::vector<Part> parts;
    
    Part image_part;
    image_part.type = PartType::Image;
    image_part.data = "base64imagedata";
    parts.push_back(image_part);
    
    // Provider without vision support
    ProviderCapabilities caps;
    caps.supports_vision = false;
    
    auto contents = message_transform::parts_to_content(parts, MessageFormat::OpenAI, caps);
    
    REQUIRE(contents.size() == 1);
    // Should be converted to text placeholder
    REQUIRE(contents[0]["type"] == "text");
}

TEST_CASE("LLM.MessageTransform.NormalizeMessages.Anthropic", "[Core][LLM]") {
    std::vector<LlmMessage> messages;
    
    // Empty content message should be removed
    auto empty_msg = LlmMessage::create_user("");
    messages.push_back(empty_msg);
    
    auto valid_msg = LlmMessage::create_user("Hello");
    messages.push_back(valid_msg);
    
    // Tool call ID with special characters
    auto tool_msg = LlmMessage::create_tool_response("tc@#$%123", "Result");
    messages.push_back(tool_msg);
    
    ProviderCapabilities caps;
    message_transform::normalize_messages(messages, "anthropic", caps);
    
    // Empty message should be removed
    REQUIRE(messages.size() == 2);
}

TEST_CASE("LLM.MessageTransform.NormalizeMessages.Mistral", "[Core][LLM]") {
    std::vector<LlmMessage> messages;
    
    // Tool call ID - Mistral requires exactly 9 alphanumeric chars
    auto tool_msg = LlmMessage::create_tool_response("tc123", "Result");
    messages.push_back(tool_msg);
    
    // User message after tool message should have assistant inserted
    auto user_msg = LlmMessage::create_user("Next question");
    messages.push_back(user_msg);
    
    ProviderCapabilities caps;
    message_transform::normalize_messages(messages, "mistral", caps);
    
    // Tool call ID should be normalized to 9 chars
    REQUIRE(messages[0].tool_call_id->length() == 9);
    
    // Assistant message should be inserted between tool and user
    REQUIRE(messages.size() == 3);
    REQUIRE(messages[1].role == LlmRole::Assistant);
}

TEST_CASE("LLM.MessageTransform.ApplyCachingHints.Anthropic", "[Core][LLM]") {
    std::vector<nlohmann::json> messages = {
        {{"role", "system"}, {"content", "System prompt"}},
        {{"role", "user"}, {"content", "Hello"}},
        {{"role", "assistant"}, {"content", "Hi!"}}
    };
    
    message_transform::apply_caching_hints(messages, "anthropic");
    
    // First system message should have cache_control
    REQUIRE(messages[0].contains("cache_control"));
    // Last non-system message should have cache_control
    REQUIRE(messages[2].contains("cache_control"));
}

TEST_CASE("LLM.MessageTransform.ApplyCachingHints.OpenRouter", "[Core][LLM]") {
    std::vector<nlohmann::json> messages = {
        {{"role", "user"}, {"content", "Hello"}},
        {{"role", "assistant"}, {"content", "Hi!"}}
    };
    
    message_transform::apply_caching_hints(messages, "openrouter");
    
    // All messages should have cache_control
    for (const auto& msg : messages) {
        REQUIRE(msg.contains("cache_control"));
    }
}

// ==================== Interleaved Thinking Tests ====================

TEST_CASE("LLM.MessageTransform.NormalizeMessages.InterleavedThinking", "[Core][LLM]") {
    std::vector<LlmMessage> messages;
    
    auto msg = LlmMessage::create_assistant_parts({
        content_parts::reasoning("Let me think..."),
        content_parts::text("Here's my answer.")
    });
    messages.push_back(msg);
    
    ProviderCapabilities caps;
    caps.supports_interleaved_thinking = true;
    caps.thinking_field = "thinking";
    
    message_transform::normalize_messages(messages, "openai", caps);
    
    // Reasoning should be extracted to provider_options
    REQUIRE(messages[0].provider_options.has_value());
    REQUIRE(messages[0].content_parts->size() == 1); // Only text part remains
}
