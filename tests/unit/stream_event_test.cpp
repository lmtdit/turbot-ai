#include <turbot/core/llm/stream_event.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>

using namespace turbot::core;

TEST_CASE("StreamEventType conversion", "[stream_event][type]") {
    SECTION("to_string") {
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
        // v2.0 Source events
        REQUIRE(stream_event_type_to_string(StreamEventType::SourceStart) == "source-start");
        REQUIRE(stream_event_type_to_string(StreamEventType::SourceEnd) == "source-end");
    }

    SECTION("from_string") {
        REQUIRE(stream_event_type_from_string("start") == StreamEventType::Start);
        REQUIRE(stream_event_type_from_string("finish") == StreamEventType::Finish);
        REQUIRE(stream_event_type_from_string("error") == StreamEventType::Error);
        REQUIRE(stream_event_type_from_string("text-start") == StreamEventType::TextStart);
        REQUIRE(stream_event_type_from_string("text-delta") == StreamEventType::TextDelta);
        REQUIRE(stream_event_type_from_string("text-end") == StreamEventType::TextEnd);
        REQUIRE(stream_event_type_from_string("reasoning-start") == StreamEventType::ReasoningStart);
        REQUIRE(stream_event_type_from_string("reasoning-delta") == StreamEventType::ReasoningDelta);
        REQUIRE(stream_event_type_from_string("reasoning-end") == StreamEventType::ReasoningEnd);
        REQUIRE(stream_event_type_from_string("tool-input-start") == StreamEventType::ToolInputStart);
        REQUIRE(stream_event_type_from_string("tool-input-delta") == StreamEventType::ToolInputDelta);
        REQUIRE(stream_event_type_from_string("tool-input-end") == StreamEventType::ToolInputEnd);
        REQUIRE(stream_event_type_from_string("tool-call") == StreamEventType::ToolCall);
        REQUIRE(stream_event_type_from_string("step-start") == StreamEventType::StepStart);
        REQUIRE(stream_event_type_from_string("step-finish") == StreamEventType::StepFinish);
        // v2.0 Source events
        REQUIRE(stream_event_type_from_string("source-start") == StreamEventType::SourceStart);
        REQUIRE(stream_event_type_from_string("source-end") == StreamEventType::SourceEnd);
    }

    SECTION("from_string unknown returns text-delta") {
        REQUIRE(stream_event_type_from_string("unknown") == StreamEventType::TextDelta);
        REQUIRE(stream_event_type_from_string("") == StreamEventType::TextDelta);
        REQUIRE(stream_event_type_from_string("invalid-type") == StreamEventType::TextDelta);
    }

    SECTION("round-trip conversion") {
        // Test all types round-trip correctly
        for (auto type : {StreamEventType::Start, StreamEventType::Finish, StreamEventType::Error,
                          StreamEventType::TextStart, StreamEventType::TextDelta, StreamEventType::TextEnd,
                          StreamEventType::ReasoningStart, StreamEventType::ReasoningDelta, StreamEventType::ReasoningEnd,
                          StreamEventType::ToolInputStart, StreamEventType::ToolInputDelta, StreamEventType::ToolInputEnd,
                          StreamEventType::ToolCall, StreamEventType::StepStart, StreamEventType::StepFinish,
                          StreamEventType::SourceStart, StreamEventType::SourceEnd}) {
            auto str = stream_event_type_to_string(type);
            REQUIRE(stream_event_type_from_string(str) == type);
        }
    }
}

TEST_CASE("FinishReason conversion", "[stream_event][finish]") {
    SECTION("to_string") {
        REQUIRE(finish_reason_to_string(FinishReason::Stop) == "stop");
        REQUIRE(finish_reason_to_string(FinishReason::Length) == "length");
        REQUIRE(finish_reason_to_string(FinishReason::ToolCall) == "tool-call");
        REQUIRE(finish_reason_to_string(FinishReason::ContentFilter) == "content-filter");
        REQUIRE(finish_reason_to_string(FinishReason::Error) == "error");
        REQUIRE(finish_reason_to_string(FinishReason::Other) == "other");
    }

    SECTION("from_string") {
        REQUIRE(finish_reason_from_string("stop") == FinishReason::Stop);
        REQUIRE(finish_reason_from_string("length") == FinishReason::Length);
        REQUIRE(finish_reason_from_string("tool-call") == FinishReason::ToolCall);
        REQUIRE(finish_reason_from_string("content-filter") == FinishReason::ContentFilter);
        REQUIRE(finish_reason_from_string("error") == FinishReason::Error);
        REQUIRE(finish_reason_from_string("other") == FinishReason::Other);
    }

    SECTION("from_string unknown returns other") {
        REQUIRE(finish_reason_from_string("unknown") == FinishReason::Other);
    }
}

