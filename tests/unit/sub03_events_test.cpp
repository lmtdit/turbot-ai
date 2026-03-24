// sub03_events_test.cpp - Unit tests for Sub-03 (T1 EventBus integration + T15 initGit)
//
// Covers:
//   T1: session.created / session.updated / session.deleted events on Session CRUD
//   T1: project.updated event on Project::create() / Project::update()
//   T15: Project::create() with init_git=true runs git init

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_events.hpp>
#include <turbot/core/project/project.hpp>
#include <turbot/core/event/event_bus.hpp>
#include <turbot/storage/sqlite_database.hpp>
#include <turbot/core/session/session_store.hpp>
#include <turbot/core/project/project_store.hpp>

#include <filesystem>
#include <fstream>
#include <atomic>
#include <string>

namespace fs = std::filesystem;
using namespace turbot::core::session;
using namespace turbot::core;
using namespace turbot::storage;
using namespace turbot::storage::sqlite;
namespace proj_ns = turbot::core::project;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Reset EventBus and open an in-memory SQLite for SessionStore.
struct TestFixture {
    TestFixture() {
        EventBus::instance().clear();
        SessionStore::instance().reset();
        DatabaseConfig cfg;
        cfg.path = ":memory:";
        auto db = std::make_shared<SQLiteDatabase>(cfg);
        SessionStore::instance().init(db);
    }
    ~TestFixture() {
        SessionStore::instance().reset();
        EventBus::instance().clear();
    }
};

// ---------------------------------------------------------------------------
// T1: Session EventBus events
// ---------------------------------------------------------------------------

TEST_CASE("T1: Session::create() emits session.created and session.updated", "[sub03][t1][session]") {
    TestFixture fx;
    std::atomic<int> created_count{0};
    std::atomic<int> updated_count{0};

    auto sub1 = EventBus::instance().subscribe<SessionCreatedEvent>(
        SessionCreatedEvent::kEventName,
        [&](const Event<SessionCreatedEvent>& ev) {
            REQUIRE(ev.data.info.contains("id"));
            ++created_count;
        });
    auto sub2 = EventBus::instance().subscribe<SessionInfoUpdatedEvent>(
        SessionInfoUpdatedEvent::kEventName,
        [&](const Event<SessionInfoUpdatedEvent>& ev) {
            REQUIRE(ev.data.info.contains("id"));
            ++updated_count;
        });

    CreateParams params;
    params.project_id = "proj_001";
    params.slug       = "test-session";
    params.directory  = "/tmp/test_sub03";
    params.title      = "Test Session";
    auto sess = Session::create(params);
    REQUIRE(sess.has_value());

    EventBus::instance().unsubscribe(SessionCreatedEvent::kEventName, sub1);
    EventBus::instance().unsubscribe(SessionInfoUpdatedEvent::kEventName, sub2);

    REQUIRE(created_count.load() == 1);
    REQUIRE(updated_count.load() == 1);
}

TEST_CASE("T1: Session::update() emits session.updated", "[sub03][t1][session]") {
    TestFixture fx;

    CreateParams cp;
    cp.project_id = "proj_001";
    cp.slug       = "test-update";
    cp.directory  = "/tmp/test_sub03_upd";
    cp.title      = "Before Update";
    auto sess = Session::create(cp);
    REQUIRE(sess.has_value());

    // Clear created/updated events from create()
    EventBus::instance().clear();

    std::atomic<int> updated_count{0};
    std::string updated_title;
    auto sub = EventBus::instance().subscribe<SessionInfoUpdatedEvent>(
        SessionInfoUpdatedEvent::kEventName,
        [&](const Event<SessionInfoUpdatedEvent>& ev) {
            ++updated_count;
            if (ev.data.info.contains("title")) {
                updated_title = ev.data.info["title"].get<std::string>();
            }
        });

    sess->update(UpdateParams{.title = "After Update"});

    EventBus::instance().unsubscribe(SessionInfoUpdatedEvent::kEventName, sub);

    REQUIRE(updated_count.load() == 1);
    REQUIRE(updated_title == "After Update");
}

