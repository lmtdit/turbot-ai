#include <catch2/catch_test_macros.hpp>
#include <turbot/core/llm/stream_event.hpp>
#include <turbot/core/llm/llm.hpp>
#include <turbot/core/llm/system_prompt.hpp>
#include <turbot/core/llm/message_builder.hpp>
#include <turbot/core/llm/prompt_builder.hpp>
#include <turbot/core/llm/provider_adapter.hpp>
#include <turbot/core/llm/tool_schema.hpp>
#include <turbot/core/common/version.hpp>

using namespace turbot::core;

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
    TokenUsage usage{100, 50, 150};
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
    result.usage = TokenUsage{100, 50, 150};
    
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
    
    TokenUsage usage{100, 50, 150};
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