TEST_CASE("ToolCallChunk serialization", "[stream_event][tool_call]") {
    SECTION("to_json") {
        ToolCallChunk chunk{"call_123", "search", R"({"query": "test"})", true};
        auto j = chunk.to_json();

        REQUIRE(j["id"] == "call_123");
        REQUIRE(j["name"] == "search");
        REQUIRE(j["arguments"] == R"({"query": "test"})");
        REQUIRE(j["is_complete"] == true);
    }

    SECTION("from_json") {
        nlohmann::json j = {
            {"id", "call_456"},
            {"name", "execute"},
            {"arguments", R"({"cmd": "ls"})"},
            {"is_complete", false}
        };

        auto chunk = ToolCallChunk::from_json(j);
        REQUIRE(chunk.id == "call_456");
        REQUIRE(chunk.name == "execute");
        REQUIRE(chunk.arguments == R"({"cmd": "ls"})");
        REQUIRE(chunk.is_complete == false);
    }
}

TEST_CASE("ReasoningMetadata serialization", "[stream_event][reasoning]") {
    SECTION("to_json with all fields") {
        ReasoningMetadata meta;
        meta.encrypted_content = "encrypted_data";
        meta.budget_tokens = 16000;
        meta.used_tokens = 5000;

        auto j = meta.to_json();
        REQUIRE(j["encrypted_content"] == "encrypted_data");
        REQUIRE(j["budget_tokens"] == 16000);
        REQUIRE(j["used_tokens"] == 5000);
    }

    SECTION("to_json with empty fields") {
        ReasoningMetadata meta;
        auto j = meta.to_json();
        REQUIRE_FALSE(j.contains("encrypted_content"));
        REQUIRE_FALSE(j.contains("budget_tokens"));
        REQUIRE_FALSE(j.contains("used_tokens"));
    }

    SECTION("from_json") {
        nlohmann::json j = {
            {"encrypted_content", "test_data"},
            {"budget_tokens", 8000}
        };

        auto meta = ReasoningMetadata::from_json(j);
        REQUIRE(meta.encrypted_content == "test_data");
        REQUIRE(meta.budget_tokens == 8000);
        REQUIRE_FALSE(meta.used_tokens.has_value());
    }
}

TEST_CASE("SourceInfo serialization", "[stream_event][source]") {
    SECTION("to_json") {
        SourceInfo info;
        info.id = "src_123";
        info.type = "document";
        info.title = "API Documentation";
        info.url = "https://example.com/docs";

        auto j = info.to_json();
        REQUIRE(j["id"] == "src_123");
        REQUIRE(j["type"] == "document");
        REQUIRE(j["title"] == "API Documentation");
        REQUIRE(j["url"] == "https://example.com/docs");
    }

    SECTION("from_json") {
        nlohmann::json j = {
            {"id", "src_456"},
            {"type", "url"},
            {"filename", "report.pdf"}
        };

        auto info = SourceInfo::from_json(j);
        REQUIRE(info.id == "src_456");
        REQUIRE(info.type == "url");
        REQUIRE_FALSE(info.title.has_value());
        REQUIRE(info.filename == "report.pdf");
    }
}

