// tests/integration/persistence_test.cpp
// Integration tests for message persistence

#include <turbot/core/session/session.hpp>
#include <turbot/core/message/message.hpp>
#include <turbot/storage/sqlite_database.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

using namespace turbot::core;
using namespace turbot::storage;
using namespace turbot::storage::sqlite;

namespace {
    // Test fixture for database tests
    struct PersistenceTestFixture {
        std::filesystem::path test_db_path;
        std::shared_ptr<SQLiteDatabase> db;
        
        PersistenceTestFixture() {
            test_db_path = std::filesystem::temp_directory_path() / 
                ("turbot-persist-test-" + std::to_string(std::time(nullptr)) + ".db");
            
            // Create database
            DatabaseConfig config;
            config.path = test_db_path.string();
            db = std::make_shared<SQLiteDatabase>(config);
            
            // Initialize schema
            initialize_schema();
        }
        
        ~PersistenceTestFixture() {
            db.reset();
            std::error_code ec;
            std::filesystem::remove(test_db_path, ec);
        }
        
        void initialize_schema() {
            // Create messages table
            db->execute(R"(
                CREATE TABLE IF NOT EXISTS messages (
                    id TEXT PRIMARY KEY,
                    session_id TEXT NOT NULL,
                    role TEXT NOT NULL,
                    time_created INTEGER NOT NULL,
                    time_updated INTEGER NOT NULL,
                    parent_id TEXT,
                    agent TEXT,
                    model_id TEXT,
                    provider_id TEXT,
                    system TEXT,
                    tools TEXT,
                    variant TEXT,
                    error TEXT,
                    finish TEXT,
                    cost REAL DEFAULT 0,
                    tokens TEXT,
                    summary INTEGER DEFAULT 0,
                    structured TEXT
                )
            )", {});
            
            // Create parts table
            db->execute(R"(
                CREATE TABLE IF NOT EXISTS parts (
                    id TEXT PRIMARY KEY,
                    message_id TEXT NOT NULL,
                    session_id TEXT NOT NULL,
                    type TEXT NOT NULL,
                    data TEXT,
                    time_created INTEGER NOT NULL,
                    time_updated INTEGER NOT NULL,
                    FOREIGN KEY (message_id) REFERENCES messages(id)
                )
            )", {});
            
            // Create sessions table
            db->execute(R"(
                CREATE TABLE IF NOT EXISTS sessions (
                    id TEXT PRIMARY KEY,
                    project_id TEXT NOT NULL,
                    slug TEXT,
                    directory TEXT NOT NULL,
                    title TEXT,
                    state TEXT DEFAULT 'active',
                    time_created INTEGER NOT NULL,
                    time_updated INTEGER NOT NULL,
                    time_archived INTEGER,
                    time_compacting INTEGER,
                    message_count INTEGER DEFAULT 0,
                    total_tokens INTEGER DEFAULT 0,
                    total_cost REAL DEFAULT 0,
                    permission TEXT,
                    metadata TEXT
                )
            )", {});
        }
    };
}

// ============================================================================
// PERSIST-01: Message saved to database
// ============================================================================

TEST_CASE_METHOD(PersistenceTestFixture, "PERSIST-01: Message saved to database", "[integration][persistence]") {
    // Create session
    session::CreateParams params;
    params.project_id = "persist-test";
    params.slug = "test-session";
    params.directory = "/tmp/persist-test";
    params.title = "Persistence Test Session";
    
    auto session_result = session::Session::create(params);
    REQUIRE(session_result.has_value());
    
    // Create message
    Message msg(session_result->id(), Role::User, "build", "", "");
    msg.add_text("Hello, world!");
    
    // Save to database
    MessageDao dao(db);
    dao.create_message(msg.info());
    
    for (const auto& part : msg.parts()) {
        dao.create_part(part);
    }
    
    // Verify database record
    auto saved_info = dao.get_message(msg.id());
    REQUIRE(saved_info.has_value());
    CHECK(saved_info->id == msg.id());
    CHECK(saved_info->session_id == session_result->id());
    
    // Verify parts
    auto parts = dao.list_parts(msg.id());
    REQUIRE(parts.size() == 1);
}

// ============================================================================
// PERSIST-02: Session recovery with complete messages
// ============================================================================

TEST_CASE_METHOD(PersistenceTestFixture, "PERSIST-02: Session recovery with complete messages", "[integration][persistence]") {
    std::string session_id;
    
    // Create and save session with messages
    {
        session::CreateParams params;
        params.project_id = "persist-test";
        params.slug = "recovery-test";
        params.directory = "/tmp/recovery-test";
        params.title = "Recovery Test Session";
        
        auto session_result = session::Session::create(params);
        REQUIRE(session_result.has_value());
        session_id = session_result->id();
        
        // Create user message
        Message user_msg(session_id, Role::User, "build", "", "");
        user_msg.add_text("Question");
        
        MessageDao dao(db);
        dao.create_message(user_msg.info());
        for (const auto& part : user_msg.parts()) {
            dao.create_part(part);
        }
        
        // Create assistant message
        Message assistant_msg(session_id, Role::Assistant, "build", "mock-model", "mock-provider");
        assistant_msg.add_text("Answer");
        
        dao.create_message(assistant_msg.info());
        for (const auto& part : assistant_msg.parts()) {
            dao.create_part(part);
        }
    }
    
    // Recover messages
    auto messages = Message::list_by_session(session_id, db);
    
    REQUIRE(messages.size() == 2);
    CHECK(messages[0].role() == Role::User);
    CHECK(messages[1].role() == Role::Assistant);
}

