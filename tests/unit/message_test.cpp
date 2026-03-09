#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <turbot/core/message/token_usage.hpp>
#include <turbot/core/message/part.hpp>
#include <turbot/core/message/message.hpp>
#include <turbot/storage/sqlite_database.hpp>
#include <turbot/core/common/logger.hpp>

using namespace turbot::core;
using namespace turbot::storage;
using namespace turbot::storage::sqlite;

// Helper to create a test database with message tables
class MessageTestDatabase {
public:
    MessageTestDatabase() {
        DatabaseConfig config;
        config.path = ":memory:";
        db_ = std::make_shared<SQLiteDatabase>(config);
        create_tables();
    }

    std::shared_ptr<Database> db() const { return db_; }

private:
    void create_tables() {
        // Create messages table
        db_->execute(R"(
            CREATE TABLE IF NOT EXISTS messages (
                id TEXT PRIMARY KEY,
                session_id TEXT NOT NULL,
                role TEXT NOT NULL,
                time_created INTEGER NOT NULL,
                time_updated INTEGER NOT NULL,
                parent_id TEXT,
                agent TEXT NOT NULL,
                model_id TEXT NOT NULL,
                provider_id TEXT NOT NULL,
                system TEXT,
                tools TEXT,
                variant TEXT,
                error TEXT,
                finish TEXT,
                cost REAL DEFAULT 0.0,
                tokens TEXT,
                summary INTEGER,
                structured TEXT
            )
        )");

        // Create parts table
        db_->execute(R"(
            CREATE TABLE IF NOT EXISTS parts (
                id TEXT PRIMARY KEY,
                message_id TEXT NOT NULL,
                session_id TEXT NOT NULL,
                type TEXT NOT NULL,
                data TEXT NOT NULL,
                time_created INTEGER NOT NULL,
                time_updated INTEGER NOT NULL,
                FOREIGN KEY (message_id) REFERENCES messages(id) ON DELETE CASCADE
            )
        )");

        // Create index
        db_->execute("CREATE INDEX IF NOT EXISTS idx_parts_message_id ON parts(message_id)");
        db_->execute("CREATE INDEX IF NOT EXISTS idx_messages_session_id ON messages(session_id)");
    }

    std::shared_ptr<SQLiteDatabase> db_;
};

// ============================================================================
// TokenUsage Tests
// ============================================================================

TEST_CASE("TokenUsage::default_values", "[message][token]") {
    TokenUsage usage;
    REQUIRE(usage.input == 0);
    REQUIRE(usage.output == 0);
    REQUIRE(usage.reasoning == 0);
    REQUIRE(usage.cache.read == 0);
    REQUIRE(usage.cache.write == 0);
}

TEST_CASE("TokenUsage::total", "[message][token]") {
    TokenUsage usage;
    usage.input = 100;
    usage.output = 50;
    usage.reasoning = 25;
    usage.cache.read = 10;
    usage.cache.write = 15;
    
    REQUIRE(usage.total() == 200);
}

TEST_CASE("TokenUsage::cost", "[message][token]") {
    TokenUsage usage;
    usage.input = 1000;
    usage.output = 500;
    usage.reasoning = 100;
    usage.cache.read = 200;
    usage.cache.write = 100;
    
    nlohmann::json pricing = {
        {"input", 0.01},        // per 1K tokens
        {"output", 0.03},
        {"reasoning", 0.02},
        {"cache_read", 0.005},
        {"cache_write", 0.01}
    };
    
    double expected = 1000*0.01 + 500*0.03 + 100*0.02 + 200*0.005 + 100*0.01;
    REQUIRE(usage.cost(pricing) == Catch::Approx(expected));
}

TEST_CASE("TokenUsage::addition", "[message][token]") {
    TokenUsage a;
    a.input = 100;
    a.output = 50;
    
    TokenUsage b;
    b.input = 50;
    b.output = 25;
    b.reasoning = 10;
    
    TokenUsage c = a + b;
    REQUIRE(c.input == 150);
    REQUIRE(c.output == 75);
    REQUIRE(c.reasoning == 10);
    
    a += b;
    REQUIRE(a.input == 150);
    REQUIRE(a.output == 75);
}

TEST_CASE("TokenUsage::serialization", "[message][token]") {
    TokenUsage usage;
    usage.input = 100;
    usage.output = 50;
    usage.reasoning = 25;
    usage.cache.read = 10;
    usage.cache.write = 15;
    
    nlohmann::json j = usage.to_json();
    
    REQUIRE(j["input"] == 100);
    REQUIRE(j["output"] == 50);
    REQUIRE(j["reasoning"] == 25);
    REQUIRE(j["cache"]["read"] == 10);
    REQUIRE(j["cache"]["write"] == 15);
    
    TokenUsage restored = TokenUsage::from_json(j);
    REQUIRE(restored.input == 100);
    REQUIRE(restored.output == 50);
    REQUIRE(restored.reasoning == 25);
    REQUIRE(restored.cache.read == 10);
    REQUIRE(restored.cache.write == 15);
}

// ============================================================================
// PartType Tests
// ============================================================================

TEST_CASE("PartType::to_string", "[message][part]") {
    REQUIRE(part_type_to_string(PartType::Text) == "text");
    REQUIRE(part_type_to_string(PartType::Tool) == "tool");
    REQUIRE(part_type_to_string(PartType::Reasoning) == "reasoning");
    REQUIRE(part_type_to_string(PartType::File) == "file");
    REQUIRE(part_type_to_string(PartType::Subtask) == "subtask");
    REQUIRE(part_type_to_string(PartType::StepStart) == "step_start");
    REQUIRE(part_type_to_string(PartType::StepFinish) == "step_finish");
    REQUIRE(part_type_to_string(PartType::Snapshot) == "snapshot");
    REQUIRE(part_type_to_string(PartType::Patch) == "patch");
    REQUIRE(part_type_to_string(PartType::Agent) == "agent");
    REQUIRE(part_type_to_string(PartType::Retry) == "retry");
    REQUIRE(part_type_to_string(PartType::Compaction) == "compaction");
}