TEST_CASE("StreamEvent factory methods", "[stream_event][factory]") {
    SECTION("create_start") {
        auto event = StreamEvent::create_start();
        REQUIRE(event.is_start());
        REQUIRE(event.type == StreamEventType::Start);
        REQUIRE(event.timestamp > 0);
    }

    SECTION("create_finish") {
        TokenUsage usage{100, 50, 20, {0, 0}};
        auto event = StreamEvent::create_finish(FinishReason::Stop, usage);
        REQUIRE(event.is_finish());
        REQUIRE(event.finish_reason == FinishReason::Stop);
        REQUIRE(event.usage.has_value());
        REQUIRE(event.usage->input == 100);
        REQUIRE(event.usage->output == 50);
    }

    SECTION("create_error") {
        auto event = StreamEvent::create_error("Something went wrong", "E001");
        REQUIRE(event.is_error());
        REQUIRE(event.error_message == "Something went wrong");
        REQUIRE(event.error_code == "E001");
    }

    SECTION("create_text_start") {
        auto event = StreamEvent::create_text_start("txt_123");
        REQUIRE(event.is_text_event());
        REQUIRE(event.type == StreamEventType::TextStart);
        REQUIRE(event.id == "txt_123");
    }

    SECTION("create_text_delta") {
        auto event = StreamEvent::create_text_delta("txt_123", "Hello");
        REQUIRE(event.type == StreamEventType::TextDelta);
        REQUIRE(event.id == "txt_123");
        REQUIRE(event.delta == "Hello");
    }

    SECTION("create_text_end") {
        auto event = StreamEvent::create_text_end("txt_123");
        REQUIRE(event.type == StreamEventType::TextEnd);
        REQUIRE(event.id == "txt_123");
    }

    SECTION("create_reasoning_start") {
        auto event = StreamEvent::create_reasoning_start("rsn_123");
        REQUIRE(event.is_reasoning_event());
        REQUIRE(event.type == StreamEventType::ReasoningStart);
    }

    SECTION("create_reasoning_delta") {
        auto event = StreamEvent::create_reasoning_delta("rsn_123", "Thinking...");
        REQUIRE(event.type == StreamEventType::ReasoningDelta);
        REQUIRE(event.delta == "Thinking...");
    }

    SECTION("create_reasoning_end") {
        auto event = StreamEvent::create_reasoning_end("rsn_123");
        REQUIRE(event.type == StreamEventType::ReasoningEnd);
    }

    SECTION("create_tool_input_start") {
        auto event = StreamEvent::create_tool_input_start("tool_123", "search");
        REQUIRE(event.is_tool_event());
        REQUIRE(event.type == StreamEventType::ToolInputStart);
        REQUIRE(event.tool_call.has_value());
        REQUIRE(event.tool_call->name == "search");
    }

    SECTION("create_tool_input_delta") {
        auto event = StreamEvent::create_tool_input_delta("tool_123", R"({"query": ")");
        REQUIRE(event.type == StreamEventType::ToolInputDelta);
        REQUIRE(event.delta == R"({"query": ")");
    }

    SECTION("create_tool_input_end") {
        auto event = StreamEvent::create_tool_input_end("tool_123");
        REQUIRE(event.type == StreamEventType::ToolInputEnd);
    }

    SECTION("create_tool_call") {
        ToolCallChunk chunk{"tool_456", "execute", R"({"cmd": "ls"})", true};
        auto event = StreamEvent::create_tool_call(chunk);
        REQUIRE(event.type == StreamEventType::ToolCall);
        REQUIRE(event.tool_call.has_value());
        REQUIRE(event.tool_call->is_complete == true);
    }

    SECTION("create_step_start") {
        auto event = StreamEvent::create_step_start("step_123", "Analysis");
        REQUIRE(event.is_step_event());
        REQUIRE(event.type == StreamEventType::StepStart);
        REQUIRE(event.delta == "Analysis");
    }

    SECTION("create_step_finish") {
        nlohmann::json result = {{"status", "completed"}};
        auto event = StreamEvent::create_step_finish("step_123", result);
        REQUIRE(event.type == StreamEventType::StepFinish);
        REQUIRE(event.provider_metadata.has_value());
    }
}

