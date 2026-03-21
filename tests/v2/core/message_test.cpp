#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <turbot/core/message/part.hpp>
#include <turbot/core/message/message.hpp>

using namespace turbot::core;
using Catch::Approx;

// ==================== PartType String Conversion Tests ====================

TEST_CASE("Part.PartType.ToString", "[Core][Message]") {
    REQUIRE(part_type_to_string(PartType::Text) == "text");
    REQUIRE(part_type_to_string(PartType::Tool) == "tool");
    REQUIRE(part_type_to_string(PartType::Reasoning) == "reasoning");
    REQUIRE(part_type_to_string(PartType::File) == "file");
    REQUIRE(part_type_to_string(PartType::Image) == "image");
    REQUIRE(part_type_to_string(PartType::Error) == "error");
    REQUIRE(part_type_to_string(PartType::Subtask) == "subtask");
    REQUIRE(part_type_to_string(PartType::StepStart) == "step_start");
    REQUIRE(part_type_to_string(PartType::StepFinish) == "step_finish");
    REQUIRE(part_type_to_string(PartType::Snapshot) == "snapshot");
    REQUIRE(part_type_to_string(PartType::Patch) == "patch");
    REQUIRE(part_type_to_string(PartType::Agent) == "agent");
    REQUIRE(part_type_to_string(PartType::Retry) == "retry");
    REQUIRE(part_type_to_string(PartType::Compaction) == "compaction");
    REQUIRE(part_type_to_string(PartType::Source) == "source");
}

TEST_CASE("Part.PartType.FromString", "[Core][Message]") {
    REQUIRE(part_type_from_string("text") == PartType::Text);
    REQUIRE(part_type_from_string("tool") == PartType::Tool);
    REQUIRE(part_type_from_string("reasoning") == PartType::Reasoning);
    REQUIRE(part_type_from_string("file") == PartType::File);
    REQUIRE(part_type_from_string("image") == PartType::Image);
    REQUIRE(part_type_from_string("error") == PartType::Error);
    REQUIRE(part_type_from_string("subtask") == PartType::Subtask);
    REQUIRE(part_type_from_string("step_start") == PartType::StepStart);
    REQUIRE(part_type_from_string("step_finish") == PartType::StepFinish);
    REQUIRE(part_type_from_string("snapshot") == PartType::Snapshot);
    REQUIRE(part_type_from_string("patch") == PartType::Patch);
    REQUIRE(part_type_from_string("agent") == PartType::Agent);
    REQUIRE(part_type_from_string("retry") == PartType::Retry);
    REQUIRE(part_type_from_string("compaction") == PartType::Compaction);
    REQUIRE(part_type_from_string("source") == PartType::Source);
}

// ==================== Role String Conversion Tests ====================

TEST_CASE("Part.Role.ToString", "[Core][Message]") {
    REQUIRE(role_to_string(Role::User) == "user");
    REQUIRE(role_to_string(Role::Assistant) == "assistant");
    REQUIRE(role_to_string(Role::System) == "system");
    REQUIRE(role_to_string(Role::Tool) == "tool");
}

TEST_CASE("Part.Role.FromString", "[Core][Message]") {
    REQUIRE(role_from_string("user") == Role::User);
    REQUIRE(role_from_string("assistant") == Role::Assistant);
    REQUIRE(role_from_string("system") == Role::System);
    REQUIRE(role_from_string("tool") == Role::Tool);
}

TEST_CASE("Part.Role.FromString.Invalid", "[Core][Message]") {
    // Non-strict mode returns User for invalid input
    REQUIRE(role_from_string("invalid") == Role::User);
    REQUIRE(role_from_string("") == Role::User);
}

// ==================== Part Factory Methods Tests ====================

TEST_CASE("Part.CreateText", "[Core][Message]") {
    Part part = Part::create_text("Hello, world!");
    
    REQUIRE(part.type == PartType::Text);
    REQUIRE(part.is_text());
    REQUIRE(part.get_text() == "Hello, world!");
    REQUIRE_FALSE(part.id.empty());
}

TEST_CASE("Part.CreateText.Empty", "[Core][Message]") {
    Part part = Part::create_text("");
    
    REQUIRE(part.type == PartType::Text);
    REQUIRE(part.get_text().empty());
}