TEST_CASE("PartType::from_string", "[message][part]") {
    REQUIRE(part_type_from_string("text") == PartType::Text);
    REQUIRE(part_type_from_string("tool") == PartType::Tool);
    REQUIRE(part_type_from_string("reasoning") == PartType::Reasoning);
    REQUIRE(part_type_from_string("file") == PartType::File);
    REQUIRE(part_type_from_string("subtask") == PartType::Subtask);
    REQUIRE(part_type_from_string("step_start") == PartType::StepStart);
    REQUIRE(part_type_from_string("step_finish") == PartType::StepFinish);
    REQUIRE(part_type_from_string("snapshot") == PartType::Snapshot);
    REQUIRE(part_type_from_string("patch") == PartType::Patch);
    REQUIRE(part_type_from_string("agent") == PartType::Agent);
    REQUIRE(part_type_from_string("retry") == PartType::Retry);
    REQUIRE(part_type_from_string("compaction") == PartType::Compaction);
    REQUIRE(part_type_from_string("unknown") == PartType::Text);  // default
}

// ============================================================================
// Role Tests
// ============================================================================

TEST_CASE("Role::to_string", "[message][role]") {
    REQUIRE(role_to_string(Role::User) == "user");
    REQUIRE(role_to_string(Role::Assistant) == "assistant");
    REQUIRE(role_to_string(Role::System) == "system");
    REQUIRE(role_to_string(Role::Tool) == "tool");  // C-3 fix
}

TEST_CASE("Role::from_string", "[message][role]") {
    REQUIRE(role_from_string("user") == Role::User);
    REQUIRE(role_from_string("assistant") == Role::Assistant);
    REQUIRE(role_from_string("system") == Role::System);
    REQUIRE(role_from_string("tool") == Role::Tool);  // C-3 fix
    REQUIRE(role_from_string("unknown") == Role::User);  // default
}

// ============================================================================
// Part Factory Tests
// ============================================================================

TEST_CASE("Part::create_text", "[message][part]") {
    auto part = Part::create_text("Hello, world!");
    
    REQUIRE(part.is_text());
    REQUIRE_FALSE(part.is_tool());
    REQUIRE(part.get_text() == "Hello, world!");
    REQUIRE(part.time_created > 0);
    REQUIRE_FALSE(part.id.empty());
}

TEST_CASE("Part::create_tool", "[message][part]") {
    nlohmann::json args = {{"x", 1}, {"y", 2}};
    nlohmann::json result = {{"sum", 3}};
    
    auto part = Part::create_tool("tool-123", "add", args, result);
    
    REQUIRE(part.is_tool());
    REQUIRE_FALSE(part.is_text());
    
    auto tool_data = part.get_tool();
    REQUIRE(tool_data["tool_id"] == "tool-123");
    REQUIRE(tool_data["tool_name"] == "add");
    REQUIRE(tool_data["arguments"]["x"] == 1);
    REQUIRE(tool_data["result"]["sum"] == 3);
}

TEST_CASE("Part::create_tool_without_result", "[message][part]") {
    nlohmann::json args = {{"file", "test.cpp"}};
    
    auto part = Part::create_tool("tool-456", "read_file", args);
    
    REQUIRE(part.is_tool());
    auto tool_data = part.get_tool();
    REQUIRE(tool_data["tool_id"] == "tool-456");
    REQUIRE_FALSE(tool_data.contains("result"));
}

TEST_CASE("Part::create_reasoning", "[message][part]") {
    auto part = Part::create_reasoning("I need to think about this problem...");
    
    REQUIRE(part.is_reasoning());
    REQUIRE(part.get_reasoning() == "I need to think about this problem...");
}

TEST_CASE("Part::create_file", "[message][part]") {
    auto part = Part::create_file("/path/to/file.cpp", "int main() {}", "text/cpp");
    
    REQUIRE(part.is_file());
    auto file_data = part.get_file();
    REQUIRE(file_data["path"] == "/path/to/file.cpp");
    REQUIRE(file_data["content"] == "int main() {}");
    REQUIRE(file_data["mime_type"] == "text/cpp");
}

TEST_CASE("Part::create_file_minimal", "[message][part]") {
    auto part = Part::create_file("/path/to/file.cpp");
    
    REQUIRE(part.is_file());
    auto file_data = part.get_file();
    REQUIRE(file_data["path"] == "/path/to/file.cpp");
    REQUIRE_FALSE(file_data.contains("content"));
}

// ============================================================================
// Part v2.0 Extensions Tests
// ============================================================================

TEST_CASE("Part::create_image", "[message][part][v2]") {
    auto part = Part::create_image("https://example.com/image.png", "A diagram", "image/png");
    
    REQUIRE(part.is_image());
    auto image_data = part.get_image();
    REQUIRE(image_data["url"] == "https://example.com/image.png");
    REQUIRE(image_data["alt_text"] == "A diagram");
    REQUIRE(image_data["mime_type"] == "image/png");
}

TEST_CASE("Part::create_image_minimal", "[message][part][v2]") {
    auto part = Part::create_image("https://example.com/photo.jpg");
    
    REQUIRE(part.is_image());
    REQUIRE_FALSE(part.is_file());
    REQUIRE_FALSE(part.is_text());
    auto image_data = part.get_image();
    REQUIRE(image_data["url"] == "https://example.com/photo.jpg");
    REQUIRE_FALSE(image_data.contains("alt_text"));
}

TEST_CASE("Part::create_image_base64", "[message][part][v2]") {
    auto part = Part::create_image_base64("iVBORw0KGgo=", "image/png", "Base64 image");
    
    REQUIRE(part.is_image());
    auto image_data = part.get_image();
    REQUIRE(image_data["url"] == "data:image/png;base64,iVBORw0KGgo=");
    REQUIRE(image_data["mime_type"] == "image/png");
    REQUIRE(image_data["alt_text"] == "Base64 image");
}

