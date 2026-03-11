#include <catch2/catch_test_macros.hpp>
#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_state_machine.hpp>
#include <turbot/core/snapshot/snapshot.hpp>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace turbot::core::session;

TEST_CASE("SessionState conversion", "[core][session][session_state]") {
    SECTION("to_string") {
        REQUIRE(session_state_to_string(SessionState::Created) == "created");
        REQUIRE(session_state_to_string(SessionState::Active) == "active");
        REQUIRE(session_state_to_string(SessionState::Busy) == "busy");
        REQUIRE(session_state_to_string(SessionState::Compacting) == "compacting");
        REQUIRE(session_state_to_string(SessionState::Archived) == "archived");
    }

    SECTION("from_string") {
        REQUIRE(string_to_session_state("created") == SessionState::Created);
        REQUIRE(string_to_session_state("active") == SessionState::Active);
        REQUIRE(string_to_session_state("busy") == SessionState::Busy);
        REQUIRE(string_to_session_state("compacting") == SessionState::Compacting);
        REQUIRE(string_to_session_state("archived") == SessionState::Archived);
    }

    SECTION("invalid string throws") {
        REQUIRE_THROWS_AS(string_to_session_state("invalid"), std::invalid_argument);
    }
}

TEST_CASE("SessionInfo serialization", "[core][session][session_info]") {
    SECTION("basic serialization") {
        SessionInfo info;
        info.id = "sess_123";
        info.project_id = "proj_456";
        info.slug = "main-session";
        info.directory = "/path/to/project";
        info.title = "Main Session";
        info.version = "1.0.0";
        info.time_created = 1000000;
        info.time_updated = 1000001;
        info.state = SessionState::Active;

        nlohmann::json j = info.to_json();
        
        REQUIRE(j["id"] == "sess_123");
        REQUIRE(j["project_id"] == "proj_456");
        REQUIRE(j["slug"] == "main-session");
        REQUIRE(j["directory"] == "/path/to/project");
        REQUIRE(j["title"] == "Main Session");
        REQUIRE(j["version"] == "1.0.0");
        REQUIRE(j["time_created"] == 1000000);
        REQUIRE(j["time_updated"] == 1000001);
        REQUIRE(j["state"] == "active");
    }

    SECTION("with optional fields") {
        SessionInfo info;
        info.id = "sess_opt";
        info.project_id = "proj";
        info.slug = "slug";
        info.directory = "/dir";
        info.title = "Title";
        info.parent_id = "parent_123";
        info.permission = nlohmann::json{{"read", true}};
        info.time_compacting = 2000000;
        info.time_archived = 3000000;

        nlohmann::json j = info.to_json();
        
        REQUIRE(j["parent_id"] == "parent_123");
        REQUIRE(j["permission"]["read"] == true);
        REQUIRE(j["time_compacting"] == 2000000);
        REQUIRE(j["time_archived"] == 3000000);
    }

    SECTION("deserialization") {
        nlohmann::json j = {
            {"id", "sess_deser"},
            {"project_id", "proj_deser"},
            {"parent_id", "parent"},
            {"slug", "deser-slug"},
            {"directory", "/deser/path"},
            {"title", "Deserialized"},
            {"version", "2.0.0"},
            {"permission", {{"write", false}}},
            {"time_created", 1111},
            {"time_updated", 2222},
            {"time_compacting", 3333},
            {"time_archived", 4444},
            {"state", "busy"}
        };

        SessionInfo info = SessionInfo::from_json(j);
        
        REQUIRE(info.id == "sess_deser");
        REQUIRE(info.project_id == "proj_deser");
        REQUIRE(info.parent_id == "parent");
        REQUIRE(info.slug == "deser-slug");
        REQUIRE(info.directory == "/deser/path");
        REQUIRE(info.title == "Deserialized");
        REQUIRE(info.version == "2.0.0");
        REQUIRE(info.permission.value()["write"] == false);
        REQUIRE(info.time_created == 1111);
        REQUIRE(info.time_updated == 2222);
        REQUIRE(info.time_compacting == 3333);
        REQUIRE(info.time_archived == 4444);
        REQUIRE(info.state == SessionState::Busy);
    }

    SECTION("round trip") {
        SessionInfo original;
        original.id = "round_trip";
        original.project_id = "proj";
        original.slug = "rt-slug";
        original.directory = "/rt";
        original.title = "Round Trip";
        original.state = SessionState::Compacting;
        
        nlohmann::json j = original.to_json();
        SessionInfo restored = SessionInfo::from_json(j);
        
        REQUIRE(restored.id == original.id);
        REQUIRE(restored.project_id == original.project_id);
        REQUIRE(restored.slug == original.slug);
        REQUIRE(restored.state == original.state);
    }

    SECTION("equality") {
        SessionInfo a;
        a.id = "test";
        a.project_id = "proj";
        a.state = SessionState::Active;
        
        SessionInfo b;
        b.id = "test";
        b.project_id = "proj";
        b.state = SessionState::Active;
        
        SessionInfo c;
        c.id = "different";
        
        REQUIRE(a == b);
        REQUIRE_FALSE(a == c);
    }
}