TEST_CASE("Part.CreateTool", "[Core][Message]") {
    nlohmann::json args = {{"path", "/tmp/test"}};
    nlohmann::json result = {{"output", "success"}};
    
    Part part = Part::create_tool("tool_123", "read_file", args, result);
    
    REQUIRE(part.type == PartType::Tool);
    REQUIRE(part.is_tool());
    REQUIRE_FALSE(part.is_text());
    
    auto tool = part.get_tool();
    REQUIRE(tool["tool_id"] == "tool_123");
    REQUIRE(tool["tool_name"] == "read_file");
    REQUIRE(tool["arguments"] == args);
    REQUIRE(tool["result"] == result);
}

TEST_CASE("Part.CreateTool.WithoutResult", "[Core][Message]") {
    nlohmann::json args = {{"query", "test"}};
    
    Part part = Part::create_tool("tool_456", "search", args);
    
    REQUIRE(part.type == PartType::Tool);
    auto tool = part.get_tool();
    REQUIRE(tool["tool_id"] == "tool_456");
    REQUIRE_FALSE(tool.contains("result"));
}

TEST_CASE("Part.CreateReasoning", "[Core][Message]") {
    Part part = Part::create_reasoning("Let me think about this...");
    
    REQUIRE(part.type == PartType::Reasoning);
    REQUIRE(part.is_reasoning());
    REQUIRE(part.get_reasoning() == "Let me think about this...");
}

TEST_CASE("Part.CreateFile", "[Core][Message]") {
    Part part = Part::create_file("/path/to/file.txt", "file content", "text/plain");
    
    REQUIRE(part.type == PartType::File);
    REQUIRE(part.is_file());
    
    auto file = part.get_file();
    REQUIRE(file["path"] == "/path/to/file.txt");
    REQUIRE(file["content"] == "file content");
    REQUIRE(file["mime_type"] == "text/plain");
}

TEST_CASE("Part.CreateFile.WithoutContent", "[Core][Message]") {
    Part part = Part::create_file("/path/to/file.txt");
    
    REQUIRE(part.type == PartType::File);
    auto file = part.get_file();
    REQUIRE(file["path"] == "/path/to/file.txt");
    REQUIRE_FALSE(file.contains("content"));
}

TEST_CASE("Part.CreateImage", "[Core][Message]") {
    Part part = Part::create_image("https://example.com/image.png", "An image", "image/png");
    
    REQUIRE(part.type == PartType::Image);
    REQUIRE(part.is_image());
    
    auto img = part.get_image();
    REQUIRE(img["url"] == "https://example.com/image.png");
    REQUIRE(img["alt_text"] == "An image");
}

TEST_CASE("Part.CreateImageBase64", "[Core][Message]") {
    Part part = Part::create_image_base64("base64data", "image/jpeg", "A JPEG");
    
    REQUIRE(part.type == PartType::Image);
    auto img = part.get_image();
    // base64 data is stored in url field as data URI
    REQUIRE(img["url"] == "data:image/jpeg;base64,base64data");
    REQUIRE(img["mime_type"] == "image/jpeg");
}

TEST_CASE("Part.CreateError", "[Core][Message]") {
    nlohmann::json details = {{"line", 42}};
    Part part = Part::create_error("Something went wrong", "ERR001", details);
    
    REQUIRE(part.type == PartType::Error);
    REQUIRE(part.is_error());
    
    auto err = part.get_error();
    REQUIRE(err["message"] == "Something went wrong");
    REQUIRE(err["code"] == "ERR001");
    REQUIRE(err["details"] == details);
}

TEST_CASE("Part.CreateSource", "[Core][Message]") {
    Part part = Part::create_source("src1", "web", "Wikipedia", "https://wikipedia.org");
    
    REQUIRE(part.type == PartType::Source);
    REQUIRE(part.is_source());
    
    auto src = part.get_source();
    REQUIRE(src["source_id"] == "src1");
    REQUIRE(src["source_type"] == "web");
    REQUIRE(src["title"] == "Wikipedia");
}

TEST_CASE("Part.CreateSubtask", "[Core][Message]") {
    Part part = Part::create_subtask("task_123", "planner", "running");
    
    REQUIRE(part.type == PartType::Subtask);
    REQUIRE(part.is_subtask());
    
    auto subtask = part.get_subtask();
    REQUIRE(subtask["task_id"] == "task_123");
    REQUIRE(subtask["agent"] == "planner");
    REQUIRE(subtask["status"] == "running");
}