TEST_CASE("Part::create_error", "[message][part][v2]") {
    nlohmann::json details = {{"line", 42}, {"file", "test.cpp"}};
    auto part = Part::create_error("Compilation failed", "COMPILE_ERROR", details);
    
    REQUIRE(part.is_error());
    auto error_data = part.get_error();
    REQUIRE(error_data["message"] == "Compilation failed");
    REQUIRE(error_data["code"] == "COMPILE_ERROR");
    REQUIRE(error_data["details"]["line"] == 42);
}

TEST_CASE("Part::create_error_minimal", "[message][part][v2]") {
    auto part = Part::create_error("Something went wrong");
    
    REQUIRE(part.is_error());
    REQUIRE_FALSE(part.is_text());
    auto error_data = part.get_error();
    REQUIRE(error_data["message"] == "Something went wrong");
    REQUIRE_FALSE(error_data.contains("code"));
    REQUIRE_FALSE(error_data.contains("details"));
}

TEST_CASE("Part::create_source", "[message][part][v2]") {
    auto part = Part::create_source("src-123", "document", "API Documentation", "https://docs.example.com");
    
    REQUIRE(part.is_source());
    auto source_data = part.get_source();
    REQUIRE(source_data["source_id"] == "src-123");
    REQUIRE(source_data["source_type"] == "document");
    REQUIRE(source_data["title"] == "API Documentation");
    REQUIRE(source_data["url"] == "https://docs.example.com");
}

TEST_CASE("Part::create_source_minimal", "[message][part][v2]") {
    auto part = Part::create_source("src-456", "url");
    
    REQUIRE(part.is_source());
    auto source_data = part.get_source();
    REQUIRE(source_data["source_id"] == "src-456");
    REQUIRE(source_data["source_type"] == "url");
    REQUIRE_FALSE(source_data.contains("title"));
    REQUIRE_FALSE(source_data.contains("url"));
}

TEST_CASE("PartType string conversion v2", "[message][part][v2]") {
    REQUIRE(part_type_to_string(PartType::Image) == "image");
    REQUIRE(part_type_to_string(PartType::Error) == "error");
    REQUIRE(part_type_to_string(PartType::Source) == "source");
    
    REQUIRE(part_type_from_string("image") == PartType::Image);
    REQUIRE(part_type_from_string("error") == PartType::Error);
    REQUIRE(part_type_from_string("source") == PartType::Source);
}

TEST_CASE("Part serialization v2 types", "[message][part][v2]") {
    SECTION("image part roundtrip") {
        auto original = Part::create_image("https://example.com/img.png", "Test");
        auto json = original.to_json();
        auto restored = Part::from_json(json);
        
        REQUIRE(restored.is_image());
        REQUIRE(restored.get_image()["url"] == "https://example.com/img.png");
    }
    
    SECTION("error part roundtrip") {
        auto original = Part::create_error("Test error", "TEST_ERR");
        auto json = original.to_json();
        auto restored = Part::from_json(json);
        
        REQUIRE(restored.is_error());
        REQUIRE(restored.get_error()["message"] == "Test error");
    }
    
    SECTION("source part roundtrip") {
        auto original = Part::create_source("s1", "file", "Document");
        auto json = original.to_json();
        auto restored = Part::from_json(json);
        
        REQUIRE(restored.is_source());
        REQUIRE(restored.get_source()["source_id"] == "s1");
    }
}

TEST_CASE("Part::create_subtask", "[message][part]") {
    auto part = Part::create_subtask("task-789", "code-agent", "running");
    
    REQUIRE(part.is_subtask());
    auto subtask_data = part.get_subtask();
    REQUIRE(subtask_data["task_id"] == "task-789");
    REQUIRE(subtask_data["agent"] == "code-agent");
    REQUIRE(subtask_data["status"] == "running");
}

TEST_CASE("Part::create_step_start", "[message][part]") {
    auto part = Part::create_step_start("step-1", "Code Generation");
    
    REQUIRE(part.is_step_start());
    auto step_data = part.get_step();
    REQUIRE(step_data["step_id"] == "step-1");
    REQUIRE(step_data["name"] == "Code Generation");
}

TEST_CASE("Part::create_step_finish", "[message][part]") {
    nlohmann::json result = {{"files", 5}};
    auto part = Part::create_step_finish("step-1", "success", result);
    
    REQUIRE(part.is_step_finish());
    auto step_data = part.get_step();
    REQUIRE(step_data["step_id"] == "step-1");
    REQUIRE(step_data["status"] == "success");
    REQUIRE(step_data["result"]["files"] == 5);
}

TEST_CASE("Part::create_snapshot", "[message][part]") {
    nlohmann::json files = {
        {"file1.cpp", "content1"},
        {"file2.cpp", "content2"}
    };
    
    auto part = Part::create_snapshot(files);
    
    REQUIRE(part.is_snapshot());
    auto snapshot_data = part.get_snapshot();
    REQUIRE(snapshot_data["file1.cpp"] == "content1");
    REQUIRE(snapshot_data["file2.cpp"] == "content2");
}

TEST_CASE("Part::create_patch", "[message][part]") {
    nlohmann::json diff = {
        {"additions", 10},
        {"deletions", 5}
    };
    
    auto part = Part::create_patch("/src/main.cpp", diff);
    
    REQUIRE(part.is_patch());
    auto patch_data = part.get_patch();
    REQUIRE(patch_data["file_path"] == "/src/main.cpp");
    REQUIRE(patch_data["diff"]["additions"] == 10);
}

TEST_CASE("Part::create_agent", "[message][part]") {
    auto part = Part::create_agent("agent-001", "CodeWriter", "gpt-4");
    
    REQUIRE(part.is_agent());
    auto agent_data = part.get_agent();
    REQUIRE(agent_data["agent_id"] == "agent-001");
    REQUIRE(agent_data["agent_name"] == "CodeWriter");
    REQUIRE(agent_data["model"] == "gpt-4");
}

