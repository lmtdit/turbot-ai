#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_store.hpp>
#include <turbot/core/message/message.hpp>

using namespace turbot::core::session;
using namespace turbot::test;

// ==================== SessionInfo 测试 ====================

TEST_CASE("Session.Info.JsonSerialization", "[Session]") {
    SessionInfo info;
    info.id = "session-123";
    info.project_id = "project-456";
    info.slug = "my-session";
    info.directory = "/home/user/project";
    info.title = "Test Session";
    info.version = "1.0";
    info.state = SessionState::Active;
    info.time_created = 1700000000;
    info.time_updated = 1700000100;
    
    nlohmann::json j = info.to_json();
    REQUIRE(j["id"] == "session-123");
    REQUIRE(j["project_id"] == "project-456");
    REQUIRE(j["slug"] == "my-session");
    REQUIRE(j["directory"] == "/home/user/project");
    REQUIRE(j["title"] == "Test Session");
    REQUIRE(j["state"] == "active");
    
    SessionInfo restored = SessionInfo::from_json(j);
    REQUIRE(restored.id == info.id);
    REQUIRE(restored.project_id == info.project_id);
    REQUIRE(restored.slug == info.slug);
}

TEST_CASE("Session.Info.StateConversion", "[Session]") {
    REQUIRE(session_state_to_string(SessionState::Created) == "created");
    REQUIRE(session_state_to_string(SessionState::Active) == "active");
    REQUIRE(session_state_to_string(SessionState::Busy) == "busy");
    REQUIRE(session_state_to_string(SessionState::Compacting) == "compacting");
    REQUIRE(session_state_to_string(SessionState::Archived) == "archived");
    
    REQUIRE(string_to_session_state("created") == SessionState::Created);
    REQUIRE(string_to_session_state("active") == SessionState::Active);
    REQUIRE(string_to_session_state("busy") == SessionState::Busy);
    REQUIRE(string_to_session_state("compacting") == SessionState::Compacting);
    REQUIRE(string_to_session_state("archived") == SessionState::Archived);
}

TEST_CASE("Session.Info.WithParent", "[Session]") {
    SessionInfo info;
    info.id = "child-session";
    info.project_id = "project-123";
    info.parent_id = "parent-session";
    info.slug = "forked-session";
    
    nlohmann::json j = info.to_json();
    REQUIRE(j["parent_id"] == "parent-session");
    
    SessionInfo restored = SessionInfo::from_json(j);
    REQUIRE(restored.parent_id.has_value());
    REQUIRE(*restored.parent_id == "parent-session");
}

TEST_CASE("Session.Info.WithRevert", "[Session]") {
    SessionInfo info;
    info.id = "session-with-revert";
    info.project_id = "project-123";
    info.slug = "revert-test";
    info.revert = RevertInfo{
        "message-456",           // message_id
        "part-789",              // part_id
        "snapshot-001",          // snapshot_id
        "--- a/file.txt\n...",   // diff
        std::nullopt             // pre_patch
    };
    
    nlohmann::json j = info.to_json();
    REQUIRE(j.contains("revert"));
    // OpenCode compatible camelCase keys
    REQUIRE(j["revert"]["messageID"] == "message-456");
    
    SessionInfo restored = SessionInfo::from_json(j);
    REQUIRE(restored.revert.has_value());
    REQUIRE(restored.revert->message_id == "message-456");
    REQUIRE(restored.revert->part_id == "part-789");
}

// ==================== RevertInfo 测试 ====================

TEST_CASE("Session.RevertInfo.JsonSerialization", "[Session]") {
    RevertInfo revert{
        "msg-123",
        "part-456",
        "snap-789",
        "diff content here",
        std::nullopt
    };
    
    nlohmann::json j = revert.to_json();
    // OpenCode compatible camelCase keys
    REQUIRE(j["messageID"] == "msg-123");
    REQUIRE(j["partID"] == "part-456");
    REQUIRE(j["snapshot"] == "snap-789");
    REQUIRE(j["diff"] == "diff content here");
    
    RevertInfo restored = RevertInfo::from_json(j);
    REQUIRE(restored.message_id == revert.message_id);
    REQUIRE(restored.part_id == revert.part_id);
    REQUIRE(restored.snapshot_id == revert.snapshot_id);
    REQUIRE(restored.diff == revert.diff);
}

TEST_CASE("Session.RevertInfo.OptionalFields", "[Session]") {
    RevertInfo revert{
        "msg-123",
        std::nullopt,  // no part_id
        std::nullopt,  // no snapshot_id
        std::nullopt,  // no diff
        std::nullopt   // no pre_patch
    };
    
    nlohmann::json j = revert.to_json();
    // OpenCode compatible camelCase keys
    REQUIRE(j["messageID"] == "msg-123");
    REQUIRE_FALSE(j.contains("partID"));
    REQUIRE_FALSE(j.contains("snapshot"));
    REQUIRE_FALSE(j.contains("diff"));
    
    RevertInfo restored = RevertInfo::from_json(j);
    REQUIRE_FALSE(restored.part_id.has_value());
    REQUIRE_FALSE(restored.snapshot_id.has_value());
    REQUIRE_FALSE(restored.diff.has_value());
}