TEST_CASE("Part.CreateStepStart", "[Core][Message]") {
    Part part = Part::create_step_start("step_1", "Analysis");
    
    REQUIRE(part.type == PartType::StepStart);
    REQUIRE(part.is_step_start());
    
    auto step = part.get_step();
    REQUIRE(step["step_id"] == "step_1");
    REQUIRE(step["name"] == "Analysis");
}

TEST_CASE("Part.CreateStepFinish", "[Core][Message]") {
    nlohmann::json result = {{"status", "completed"}};
    Part part = Part::create_step_finish("step_1", "success", result);
    
    REQUIRE(part.type == PartType::StepFinish);
    REQUIRE(part.is_step_finish());
    
    auto step = part.get_step();
    REQUIRE(step["step_id"] == "step_1");
    REQUIRE(step["status"] == "success");
}

TEST_CASE("Part.CreateSnapshot", "[Core][Message]") {
    nlohmann::json files = {
        {"file1.txt", "content1"},
        {"file2.txt", "content2"}
    };
    Part part = Part::create_snapshot(files);
    
    REQUIRE(part.type == PartType::Snapshot);
    REQUIRE(part.is_snapshot());
    
    auto snap = part.get_snapshot();
    REQUIRE(snap["file1.txt"] == "content1");
}

TEST_CASE("Part.CreatePatch", "[Core][Message]") {
    nlohmann::json diff = {
        {"old", "hello"},
        {"new", "world"}
    };
    Part part = Part::create_patch("/path/to/file.cpp", diff);
    
    REQUIRE(part.type == PartType::Patch);
    REQUIRE(part.is_patch());
    
    auto patch = part.get_patch();
    REQUIRE(patch["file_path"] == "/path/to/file.cpp");
}

TEST_CASE("Part.CreateAgent", "[Core][Message]") {
    Part part = Part::create_agent("agent_123", "coder", "gpt-4");
    
    REQUIRE(part.type == PartType::Agent);
    REQUIRE(part.is_agent());
    
    auto agent = part.get_agent();
    REQUIRE(agent["agent_id"] == "agent_123");
    REQUIRE(agent["agent_name"] == "coder");
    REQUIRE(agent["model"] == "gpt-4");
}

TEST_CASE("Part.CreateRetry", "[Core][Message]") {
    Part part = Part::create_retry(2, "timeout", 5);
    
    REQUIRE(part.type == PartType::Retry);
    REQUIRE(part.is_retry());
    
    auto retry = part.get_retry();
    REQUIRE(retry["attempt"] == 2);
    REQUIRE(retry["reason"] == "timeout");
    REQUIRE(retry["max_attempts"] == 5);
}

TEST_CASE("Part.CreateCompaction", "[Core][Message]") {
    nlohmann::json summary = {{"key_points", {"point1", "point2"}}};
    Part part = Part::create_compaction(10000, 2000, summary);
    
    REQUIRE(part.type == PartType::Compaction);
    REQUIRE(part.is_compaction());
    
    auto comp = part.get_compaction();
    REQUIRE(comp["original_tokens"] == 10000);
    REQUIRE(comp["compacted_tokens"] == 2000);
}

// ==================== Part Serialization Tests ====================

TEST_CASE("Part.ToJson", "[Core][Message]") {
    Part part = Part::create_text("test content");
    part.message_id = "msg_123";
    part.session_id = "sess_456";
    
    nlohmann::json j = part.to_json();
    
    REQUIRE(j["type"] == "text");
    REQUIRE(j["message_id"] == "msg_123");
    REQUIRE(j["session_id"] == "sess_456");
    REQUIRE(j["data"]["content"] == "test content");
}

TEST_CASE("Part.FromJson", "[Core][Message]") {
    nlohmann::json j = {
        {"id", "part_123"},
        {"message_id", "msg_456"},
        {"session_id", "sess_789"},
        {"type", "text"},
        {"data", {{"content", "hello"}}},
        {"time_created", 1234567890}
    };
    
    Part part = Part::from_json(j);
    
    REQUIRE(part.id == "part_123");
    REQUIRE(part.message_id == "msg_456");
    REQUIRE(part.session_id == "sess_789");
    REQUIRE(part.type == PartType::Text);
    REQUIRE(part.get_text() == "hello");
}