TEST_CASE("Part::create_retry", "[message][part]") {
    auto part = Part::create_retry(2, "Rate limit exceeded", 3);
    
    REQUIRE(part.is_retry());
    auto retry_data = part.get_retry();
    REQUIRE(retry_data["attempt"] == 2);
    REQUIRE(retry_data["reason"] == "Rate limit exceeded");
    REQUIRE(retry_data["max_attempts"] == 3);
}

TEST_CASE("Part::create_compaction", "[message][part]") {
    nlohmann::json summary = {{"key_points", {"point1", "point2"}}};
    auto part = Part::create_compaction(10000, 2000, summary);
    
    REQUIRE(part.is_compaction());
    auto compaction_data = part.get_compaction();
    REQUIRE(compaction_data["original_tokens"] == 10000);
    REQUIRE(compaction_data["compacted_tokens"] == 2000);
}

// ============================================================================
// Part Serialization Tests
// ============================================================================

TEST_CASE("Part::serialization", "[message][part]") {
    auto part = Part::create_text("Test content");
    part.message_id = "msg-123";
    part.session_id = "session-456";
    
    nlohmann::json j = part.to_json();
    
    REQUIRE(j["type"] == "text");
    REQUIRE(j["data"]["content"] == "Test content");
    REQUIRE(j["message_id"] == "msg-123");
    REQUIRE(j["session_id"] == "session-456");
    
    Part restored = Part::from_json(j);
    REQUIRE(restored.type == PartType::Text);
    REQUIRE(restored.get_text() == "Test content");
    REQUIRE(restored.message_id == "msg-123");
    REQUIRE(restored.session_id == "session-456");
}

// ============================================================================
// Part Type Checking Tests
// ============================================================================

TEST_CASE("Part::type_checking", "[message][part]") {
    auto text_part = Part::create_text("text");
    auto tool_part = Part::create_tool("id", "name", {});
    auto reasoning_part = Part::create_reasoning("thought");
    auto file_part = Part::create_file("/path");
    auto subtask_part = Part::create_subtask("id", "agent", "status");
    auto step_start_part = Part::create_step_start("id");
    auto step_finish_part = Part::create_step_finish("id", "done");
    auto snapshot_part = Part::create_snapshot({});
    auto patch_part = Part::create_patch("/path", {});
    auto agent_part = Part::create_agent("id", "name");
    auto retry_part = Part::create_retry(1, "reason");
    auto compaction_part = Part::create_compaction(100, 50, {});
    
    REQUIRE(text_part.is_text());
    REQUIRE(tool_part.is_tool());
    REQUIRE(reasoning_part.is_reasoning());
    REQUIRE(file_part.is_file());
    REQUIRE(subtask_part.is_subtask());
    REQUIRE(step_start_part.is_step_start());
    REQUIRE(step_finish_part.is_step_finish());
    REQUIRE(snapshot_part.is_snapshot());
    REQUIRE(patch_part.is_patch());
    REQUIRE(agent_part.is_agent());
    REQUIRE(retry_part.is_retry());
    REQUIRE(compaction_part.is_compaction());
    
    // Cross-check: all should be false for other types
    REQUIRE_FALSE(text_part.is_tool());
    REQUIRE_FALSE(tool_part.is_text());
    REQUIRE_FALSE(reasoning_part.is_file());
}

// ============================================================================
// MessageInfo Tests
// ============================================================================

TEST_CASE("MessageInfo::serialization", "[message][info]") {
    MessageInfo info;
    info.id = "msg-001";
    info.session_id = "session-001";
    info.role = Role::Assistant;
    info.time_created = 1000000;
    info.time_updated = 1000001;
    info.agent = "build-agent";
    info.model_id = "gpt-4";
    info.provider_id = "openai";
    info.cost = 0.05;
    info.tokens.input = 100;
    info.tokens.output = 50;
    info.parent_id = "parent-msg";
    info.finish = "stop";
    
    nlohmann::json j = info.to_json();
    
    REQUIRE(j["id"] == "msg-001");
    REQUIRE(j["session_id"] == "session-001");
    REQUIRE(j["role"] == "assistant");
    REQUIRE(j["agent"] == "build-agent");
    REQUIRE(j["model_id"] == "gpt-4");
    REQUIRE(j["cost"] == Catch::Approx(0.05));
    REQUIRE(j["tokens"]["input"] == 100);
    REQUIRE(j["parent_id"] == "parent-msg");
    REQUIRE(j["finish"] == "stop");
    
    MessageInfo restored = MessageInfo::from_json(j);
    REQUIRE(restored.id == "msg-001");
    REQUIRE(restored.session_id == "session-001");
    REQUIRE(restored.role == Role::Assistant);
    REQUIRE(restored.tokens.input == 100);
    REQUIRE(restored.parent_id == "parent-msg");
    REQUIRE(restored.finish == "stop");
}

// ============================================================================
// Message Tests
// ============================================================================

TEST_CASE("Message::constructor", "[message]") {
    Message msg("session-001", Role::User, "build", "gpt-4", "openai");
    
    REQUIRE_FALSE(msg.id().empty());
    REQUIRE(msg.session_id() == "session-001");
    REQUIRE(msg.role() == Role::User);
    REQUIRE(msg.parts().empty());
}

TEST_CASE("Message::add_part", "[message]") {
    Message msg("session-001", Role::User, "build", "gpt-4", "openai");
    
    msg.add_part(Part::create_text("Hello"));
    msg.add_part(Part::create_text(" World"));
    
    REQUIRE(msg.parts().size() == 2);
    REQUIRE(msg.get_text() == "Hello World");
}

TEST_CASE("Message::add_text_convenience", "[message]") {
    Message msg("session-001", Role::User, "build", "gpt-4", "openai");
    
    msg.add_text("First line\n");
    msg.add_text("Second line");
    
    REQUIRE(msg.parts().size() == 2);
    REQUIRE(msg.get_text() == "First line\nSecond line");
}

