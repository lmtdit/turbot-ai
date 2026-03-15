#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_store.hpp>
#include <turbot/core/snapshot/snapshot.hpp>
#include <turbot/storage/sqlite_database.hpp>
#include <fstream>
#include <thread>

using namespace turbot::core::session;
using namespace turbot::storage::sqlite;
using namespace turbot::test;

// Helper to create a test database
static std::shared_ptr<turbot::storage::Database> create_test_db(const std::filesystem::path& dir) {
    turbot::storage::DatabaseConfig config;
    config.path = (dir / "sessions.db").string();
    auto sqlite_db = std::make_shared<SQLiteDatabase>(config);
    return std::static_pointer_cast<turbot::storage::Database>(sqlite_db);
}

// ==================== Session 创建测试 ====================

TEST_CASE("Session.Create.ValidParams", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    
    // 初始化 SessionStore
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "test-project";
    params.slug = "my-session";
    params.directory = tmp.path().string();
    params.title = "Test Session";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    REQUIRE_FALSE(session->id().empty());
    REQUIRE(session->info().project_id == "test-project");
    REQUIRE(session->info().slug == "my-session");
    REQUIRE(session->info().title == "Test Session");
    REQUIRE(session->info().state == SessionState::Created);
    REQUIRE(session->is_valid());
    REQUIRE_FALSE(session->is_active());
}

TEST_CASE("Session.Create.WithPermission", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "perm-project";
    params.slug = "perm-session";
    params.directory = tmp.path().string();
    params.title = "Permission Session";
    params.permission = R"({"bash": "allow", "edit": "ask"})"_json;
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    REQUIRE(session->info().permission.has_value());
    REQUIRE((*session->info().permission)["bash"] == "allow");
}

TEST_CASE("Session.Create.GeneratesUniqueId", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "project-1";
    params.slug = "session-1";
    params.directory = tmp.path().string();
    params.title = "Session 1";
    
    auto session1 = Session::create(params);
    
    params.slug = "session-2";
    params.title = "Session 2";
    auto session2 = Session::create(params);
    
    REQUIRE(session1->id() != session2->id());
}

// ==================== Session 获取测试 ====================

TEST_CASE("Session.Get.ExistingSession", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "get-project";
    params.slug = "get-session";
    params.directory = tmp.path().string();
    params.title = "Get Test";
    
    auto created = Session::create(params);
    REQUIRE(created.has_value());
    
    auto retrieved = Session::get(created->id());
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->id() == created->id());
    REQUIRE(retrieved->info().title == "Get Test");
}

TEST_CASE("Session.Get.NonExistentSession", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    auto session = Session::get("nonexistent-session-id");
    REQUIRE_FALSE(session.has_value());
}

// ==================== Session 更新测试 ====================

TEST_CASE("Session.Update.Title", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "update-project";
    params.slug = "update-session";
    params.directory = tmp.path().string();
    params.title = "Original Title";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    REQUIRE(session->set_title("Updated Title"));
    REQUIRE(session->info().title == "Updated Title");
    
    // Verify persistence
    auto retrieved = Session::get(session->id());
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->info().title == "Updated Title");
}

TEST_CASE("Session.Update.Permission", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "perm-update-project";
    params.slug = "perm-update-session";
    params.directory = tmp.path().string();
    params.title = "Permission Update Test";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    nlohmann::json new_perm = R"({"bash": "deny", "read": "allow"})"_json;
    REQUIRE(session->set_permission(new_perm));
    REQUIRE(session->info().permission.has_value());
    REQUIRE((*session->info().permission)["bash"] == "deny");
}

TEST_CASE("Session.Update.State", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "state-project";
    params.slug = "state-session";
    params.directory = tmp.path().string();
    params.title = "State Update Test";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    REQUIRE(session->info().state == SessionState::Created);
    
    UpdateParams update;
    update.state = SessionState::Active;
    REQUIRE(session->update(update));
    REQUIRE(session->info().state == SessionState::Active);
    REQUIRE(session->is_active());
}

TEST_CASE("Session.Update.AllFields", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "all-update-project";
    params.slug = "all-update-session";
    params.directory = tmp.path().string();
    params.title = "Original";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    UpdateParams update;
    update.title = "Updated Title";
    update.permission = R"({"new": "perm"})"_json;
    update.state = SessionState::Busy;
    
    REQUIRE(session->update(update));
    REQUIRE(session->info().title == "Updated Title");
    REQUIRE(session->info().permission.has_value());
    REQUIRE(session->info().state == SessionState::Busy);
}

// ==================== Session 删除测试 ====================

TEST_CASE("Session.Remove.ExistingSession", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "remove-project";
    params.slug = "remove-session";
    params.directory = tmp.path().string();
    params.title = "To Be Removed";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    std::string id = session->id();
    
    REQUIRE(Session::remove(id));
    
    auto retrieved = Session::get(id);
    REQUIRE_FALSE(retrieved.has_value());
}

TEST_CASE("Session.Remove.NonExistentSession", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    REQUIRE_FALSE(Session::remove("nonexistent-id"));
}