TEST_CASE("Part.RoundTrip", "[Core][Message]") {
    Part original = Part::create_tool("tool_1", "bash", {{"cmd", "ls"}}, {{"output", "files"}});
    original.message_id = "msg_1";
    original.session_id = "sess_1";
    
    nlohmann::json j = original.to_json();
    Part restored = Part::from_json(j);
    
    REQUIRE(restored.type == PartType::Tool);
    REQUIRE(restored.message_id == "msg_1");
    REQUIRE(restored.session_id == "sess_1");
    
    auto tool = restored.get_tool();
    REQUIRE(tool["tool_id"] == "tool_1");
    REQUIRE(tool["tool_name"] == "bash");
}

// ==================== Tool Compaction Tests ====================

TEST_CASE("Part.ToolCompaction.NotCompacted", "[Core][Message]") {
    Part part = Part::create_tool("tool_1", "test", {});
    
    REQUIRE_FALSE(part.is_tool_compacted());
    REQUIRE_FALSE(part.get_tool_compacted_at().has_value());
}

TEST_CASE("Part.ToolCompaction.MarkCompacted", "[Core][Message]") {
    Part part = Part::create_tool("tool_1", "test", {}, {{"result", "big data"}});
    
    REQUIRE_FALSE(part.is_tool_compacted());
    
    part.set_tool_compacted_at(1234567890);
    
    REQUIRE(part.is_tool_compacted());
    REQUIRE(part.get_tool_compacted_at() == 1234567890);
    
    // Result should be cleared
    auto tool = part.get_tool();
    REQUIRE_FALSE(tool.contains("result"));
}

TEST_CASE("Part.ToolCompaction.NonToolPart", "[Core][Message]") {
    Part part = Part::create_text("hello");
    
    // Should not affect non-tool parts
    part.set_tool_compacted_at(1234567890);
    
    REQUIRE_FALSE(part.is_tool_compacted());
}

// ==================== MessageInfo Tests ====================

TEST_CASE("MessageInfo.ToJson", "[Core][Message]") {
    MessageInfo info;
    info.id = "msg_123";
    info.session_id = "sess_456";
    info.role = Role::Assistant;
    info.agent = "coder";
    info.model_id = "gpt-4";
    info.provider_id = "openai";
    
    nlohmann::json j = info.to_json();
    
    REQUIRE(j["id"] == "msg_123");
    REQUIRE(j["session_id"] == "sess_456");
    REQUIRE(j["role"] == "assistant");
    REQUIRE(j["agent"] == "coder");
}

TEST_CASE("MessageInfo.FromJson", "[Core][Message]") {
    nlohmann::json j = {
        {"id", "msg_789"},
        {"session_id", "sess_012"},
        {"role", "user"},
        {"agent", "default"},
        {"model_id", "claude-3"},
        {"provider_id", "anthropic"},
        {"time_created", 1234567890}
    };
    
    MessageInfo info = MessageInfo::from_json(j);
    
    REQUIRE(info.id == "msg_789");
    REQUIRE(info.session_id == "sess_012");
    REQUIRE(info.role == Role::User);
    REQUIRE(info.agent == "default");
    REQUIRE(info.model_id == "claude-3");
}

// ==================== Utility Functions Tests ====================

TEST_CASE("Message.GenerateUUID", "[Core][Message]") {
    std::string uuid = generate_uuid();
    
    REQUIRE(uuid.length() == 36);
    REQUIRE(uuid[8] == '-');
    REQUIRE(uuid[13] == '-');
    REQUIRE(uuid[18] == '-');
    REQUIRE(uuid[23] == '-');
}

TEST_CASE("Message.GenerateUUID.Uniqueness", "[Core][Message]") {
    std::string uuid1 = generate_uuid();
    std::string uuid2 = generate_uuid();
    
    REQUIRE(uuid1 != uuid2);
}

TEST_CASE("Message.CurrentTimestamp", "[Core][Message]") {
    int64_t ts1 = current_timestamp_ms();
    int64_t ts2 = current_timestamp_ms();
    
    REQUIRE(ts1 > 0);
    REQUIRE(ts2 >= ts1);
}

// ==================== Message Class Tests ====================

TEST_CASE("Message.Constructor.Basic", "[Core][Message]") {
    Message msg("session-123", Role::User, "test-agent", "gpt-4", "openai");
    
    REQUIRE_FALSE(msg.id().empty());
    REQUIRE(msg.session_id() == "session-123");
    REQUIRE(msg.role() == Role::User);
    REQUIRE(msg.info().agent == "test-agent");
    REQUIRE(msg.info().model_id == "gpt-4");
    REQUIRE(msg.info().provider_id == "openai");
}

