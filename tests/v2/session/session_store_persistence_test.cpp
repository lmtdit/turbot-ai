/**
 * @file session_store_persistence_test.cpp
 * @brief Session 持久化测试
 */

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/session/session_store.hpp>
#include <turbot/core/session/session.hpp>
#include <turbot/storage/database.hpp>
#include <turbot/storage/sqlite_database.hpp>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <random>

namespace turbot::test {

using namespace turbot::storage;
using namespace turbot::storage::sqlite;

/// 生成唯一 ID
static std::string generate_unique_id() {
    static std::atomic<int> counter{0};
    return std::to_string(std::time(nullptr)) + "-" + std::to_string(++counter) + "-" + std::to_string(::getpid());
}

/// Session Store 测试夹具
struct SessionStoreFixture {
    std::filesystem::path test_dir;
    std::shared_ptr<Database> db;
    std::string unique_id;

    void setup() {
        unique_id = generate_unique_id();
        test_dir = std::filesystem::temp_directory_path() / ("turbot-store-" + unique_id);
        
        // 使用内存数据库避免文件系统问题
        DatabaseConfig config;
        config.path = ":memory:";
        db = std::make_shared<SQLiteDatabase>(config);
    }

    void teardown() {
        db.reset();
        if (std::filesystem::exists(test_dir)) {
            std::error_code ec;
            std::filesystem::remove_all(test_dir, ec);
        }
    }
};

} // namespace turbot::test

// ============================================================================
// Session Store 初始化测试
// ============================================================================

TEST_CASE("Session.Store.Init.Success", "[Session][Store]") {
    turbot::test::SessionStoreFixture fixture;
    fixture.setup();

    auto& store = turbot::core::session::SessionStore::instance();
    
    // 首次初始化
    REQUIRE_NOTHROW(store.init(fixture.db));
    REQUIRE(store.is_initialized());

    fixture.teardown();
}

TEST_CASE("Session.Store.Init.Idempotent", "[Session][Store]") {
    turbot::test::SessionStoreFixture fixture;
    fixture.setup();

    auto& store = turbot::core::session::SessionStore::instance();
    
    // 多次初始化应该安全
    store.init(fixture.db);
    REQUIRE(store.is_initialized());
    
    // 第二次初始化应该被忽略
    turbot::storage::DatabaseConfig config2;
    config2.path = (fixture.test_dir / "sessions2.db").string();
    auto db2 = std::make_shared<turbot::storage::sqlite::SQLiteDatabase>(config2);
    store.init(db2);  // 应该被忽略，不改变数据库

    fixture.teardown();
}

// ============================================================================
// Session Store 持久化测试
// ============================================================================

TEST_CASE("Session.Store.SaveAndFind", "[Session][Store]") {
    turbot::test::SessionStoreFixture fixture;
    fixture.setup();

    auto& store = turbot::core::session::SessionStore::instance();
    store.init(fixture.db);

    // 创建 Session
    turbot::core::session::CreateParams params;
    params.project_id = "test-project";
    params.slug = "test-slug";
    params.directory = fixture.test_dir.string();
    params.title = "Test Session";

    auto session_result = turbot::core::session::Session::create(params);
    REQUIRE(session_result.has_value());
    
    auto session = std::move(*session_result);
    auto info = session.info();

    // 保存
    REQUIRE(store.save(info));

    // 查找
    auto found = store.find_by_id(info.id);
    REQUIRE(found.has_value());
    REQUIRE((*found)["id"] == info.id);
    REQUIRE((*found)["project_id"] == info.project_id);

    fixture.teardown();
}

TEST_CASE("Session.Store.FindAll", "[Session][Store]") {
    turbot::test::SessionStoreFixture fixture;
    fixture.setup();

    auto& store = turbot::core::session::SessionStore::instance();
    store.init(fixture.db);

    // 创建多个 Session
    for (int i = 0; i < 3; ++i) {
        turbot::core::session::CreateParams params;
        params.project_id = "test-project";
        params.slug = "test-slug-" + std::to_string(i);
        params.directory = fixture.test_dir.string();
        params.title = "Test Session " + std::to_string(i);

        auto session_result = turbot::core::session::Session::create(params);
        REQUIRE(session_result.has_value());
        REQUIRE(store.save(session_result->info()));
    }

    // 查找所有
    auto all = store.find_all("test-project");
    REQUIRE(all.size() == 3);

    fixture.teardown();
}

TEST_CASE("Session.Store.FindAllPaginated", "[Session][Store]") {
    turbot::test::SessionStoreFixture fixture;
    fixture.setup();

    auto& store = turbot::core::session::SessionStore::instance();
    store.init(fixture.db);

    // 创建 5 个 Session
    for (int i = 0; i < 5; ++i) {
        turbot::core::session::CreateParams params;
        params.project_id = "paginated-project";
        params.slug = "page-slug-" + std::to_string(i);
        params.directory = fixture.test_dir.string();
        params.title = "Page Session " + std::to_string(i);

        auto session_result = turbot::core::session::Session::create(params);
        REQUIRE(session_result.has_value());
        REQUIRE(store.save(session_result->info()));
    }

    // 分页查询 - 第一页
    auto [page1, cursor1] = store.find_all_paginated("paginated-project", 3);
    REQUIRE(page1.size() == 3);
    
    // 分页查询 - 第二页
    if (cursor1.has_value()) {
        auto [page2, cursor2] = store.find_all_paginated("paginated-project", 3, cursor1);
        REQUIRE(page2.size() == 2);
        REQUIRE(!cursor2.has_value());  // 没有更多数据
    }

    fixture.teardown();
}