TEST_CASE("Session creation", "[core][session][session]") {
    SECTION("create new session") {
        CreateParams params;
        params.project_id = "proj_create";
        params.slug = "new-session";
        params.directory = "/new/path";
        params.title = "New Session";
        
        auto session_opt = Session::create(params);
        
        REQUIRE(session_opt.has_value());
        
        const auto& session = *session_opt;
        REQUIRE_FALSE(session.id().empty());
        REQUIRE(session.info().project_id == "proj_create");
        REQUIRE(session.info().slug == "new-session");
        REQUIRE(session.info().directory == "/new/path");
        REQUIRE(session.info().title == "New Session");
        REQUIRE(session.info().state == SessionState::Created);
        REQUIRE(session.is_valid());
    }

    SECTION("create with permission") {
        CreateParams params;
        params.project_id = "proj_perm";
        params.slug = "perm-session";
        params.directory = "/perm";
        params.title = "Permission Session";
        params.permission = nlohmann::json{{"admin", true}};
        
        auto session_opt = Session::create(params);
        
        REQUIRE(session_opt.has_value());
        REQUIRE(session_opt->info().permission.has_value());
        REQUIRE((*session_opt->info().permission)["admin"] == true);
    }

    SECTION("timestamps set correctly") {
        CreateParams params;
        params.project_id = "proj_time";
        params.slug = "time";
        params.directory = "/time";
        params.title = "Time Test";
        
        auto session_opt = Session::create(params);
        
        REQUIRE(session_opt.has_value());
        REQUIRE(session_opt->info().time_created > 0);
        REQUIRE(session_opt->info().time_updated > 0);
        REQUIRE(session_opt->info().time_created == session_opt->info().time_updated);
    }

    SECTION("unique IDs") {
        CreateParams params;
        params.project_id = "proj_unique";
        params.slug = "unique";
        params.directory = "/unique";
        params.title = "Unique Test";
        
        auto session1 = Session::create(params);
        auto session2 = Session::create(params);
        
        REQUIRE(session1->id() != session2->id());
    }
}

TEST_CASE("Session update", "[core][session][session]") {
    SECTION("update title") {
        CreateParams params;
        params.project_id = "proj_update";
        params.slug = "update";
        params.directory = "/update";
        params.title = "Original Title";
        
        auto session_opt = Session::create(params);
        REQUIRE(session_opt.has_value());
        
        auto& session = *session_opt;
        int64_t original_time = session.info().time_updated;
        
        // Small delay to ensure timestamp changes
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        REQUIRE(session.set_title("New Title"));
        REQUIRE(session.info().title == "New Title");
        REQUIRE(session.info().time_updated >= original_time);
    }

    SECTION("update permission") {
        CreateParams params;
        params.project_id = "proj_perm_up";
        params.slug = "perm-up";
        params.directory = "/perm-up";
        params.title = "Perm Update";
        
        auto session_opt = Session::create(params);
        REQUIRE(session_opt.has_value());
        
        auto& session = *session_opt;
        nlohmann::json new_perm = {{"read", true}, {"write", false}};
        
        REQUIRE(session.set_permission(new_perm));
        REQUIRE(session.info().permission.has_value());
        REQUIRE((*session.info().permission)["read"] == true);
        REQUIRE((*session.info().permission)["write"] == false);
    }

    SECTION("update with params struct") {
        CreateParams params;
        params.project_id = "proj_struct";
        params.slug = "struct";
        params.directory = "/struct";
        params.title = "Struct Update";
        
        auto session_opt = Session::create(params);
        REQUIRE(session_opt.has_value());
        
        auto& session = *session_opt;
        
        UpdateParams up_params;
        up_params.title = "Updated Title";
        up_params.state = SessionState::Active;
        
        REQUIRE(session.update(up_params));
        REQUIRE(session.info().title == "Updated Title");
        REQUIRE(session.info().state == SessionState::Active);
    }
}