TEST_CASE("Message.Constructor.Assistant", "[Core][Message]") {
    Message msg("session-456", Role::Assistant, "coder", "claude-3", "anthropic");
    
    REQUIRE(msg.role() == Role::Assistant);
    REQUIRE(msg.info().agent == "coder");
}

TEST_CASE("Message.AddPart", "[Core][Message]") {
    Message msg("session-789", Role::User, "default", "model", "provider");
    
    Part part = Part::create_text("Hello");
    msg.add_part(part);
    
    REQUIRE(msg.parts().size() == 1);
    REQUIRE(msg.parts()[0].get_text() == "Hello");
}

TEST_CASE("Message.AddText", "[Core][Message]") {
    Message msg("session-abc", Role::User, "default", "model", "provider");
    
    msg.add_text("First text");
    msg.add_text(" Second text");
    
    REQUIRE(msg.parts().size() == 2);
    REQUIRE(msg.get_text() == "First text Second text");
}

TEST_CASE("Message.AddTool", "[Core][Message]") {
    Message msg("session-def", Role::Assistant, "coder", "model", "provider");
    
    nlohmann::json args = {{"path", "/tmp/test"}};
    nlohmann::json result = {{"output", "success"}};
    
    msg.add_tool("tool-123", "read_file", args, result);
    
    REQUIRE(msg.has_tool_calls());
    auto calls = msg.get_tool_calls();
    REQUIRE(calls.size() == 1);
    REQUIRE(calls[0]["tool_name"] == "read_file");
}

TEST_CASE("Message.AddTool.WithoutResult", "[Core][Message]") {
    Message msg("session-ghi", Role::Assistant, "coder", "model", "provider");
    
    nlohmann::json args = {{"cmd", "ls"}};
    msg.add_tool("tool-456", "bash", args);
    
    REQUIRE(msg.has_tool_calls());
    auto calls = msg.get_tool_calls();
    REQUIRE(calls.size() == 1);
    REQUIRE_FALSE(calls[0].contains("result"));
}

TEST_CASE("Message.AddReasoning", "[Core][Message]") {
    Message msg("session-jkl", Role::Assistant, "coder", "model", "provider");
    
    msg.add_reasoning("Let me think...");
    
    auto reasoning_parts = msg.get_parts(PartType::Reasoning);
    REQUIRE(reasoning_parts.size() == 1);
    REQUIRE(reasoning_parts[0].get_reasoning() == "Let me think...");
}

TEST_CASE("Message.GetParts.ByType", "[Core][Message]") {
    Message msg("session-mno", Role::Assistant, "coder", "model", "provider");
    
    msg.add_text("Text 1");
    msg.add_reasoning("Thinking");
    msg.add_text("Text 2");
    
    auto text_parts = msg.get_parts(PartType::Text);
    REQUIRE(text_parts.size() == 2);
    
    auto reasoning_parts = msg.get_parts(PartType::Reasoning);
    REQUIRE(reasoning_parts.size() == 1);
}

TEST_CASE("Message.GetFullText", "[Core][Message]") {
    Message msg("session-pqr", Role::Assistant, "coder", "model", "provider");
    
    msg.add_text("Hello ");
    msg.add_reasoning("thinking");
    msg.add_text("World");
    
    REQUIRE(msg.get_full_text() == "Hello thinkingWorld");
}

TEST_CASE("Message.SetError", "[Core][Message]") {
    Message msg("session-stu", Role::Assistant, "coder", "model", "provider");
    
    nlohmann::json error = {{"message", "Something went wrong"}, {"code", 500}};
    msg.set_error(error);
    
    REQUIRE(msg.info().error.has_value());
    REQUIRE((*msg.info().error)["code"] == 500);
}

TEST_CASE("Message.SetFinish", "[Core][Message]") {
    Message msg("session-vwx", Role::Assistant, "coder", "model", "provider");
    
    msg.set_finish("stop");
    
    REQUIRE(msg.info().finish == "stop");
}