TEST_CASE("StreamEvent type checking", "[stream_event][type_check]") {
    SECTION("is_text_event") {
        REQUIRE(StreamEvent::create_text_start("id").is_text_event());
        REQUIRE(StreamEvent::create_text_delta("id", "x").is_text_event());
        REQUIRE(StreamEvent::create_text_end("id").is_text_event());
        REQUIRE_FALSE(StreamEvent::create_reasoning_start("id").is_text_event());
    }

    SECTION("is_reasoning_event") {
        REQUIRE(StreamEvent::create_reasoning_start("id").is_reasoning_event());
        REQUIRE(StreamEvent::create_reasoning_delta("id", "x").is_reasoning_event());
        REQUIRE(StreamEvent::create_reasoning_end("id").is_reasoning_event());
        REQUIRE_FALSE(StreamEvent::create_text_start("id").is_reasoning_event());
    }

    SECTION("is_tool_event") {
        REQUIRE(StreamEvent::create_tool_input_start("id", "name").is_tool_event());
        REQUIRE(StreamEvent::create_tool_input_delta("id", "x").is_tool_event());
        REQUIRE(StreamEvent::create_tool_input_end("id").is_tool_event());
        ToolCallChunk chunk{"id", "name", "{}", true};
        REQUIRE(StreamEvent::create_tool_call(chunk).is_tool_event());
        REQUIRE_FALSE(StreamEvent::create_text_start("id").is_tool_event());
    }

    SECTION("is_step_event") {
        REQUIRE(StreamEvent::create_step_start("id").is_step_event());
        REQUIRE(StreamEvent::create_step_finish("id").is_step_event());
        REQUIRE_FALSE(StreamEvent::create_text_start("id").is_step_event());
    }
}

TEST_CASE("StreamEvent serialization", "[stream_event][serialize]") {
    SECTION("text-delta roundtrip") {
        auto original = StreamEvent::create_text_delta("txt_123", "Hello world");
        auto j = original.to_json();
        auto restored = StreamEvent::from_json(j);

        REQUIRE(restored.type == StreamEventType::TextDelta);
        REQUIRE(restored.id == "txt_123");
        REQUIRE(restored.delta == "Hello world");
    }

    SECTION("finish event roundtrip") {
        TokenUsage usage{100, 50, 10, {0, 0}};
        auto original = StreamEvent::create_finish(FinishReason::ToolCall, usage);
        auto j = original.to_json();
        auto restored = StreamEvent::from_json(j);

        REQUIRE(restored.type == StreamEventType::Finish);
        REQUIRE(restored.finish_reason == FinishReason::ToolCall);
        REQUIRE(restored.usage.has_value());
        REQUIRE(restored.usage->input == 100);
        REQUIRE(restored.usage->output == 50);
    }

    SECTION("tool event with all fields") {
        auto event = StreamEvent::create_tool_input_start("tool_123", "search");
        event.tool_call->arguments = R"({"query": "test"})";
        event.provider_metadata = {{"provider", "openai"}};

        auto j = event.to_json();
        auto restored = StreamEvent::from_json(j);

        REQUIRE(restored.type == StreamEventType::ToolInputStart);
        REQUIRE(restored.tool_call.has_value());
        REQUIRE(restored.tool_call->name == "search");
        REQUIRE(restored.provider_metadata.has_value());
    }

    SECTION("error event roundtrip") {
        auto original = StreamEvent::create_error("Rate limit exceeded", "RATE_LIMIT");
        auto j = original.to_json();
        auto restored = StreamEvent::from_json(j);

        REQUIRE(restored.type == StreamEventType::Error);
        REQUIRE(restored.error_message == "Rate limit exceeded");
        REQUIRE(restored.error_code == "RATE_LIMIT");
    }
}

TEST_CASE("StreamResult text aggregation", "[stream_event][result]") {
    StreamResult result;
    result.events = {
        StreamEvent::create_text_start("txt_1"),
        StreamEvent::create_text_delta("txt_1", "Hello "),
        StreamEvent::create_text_delta("txt_1", "world"),
        StreamEvent::create_text_end("txt_1")
    };

    SECTION("get_text") {
        REQUIRE(result.get_text() == "Hello world");
    }

    SECTION("get_reasoning returns empty") {
        REQUIRE(result.get_reasoning().empty());
    }

    SECTION("get_tool_calls returns empty") {
        REQUIRE(result.get_tool_calls().empty());
    }
}