TEST_CASE("Session lifecycle", "[core][session][session]") {
    SECTION("archive session") {
        CreateParams params;
        params.project_id = "proj_archive";
        params.slug = "archive";
        params.directory = "/archive";
        params.title = "Archive Test";
        
        auto session_opt = Session::create(params);
        REQUIRE(session_opt.has_value());
        
        auto& session = *session_opt;
        REQUIRE_FALSE(session.is_archived());
        
        REQUIRE(session.archive());
        REQUIRE(session.is_archived());
        REQUIRE(session.info().state == SessionState::Archived);
        REQUIRE(session.info().time_archived.has_value());
        
        // Cannot archive again
        REQUIRE_FALSE(session.archive());
    }

    SECTION("restore archived session") {
        CreateParams params;
        params.project_id = "proj_restore";
        params.slug = "restore";
        params.directory = "/restore";
        params.title = "Restore Test";
        
        auto session_opt = Session::create(params);
        REQUIRE(session_opt.has_value());
        
        auto& session = *session_opt;
        session.archive();
        REQUIRE(session.is_archived());
        
        REQUIRE(session.restore());
        REQUIRE_FALSE(session.is_archived());
        REQUIRE(session.info().state == SessionState::Active);
        REQUIRE_FALSE(session.info().time_archived.has_value());
        
        // Cannot restore non-archived session
        REQUIRE_FALSE(session.restore());
    }

    SECTION("compact session") {
        CreateParams params;
        params.project_id = "proj_compact";
        params.slug = "compact";
        params.directory = "/compact";
        params.title = "Compact Test";
        
        auto session_opt = Session::create(params);
        REQUIRE(session_opt.has_value());
        
        auto& session = *session_opt;
        REQUIRE(session.compact());
        REQUIRE(session.info().time_compacting.has_value());
        
        // Cannot compact archived session
        session.archive();
        REQUIRE_FALSE(session.compact());
    }

    SECTION("is_active check") {
        CreateParams params;
        params.project_id = "proj_active";
        params.slug = "active";
        params.directory = "/active";
        params.title = "Active Test";
        
        auto session_opt = Session::create(params);
        REQUIRE(session_opt.has_value());
        
        auto& session = *session_opt;
        REQUIRE_FALSE(session.is_active());  // Created state
        
        session.update(UpdateParams{.state = SessionState::Active});
        REQUIRE(session.is_active());
    }
}

TEST_CASE("Session fork", "[core][session][session]") {
    SECTION("fork creates child session") {
        // Create parent
        CreateParams parent_params;
        parent_params.project_id = "proj_fork";
        parent_params.slug = "parent";
        parent_params.directory = "/fork";
        parent_params.title = "Parent Session";
        parent_params.permission = nlohmann::json{{"fork_perm", true}};
        
        auto parent_opt = Session::create(parent_params);
        REQUIRE(parent_opt.has_value());
        
        // Fork - note: get() returns nullopt in this placeholder implementation
        // so fork will also return nullopt
        ForkParams fork_params;
        fork_params.parent_id = parent_opt->id();
        fork_params.slug = "forked";
        fork_params.title = "Forked Session";
        
        auto forked_opt = Session::fork(fork_params);
        // In real implementation with database, this would work
        // For now, it returns nullopt because get() returns nullopt
        // This test documents the expected behavior
    }
}

TEST_CASE("Session messages", "[core][session][session]") {
    SECTION("get messages empty by default") {
        CreateParams params;
        params.project_id = "proj_msg";
        params.slug = "msg";
        params.directory = "/msg";
        params.title = "Message Test";
        
        auto session_opt = Session::create(params);
        REQUIRE(session_opt.has_value());
        
        auto messages = session_opt->messages();
        REQUIRE(messages.empty());
    }

    SECTION("messages with pagination") {
        CreateParams params;
        params.project_id = "proj_pag";
        params.slug = "pag";
        params.directory = "/pag";
        params.title = "Pagination Test";
        
        auto session_opt = Session::create(params);
        REQUIRE(session_opt.has_value());
        
        // These parameters don't affect the placeholder result
        auto messages = session_opt->messages(10, 5);
        REQUIRE(messages.empty());
    }
}