TEST_CASE("Message.UpdateTokens", "[Core][Message]") {
    Message msg("session-yz", Role::Assistant, "coder", "model", "provider");
    
    TokenUsage tokens;
    tokens.input = 100;
    tokens.output = 50;
    
    nlohmann::json pricing = {
        {"input", 0.00001},
        {"output", 0.00003}
    };
    
    msg.update_tokens(tokens, pricing);
    
    REQUIRE(msg.info().tokens.input == 100);
    REQUIRE(msg.info().tokens.output == 50);
    REQUIRE(msg.info().cost > 0);
}

TEST_CASE("Message.HasToolCalls.False", "[Core][Message]") {
    Message msg("session-123", Role::User, "default", "model", "provider");
    
    msg.add_text("Just text");
    
    REQUIRE_FALSE(msg.has_tool_calls());
}

// ==================== TokenUsage Tests ====================

TEST_CASE("TokenUsage.ToJson", "[Core][Message]") {
    TokenUsage tokens;
    tokens.input = 100;
    tokens.output = 50;
    tokens.reasoning = 25;
    
    nlohmann::json j = tokens.to_json();
    
    REQUIRE(j["input"] == 100);
    REQUIRE(j["output"] == 50);
    REQUIRE(j["reasoning"] == 25);
}

TEST_CASE("TokenUsage.FromJson", "[Core][Message]") {
    nlohmann::json j = {
        {"input", 200},
        {"output", 100},
        {"reasoning", 50},
        {"cache", {{"read", 10}, {"write", 5}}}
    };
    
    auto tokens = TokenUsage::from_json(j);
    
    REQUIRE(tokens.input == 200);
    REQUIRE(tokens.output == 100);
    REQUIRE(tokens.reasoning == 50);
    REQUIRE(tokens.cache.read == 10);
    REQUIRE(tokens.cache.write == 5);
}

TEST_CASE("TokenUsage.Total", "[Core][Message]") {
    TokenUsage tokens;
    tokens.input = 100;
    tokens.output = 50;
    tokens.reasoning = 25;
    tokens.cache.read = 10;
    tokens.cache.write = 5;
    
    REQUIRE(tokens.total() == 190);
}

TEST_CASE("TokenUsage.Cost", "[Core][Message]") {
    TokenUsage tokens;
    tokens.input = 1000;
    tokens.output = 500;
    
    nlohmann::json pricing = {
        {"input", 0.01},
        {"output", 0.03}
    };
    
    double cost = tokens.cost(pricing);
    
    // 1000 * 0.01 + 500 * 0.03 = 10 + 15 = 25
    REQUIRE(cost == Approx(25.0));
}

TEST_CASE("TokenUsage.PlusEquals", "[Core][Message]") {
    TokenUsage t1;
    t1.input = 100;
    t1.output = 50;
    
    TokenUsage t2;
    t2.input = 200;
    t2.output = 100;
    
    t1 += t2;
    
    REQUIRE(t1.input == 300);
    REQUIRE(t1.output == 150);
}

TEST_CASE("TokenUsage.MinusEquals", "[Core][Message]") {
    TokenUsage t1;
    t1.input = 300;
    t1.output = 150;
    
    TokenUsage t2;
    t2.input = 100;
    t2.output = 50;
    
    t1 -= t2;
    
    REQUIRE(t1.input == 200);
    REQUIRE(t1.output == 100);
}

TEST_CASE("TokenUsage.OperatorPlus", "[Core][Message]") {
    TokenUsage t1;
    t1.input = 100;
    t1.output = 50;
    
    TokenUsage t2;
    t2.input = 200;
    t2.output = 100;
    
    TokenUsage t3 = t1 + t2;
    
    REQUIRE(t3.input == 300);
    REQUIRE(t3.output == 150);
    // Originals unchanged
    REQUIRE(t1.input == 100);
    REQUIRE(t2.input == 200);
}

// ==================== Extended Message Tests ====================

TEST_CASE("Message.WithMultipleParts", "[Core][Message]") {
    Message msg("test-session", Role::Assistant, "test-agent", "test-model", "test-provider");
    
    msg.add_part(Part::create_text("First part"));
    msg.add_part(Part::create_text("Second part"));
    msg.add_part(Part::create_reasoning("Thinking..."));
    
    REQUIRE(msg.parts().size() == 3);
    REQUIRE(msg.parts()[0].is_text());
    REQUIRE(msg.parts()[2].is_reasoning());
}