TEST_CASE("Message::add_tool", "[message]") {
    Message msg("session-001", Role::Assistant, "build", "gpt-4", "openai");
    
    msg.add_tool("tool-1", "read_file", {{"path", "/src/main.cpp"}});
    
    REQUIRE(msg.has_tool_calls());
    REQUIRE(msg.get_tool_calls().size() == 1);
}

TEST_CASE("Message::add_reasoning", "[message]") {
    Message msg("session-001", Role::Assistant, "build", "gpt-4", "openai");
    
    msg.add_reasoning("Let me think about this...");
    msg.add_text("Here's the answer.");
    
    REQUIRE(msg.get_full_text() == "Let me think about this...Here's the answer.");
    REQUIRE(msg.get_text() == "Here's the answer.");
}

TEST_CASE("Message::get_parts_by_type", "[message]") {
    Message msg("session-001", Role::Assistant, "build", "gpt-4", "openai");
    
    msg.add_text("Text 1");
    msg.add_tool("tool-1", "name", {});
    msg.add_text("Text 2");
    msg.add_tool("tool-2", "name", {});
    
    auto text_parts = msg.get_parts(PartType::Text);
    auto tool_parts = msg.get_parts(PartType::Tool);
    
    REQUIRE(text_parts.size() == 2);
    REQUIRE(tool_parts.size() == 2);
}

TEST_CASE("Message::set_error", "[message]") {
    Message msg("session-001", Role::Assistant, "build", "gpt-4", "openai");
    
    nlohmann::json error = {
        {"code", "rate_limit"},
        {"message", "Too many requests"}
    };
    msg.set_error(error);
    
    REQUIRE(msg.info().error.has_value());
    REQUIRE((*msg.info().error)["code"] == "rate_limit");
}

TEST_CASE("Message::set_finish", "[message]") {
    Message msg("session-001", Role::Assistant, "build", "gpt-4", "openai");
    
    msg.set_finish("stop");
    
    REQUIRE(msg.info().finish.has_value());
    REQUIRE(*msg.info().finish == "stop");
}

TEST_CASE("Message::update_tokens", "[message]") {
    Message msg("session-001", Role::Assistant, "build", "gpt-4", "openai");
    
    TokenUsage usage1;
    usage1.input = 100;
    usage1.output = 50;
    
    nlohmann::json pricing = {
        {"input", 0.01},
        {"output", 0.03}
    };
    
    msg.update_tokens(usage1, pricing);
    
    REQUIRE(msg.tokens().input == 100);
    REQUIRE(msg.tokens().output == 50);
    REQUIRE(msg.cost() == Catch::Approx(100*0.01 + 50*0.03));
    
    // Add more tokens
    TokenUsage usage2;
    usage2.input = 50;
    usage2.output = 25;
    
    msg.update_tokens(usage2, pricing);
    
    REQUIRE(msg.tokens().input == 150);
    REQUIRE(msg.tokens().output == 75);
    REQUIRE(msg.cost() == Catch::Approx(150*0.01 + 75*0.03));
}

TEST_CASE("Message::serialization", "[message]") {
    Message msg("session-001", Role::Assistant, "build", "gpt-4", "openai");
    msg.add_text("Hello");
    msg.add_tool("tool-1", "test", {{"arg", "value"}});
    
    nlohmann::json j = msg.to_json();
    
    REQUIRE(j["session_id"] == "session-001");
    REQUIRE(j["role"] == "assistant");
    REQUIRE(j["parts"].size() == 2);
    REQUIRE(j["parts"][0]["type"] == "text");
    REQUIRE(j["parts"][1]["type"] == "tool");
    
    Message restored = Message::from_json(j);
    REQUIRE(restored.session_id() == "session-001");
    REQUIRE(restored.role() == Role::Assistant);
    REQUIRE(restored.parts().size() == 2);
}

// ============================================================================
// MessageDAO Tests
// ============================================================================

TEST_CASE("MessageDao::create_and_get", "[message][dao]") {
    MessageTestDatabase test_db;
    MessageDao dao(test_db.db());
    
    MessageInfo info;
    info.id = "msg-001";
    info.session_id = "session-001";
    info.role = Role::User;
    info.time_created = 1000000;
    info.time_updated = 1000000;
    info.agent = "build";
    info.model_id = "gpt-4";
    info.provider_id = "openai";
    
    dao.create_message(info);
    
    auto retrieved = dao.get_message("msg-001");
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->id == "msg-001");
    REQUIRE(retrieved->session_id == "session-001");
    REQUIRE(retrieved->role == Role::User);
    REQUIRE(retrieved->agent == "build");
}

TEST_CASE("MessageDao::update_message", "[message][dao]") {
    MessageTestDatabase test_db;
    MessageDao dao(test_db.db());
    
    MessageInfo info;
    info.id = "msg-002";
    info.session_id = "session-001";
    info.role = Role::Assistant;
    info.time_created = 1000000;
    info.time_updated = 1000000;
    info.agent = "build";
    info.model_id = "gpt-4";
    info.provider_id = "openai";
    
    dao.create_message(info);
    
    // Update
    info.time_updated = 2000000;
    info.cost = 0.05;
    info.finish = "stop";
    info.tokens.input = 100;
    info.tokens.output = 50;
    
    dao.update_message(info);
    
    auto retrieved = dao.get_message("msg-002");
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->cost == Catch::Approx(0.05));
    REQUIRE(retrieved->finish == "stop");
    REQUIRE(retrieved->tokens.input == 100);
}

TEST_CASE("MessageDao::delete_message", "[message][dao]") {
    MessageTestDatabase test_db;
    MessageDao dao(test_db.db());
    
    MessageInfo info;
    info.id = "msg-003";
    info.session_id = "session-001";
    info.role = Role::User;
    info.time_created = 1000000;
    info.time_updated = 1000000;
    info.agent = "build";
    info.model_id = "gpt-4";
    info.provider_id = "openai";
    
    dao.create_message(info);
    
    auto retrieved = dao.get_message("msg-003");
    REQUIRE(retrieved.has_value());
    
    dao.delete_message("msg-003");
    
    retrieved = dao.get_message("msg-003");
    REQUIRE_FALSE(retrieved.has_value());
}

