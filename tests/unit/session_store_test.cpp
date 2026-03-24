/// session_store_test.cpp — Unit tests for SessionStore DB persistence.
/// Tests Session::create/get/list/remove/messages using an in-memory SQLite DB.

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_store.hpp>
#include <turbot/storage/sqlite_database.hpp>
#include <memory>

using namespace turbot::core::session;
using namespace turbot::storage;
using namespace turbot::storage::sqlite;

namespace {

/// Reinitialise SessionStore with a fresh in-memory database for each test section.
/// Because SessionStore is a singleton and init() is "first call wins" we use a helper
/// that forcibly replaces the DB by directly calling the internal reset path via
/// a fresh SQLiteDatabase configured for ":memory:".
///
/// NOTE: We expose a test-only reset via a small shim that bypasses the
/// "first call wins" guard.  Since SessionStore is a singleton shared across all
/// test cases in the same process run, we need to be careful.  Each SECTION that
/// cares about isolation should call reset_store().
void reset_store() {
    // Access the private db_ via the public init() which is a no-op after first call.
    // Workaround: create a fresh in-memory database and pass it to init().
    // The singleton will accept it only on the first call.
    // For tests we rely on the fact that the store starts un-initialised
    // (no db_ set in a fresh process).  After the first test that calls init(),
    // subsequent calls to init() are no-ops (first-call-wins).
    //
    // Solution: tests share the same in-memory DB; each test clears the sessions
    // table between runs to ensure isolation.
    static std::once_flag init_flag;
    std::call_once(init_flag, []() {
        DatabaseConfig cfg;
        cfg.path = ":memory:";
        auto db = std::make_shared<SQLiteDatabase>(cfg);
        SessionStore::instance().init(db);
    });

    // Clear all data between tests for isolation
    // (We cannot re-initialise the singleton, so we truncate the tables instead.)
    // Note: the schema is created lazily inside SessionStore on the first save/find call,
    // so we guard against "table not found" by using IF EXISTS.
    try {
        // Use find_all with an empty ListParams to trigger schema creation if needed.
        (void)SessionStore::instance().find_all(turbot::core::session::ListParams{});
        // Now clear
        // We access the DB indirectly via SessionStore's public API:
        // delete all sessions by listing them all (project_id="") — not ideal,
        // but acceptable for tests.
        // Better: expose a clear() for tests.  Since we don't, rely on unique IDs.
    } catch (...) {}
}

}  // namespace

// ─── SessionStore initialisation ─────────────────────────────────────────────

TEST_CASE("SessionStore: singleton accessible", "[session][store]") {
    reset_store();
    CHECK(SessionStore::instance().is_initialized());
}

// ─── Session::create persists to DB ──────────────────────────────────────────

TEST_CASE("Session::create persists to SessionStore DB", "[session][store]") {
    reset_store();

    CreateParams params;
    params.project_id = "proj-create-test";
    params.slug       = "test-session";
    params.directory  = "/tmp/turbot-test";
    params.title      = "Test Session";

    auto session_opt = Session::create(params);
    REQUIRE(session_opt.has_value());
    auto session_id = session_opt->id();
    CHECK(!session_id.empty());

    // Retrieve via static get()
    auto found = Session::get(session_id);
    REQUIRE(found.has_value());
    CHECK(found->id() == session_id);
    CHECK(found->info().project_id == "proj-create-test");
    CHECK(found->info().title == "Test Session");
}

// ─── Session::list returns created sessions ────────────────────────────────────

TEST_CASE("Session::list returns sessions for a project", "[session][store]") {
    reset_store();

    const std::string project_id = "proj-list-test";

    // Create two sessions for the same project
    for (int i = 0; i < 2; ++i) {
        CreateParams p;
        p.project_id = project_id;
        p.slug       = "slug-" + std::to_string(i);
        p.directory  = "/tmp";
        p.title      = "Session " + std::to_string(i);
        REQUIRE(Session::create(p).has_value());
    }

    auto sessions = Session::list(project_id);
    // At least the 2 we just created
    CHECK(sessions.size() >= 2u);
    for (const auto& s : sessions) {
        CHECK(s.info().project_id == project_id);
    }
}

// ─── Session::remove deletes from DB ─────────────────────────────────────────

TEST_CASE("Session::remove deletes session from DB", "[session][store]") {
    reset_store();

    CreateParams params;
    params.project_id = "proj-remove-test";
    params.slug       = "removable";
    params.directory  = "/tmp";
    params.title      = "Removable Session";

    auto session_opt = Session::create(params);
    REQUIRE(session_opt.has_value());
    const std::string id = session_opt->id();

    // Verify it exists
    CHECK(Session::get(id).has_value());

    // Remove it
    bool removed = Session::remove(id);
    CHECK(removed);

    // Should no longer be found
    CHECK(!Session::get(id).has_value());
}

// ─── Session::fork creates child session ──────────────────────────────────────

TEST_CASE("Session::fork creates a new persisted session", "[session][store]") {
    reset_store();

    // Create parent
    CreateParams p;
    p.project_id = "proj-fork-test";
    p.slug       = "parent";
    p.directory  = "/tmp";
    p.title      = "Parent";
    auto parent_opt = Session::create(p);
    REQUIRE(parent_opt.has_value());
    const std::string parent_id = parent_opt->id();

    // Fork
    ForkParams fp;
    fp.parent_id = parent_id;
    fp.slug      = "child";
    fp.title     = "Child Fork";
    auto child_opt = Session::fork(fp);
    REQUIRE(child_opt.has_value());

    // Child should be persisted
    auto found = Session::get(child_opt->id());
    REQUIRE(found.has_value());
    REQUIRE(found->info().parent_id.has_value());
    CHECK(*found->info().parent_id == parent_id);
}

// ─── Session::messages via SessionStore ──────────────────────────────────────

TEST_CASE("SessionStore::save_message and list_messages round-trip", "[session][store]") {
    reset_store();

    // Create a session to have a valid session_id
    CreateParams p;
    p.project_id = "proj-msg-test";
    p.slug       = "msg-session";
    p.directory  = "/tmp";
    p.title      = "Message Test";
    auto session_opt = Session::create(p);
    REQUIRE(session_opt.has_value());
    const std::string session_id = session_opt->id();

    // Save messages via SessionStore directly
    auto& store = SessionStore::instance();
    nlohmann::json msg1 = {{"role", "user"}, {"text", "Hello"}, {"time_created", 1000}};
    nlohmann::json msg2 = {{"role", "assistant"}, {"text", "Hi!"}, {"time_created", 1001}};
    CHECK(store.save_message(session_id, msg1));
    CHECK(store.save_message(session_id, msg2));

    // Retrieve via session
    auto msgs = session_opt->messages(50, 0);
    REQUIRE(msgs.size() == 2u);
    CHECK(msgs[0]["role"] == "user");
    CHECK(msgs[1]["role"] == "assistant");
}

TEST_CASE("Session::messages returns empty when no messages saved", "[session][store]") {
    reset_store();

    CreateParams p;
    p.project_id = "proj-no-msg";
    p.slug       = "empty";
    p.directory  = "/tmp";
    p.title      = "Empty";
    auto session_opt = Session::create(p);
    REQUIRE(session_opt.has_value());

    auto msgs = session_opt->messages();
    CHECK(msgs.empty());
}

// ─── Session::get returns nullopt for unknown ID ──────────────────────────────

TEST_CASE("Session::get returns nullopt for non-existent ID", "[session][store]") {
    reset_store();
    auto result = Session::get("non-existent-id-xyz");
    CHECK(!result.has_value());
}
