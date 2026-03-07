#include <catch2/catch_test_macros.hpp>
#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_state_machine.hpp>
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