TEST_CASE("Session.RevertInfo.Equality", "[Session]") {
    RevertInfo r1{"msg-1", "part-1", "snap-1", "diff-1", std::nullopt};
    RevertInfo r2{"msg-1", "part-1", "snap-1", "diff-1", std::nullopt};
    RevertInfo r3{"msg-2", "part-1", "snap-1", "diff-1", std::nullopt};
    
    REQUIRE(r1 == r2);
    REQUIRE_FALSE(r1 == r3);
}

// ==================== CreateParams 测试 ====================

TEST_CASE("Session.CreateParams.Defaults", "[Session]") {
    CreateParams params;
    // 默认值应该是空字符串或 nullopt
    REQUIRE(params.project_id.empty());
    REQUIRE(params.slug.empty());
    REQUIRE(params.directory.empty());
    REQUIRE(params.title.empty());
    REQUIRE_FALSE(params.permission.has_value());
}

// ==================== SessionInfo 时间戳测试 ====================

TEST_CASE("Session.Info.Timestamps", "[Session]") {
    SessionInfo info;
    info.id = "ts-session";
    info.project_id = "project-123";
    info.slug = "ts-test";
    info.time_created = 1700000000;
    info.time_updated = 1700000100;
    info.time_compacting = 1700000200;
    info.time_archived = 1700000300;
    
    nlohmann::json j = info.to_json();
    REQUIRE(j["time_created"] == 1700000000);
    REQUIRE(j["time_updated"] == 1700000100);
    REQUIRE(j["time_compacting"] == 1700000200);
    REQUIRE(j["time_archived"] == 1700000300);
    
    SessionInfo restored = SessionInfo::from_json(j);
    REQUIRE(restored.time_created == info.time_created);
    REQUIRE(restored.time_updated == info.time_updated);
    REQUIRE(restored.time_compacting == info.time_compacting);
    REQUIRE(restored.time_archived == info.time_archived);
}

// ==================== SessionInfo 权限配置测试 ====================

TEST_CASE("Session.Info.PermissionConfig", "[Session]") {
    SessionInfo info;
    info.id = "perm-session";
    info.project_id = "project-123";
    info.slug = "perm-test";
    info.permission = nlohmann::json{
        {"bash", "allow"},
        {"edit", "ask"},
        {"task", {{"*", "deny"}, {"general", "allow"}}}
    };
    
    nlohmann::json j = info.to_json();
    REQUIRE(j.contains("permission"));
    REQUIRE(j["permission"]["bash"] == "allow");
    REQUIRE(j["permission"]["edit"] == "ask");
    
    SessionInfo restored = SessionInfo::from_json(j);
    REQUIRE(restored.permission.has_value());
    REQUIRE((*restored.permission)["bash"] == "allow");
}

// ==================== Session 状态转换测试 ====================

TEST_CASE("Session.StateTransitions", "[Session]") {
    // 测试状态字符串转换
    REQUIRE(string_to_session_state("created") == SessionState::Created);
    REQUIRE(string_to_session_state("active") == SessionState::Active);
    REQUIRE(string_to_session_state("busy") == SessionState::Busy);
    REQUIRE(string_to_session_state("compacting") == SessionState::Compacting);
    REQUIRE(string_to_session_state("archived") == SessionState::Archived);
}

// ==================== SessionInfo 等价性测试 ====================

TEST_CASE("Session.Info.Equality", "[Session]") {
    SessionInfo info1;
    info1.id = "session-1";
    info1.project_id = "project-1";
    info1.slug = "test-slug";
    info1.directory = "/test/dir";
    info1.title = "Test";
    info1.version = "1.0";
    info1.state = SessionState::Active;
    info1.time_created = 1700000000;
    info1.time_updated = 1700000100;
    
    SessionInfo info2 = info1;
    REQUIRE(info1 == info2);
    
    info2.title = "Different Title";
    REQUIRE_FALSE(info1 == info2);
}

// ==================== SessionInfo 版本测试 ====================

TEST_CASE("Session.Info.Version", "[Session]") {
    SessionInfo info;
    info.id = "version-session";
    info.project_id = "project-123";
    info.slug = "version-test";
    info.version = "2.0.0";
    
    nlohmann::json j = info.to_json();
    REQUIRE(j["version"] == "2.0.0");
    
    SessionInfo restored = SessionInfo::from_json(j);
    REQUIRE(restored.version == "2.0.0");
}