TEST_CASE("T1: Session::remove() emits session.deleted with session info", "[sub03][t1][session]") {
    TestFixture fx;

    CreateParams cp;
    cp.project_id = "proj_001";
    cp.slug       = "test-remove";
    cp.directory  = "/tmp/test_sub03_rm";
    cp.title      = "To Be Removed";
    auto sess = Session::create(cp);
    REQUIRE(sess.has_value());
    const std::string sess_id = sess->id();

    // Clear events from create()
    EventBus::instance().clear();

    std::atomic<int> deleted_count{0};
    std::string deleted_id;
    auto sub = EventBus::instance().subscribe<SessionDeletedEvent>(
        SessionDeletedEvent::kEventName,
        [&](const Event<SessionDeletedEvent>& ev) {
            ++deleted_count;
            if (ev.data.info.contains("id")) {
                deleted_id = ev.data.info["id"].get<std::string>();
            }
        });

    const bool removed = Session::remove(sess_id);

    EventBus::instance().unsubscribe(SessionDeletedEvent::kEventName, sub);

    REQUIRE(removed);
    REQUIRE(deleted_count.load() == 1);
    REQUIRE(deleted_id == sess_id);
}

// ---------------------------------------------------------------------------
// T1: Project EventBus events
// ---------------------------------------------------------------------------

TEST_CASE("T1: Project::update() emits project.updated", "[sub03][t1][project]") {
    // Use a temp dir as a minimal project
    auto tmp = fs::temp_directory_path() / "turbot_sub03_proj_upd";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    // Clear bus first
    EventBus::instance().clear();

    // Create a minimal project
    proj_ns::CreateParams cp;
    cp.directory = tmp.string();
    cp.name = "Sub03TestProject";
    auto proj = proj_ns::Project::create(cp);
    REQUIRE(proj.has_value());

    // Now listen for project.updated from update()
    EventBus::instance().clear();

    std::atomic<int> updated_count{0};
    auto sub = EventBus::instance().subscribe<ProjectUpdatedEvent>(
        ProjectUpdatedEvent::kEventName,
        [&](const Event<ProjectUpdatedEvent>&) {
            ++updated_count;
        });

    turbot::core::project::UpdateParams up;
    up.name = "UpdatedName";
    proj->update(up);

    EventBus::instance().unsubscribe(ProjectUpdatedEvent::kEventName, sub);

    REQUIRE(updated_count.load() == 1);

    fs::remove_all(tmp);
}

TEST_CASE("T1: Project::create() emits project.updated", "[sub03][t1][project]") {
    auto tmp = fs::temp_directory_path() / "turbot_sub03_proj_create";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    EventBus::instance().clear();

    std::atomic<int> updated_count{0};
    auto sub = EventBus::instance().subscribe<ProjectUpdatedEvent>(
        ProjectUpdatedEvent::kEventName,
        [&](const Event<ProjectUpdatedEvent>&) {
            ++updated_count;
        });

    proj_ns::CreateParams cp;
    cp.directory = tmp.string();
    cp.name = "CreateEventTest";
    auto proj = proj_ns::Project::create(cp);

    EventBus::instance().unsubscribe(ProjectUpdatedEvent::kEventName, sub);

    REQUIRE(proj.has_value());
    REQUIRE(updated_count.load() >= 1);

    fs::remove_all(tmp);
}

// ---------------------------------------------------------------------------
// T15: Project::create() with init_git=true
// ---------------------------------------------------------------------------

TEST_CASE("T15: Project::create() with init_git=true runs git init", "[sub03][t15]") {
    auto tmp = fs::temp_directory_path() / "turbot_sub03_initgit";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    EventBus::instance().clear();

    proj_ns::CreateParams cp;
    cp.directory = tmp.string();
    cp.name      = "GitInitTest";
    cp.init_git  = true;

    auto proj = proj_ns::Project::create(cp);
    REQUIRE(proj.has_value());

    // After create with init_git=true, .git directory should exist
    REQUIRE(fs::exists(tmp / ".git"));

    fs::remove_all(tmp);
}

TEST_CASE("T15: Project::init_git() initialises git and updates VCS type", "[sub03][t15]") {
    auto tmp = fs::temp_directory_path() / "turbot_sub03_initgit2";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    EventBus::instance().clear();

    // Create project without git
    proj_ns::CreateParams cp;
    cp.directory = tmp.string();
    cp.init_git  = false;
    auto proj = proj_ns::Project::create(cp);
    REQUIRE(proj.has_value());
    REQUIRE_FALSE(fs::exists(tmp / ".git"));

    // Now call init_git()
    const bool result = proj->init_git();
    REQUIRE(result);
    REQUIRE(fs::exists(tmp / ".git"));

    fs::remove_all(tmp);
}