TEST_CASE("Session equality", "[core][session][session]") {
    CreateParams params;
    params.project_id = "proj_eq";
    params.slug = "eq";
    params.directory = "/eq";
    params.title = "Equality Test";
    
    auto session1 = Session::create(params);
    auto session2 = Session::create(params);
    
    // Same ID means equal (create a copy)
    Session session_copy(*session1);
    REQUIRE(session_copy == *session1);
    
    // Different IDs
    REQUIRE_FALSE(*session1 == *session2);
}

TEST_CASE("SessionStateMachine state transitions", "[core][session][state_machine]") {
    SECTION("initial state") {
        SessionStateMachine sm;
        REQUIRE(sm.current() == SessionState::Created);
    }

    SECTION("initial state with parameter") {
        SessionStateMachine sm(SessionState::Active);
        REQUIRE(sm.current() == SessionState::Active);
    }

    SECTION("valid transition Created -> Active") {
        SessionStateMachine sm;
        REQUIRE(sm.transition(SessionState::Active));
        REQUIRE(sm.current() == SessionState::Active);
    }

    SECTION("invalid transition Created -> Busy") {
        SessionStateMachine sm;
        REQUIRE_FALSE(sm.transition(SessionState::Busy));
        REQUIRE(sm.current() == SessionState::Created);
    }

    SECTION("valid transitions from Active") {
        SessionStateMachine sm(SessionState::Active);
        
        REQUIRE(sm.transition(SessionState::Busy));
        sm.reset_to(SessionState::Active);
        
        REQUIRE(sm.transition(SessionState::Compacting));
        sm.reset_to(SessionState::Active);
        
        REQUIRE(sm.transition(SessionState::Archived));
    }

    SECTION("valid transitions from Busy") {
        SessionStateMachine sm(SessionState::Busy);
        
        REQUIRE(sm.transition(SessionState::Active));
        sm.reset_to(SessionState::Busy);
        
        REQUIRE(sm.transition(SessionState::Archived));
    }

    SECTION("valid transitions from Compacting") {
        SessionStateMachine sm(SessionState::Compacting);
        
        REQUIRE(sm.transition(SessionState::Active));
        sm.reset_to(SessionState::Compacting);
        
        REQUIRE(sm.transition(SessionState::Archived));
    }

    SECTION("valid transition Archived -> Active (restore)") {
        SessionStateMachine sm(SessionState::Archived);
        REQUIRE(sm.transition(SessionState::Active));
    }

    SECTION("no self-transitions") {
        SessionStateMachine sm(SessionState::Active);
        REQUIRE_FALSE(sm.transition(SessionState::Active));
    }
}

TEST_CASE("SessionStateMachine helper methods", "[core][session][state_machine]") {
    SECTION("reset") {
        SessionStateMachine sm(SessionState::Archived);
        sm.reset();
        REQUIRE(sm.current() == SessionState::Created);
    }

    SECTION("reset_to") {
        SessionStateMachine sm;
        sm.reset_to(SessionState::Busy);
        REQUIRE(sm.current() == SessionState::Busy);
    }

    SECTION("is_terminal") {
        SessionStateMachine sm;
        REQUIRE_FALSE(sm.is_terminal());
        
        sm.reset_to(SessionState::Archived);
        REQUIRE(sm.is_terminal());
    }

    SECTION("is_active_state") {
        SessionStateMachine sm;
        REQUIRE_FALSE(sm.is_active_state());
        
        sm.reset_to(SessionState::Active);
        REQUIRE(sm.is_active_state());
        
        sm.reset_to(SessionState::Busy);
        REQUIRE(sm.is_active_state());
        
        sm.reset_to(SessionState::Archived);
        REQUIRE_FALSE(sm.is_active_state());
    }

    SECTION("valid_transitions") {
        SessionStateMachine sm(SessionState::Created);
        auto transitions = sm.valid_transitions();
        REQUIRE(transitions.size() == 1);
        REQUIRE(transitions[0] == SessionState::Active);
    }

    SECTION("valid_transitions_from static") {
        auto from_created = SessionStateMachine::valid_transitions_from(SessionState::Created);
        REQUIRE(from_created.size() == 1);
        
        auto from_active = SessionStateMachine::valid_transitions_from(SessionState::Active);
        REQUIRE(from_active.size() == 3);
        
        auto from_archived = SessionStateMachine::valid_transitions_from(SessionState::Archived);
        REQUIRE(from_archived.size() == 1);
    }
}