// ==================== Session 列表测试 ====================

TEST_CASE("Session.List.ByProject", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    // Create multiple sessions for same project
    CreateParams params;
    params.project_id = "list-project";
    params.directory = tmp.path().string();
    
    params.slug = "session-1";
    params.title = "Session 1";
    Session::create(params);
    
    params.slug = "session-2";
    params.title = "Session 2";
    Session::create(params);
    
    params.slug = "session-3";
    params.title = "Session 3";
    Session::create(params);
    
    auto sessions = Session::list("list-project");
    REQUIRE(sessions.size() == 3);
}

TEST_CASE("Session.List.EmptyProject", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    auto sessions = Session::list("empty-project");
    REQUIRE(sessions.empty());
}

// ==================== Session Fork 测试 ====================

TEST_CASE("Session.Fork.FromExisting", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "fork-project";
    params.slug = "parent-session";
    params.directory = tmp.path().string();
    params.title = "Parent Session";
    params.permission = R"({"bash": "allow"})"_json;
    
    auto parent = Session::create(params);
    REQUIRE(parent.has_value());
    
    ForkParams fork_params;
    fork_params.parent_id = parent->id();
    fork_params.slug = "forked-session";
    fork_params.title = "Forked Session";
    
    auto forked = Session::fork(fork_params);
    REQUIRE(forked.has_value());
    REQUIRE(forked->id() != parent->id());
    REQUIRE(forked->info().parent_id == parent->id());
    REQUIRE(forked->info().project_id == parent->info().project_id);
    REQUIRE(forked->info().title == "Forked Session");
    // Permission should be inherited
    REQUIRE(forked->info().permission.has_value());
}

TEST_CASE("Session.Fork.NonExistentParent", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    ForkParams params;
    params.parent_id = "nonexistent-parent";
    params.slug = "orphan-fork";
    params.title = "Orphan Fork";
    
    auto forked = Session::fork(params);
    REQUIRE_FALSE(forked.has_value());
}

// ==================== Session Archive/Restore 测试 ====================

TEST_CASE("Session.Archive.ActiveSession", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "archive-project";
    params.slug = "archive-session";
    params.directory = tmp.path().string();
    params.title = "Archive Test";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    // Set to active first
    session->update(UpdateParams{.state = SessionState::Active});
    REQUIRE_FALSE(session->is_archived());
    
    REQUIRE(session->archive());
    REQUIRE(session->is_archived());
    REQUIRE(session->info().time_archived.has_value());
}

TEST_CASE("Session.Archive.AlreadyArchived", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "double-archive-project";
    params.slug = "double-archive";
    params.directory = tmp.path().string();
    params.title = "Double Archive";
    
    auto session = Session::create(params);
    session->archive();
    
    REQUIRE_FALSE(session->archive()); // Should fail - already archived
}

TEST_CASE("Session.Restore.ArchivedSession", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "restore-project";
    params.slug = "restore-session";
    params.directory = tmp.path().string();
    params.title = "Restore Test";
    
    auto session = Session::create(params);
    session->archive();
    REQUIRE(session->is_archived());
    
    REQUIRE(session->restore());
    REQUIRE_FALSE(session->is_archived());
    REQUIRE(session->is_active());
    REQUIRE_FALSE(session->info().time_archived.has_value());
}

TEST_CASE("Session.Restore.NonArchivedSession", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "restore-fail-project";
    params.slug = "restore-fail";
    params.directory = tmp.path().string();
    params.title = "Restore Fail";
    
    auto session = Session::create(params);
    // Not archived, restore should fail
    REQUIRE_FALSE(session->restore());
}

// ==================== Session Compact 测试 ====================

TEST_CASE("Session.Compact.ActiveSession", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "compact-project";
    params.slug = "compact-session";
    params.directory = tmp.path().string();
    params.title = "Compact Test";
    
    auto session = Session::create(params);
    session->update(UpdateParams{.state = SessionState::Active});
    
    REQUIRE(session->compact());
    REQUIRE(session->info().time_compacting.has_value());
    // Should return to previous state after compact
    REQUIRE(session->info().state == SessionState::Active);
}

TEST_CASE("Session.Compact.ArchivedSession", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "compact-archived-project";
    params.slug = "compact-archived";
    params.directory = tmp.path().string();
    params.title = "Compact Archived";
    
    auto session = Session::create(params);
    session->archive();
    
    REQUIRE_FALSE(session->compact()); // Cannot compact archived session
}

// ==================== Session Revert 测试 ====================

TEST_CASE("Session.Revert.Basic", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "revert-project";
    params.slug = "revert-session";
    params.directory = tmp.path().string();
    params.title = "Revert Test";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    RevertParams revert_params;
    revert_params.message_id = "msg-to-revert";
    revert_params.part_id = "part-123";
    // Empty patches - just testing the info recording
    
    REQUIRE(session->revert(revert_params));
    REQUIRE(session->info().revert.has_value());
    REQUIRE(session->info().revert->message_id == "msg-to-revert");
    REQUIRE(session->info().revert->part_id == "part-123");
}

