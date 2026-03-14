#include <catch2/catch_test_macros.hpp>
#include <turbot/core/acp/acp.hpp>

using namespace turbot::core::acp;

// ---------------------------------------------------------------------------
// InitializeRequest / Response round-trip
// ---------------------------------------------------------------------------

TEST_CASE("ACP types: InitializeRequest serialization", "[acp][types]") {
    SECTION("basic round-trip") {
        InitializeRequest req;
        req.protocol_version = 1;

        nlohmann::json j;
        to_json(j, req);
        REQUIRE(j["protocolVersion"] == 1);

        InitializeRequest req2;
        from_json(j, req2);
        REQUIRE(req2.protocol_version == 1);
    }

    SECTION("with authentication field") {
        InitializeRequest req;
        req.authentication = "token-abc";

        nlohmann::json j;
        to_json(j, req);
        REQUIRE(j["authentication"] == "token-abc");

        InitializeRequest req2;
        from_json(j, req2);
        REQUIRE(req2.authentication.has_value());
        REQUIRE(*req2.authentication == "token-abc");
    }
}

TEST_CASE("ACP types: InitializeResponse serialization", "[acp][types]") {
    InitializeResponse resp;
    resp.protocol_version = 1;
    resp.agent_info = {"Turbot", "1.0.0"};
    resp.auth_methods = {{"opencode-login", "Login with opencode",
                          "Run `turbot auth login`", nlohmann::json::object()}};

    nlohmann::json j;
    to_json(j, resp);

    REQUIRE(j["protocolVersion"] == 1);
    REQUIRE(j["agentInfo"]["name"] == "Turbot");
    REQUIRE(j["agentInfo"]["version"] == "1.0.0");
    REQUIRE(j["authMethods"].size() == 1);
    REQUIRE(j["agentCapabilities"]["loadSession"] == true);
    REQUIRE(j["agentCapabilities"]["mcpCapabilities"]["http"] == true);
    REQUIRE(j["agentCapabilities"]["sessionCapabilities"].contains("fork"));
}

// ---------------------------------------------------------------------------
// SessionInfo round-trip
// ---------------------------------------------------------------------------

TEST_CASE("ACP types: SessionInfo serialization", "[acp][types]") {
    SessionInfo info;
    info.session_id = "sess-001";
    info.cwd        = "/home/user/project";
    info.title      = "My Session";
    info.updated_at = "2026-03-14T00:00:00Z";

    nlohmann::json j;
    to_json(j, info);

    REQUIRE(j["sessionId"] == "sess-001");
    REQUIRE(j["cwd"] == "/home/user/project");
    REQUIRE(j["title"] == "My Session");
    REQUIRE(j["updatedAt"] == "2026-03-14T00:00:00Z");

    SessionInfo info2;
    from_json(j, info2);
    REQUIRE(info2.session_id == "sess-001");
    REQUIRE(info2.title.has_value());
    REQUIRE(*info2.title == "My Session");
}

// ---------------------------------------------------------------------------
// NewSessionRequest
// ---------------------------------------------------------------------------

TEST_CASE("ACP types: NewSessionRequest round-trip", "[acp][types]") {
    nlohmann::json j = {{"cwd", "/tmp/proj"}, {"mcpServers", nlohmann::json::array()}, {"modelId", "claude-3-5"}};

    NewSessionRequest req;
    from_json(j, req);
    REQUIRE(req.cwd == "/tmp/proj");
    REQUIRE(req.model_id.has_value());
    REQUIRE(*req.model_id == "claude-3-5");

    nlohmann::json j2;
    to_json(j2, req);
    REQUIRE(j2["modelId"] == "claude-3-5");
}

// ---------------------------------------------------------------------------
// LoadSessionRequest
// ---------------------------------------------------------------------------

TEST_CASE("ACP types: LoadSessionRequest round-trip", "[acp][types]") {
    nlohmann::json j = {{"sessionId", "s1"}, {"cwd", "/proj"}, {"mcpServers", nlohmann::json::array()}};

    LoadSessionRequest req;
    from_json(j, req);
    REQUIRE(req.session_id == "s1");
    REQUIRE(req.cwd == "/proj");
    REQUIRE(!req.model_id.has_value());
}

// ---------------------------------------------------------------------------
// ForkSessionResponse
// ---------------------------------------------------------------------------

TEST_CASE("ACP types: ForkSessionResponse round-trip", "[acp][types]") {
    ForkSessionResponse resp;
    resp.session.session_id = "forked-001";
    resp.session.cwd        = "/project";

    nlohmann::json j;
    to_json(j, resp);
    REQUIRE(j["session"]["sessionId"] == "forked-001");

    ForkSessionResponse resp2;
    from_json(j, resp2);
    REQUIRE(resp2.session.session_id == "forked-001");
}

// ---------------------------------------------------------------------------
// PromptRequest
// ---------------------------------------------------------------------------