TEST_CASE("Message.WithToolParts", "[Core][Message]") {
    Message msg("test-session", Role::Assistant, "test-agent", "test-model", "test-provider");
    
    nlohmann::json args = {{"path", "/test"}};
    nlohmann::json result = {{"content", "file content"}};
    msg.add_tool("call_1", "read", args, result);
    
    REQUIRE(msg.parts().size() == 1);
    REQUIRE(msg.parts()[0].is_tool());
}

TEST_CASE("Message.WithError", "[Core][Message]") {
    Message msg("test-session", Role::Assistant, "test-agent", "test-model", "test-provider");
    
    msg.add_part(Part::create_error("Something went wrong"));
    
    REQUIRE(msg.parts().size() == 1);
    REQUIRE(msg.parts()[0].is_error());
}

TEST_CASE("Message.WithFile", "[Core][Message]") {
    Message msg("test-session", Role::User, "test-agent", "test-model", "test-provider");
    
    Part file_part = Part::create_file("/path/to/file.txt", "file content here");
    msg.add_part(file_part);
    
    REQUIRE(msg.parts().size() == 1);
    REQUIRE(msg.parts()[0].is_file());
}

TEST_CASE("Message.WithImage", "[Core][Message]") {
    Message msg("test-session", Role::User, "test-agent", "test-model", "test-provider");
    
    Part image_part = Part::create_image("base64imagedata", "image/png");
    msg.add_part(image_part);
    
    REQUIRE(msg.parts().size() == 1);
    REQUIRE(msg.parts()[0].is_image());
}

TEST_CASE("Message.WithSubtask", "[Core][Message]") {
    Message msg("test-session", Role::Assistant, "test-agent", "test-model", "test-provider");
    
    Part subtask = Part::create_subtask("subtask-1", "explore", "Explore the codebase");
    msg.add_part(subtask);
    
    REQUIRE(msg.parts().size() == 1);
    REQUIRE(msg.parts()[0].is_subtask());
}

TEST_CASE("Message.WithStepStart", "[Core][Message]") {
    Message msg("test-session", Role::Assistant, "test-agent", "test-model", "test-provider");
    
    Part step = Part::create_step_start("step-1", "Processing");
    msg.add_part(step);
    
    REQUIRE(msg.parts().size() == 1);
    REQUIRE(msg.parts()[0].is_step_start());
}

TEST_CASE("Message.WithStepFinish", "[Core][Message]") {
    Message msg("test-session", Role::Assistant, "test-agent", "test-model", "test-provider");
    
    Part step = Part::create_step_finish("step-1", "Completed", true);
    msg.add_part(step);
    
    REQUIRE(msg.parts().size() == 1);
    REQUIRE(msg.parts()[0].is_step_finish());
}

TEST_CASE("Message.WithSnapshot", "[Core][Message]") {
    Message msg("test-session", Role::Assistant, "test-agent", "test-model", "test-provider");
    
    Part snapshot = Part::create_snapshot({{"key", "value"}});
    msg.add_part(snapshot);
    
    REQUIRE(msg.parts().size() == 1);
    REQUIRE(msg.parts()[0].is_snapshot());
}

TEST_CASE("Message.WithPatch", "[Core][Message]") {
    Message msg("test-session", Role::Assistant, "test-agent", "test-model", "test-provider");
    
    Part patch = Part::create_patch("/file.txt", {{"old", "new"}});
    msg.add_part(patch);
    
    REQUIRE(msg.parts().size() == 1);
    REQUIRE(msg.parts()[0].is_patch());
}

TEST_CASE("Message.WithAgent", "[Core][Message]") {
    Message msg("test-session", Role::Assistant, "test-agent", "test-model", "test-provider");
    
    Part agent_part = Part::create_agent("agent-1", "build");
    msg.add_part(agent_part);
    
    REQUIRE(msg.parts().size() == 1);
    REQUIRE(msg.parts()[0].is_agent());
}

TEST_CASE("Message.WithRetry", "[Core][Message]") {
    Message msg("test-session", Role::Assistant, "test-agent", "test-model", "test-provider");
    
    Part retry = Part::create_retry(3, "Rate limited");
    msg.add_part(retry);
    
    REQUIRE(msg.parts().size() == 1);
    REQUIRE(msg.parts()[0].is_retry());
}