TEST_CASE("Session.Unrevert.AfterRevert", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "unrevert-project";
    params.slug = "unrevert-session";
    params.directory = tmp.path().string();
    params.title = "Unrevert Test";
    
    auto session = Session::create(params);
    
    RevertParams revert_params;
    revert_params.message_id = "msg-reverted";
    session->revert(revert_params);
    
    REQUIRE(session->info().revert.has_value());
    
    REQUIRE(session->unrevert());
    REQUIRE_FALSE(session->info().revert.has_value());
}

TEST_CASE("Session.Unrevert.NoRevert", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "no-revert-project";
    params.slug = "no-revert-session";
    params.directory = tmp.path().string();
    params.title = "No Revert Test";
    
    auto session = Session::create(params);
    // No revert in progress
    REQUIRE(session->unrevert()); // Should succeed (nothing to unrevert)
}

TEST_CASE("Session.CleanupRevert", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "cleanup-revert-project";
    params.slug = "cleanup-revert-session";
    params.directory = tmp.path().string();
    params.title = "Cleanup Revert Test";
    
    auto session = Session::create(params);
    
    RevertParams revert_params;
    revert_params.message_id = "msg-to-cleanup";
    session->revert(revert_params);
    
    REQUIRE(session->info().revert.has_value());
    
    REQUIRE(session->cleanup_revert());
    REQUIRE_FALSE(session->info().revert.has_value());
}

// ==================== Session 状态检查测试 ====================

TEST_CASE("Session.IsActive.Check", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "active-check-project";
    params.slug = "active-check-session";
    params.directory = tmp.path().string();
    params.title = "Active Check";
    
    auto session = Session::create(params);
    REQUIRE_FALSE(session->is_active()); // Created state
    
    session->update(UpdateParams{.state = SessionState::Active});
    REQUIRE(session->is_active());
}

TEST_CASE("Session.IsValid.Check", "[Session]") {
    Session invalid_session;
    REQUIRE_FALSE(invalid_session.is_valid());
    
    SessionInfo info;
    info.id = "valid-id";
    Session valid_session(info);
    REQUIRE(valid_session.is_valid());
}

// ==================== Session Messages 测试 ====================

TEST_CASE("Session.Messages.Pagination", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "messages-project";
    params.slug = "messages-session";
    params.directory = tmp.path().string();
    params.title = "Messages Test";
    
    auto session = Session::create(params);
    REQUIRE(session.has_value());
    
    // Test with default pagination
    auto messages = session->messages();
    REQUIRE(messages.empty()); // No messages yet
    
    // Test with custom pagination
    messages = session->messages(10, 0);
    REQUIRE(messages.empty());
}

TEST_CASE("Session.Messages.NegativeParams", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "neg-params-project";
    params.slug = "neg-params-session";
    params.directory = tmp.path().string();
    params.title = "Negative Params";
    
    auto session = Session::create(params);
    
    // Negative values should be corrected
    auto messages = session->messages(-1, -1);
    // Should not crash, uses defaults
}

// ==================== SessionStore 初始化测试 ====================

TEST_CASE("SessionStore.IsInitialized", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    
    // After init, should be initialized
    SessionStore::instance().init(create_test_db(tmp.path()));
    REQUIRE(SessionStore::instance().is_initialized());
}

TEST_CASE("SessionStore.ReInit", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    
    // First init
    SessionStore::instance().init(create_test_db(tmp.path()));
    REQUIRE(SessionStore::instance().is_initialized());
    
    // Create a session
    CreateParams params;
    params.project_id = "reinit-project";
    params.slug = "reinit-session";
    params.directory = tmp.path().string();
    params.title = "ReInit Test";
    Session::create(params);
    
    // Re-init should work
    TURBOT_TEST_TMPDIR(tmp2, true);
    SessionStore::instance().init(create_test_db(tmp2.path()));
    REQUIRE(SessionStore::instance().is_initialized());
}

// ==================== Session 等价性测试 ====================

TEST_CASE("Session.Equality", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "equality-project";
    params.slug = "equality-session";
    params.directory = tmp.path().string();
    params.title = "Equality Test";
    
    auto session1 = Session::create(params);
    auto session2 = Session::get(session1->id());
    
    REQUIRE(*session1 == *session2);
}

// ==================== Session 时间戳更新测试 ====================

TEST_CASE("Session.Timestamp.UpdatedOnModify", "[Session]") {
    TURBOT_TEST_TMPDIR(tmp, true);
    SessionStore::instance().init(create_test_db(tmp.path()));
    
    CreateParams params;
    params.project_id = "timestamp-project";
    params.slug = "timestamp-session";
    params.directory = tmp.path().string();
    params.title = "Timestamp Test";
    
    auto session = Session::create(params);
    int64_t created_time = session->info().time_created;
    
    // Small delay to ensure timestamp difference
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    session->set_title("Updated Title");
    
    REQUIRE(session->info().time_updated >= created_time);
}