TEST_CASE("MessageDao::list_messages_by_session", "[message][dao]") {
    MessageTestDatabase test_db;
    MessageDao dao(test_db.db());
    
    // Create multiple messages
    for (int i = 0; i < 5; i++) {
        MessageInfo info;
        info.id = "msg-" + std::to_string(i);
        info.session_id = "session-001";
        info.role = (i % 2 == 0) ? Role::User : Role::Assistant;
        info.time_created = 1000000 + i * 1000;
        info.time_updated = info.time_created;
        info.agent = "build";
        info.model_id = "gpt-4";
        info.provider_id = "openai";
        
        dao.create_message(info);
    }
    
    auto messages = dao.list_messages_by_session("session-001");
    REQUIRE(messages.size() == 5);
    
    // Test pagination
    auto page1 = dao.list_messages_by_session("session-001", 2, 0);
    REQUIRE(page1.size() == 2);
    
    auto page2 = dao.list_messages_by_session("session-001", 2, 2);
    REQUIRE(page2.size() == 2);
}

TEST_CASE("MessageDao::create_and_list_parts", "[message][dao]") {
    MessageTestDatabase test_db;
    MessageDao dao(test_db.db());
    
    // Create message first
    MessageInfo info;
    info.id = "msg-parts";
    info.session_id = "session-001";
    info.role = Role::Assistant;
    info.time_created = 1000000;
    info.time_updated = 1000000;
    info.agent = "build";
    info.model_id = "gpt-4";
    info.provider_id = "openai";
    dao.create_message(info);
    
    // Create parts
    auto text_part = Part::create_text("Hello");
    text_part.message_id = "msg-parts";
    text_part.session_id = "session-001";
    dao.create_part(text_part);
    
    auto tool_part = Part::create_tool("tool-1", "test", {});
    tool_part.message_id = "msg-parts";
    tool_part.session_id = "session-001";
    dao.create_part(tool_part);
    
    // List parts
    auto parts = dao.list_parts("msg-parts");
    REQUIRE(parts.size() == 2);
    REQUIRE(parts[0].is_text());
    REQUIRE(parts[1].is_tool());
}

TEST_CASE("MessageDao::delete_parts", "[message][dao]") {
    MessageTestDatabase test_db;
    MessageDao dao(test_db.db());
    
    // Create message
    MessageInfo info;
    info.id = "msg-del-parts";
    info.session_id = "session-001";
    info.role = Role::Assistant;
    info.time_created = 1000000;
    info.time_updated = 1000000;
    info.agent = "build";
    info.model_id = "gpt-4";
    info.provider_id = "openai";
    dao.create_message(info);
    
    // Create parts
    auto part = Part::create_text("Test");
    part.message_id = "msg-del-parts";
    part.session_id = "session-001";
    dao.create_part(part);
    
    REQUIRE(dao.list_parts("msg-del-parts").size() == 1);
    
    dao.delete_parts("msg-del-parts");
    
    REQUIRE(dao.list_parts("msg-del-parts").empty());
}

TEST_CASE("MessageDao::delete_messages_by_session", "[message][dao]") {
    MessageTestDatabase test_db;
    MessageDao dao(test_db.db());
    
    // Create messages for two sessions
    for (const auto& session : {"session-A", "session-B"}) {
        for (int i = 0; i < 3; i++) {
            MessageInfo info;
            info.id = std::string(session) + "-msg-" + std::to_string(i);
            info.session_id = session;
            info.role = Role::User;
            info.time_created = 1000000;
            info.time_updated = 1000000;
            info.agent = "build";
            info.model_id = "gpt-4";
            info.provider_id = "openai";
            dao.create_message(info);
        }
    }
    
    REQUIRE(dao.list_messages_by_session("session-A").size() == 3);
    REQUIRE(dao.list_messages_by_session("session-B").size() == 3);
    
    dao.delete_messages_by_session("session-A");
    
    REQUIRE(dao.list_messages_by_session("session-A").empty());
    REQUIRE(dao.list_messages_by_session("session-B").size() == 3);
}

// ============================================================================
// Message Persistence Integration Tests
// ============================================================================

TEST_CASE("Message::save_and_retrieve", "[message][integration]") {
    MessageTestDatabase test_db;
    
    // Create and save message
    Message msg("session-int", Role::User, "build", "gpt-4", "openai");
    msg.add_text("Hello, AI!");
    msg.set_database(test_db.db());
    msg.save();
    
    // Retrieve message
    auto retrieved = Message::get(msg.id(), test_db.db());
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->session_id() == "session-int");
    REQUIRE(retrieved->get_text() == "Hello, AI!");
}

TEST_CASE("Message::save_with_parts", "[message][integration]") {
    MessageTestDatabase test_db;
    
    Message msg("session-parts", Role::Assistant, "build", "gpt-4", "openai");
    msg.add_reasoning("Thinking...");
    msg.add_text("Here's the answer.");
    msg.add_tool("tool-1", "read_file", {{"path", "/test.cpp"}}, {{"content", "int main() {}"}});
    msg.set_database(test_db.db());
    msg.save();
    
    auto retrieved = Message::get(msg.id(), test_db.db());
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->parts().size() == 3);
    REQUIRE(retrieved->has_tool_calls());
    REQUIRE(retrieved->get_text() == "Here's the answer.");
}

TEST_CASE("Message::list_by_session", "[message][integration]") {
    MessageTestDatabase test_db;
    
    // Create multiple messages
    for (int i = 0; i < 5; i++) {
        Message msg("session-list", Role::User, "build", "gpt-4", "openai");
        msg.add_text("Message " + std::to_string(i));
        msg.set_database(test_db.db());
        msg.save();
    }
    
    auto messages = Message::list_by_session("session-list", test_db.db());
    REQUIRE(messages.size() == 5);
}