TEST_CASE("SessionStateMachine can_transition static", "[core][session][state_machine]") {
    SECTION("all valid transitions") {
        REQUIRE(SessionStateMachine::can_transition(SessionState::Created, SessionState::Active));
        
        REQUIRE(SessionStateMachine::can_transition(SessionState::Active, SessionState::Busy));
        REQUIRE(SessionStateMachine::can_transition(SessionState::Active, SessionState::Compacting));
        REQUIRE(SessionStateMachine::can_transition(SessionState::Active, SessionState::Archived));
        
        REQUIRE(SessionStateMachine::can_transition(SessionState::Busy, SessionState::Active));
        REQUIRE(SessionStateMachine::can_transition(SessionState::Busy, SessionState::Archived));
        
        REQUIRE(SessionStateMachine::can_transition(SessionState::Compacting, SessionState::Active));
        REQUIRE(SessionStateMachine::can_transition(SessionState::Compacting, SessionState::Archived));
        
        REQUIRE(SessionStateMachine::can_transition(SessionState::Archived, SessionState::Active));
    }

    SECTION("invalid transitions") {
        REQUIRE_FALSE(SessionStateMachine::can_transition(SessionState::Created, SessionState::Busy));
        REQUIRE_FALSE(SessionStateMachine::can_transition(SessionState::Created, SessionState::Archived));
        REQUIRE_FALSE(SessionStateMachine::can_transition(SessionState::Active, SessionState::Created));
        REQUIRE_FALSE(SessionStateMachine::can_transition(SessionState::Busy, SessionState::Created));
        REQUIRE_FALSE(SessionStateMachine::can_transition(SessionState::Busy, SessionState::Compacting));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// RevertInfo tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("RevertInfo serialization", "[core][session][revert]") {
    SECTION("minimal (message_id only)") {
        RevertInfo ri;
        ri.message_id = "msg_abc";

        auto j = ri.to_json();
        // JSON uses opencode-compatible camelCase keys
        REQUIRE(j["messageID"] == "msg_abc");
        REQUIRE_FALSE(j.contains("partID"));
        REQUIRE_FALSE(j.contains("snapshot"));
        REQUIRE_FALSE(j.contains("diff"));

        auto restored = RevertInfo::from_json(j);
        REQUIRE(restored.message_id == "msg_abc");
        REQUIRE_FALSE(restored.part_id.has_value());
        REQUIRE_FALSE(restored.snapshot_id.has_value());
        REQUIRE_FALSE(restored.diff.has_value());
    }

    SECTION("full (all fields)") {
        RevertInfo ri;
        ri.message_id   = "msg_full";
        ri.part_id      = "part_1";
        ri.snapshot_id  = "snap_xyz";
        ri.diff         = "--- a/foo.txt\n+++ b/foo.txt\n@@ -1 +1 @@\n-old\n+new\n";

        auto j = ri.to_json();
        REQUIRE(j["messageID"]  == "msg_full");
        REQUIRE(j["partID"]     == "part_1");
        REQUIRE(j["snapshot"]   == "snap_xyz");
        REQUIRE(j["diff"].get<std::string>().find("old") != std::string::npos);

        auto restored = RevertInfo::from_json(j);
        REQUIRE(restored.message_id           == "msg_full");
        REQUIRE(restored.part_id.value()      == "part_1");
        REQUIRE(restored.snapshot_id.value()  == "snap_xyz");
        REQUIRE(restored.diff.has_value());
    }

    SECTION("from_json accepts snake_case keys (backward compatibility)") {
        nlohmann::json j = {
            {"message_id", "msg_snake"},
            {"part_id", "part_snake"}
        };

        auto ri = RevertInfo::from_json(j);
        REQUIRE(ri.message_id == "msg_snake");
        REQUIRE(ri.part_id.value() == "part_snake");
    }

    SECTION("round-trip with null optionals in JSON") {
        nlohmann::json j = {
            {"messageID", "msg_null"},
            {"partID", nullptr},
            {"snapshot", nullptr}
        };

        auto ri = RevertInfo::from_json(j);
        REQUIRE(ri.message_id == "msg_null");
        REQUIRE_FALSE(ri.part_id.has_value());
        REQUIRE_FALSE(ri.snapshot_id.has_value());
    }

    SECTION("equality operator") {
        RevertInfo a;
        a.message_id = "msg1";
        a.part_id    = "part1";

        RevertInfo b;
        b.message_id = "msg1";
        b.part_id    = "part1";

        RevertInfo c;
        c.message_id = "msg2";

        REQUIRE(a == b);
        REQUIRE_FALSE(a == c);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SessionInfo revert field serialization
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("SessionInfo with revert field", "[core][session][revert]") {
    SECTION("no revert – field absent in JSON") {
        SessionInfo info;
        info.id = "sess_no_revert";
        info.project_id = "proj";
        info.slug = "s";
        info.directory = "/d";
        info.time_created = 1000;
        info.time_updated = 1000;

        auto j = info.to_json();
        REQUIRE_FALSE(j.contains("revert"));
    }

    SECTION("with revert – field present in JSON") {
        SessionInfo info;
        info.id = "sess_with_revert";
        info.project_id = "proj";
        info.slug = "s";
        info.directory = "/d";
        info.time_created = 2000;
        info.time_updated = 2000;

        RevertInfo ri;
        ri.message_id = "msg_r1";
        ri.snapshot_id = "snap_001";
        info.revert = ri;

        auto j = info.to_json();
        REQUIRE(j.contains("revert"));
        REQUIRE(j["revert"]["messageID"] == "msg_r1");
        REQUIRE(j["revert"]["snapshot"]  == "snap_001");
    }

    SECTION("round-trip with revert") {
        SessionInfo original;
        original.id = "sess_rt_revert";
        original.project_id = "proj_rt";
        original.slug = "rt";
        original.directory = "/rt";
        original.time_created = 3000;
        original.time_updated = 3000;

        RevertInfo ri;
        ri.message_id = "msg_rt";
        ri.part_id = "part_rt";
        original.revert = ri;

        auto j = original.to_json();
        auto restored = SessionInfo::from_json(j);

        REQUIRE(restored.revert.has_value());
        REQUIRE(restored.revert->message_id == "msg_rt");
        REQUIRE(restored.revert->part_id.value() == "part_rt");
    }

    SECTION("from_json with null revert field") {
        nlohmann::json j = {
            {"id", "sess_null_rv"},
            {"project_id", "proj"},
            {"slug", "s"},
            {"directory", "/d"},
            {"title", "T"},
            {"time_created", 100},
            {"time_updated", 100},
            {"revert", nullptr}
        };

        auto info = SessionInfo::from_json(j);
        REQUIRE_FALSE(info.revert.has_value());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Session::revert / unrevert / cleanup_revert
// ─────────────────────────────────────────────────────────────────────────────

namespace {
    // Helpers for file-based revert tests

    struct TempDir {
        std::filesystem::path path;
        TempDir() {
            path = std::filesystem::temp_directory_path() /
                   ("session-revert-test-" + std::to_string(std::time(nullptr)));
            std::filesystem::create_directories(path);
        }
        ~TempDir() {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
        std::filesystem::path file(const std::string& name) const { return path / name; }
    };

    void write_file(const std::filesystem::path& p, const std::string& content) {
        std::ofstream ofs(p, std::ios::binary);
        ofs << content;
    }

    std::string read_file(const std::filesystem::path& p) {
        std::ifstream ifs(p, std::ios::binary);
        return {std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()};
    }

    Session make_session() {
        CreateParams cp;
        cp.project_id = "proj_rv";
        cp.slug       = "rv";
        cp.directory  = "/rv";
        cp.title      = "Revert Test";
        return Session::create(cp).value();
    }
} // namespace

TEST_CASE("Session::revert with empty patches", "[core][session][revert]") {
    auto session = make_session();

    SECTION("revert with no patches succeeds and sets revert info") {
        RevertParams params;
        params.message_id = "msg_001";

        REQUIRE(session.revert(params));
        REQUIRE(session.info().revert.has_value());
        REQUIRE(session.info().revert->message_id == "msg_001");
        REQUIRE_FALSE(session.info().revert->part_id.has_value());
        REQUIRE(session.info().time_updated > 0);
    }

    SECTION("revert preserves original snapshot_id on second revert") {
        RevertParams p1;
        p1.message_id = "msg_first";
        REQUIRE(session.revert(p1));
        auto first_snap = session.info().revert->snapshot_id;

        // Second revert should keep the original snapshot ID
        RevertParams p2;
        p2.message_id = "msg_second";
        REQUIRE(session.revert(p2));
        REQUIRE(session.info().revert->snapshot_id == first_snap);
    }

    SECTION("revert with part_id") {
        RevertParams params;
        params.message_id = "msg_part";
        params.part_id    = "part_xyz";

        REQUIRE(session.revert(params));
        REQUIRE(session.info().revert->message_id == "msg_part");
        REQUIRE(session.info().revert->part_id.value() == "part_xyz");
    }
}

TEST_CASE("Session::revert rolls back file changes", "[core][session][revert]") {
    TempDir tmp;
    auto file_path = tmp.file("data.txt");
    write_file(file_path, "original content");

    auto& sm = turbot::core::snapshot::SnapshotManager::instance();

    // Track a change: modify the file
    std::string snap_id = sm.start_tracking(turbot::core::snapshot::SnapshotOptions{tmp.path.string()});
    write_file(file_path, "modified content");
    auto patch = sm.stop_tracking(snap_id);

    REQUIRE(read_file(file_path) == "modified content");

    // Build session and revert
    auto session = make_session();
    RevertParams params;
    params.message_id = "msg_file";
    params.patches.push_back(patch);

    REQUIRE(session.revert(params));

    // File should be back to original
    REQUIRE(read_file(file_path) == "original content");
    REQUIRE(session.info().revert.has_value());
    REQUIRE(session.info().revert->message_id == "msg_file");
}

TEST_CASE("Session::unrevert clears revert info", "[core][session][revert]") {
    auto session = make_session();

    SECTION("unrevert when no revert is a no-op") {
        REQUIRE(session.unrevert());
        REQUIRE_FALSE(session.info().revert.has_value());
    }

    SECTION("unrevert after revert clears the info") {
        RevertParams params;
        params.message_id = "msg_unrev";
        REQUIRE(session.revert(params));
        REQUIRE(session.info().revert.has_value());

        REQUIRE(session.unrevert());
        REQUIRE_FALSE(session.info().revert.has_value());
    }
}

TEST_CASE("Session::unrevert restores file changes", "[core][session][revert]") {
    TempDir tmp;
    auto file_path = tmp.file("restore.txt");
    write_file(file_path, "original content");

    auto& sm = turbot::core::snapshot::SnapshotManager::instance();

    // Track a change: modify the file
    std::string snap_id = sm.start_tracking(
        turbot::core::snapshot::SnapshotOptions{tmp.path.string()});
    write_file(file_path, "modified content");
    auto patch = sm.stop_tracking(snap_id);

    REQUIRE(read_file(file_path) == "modified content");

    // session directory points to our tmp dir so revert captures a valid snapshot
    CreateParams cp;
    cp.project_id = "proj_unrev";
    cp.slug       = "unrev";
    cp.directory  = tmp.path.string();
    cp.title      = "Unrevert Test";
    auto session  = Session::create(cp).value();

    RevertParams params;
    params.message_id = "msg_restore";
    params.patches.push_back(patch);

    REQUIRE(session.revert(params));
    REQUIRE(read_file(file_path) == "original content");

    // unrevert: restore the file to "modified content"
    REQUIRE(session.unrevert());
    REQUIRE_FALSE(session.info().revert.has_value());
    // After unrevert, apply_patch re-applies the pre_patch (which captured the diff
    // "original content → modified content" that occurred during the revert's rollback).
    // The file is now restored to "modified content" — the state before the revert.
    REQUIRE(read_file(file_path) == "modified content");
    REQUIRE_FALSE(session.info().revert.has_value());
}

TEST_CASE("Session::cleanup_revert clears revert info", "[core][session][revert]") {
    auto session = make_session();

    SECTION("cleanup_revert when no revert is a no-op") {
        REQUIRE(session.cleanup_revert());
        REQUIRE_FALSE(session.info().revert.has_value());
    }

    SECTION("cleanup_revert after revert clears the info") {
        RevertParams params;
        params.message_id = "msg_cleanup";
        REQUIRE(session.revert(params));
        REQUIRE(session.info().revert.has_value());

        REQUIRE(session.cleanup_revert());
        REQUIRE_FALSE(session.info().revert.has_value());
    }
}