TEST_CASE("StreamResult reasoning aggregation", "[stream_event][result]") {
    StreamResult result;
    result.events = {
        StreamEvent::create_reasoning_start("rsn_1"),
        StreamEvent::create_reasoning_delta("rsn_1", "Step 1: "),
        StreamEvent::create_reasoning_delta("rsn_1", "Analyze"),
        StreamEvent::create_reasoning_end("rsn_1"),
        StreamEvent::create_text_start("txt_1"),
        StreamEvent::create_text_delta("txt_1", "Result"),
        StreamEvent::create_text_end("txt_1")
    };

    SECTION("get_reasoning") {
        REQUIRE(result.get_reasoning() == "Step 1: Analyze");
    }

    SECTION("get_text") {
        REQUIRE(result.get_text() == "Result");
    }
}

TEST_CASE("StreamResult tool calls aggregation", "[stream_event][result]") {
    StreamResult result;

    // Simulate streaming tool call
    result.events = {
        StreamEvent::create_tool_input_start("tool_1", "search"),
        StreamEvent::create_tool_input_delta("tool_1", R"({"qu)"),
        StreamEvent::create_tool_input_delta("tool_1", R"(ery": "test"})"),
        StreamEvent::create_tool_input_end("tool_1")
    };

    SECTION("get_tool_calls aggregates chunks") {
        auto tools = result.get_tool_calls();
        REQUIRE(tools.size() == 1);
        REQUIRE(tools[0].id == "tool_1");
        REQUIRE(tools[0].name == "search");
        REQUIRE(tools[0].arguments == R"({"query": "test"})");
        REQUIRE(tools[0].is_complete == true);
    }
}

TEST_CASE("StreamResult multiple tool calls", "[stream_event][result]") {
    StreamResult result;

    // Multiple concurrent tool calls
    result.events = {
        StreamEvent::create_tool_input_start("tool_1", "search"),
        StreamEvent::create_tool_input_delta("tool_1", R"({"q1")"),
        StreamEvent::create_tool_input_start("tool_2", "execute"),
        StreamEvent::create_tool_input_delta("tool_1", R"(: "a"})"),
        StreamEvent::create_tool_input_end("tool_1"),
        StreamEvent::create_tool_input_delta("tool_2", R"({"cmd": "ls"})"),
        StreamEvent::create_tool_input_end("tool_2")
    };

    auto tools = result.get_tool_calls();
    REQUIRE(tools.size() == 2);

    // Find by id
    auto find_tool = [&](const std::string& id) -> const ToolCallChunk* {
        for (const auto& t : tools) {
            if (t.id == id) return &t;
        }
        return nullptr;
    };

    auto t1 = find_tool("tool_1");
    REQUIRE(t1 != nullptr);
    REQUIRE(t1->name == "search");
    REQUIRE(t1->arguments == R"({"q1": "a"})");

    auto t2 = find_tool("tool_2");
    REQUIRE(t2 != nullptr);
    REQUIRE(t2->name == "execute");
    REQUIRE(t2->arguments == R"({"cmd": "ls"})");
}

TEST_CASE("StreamResult serialization", "[stream_event][result]") {
    StreamResult original;
    original.id = "resp_123";
    original.model = "gpt-4";
    original.finish_reason = FinishReason::Stop;
    original.usage = TokenUsage{100, 50, 10, {0, 0}};
    original.events = {
        StreamEvent::create_text_delta("txt_1", "Hello"),
        StreamEvent::create_finish(FinishReason::Stop, TokenUsage{100, 50, 10, {0, 0}})
    };

    auto j = original.to_json();
    auto restored = StreamResult::from_json(j);

    REQUIRE(restored.id == "resp_123");
    REQUIRE(restored.model == "gpt-4");
    REQUIRE(restored.finish_reason == FinishReason::Stop);
    REQUIRE(restored.usage.input == 100);
    REQUIRE(restored.events.size() == 2);
}