TEST_CASE("Message::remove", "[message][integration]") {
    MessageTestDatabase test_db;
    
    Message msg("session-del", Role::User, "build", "gpt-4", "openai");
    msg.add_text("To be deleted");
    msg.set_database(test_db.db());
    msg.save();
    
    std::string msg_id = msg.id();
    
    auto retrieved = Message::get(msg_id, test_db.db());
    REQUIRE(retrieved.has_value());
    
    msg.remove();
    
    retrieved = Message::get(msg_id, test_db.db());
    REQUIRE_FALSE(retrieved.has_value());
}

TEST_CASE("Message::remove_by_session", "[message][integration]") {
    MessageTestDatabase test_db;
    
    for (int i = 0; i < 3; i++) {
        Message msg("session-bulk-del", Role::User, "build", "gpt-4", "openai");
        msg.add_text("Message " + std::to_string(i));
        msg.set_database(test_db.db());
        msg.save();
    }
    
    REQUIRE(Message::list_by_session("session-bulk-del", test_db.db()).size() == 3);
    
    Message::remove_by_session("session-bulk-del", test_db.db());
    
    REQUIRE(Message::list_by_session("session-bulk-del", test_db.db()).empty());
}

// ============================================================================
// Utility Functions Tests
// ============================================================================

TEST_CASE("generate_uuid", "[message][util]") {
    std::string uuid = generate_uuid();
    
    REQUIRE_FALSE(uuid.empty());
    REQUIRE(uuid.length() == 36);  // Standard UUID format
    REQUIRE(uuid[8] == '-');
    REQUIRE(uuid[13] == '-');
    REQUIRE(uuid[18] == '-');
    REQUIRE(uuid[23] == '-');
    
    // UUIDs should be unique
    std::string uuid2 = generate_uuid();
    REQUIRE(uuid != uuid2);
}

TEST_CASE("current_timestamp_ms", "[message][util]") {
    int64_t ts1 = current_timestamp_ms();
    int64_t ts2 = current_timestamp_ms();
    
    REQUIRE(ts1 > 0);
    REQUIRE(ts2 >= ts1);  // Should be same or later
}

// ===== Additional A- level tests for Part system =====

TEST_CASE("PartType O(1) string parsing", "[message][part][performance]") {
    SECTION("all types parse correctly via hash map") {
        REQUIRE(part_type_from_string("text") == PartType::Text);
        REQUIRE(part_type_from_string("tool") == PartType::Tool);
        REQUIRE(part_type_from_string("reasoning") == PartType::Reasoning);
        REQUIRE(part_type_from_string("file") == PartType::File);
        REQUIRE(part_type_from_string("image") == PartType::Image);
        REQUIRE(part_type_from_string("error") == PartType::Error);
        REQUIRE(part_type_from_string("source") == PartType::Source);
        REQUIRE(part_type_from_string("subtask") == PartType::Subtask);
        REQUIRE(part_type_from_string("step_start") == PartType::StepStart);
        REQUIRE(part_type_from_string("step_finish") == PartType::StepFinish);
        REQUIRE(part_type_from_string("snapshot") == PartType::Snapshot);
        REQUIRE(part_type_from_string("patch") == PartType::Patch);
        REQUIRE(part_type_from_string("agent") == PartType::Agent);
        REQUIRE(part_type_from_string("retry") == PartType::Retry);
        REQUIRE(part_type_from_string("compaction") == PartType::Compaction);
    }

    SECTION("unknown type defaults to text") {
        REQUIRE(part_type_from_string("unknown_type") == PartType::Text);
    }
}

TEST_CASE("PartType to_string consistency", "[message][part]") {
    SECTION("round-trip conversion for all types") {
        REQUIRE(part_type_from_string(std::string(part_type_to_string(PartType::Text))) == PartType::Text);
        REQUIRE(part_type_from_string(std::string(part_type_to_string(PartType::Tool))) == PartType::Tool);
        REQUIRE(part_type_from_string(std::string(part_type_to_string(PartType::Reasoning))) == PartType::Reasoning);
        REQUIRE(part_type_from_string(std::string(part_type_to_string(PartType::File))) == PartType::File);
        REQUIRE(part_type_from_string(std::string(part_type_to_string(PartType::Image))) == PartType::Image);
        REQUIRE(part_type_from_string(std::string(part_type_to_string(PartType::Error))) == PartType::Error);
        REQUIRE(part_type_from_string(std::string(part_type_to_string(PartType::Source))) == PartType::Source);
    }
}

TEST_CASE("Role O(1) string parsing", "[message][role][performance]") {
    SECTION("all roles parse correctly via hash map") {
        REQUIRE(role_from_string("user") == Role::User);
        REQUIRE(role_from_string("assistant") == Role::Assistant);
        REQUIRE(role_from_string("system") == Role::System);
        REQUIRE(role_from_string("tool") == Role::Tool);  // C-3 fix
    }

    SECTION("unknown role defaults to user") {
        REQUIRE(role_from_string("unknown_role") == Role::User);
    }
}

TEST_CASE("Role to_string consistency", "[message][role]") {
    SECTION("round-trip conversion") {
        REQUIRE(role_from_string(std::string(role_to_string(Role::User))) == Role::User);
        REQUIRE(role_from_string(std::string(role_to_string(Role::Assistant))) == Role::Assistant);
        REQUIRE(role_from_string(std::string(role_to_string(Role::System))) == Role::System);
        REQUIRE(role_from_string(std::string(role_to_string(Role::Tool))) == Role::Tool);  // C-3 fix
    }
}

