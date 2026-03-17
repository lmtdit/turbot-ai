#include <catch2/catch_test_macros.hpp>
#include <turbot/core/message/part.hpp>
#include <turbot/core/message/message.hpp>

using namespace turbot::core;

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