TEST_CASE("Message.WithCompaction", "[Core][Message]") {
    Message msg("test-session", Role::Assistant, "test-agent", "test-model", "test-provider");
    
    Part compaction = Part::create_compaction(100, 50, {{"summary", "compacted"}});
    msg.add_part(compaction);
    
    REQUIRE(msg.parts().size() == 1);
    REQUIRE(msg.parts()[0].is_compaction());
}

TEST_CASE("Message.WithSource", "[Core][Message]") {
    Message msg("test-session", Role::Assistant, "test-agent", "test-model", "test-provider");
    
    Part source = Part::create_source("source-1", "read_file", {{"path", "/test"}});
    msg.add_part(source);
    
    REQUIRE(msg.parts().size() == 1);
    REQUIRE(msg.parts()[0].is_source());
}

// ==================== Extended MessageInfo Tests ====================

TEST_CASE("MessageInfo.ToJson.AllFields", "[Core][Message]") {
    MessageInfo info;
    info.id = "msg-123";
    info.session_id = "sess-456";
    info.role = Role::Assistant;
    info.time_created = 1000;
    info.time_updated = 2000;
    info.agent = "test-agent";
    info.model_id = "gpt-4";
    info.provider_id = "openai";
    info.cost = 0.05;
    info.parent_id = "parent-msg";
    info.system = "system prompt";
    info.tools = nlohmann::json::array({"tool1", "tool2"});
    info.variant = "variant-1";
    info.error = nlohmann::json{{"message", "test error"}};
    info.finish = "stop";
    info.summary = true;
    info.structured = nlohmann::json{{"key", "value"}};
    
    auto j = info.to_json();
    REQUIRE(j["id"] == "msg-123");
    REQUIRE(j["session_id"] == "sess-456");
    REQUIRE(j["role"] == "assistant");
    REQUIRE(j["time_created"] == 1000);
    REQUIRE(j["time_updated"] == 2000);
    REQUIRE(j["agent"] == "test-agent");
    REQUIRE(j["model_id"] == "gpt-4");
    REQUIRE(j["provider_id"] == "openai");
    REQUIRE(j["cost"] == 0.05);
    REQUIRE(j["parent_id"] == "parent-msg");
    REQUIRE(j["system"] == "system prompt");
    REQUIRE(j["finish"] == "stop");
    REQUIRE(j["summary"] == true);
}

TEST_CASE("MessageInfo.FromJson.AllFields", "[Core][Message]") {
    nlohmann::json j = {
        {"id", "msg-789"},
        {"session_id", "sess-012"},
        {"role", "user"},
        {"time_created", 3000},
        {"time_updated", 4000},
        {"agent", "user-agent"},
        {"model_id", "claude-3"},
        {"provider_id", "anthropic"},
        {"cost", 0.10},
        {"tokens", {{"input", 100}, {"output", 50}, {"cache_read", 0}, {"cache_write", 0}}},
        {"parent_id", "parent-123"},
        {"system", "system prompt"},
        {"tools", nlohmann::json::array({"tool_a", "tool_b"})},
        {"variant", "v2"},
        {"error", {{"code", "rate_limit"}}},
        {"finish", "tool_calls"},
        {"summary", false},
        {"structured", {{"data", "test"}}}
    };
    
    auto info = MessageInfo::from_json(j);
    REQUIRE(info.id == "msg-789");
    REQUIRE(info.session_id == "sess-012");
    REQUIRE(info.role == Role::User);
    REQUIRE(info.time_created == 3000);
    REQUIRE(info.time_updated == 4000);
    REQUIRE(info.agent == "user-agent");
    REQUIRE(info.model_id == "claude-3");
    REQUIRE(info.provider_id == "anthropic");
    REQUIRE(info.cost == 0.10);
    REQUIRE(info.parent_id == "parent-123");
    REQUIRE(info.system == "system prompt");
    REQUIRE(info.finish == "tool_calls");
    REQUIRE(info.summary == false);
}

TEST_CASE("MessageInfo.FromJson.Minimal", "[Core][Message]") {
    nlohmann::json j = {{"id", "simple-msg"}};
    
    auto info = MessageInfo::from_json(j);
    REQUIRE(info.id == "simple-msg");
    REQUIRE(info.role == Role::User);  // Default
    REQUIRE(info.time_created == 0);
    REQUIRE(info.cost == 0.0);
    REQUIRE_FALSE(info.parent_id.has_value());
    REQUIRE_FALSE(info.system.has_value());
}