TEST_CASE("Part edge cases", "[message][part][edge]") {
    SECTION("text part with empty content") {
        auto part = Part::create_text("");
        REQUIRE(part.is_text());
        REQUIRE(part.get_text().empty());
    }

    SECTION("text part with special characters") {
        auto part = Part::create_text("Special: \"quotes\" \\backslash\\ \n newline \t tab");
        REQUIRE(part.get_text().find("quotes") != std::string::npos);
        REQUIRE(part.get_text().find("newline") != std::string::npos);
    }

    SECTION("text part with unicode") {
        auto part = Part::create_text("Unicode: 你好世界 🌍 مرحبا");
        REQUIRE(part.get_text().find("你好世界") != std::string::npos);
    }

    SECTION("tool part with empty arguments") {
        auto part = Part::create_tool("tool-1", "test", nlohmann::json::object());
        REQUIRE(part.is_tool());
        REQUIRE(part.get_tool()["arguments"].is_object());
        REQUIRE(part.get_tool()["arguments"].empty());
    }

    SECTION("tool part with complex nested arguments") {
        nlohmann::json complex_args = {
            {"filter", {{"field", "name"}, {"value", "test"}}},
            {"options", {{"case_sensitive", false}, {"limit", 10}}},
            {"metadata", nlohmann::json::array({"tag1", "tag2"})}
        };
        auto part = Part::create_tool("tool-2", "complex_search", complex_args);
        REQUIRE(part.is_tool());
        REQUIRE(part.get_tool()["arguments"]["filter"]["field"] == "name");
        REQUIRE(part.get_tool()["arguments"]["metadata"].size() == 2);
    }

    SECTION("error part with detailed information") {
        nlohmann::json details = {
            {"stack_trace", "at line 42 in test.cpp"},
            {"context", {{"var1", "value1"}}}
        };
        auto part = Part::create_error("Critical failure", "ERR_001", details);
        REQUIRE(part.is_error());
        REQUIRE(part.get_error()["code"] == "ERR_001");
        REQUIRE(part.get_error()["details"]["stack_trace"].is_string());
    }

    SECTION("image part with all optional fields") {
        auto part = Part::create_image(
            "https://example.com/image.png",
            "A sample image",
            "image/png"
        );
        REQUIRE(part.is_image());
        REQUIRE(part.get_image()["alt_text"] == "A sample image");
        REQUIRE(part.get_image()["mime_type"] == "image/png");
    }

    SECTION("source part with all optional fields") {
        auto part = Part::create_source(
            "src-001",
            "document",
            "API Reference",
            "https://docs.example.com/api"
        );
        REQUIRE(part.is_source());
        REQUIRE(part.get_source()["title"] == "API Reference");
    }
}

TEST_CASE("Part serialization comprehensive", "[message][part][serialization]") {
    SECTION("tool part with result roundtrip") {
        nlohmann::json args = {{"x", 10}};
        nlohmann::json result = {{"sum", 30}};
        auto original = Part::create_tool("t1", "add", args, result);
        
        auto json = original.to_json();
        auto restored = Part::from_json(json);
        
        REQUIRE(restored.is_tool());
        REQUIRE(restored.get_tool()["tool_id"] == "t1");
        REQUIRE(restored.get_tool()["result"]["sum"] == 30);
    }

    SECTION("compaction part roundtrip") {
        nlohmann::json summary = {
            {"key_points", nlohmann::json::array({"point1", "point2"})},
            {"action_items", nlohmann::json::array({"item1"})}
        };
        auto original = Part::create_compaction(50000, 5000, summary);
        
        auto json = original.to_json();
        auto restored = Part::from_json(json);
        
        REQUIRE(restored.is_compaction());
        REQUIRE(restored.get_compaction()["original_tokens"] == 50000);
        REQUIRE(restored.get_compaction()["summary"]["key_points"].size() == 2);
    }

    SECTION("agent part with model roundtrip") {
        auto original = Part::create_agent("agent-001", "CodeGenerator", "gpt-4-turbo");
        
        auto json = original.to_json();
        auto restored = Part::from_json(json);
        
        REQUIRE(restored.is_agent());
        REQUIRE(restored.get_agent()["agent_name"] == "CodeGenerator");
        REQUIRE(restored.get_agent()["model"] == "gpt-4-turbo");
    }

    SECTION("step finish with result roundtrip") {
        nlohmann::json result = {
            {"files_modified", 5},
            {"success", true}
        };
        auto original = Part::create_step_finish("step-1", "completed", result);
        
        auto json = original.to_json();
        auto restored = Part::from_json(json);
        
        REQUIRE(restored.is_step_finish());
        REQUIRE(restored.get_step()["status"] == "completed");
        REQUIRE(restored.get_step()["result"]["files_modified"] == 5);
    }
}

TEST_CASE("Part type checking comprehensive", "[message][part][type_check]") {
    SECTION("all type checks return correct boolean") {
        auto text = Part::create_text("text");
        auto tool = Part::create_tool("id", "name", {});
        auto reasoning = Part::create_reasoning("thought");
        auto file = Part::create_file("/path");
        auto image = Part::create_image("url");
        auto error = Part::create_error("msg");
        auto source = Part::create_source("id", "type");
        auto subtask = Part::create_subtask("id", "agent", "status");
        auto step_start = Part::create_step_start("id");
        auto step_finish = Part::create_step_finish("id", "done");
        auto snapshot = Part::create_snapshot({});
        auto patch = Part::create_patch("/path", {});
        auto agent = Part::create_agent("id", "name");
        auto retry = Part::create_retry(1, "reason");
        auto compaction = Part::create_compaction(100, 50, {});

        REQUIRE(text.is_text());
        REQUIRE_FALSE(text.is_tool());
        REQUIRE_FALSE(text.is_reasoning());

        REQUIRE(tool.is_tool());
        REQUIRE_FALSE(tool.is_text());
        REQUIRE_FALSE(tool.is_reasoning());

        REQUIRE(reasoning.is_reasoning());
        REQUIRE_FALSE(reasoning.is_text());

        REQUIRE(image.is_image());
        REQUIRE_FALSE(image.is_file());
        REQUIRE_FALSE(image.is_source());

        REQUIRE(error.is_error());
        REQUIRE_FALSE(error.is_text());

        REQUIRE(source.is_source());
        REQUIRE_FALSE(source.is_image());
        REQUIRE_FALSE(source.is_file());
    }
}