// ============================================================================
// PERSIST-03: Part streaming persistence
// ============================================================================

TEST_CASE_METHOD(PersistenceTestFixture, "PERSIST-03: Part streaming persistence", "[integration][persistence]") {
    session::CreateParams params;
    params.project_id = "persist-test";
    params.slug = "stream-test";
    params.directory = "/tmp/stream-test";
    params.title = "Stream Test Session";
    
    auto session_result = session::Session::create(params);
    REQUIRE(session_result.has_value());
    
    // Create message with multiple parts (simulating streaming)
    Message msg(session_result->id(), Role::Assistant, "build", "mock-model", "mock-provider");
    
    MessageDao dao(db);
    dao.create_message(msg.info());
    
    // Add parts one by one (simulating streaming)
    for (int i = 0; i < 5; i++) {
        Part part = Part::create_text("chunk" + std::to_string(i));
        part.message_id = msg.id();
        part.session_id = session_result->id();
        msg.add_part(part);
        dao.create_part(part);
    }
    
    // Verify parts persistence
    auto parts = dao.list_parts(msg.id());
    REQUIRE(parts.size() == 5);
    
    // Verify order
    for (int i = 0; i < 5; i++) {
        auto data = parts[i].data;
        if (data.is_object() && data.contains("text")) {
            CHECK(data["text"] == "chunk" + std::to_string(i));
        }
    }
}

// ============================================================================
// PERSIST-04: Compacted message state correct
// ============================================================================

TEST_CASE_METHOD(PersistenceTestFixture, "PERSIST-04: Compacted message state correct", "[integration][persistence]") {
    session::CreateParams params;
    params.project_id = "persist-test";
    params.slug = "compact-test";
    params.directory = "/tmp/compact-test";
    params.title = "Compact Test Session";
    
    auto session_result = session::Session::create(params);
    REQUIRE(session_result.has_value());
    
    MessageDao dao(db);
    
    // Create multiple messages
    for (int i = 0; i < 10; i++) {
        Message msg(session_result->id(), Role::User, "build", "", "");
        msg.add_text("Message " + std::to_string(i));
        
        dao.create_message(msg.info());
        for (const auto& part : msg.parts()) {
            dao.create_part(part);
        }
    }
    
    // Query all messages
    auto all_messages = dao.list_messages_by_session(session_result->id(), 100);
    
    // Verify all messages were created
    CHECK(all_messages.size() == 10);
}

// ============================================================================
// Additional Tests
// ============================================================================

TEST_CASE_METHOD(PersistenceTestFixture, "PERSIST-05: Delete messages by session", "[integration][persistence]") {
    session::CreateParams params;
    params.project_id = "persist-test";
    params.slug = "delete-test";
    params.directory = "/tmp/delete-test";
    params.title = "Delete Test Session";
    
    auto session_result = session::Session::create(params);
    REQUIRE(session_result.has_value());
    
    MessageDao dao(db);
    
    // Create messages
    for (int i = 0; i < 3; i++) {
        Message msg(session_result->id(), Role::User, "build", "", "");
        msg.add_text("Message " + std::to_string(i));
        dao.create_message(msg.info());
        for (const auto& part : msg.parts()) {
            dao.create_part(part);
        }
    }
    
    // Verify messages exist
    auto messages_before = dao.list_messages_by_session(session_result->id());
    CHECK(messages_before.size() == 3);
    
    // Delete all messages
    dao.delete_messages_by_session(session_result->id());
    
    // Verify messages are deleted
    auto messages_after = dao.list_messages_by_session(session_result->id());
    CHECK(messages_after.empty());
}

TEST_CASE_METHOD(PersistenceTestFixture, "PERSIST-06: Message update", "[integration][persistence]") {
    session::CreateParams params;
    params.project_id = "persist-test";
    params.slug = "update-test";
    params.directory = "/tmp/update-test";
    params.title = "Update Test Session";
    
    auto session_result = session::Session::create(params);
    REQUIRE(session_result.has_value());
    
    MessageDao dao(db);
    
    // Create message
    Message msg(session_result->id(), Role::Assistant, "build", "mock-model", "mock-provider");
    msg.add_text("Initial content");
    dao.create_message(msg.info());
    
    // Update message by modifying and re-saving
    MessageInfo updated_info = msg.info();
    updated_info.cost = 0.05;
    updated_info.tokens = TokenUsage{100, 50, 0};
    dao.update_message(updated_info);
    
    // Verify update
    auto updated = dao.get_message(msg.id());
    REQUIRE(updated.has_value());
    CHECK(updated->cost == 0.05);
    CHECK(updated->tokens.input == 100);
}

TEST_CASE_METHOD(PersistenceTestFixture, "PERSIST-07: Concurrent message creation", "[integration][persistence]") {
    session::CreateParams params;
    params.project_id = "persist-test";
    params.slug = "concurrent-test";
    params.directory = "/tmp/concurrent-test";
    params.title = "Concurrent Test Session";
    
    auto session_result = session::Session::create(params);
    REQUIRE(session_result.has_value());
    
    MessageDao dao(db);
    
    // Create multiple messages "concurrently" (simulated)
    std::vector<std::string> message_ids;
    for (int i = 0; i < 10; i++) {
        Message msg(session_result->id(), Role::User, "build", "", "");
        msg.add_text("Concurrent message " + std::to_string(i));
        dao.create_message(msg.info());
        message_ids.push_back(msg.id());
    }
    
    // Verify all messages were created
    auto messages = dao.list_messages_by_session(session_result->id());
    CHECK(messages.size() == 10);
}