TEST_CASE("ACP types: PromptRequest round-trip", "[acp][types]") {
    nlohmann::json j = {
        {"sessionId", "sess-x"},
        {"prompt", nlohmann::json::array({{{"type", "text"}, {"text", "Hello"}}})},
        {"mode", "code"}};

    PromptRequest req;
    from_json(j, req);
    REQUIRE(req.session_id == "sess-x");
    REQUIRE(req.prompt.size() == 1);
    REQUIRE(req.mode.has_value());
    REQUIRE(*req.mode == "code");
}

// ---------------------------------------------------------------------------
// SetSessionModeRequest / Response
// ---------------------------------------------------------------------------

TEST_CASE("ACP types: SetSessionMode round-trip", "[acp][types]") {
    nlohmann::json j = {{"sessionId", "s1"}, {"modeId", "explore"}};

    SetSessionModeRequest req;
    from_json(j, req);
    REQUIRE(req.session_id == "s1");
    REQUIRE(req.mode_id == "explore");

    SetSessionModeResponse resp;
    resp.mode = "explore";
    nlohmann::json jresp;
    to_json(jresp, resp);
    REQUIRE(jresp["mode"] == "explore");
}

// ---------------------------------------------------------------------------
// SetSessionModelRequest
// ---------------------------------------------------------------------------

TEST_CASE("ACP types: SetSessionModelRequest round-trip", "[acp][types]") {
    nlohmann::json j = {{"sessionId", "s1"}, {"modelId", "gpt-4o"}};

    SetSessionModelRequest req;
    from_json(j, req);
    REQUIRE(req.session_id == "s1");
    REQUIRE(req.model_id == "gpt-4o");
}

// ---------------------------------------------------------------------------
// CancelNotification
// ---------------------------------------------------------------------------

TEST_CASE("ACP types: CancelNotification round-trip", "[acp][types]") {
    nlohmann::json j = {{"sessionId", "s1"}};

    CancelNotification notif;
    from_json(j, notif);
    REQUIRE(notif.session_id == "s1");

    nlohmann::json j2;
    to_json(j2, notif);
    REQUIRE(j2["sessionId"] == "s1");
}

// ---------------------------------------------------------------------------
// ListSessionsRequest
// ---------------------------------------------------------------------------

TEST_CASE("ACP types: ListSessionsRequest round-trip", "[acp][types]") {
    SECTION("empty params") {
        nlohmann::json j = nlohmann::json::object();
        ListSessionsRequest req;
        from_json(j, req);
        REQUIRE(!req.cwd.has_value());
        REQUIRE(!req.cursor.has_value());
    }

    SECTION("with cursor") {
        nlohmann::json j = {{"cwd", "/home"}, {"cursor", "1741900800"}};
        ListSessionsRequest req;
        from_json(j, req);
        REQUIRE(req.cwd.has_value());
        REQUIRE(*req.cursor == "1741900800");
    }
}

// ---------------------------------------------------------------------------
// Usage serialization (all optional fields)
// ---------------------------------------------------------------------------

TEST_CASE("ACP types: Usage serialization", "[acp][types]") {
    SECTION("basic fields") {
        Usage u;
        u.input_tokens  = 100;
        u.output_tokens = 200;
        u.total_tokens  = 300;

        nlohmann::json j;
        to_json(j, u);
        REQUIRE(j["inputTokens"] == 100);
        REQUIRE(j["outputTokens"] == 200);
        REQUIRE(j["totalTokens"] == 300);
        REQUIRE(!j.contains("thoughtTokens"));
    }

    SECTION("with optional token fields") {
        Usage u;
        u.input_tokens       = 50;
        u.output_tokens      = 50;
        u.total_tokens       = 100;
        u.thought_tokens     = 10;
        u.cached_read_tokens = 5;

        nlohmann::json j;
        to_json(j, u);
        REQUIRE(j["thoughtTokens"] == 10);
        REQUIRE(j["cachedReadTokens"] == 5);
        REQUIRE(!j.contains("cachedWriteTokens"));
    }

    SECTION("round-trip from JSON") {
        nlohmann::json j = {
            {"totalTokens", 300},
            {"inputTokens", 100},
            {"outputTokens", 200},
            {"thoughtTokens", 15},
            {"cachedReadTokens", 20},
            {"cachedWriteTokens", 30}};

        Usage u;
        from_json(j, u);
        REQUIRE(u.total_tokens == 300);
        REQUIRE(u.thought_tokens.has_value());
        REQUIRE(*u.thought_tokens == 15);
        REQUIRE(*u.cached_read_tokens == 20);
        REQUIRE(*u.cached_write_tokens == 30);
    }
}

// ---------------------------------------------------------------------------
// StopReason to_json
// ---------------------------------------------------------------------------

TEST_CASE("ACP types: StopReason serialization", "[acp][types]") {
    REQUIRE(stop_reason_to_string(StopReason::EndTurn)      == "end_turn");
    REQUIRE(stop_reason_to_string(StopReason::ToolUse)      == "tool_use");
    REQUIRE(stop_reason_to_string(StopReason::StopSequence) == "stop_sequence");
    REQUIRE(stop_reason_to_string(StopReason::Error)        == "error");
    REQUIRE(stop_reason_to_string(StopReason::Cancelled)    == "cancelled");

    nlohmann::json j;
    to_json(j, StopReason::EndTurn);
    REQUIRE(j == "end_turn");
}