TEST_CASE("StreamResult with error", "[stream_event][result]") {
    StreamResult result;
    result.id = "resp_err";
    result.error = "Connection timeout";

    auto j = result.to_json();
    auto restored = StreamResult::from_json(j);

    REQUIRE(restored.error.has_value());
    REQUIRE(*restored.error == "Connection timeout");
}

// ===== SourceInfo Tests (v2.0) =====

TEST_CASE("SourceInfo full serialization", "[stream_event][source]") {
    SECTION("Basic source info") {
        SourceInfo source;
        source.id = "src_123";
        source.type = "document";
        source.title = "API Documentation";
        source.url = "https://example.com/docs";

        auto j = source.to_json();
        auto restored = SourceInfo::from_json(j);

        REQUIRE(restored.id == "src_123");
        REQUIRE(restored.type == "document");
        REQUIRE(restored.title.has_value());
        REQUIRE(*restored.title == "API Documentation");
        REQUIRE(restored.url.has_value());
        REQUIRE(*restored.url == "https://example.com/docs");
    }

    SECTION("Minimal source info") {
        SourceInfo source;
        source.id = "src_min";
        source.type = "url";

        auto j = source.to_json();
        REQUIRE(j["id"] == "src_min");
        REQUIRE(j["type"] == "url");
        REQUIRE_FALSE(j.contains("title"));
        REQUIRE_FALSE(j.contains("url"));
    }

    SECTION("Source info with filename") {
        SourceInfo source;
        source.id = "src_file";
        source.type = "file";
        source.filename = "report.pdf";

        auto j = source.to_json();
        auto restored = SourceInfo::from_json(j);

        REQUIRE(restored.filename.has_value());
        REQUIRE(*restored.filename == "report.pdf");
    }
}

TEST_CASE("StreamEvent with source field", "[stream_event][source]") {
    SECTION("Event with source info") {
        auto event = StreamEvent::create_text_delta("txt_1", "Cited content");
        SourceInfo source{"src_1", "document", "Reference Doc", "https://example.com", std::nullopt};
        event.source = source;

        auto j = event.to_json();
        auto restored = StreamEvent::from_json(j);

        REQUIRE(restored.source.has_value());
        REQUIRE(restored.source->id == "src_1");
        REQUIRE(restored.source->type == "document");
        REQUIRE(restored.source->title == "Reference Doc");
    }

    SECTION("Event type checks") {
        auto text_event = StreamEvent::create_text_delta("t1", "text");
        REQUIRE(text_event.is_text_event());
        REQUIRE_FALSE(text_event.is_reasoning_event());
        REQUIRE_FALSE(text_event.is_tool_event());

        auto reasoning_event = StreamEvent::create_reasoning_delta("r1", "thought");
        REQUIRE(reasoning_event.is_reasoning_event());
        REQUIRE_FALSE(reasoning_event.is_text_event());

        auto tool_event = StreamEvent::create_tool_input_start("tl1", "search");
        REQUIRE(tool_event.is_tool_event());
        REQUIRE_FALSE(tool_event.is_text_event());

        auto step_event = StreamEvent::create_step_start("step_1", "process");
        REQUIRE(step_event.is_step_event());
        REQUIRE_FALSE(step_event.is_tool_event());
    }
}

TEST_CASE("FinishReason round-trip", "[stream_event][finish]") {
    SECTION("All finish reasons round-trip") {
        for (auto reason : {FinishReason::Stop, FinishReason::Length, FinishReason::ToolCall,
                           FinishReason::ContentFilter, FinishReason::Error, FinishReason::Other}) {
            auto str = finish_reason_to_string(reason);
            REQUIRE(finish_reason_from_string(str) == reason);
        }
    }
}

// ============================================================================
// Edge cases for enum string conversion
// ============================================================================

TEST_CASE("stream_event_type_to_string unknown value", "[stream_event]") {
    auto unknown_type = static_cast<StreamEventType>(999);
    std::string_view s = stream_event_type_to_string(unknown_type);
    CHECK(s == "unknown");
}

TEST_CASE("finish_reason_to_string unknown value", "[stream_event]") {
    auto unknown_reason = static_cast<FinishReason>(999);
    std::string_view s = finish_reason_to_string(unknown_reason);
    CHECK(s == "unknown");
}