// ============================================================================
// Session Store 删除测试
// ============================================================================

TEST_CASE("Session.Store.Remove", "[Session][Store]") {
    turbot::test::SessionStoreFixture fixture;
    fixture.setup();

    auto& store = turbot::core::session::SessionStore::instance();
    store.init(fixture.db);

    // 创建并保存 Session
    turbot::core::session::CreateParams params;
    params.project_id = "remove-project";
    params.slug = "remove-slug";
    params.directory = fixture.test_dir.string();
    params.title = "Remove Session";

    auto session_result = turbot::core::session::Session::create(params);
    REQUIRE(session_result.has_value());
    auto info = session_result->info();
    REQUIRE(store.save(info));

    // 删除
    REQUIRE(store.remove(info.id));

    // 验证已删除
    auto found = store.find_by_id(info.id);
    REQUIRE(!found.has_value());

    fixture.teardown();
}

TEST_CASE("Session.Store.RemoveNonExistent", "[Session][Store]") {
    turbot::test::SessionStoreFixture fixture;
    fixture.setup();

    auto& store = turbot::core::session::SessionStore::instance();
    store.init(fixture.db);

    // 删除不存在的 Session
    REQUIRE(!store.remove("non-existent-id"));

    fixture.teardown();
}

// ============================================================================
// Session Store 消息持久化测试
// ============================================================================

TEST_CASE("Session.Store.SaveAndListMessages", "[Session][Store]") {
    turbot::test::SessionStoreFixture fixture;
    fixture.setup();

    auto& store = turbot::core::session::SessionStore::instance();
    store.init(fixture.db);

    // 创建 Session
    turbot::core::session::CreateParams params;
    params.project_id = "message-project";
    params.slug = "message-slug";
    params.directory = fixture.test_dir.string();
    params.title = "Message Session";

    auto session_result = turbot::core::session::Session::create(params);
    REQUIRE(session_result.has_value());
    auto session_id = session_result->info().id;
    store.save(session_result->info());

    // 保存消息
    nlohmann::json msg1 = {
        {"role", "user"},
        {"content", "Hello"}
    };
    nlohmann::json msg2 = {
        {"role", "assistant"},
        {"content", "Hi there!"}
    };

    REQUIRE(store.save_message(session_id, msg1));
    REQUIRE(store.save_message(session_id, msg2));

    // 查询消息
    auto messages = store.list_messages(session_id);
    REQUIRE(messages.size() == 2);

    fixture.teardown();
}

TEST_CASE("Session.Store.ListMessagesPaginated", "[Session][Store]") {
    turbot::test::SessionStoreFixture fixture;
    fixture.setup();

    auto& store = turbot::core::session::SessionStore::instance();
    store.init(fixture.db);

    // 创建 Session
    turbot::core::session::CreateParams params;
    params.project_id = "msg-page-project";
    params.slug = "msg-page-slug";
    params.directory = fixture.test_dir.string();
    params.title = "Message Pagination Session";

    auto session_result = turbot::core::session::Session::create(params);
    REQUIRE(session_result.has_value());
    auto session_id = session_result->info().id;
    store.save(session_result->info());

    // 保存 5 条消息
    for (int i = 0; i < 5; ++i) {
        nlohmann::json msg = {
            {"role", i % 2 == 0 ? "user" : "assistant"},
            {"content", "Message " + std::to_string(i)}
        };
        REQUIRE(store.save_message(session_id, msg));
    }

    // 分页查询 - 第一页
    auto [page1, cursor1] = store.list_messages_paginated(session_id, 2);
    REQUIRE(page1.size() == 2);
    REQUIRE(cursor1.has_value());

    // 分页查询 - 第二页
    auto [page2, cursor2] = store.list_messages_paginated(session_id, 2, cursor1);
    REQUIRE(page2.size() == 2);
    REQUIRE(cursor2.has_value());

    // 分页查询 - 第三页
    auto [page3, cursor3] = store.list_messages_paginated(session_id, 2, cursor2);
    REQUIRE(page3.size() == 1);
    REQUIRE(!cursor3.has_value());

    fixture.teardown();
}

// ============================================================================
// Session Store 错误处理测试
// ============================================================================

TEST_CASE("Session.Store.FindNonExistent", "[Session][Store]") {
    turbot::test::SessionStoreFixture fixture;
    fixture.setup();

    auto& store = turbot::core::session::SessionStore::instance();
    store.init(fixture.db);

    // 查找不存在的 Session
    auto found = store.find_by_id("non-existent-session-id");
    REQUIRE(!found.has_value());

    fixture.teardown();
}

TEST_CASE("Session.Store.FindAllEmptyProject", "[Session][Store]") {
    turbot::test::SessionStoreFixture fixture;
    fixture.setup();

    auto& store = turbot::core::session::SessionStore::instance();
    store.init(fixture.db);

    // 查找空项目的 Session
    auto all = store.find_all("empty-project");
    REQUIRE(all.empty());

    fixture.teardown();
}
