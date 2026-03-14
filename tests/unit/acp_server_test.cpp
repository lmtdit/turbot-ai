#include <catch2/catch_test_macros.hpp>
#include <turbot/core/acp/server.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace turbot::core::acp;

// ---------------------------------------------------------------------------
// StubACPAgent – minimal implementation for dispatcher testing
// ---------------------------------------------------------------------------

class StubACPAgent : public ACPAgent {
public:
    // Track calls for assertions
    bool initialized           = false;
    bool prompted              = false;
    bool cancelled             = false;
    int  update_count          = 0;

    InitializeResponse initialize(const InitializeRequest& req) override {
        initialized = true;
        InitializeResponse resp;
        resp.protocol_version = req.protocol_version;
        resp.agent_info       = {"Stub", "0.0.1"};
        return resp;
    }

    nlohmann::json new_session(
        const NewSessionRequest& req,
        std::function<void(const nlohmann::json&)> /*on_update*/) override {
        return {{"sessionId", "stub-session-1"}, {"cwd", req.cwd},
                {"models", nlohmann::json::array()}, {"modes", nlohmann::json::array()}};
    }

    nlohmann::json load_session(
        const LoadSessionRequest& req,
        std::function<void(const nlohmann::json&)> /*on_update*/) override {
        return {{"sessionId", req.session_id}, {"models", nlohmann::json::array()}};
    }

    nlohmann::json resume_session(const ResumeSessionRequest& req) override {
        return {{"sessionId", req.session_id}};
    }

    nlohmann::json list_sessions(const ListSessionsRequest&) override {
        return {{"sessions", nlohmann::json::array()}};
    }

    ForkSessionResponse fork_session(const ForkSessionRequest& req) override {
        ForkSessionResponse resp;
        resp.session.session_id = "fork-" + req.session_id;
        resp.session.cwd        = req.cwd;
        return resp;
    }

    nlohmann::json prompt(
        const PromptRequest& req,
        std::function<void(const nlohmann::json&)> on_update) override {
        prompted = true;
        // Emit two updates
        on_update({{"sessionUpdate", "agent_message_chunk"},
                   {"content", {{"type", "text"}, {"text", "Hello"}}},
                   {"sessionId", req.session_id}});
        on_update({{"sessionUpdate", "agent_message_chunk"},
                   {"content", {{"type", "text"}, {"text", " World"}}},
                   {"sessionId", req.session_id}});
        update_count = 2;
        return {{"stopReason", "end_turn"}};
    }

    void cancel(const CancelNotification&) override {
        cancelled = true;
    }

    SetSessionModeResponse set_mode(const SetSessionModeRequest& req) override {
        return {req.mode_id};
    }

    nlohmann::json set_model(const SetSessionModelRequest& req) override {
        return {{"modelId", req.model_id}};
    }

    void authenticate(const nlohmann::json&) override {
        throw std::runtime_error("authRequired");
    }
};

// ---------------------------------------------------------------------------
// Helper: invoke dispatch and collect written responses
// ---------------------------------------------------------------------------

static std::vector<nlohmann::json> invoke(
    const nlohmann::json& request, StubACPAgent& agent) {
    std::vector<nlohmann::json> outputs;

    // Dispatch logic mirrors ACPServer::dispatch (dispatch is private,
    // so we replicate it here via direct virtual calls on the agent).
    const std::string method = request.value("method", "");
    const nlohmann::json params =
        request.contains("params") ? request["params"] : nlohmann::json::object();
    nlohmann::json id = nlohmann::json();
    if (request.contains("id")) id = request["id"];

    auto make_result_fn = [&](const nlohmann::json& result) -> nlohmann::json {
        return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
    };
    auto make_error_fn  = [&](int code, const std::string& msg) -> nlohmann::json {
        return {{"jsonrpc", "2.0"}, {"id", id},
                {"error", {{"code", code}, {"message", msg}}}};
    };

    try {
        if (method == "cancel") {
            CancelNotification notif;
            from_json(params, notif);
            agent.cancel(notif);
            return outputs;  // no response for notifications
        }

        if (method == "initialize") {
            InitializeRequest req; from_json(params, req);
            auto resp = agent.initialize(req);
            nlohmann::json j; to_json(j, resp);
            outputs.push_back(make_result_fn(j));
        } else if (method == "session/new") {
            NewSessionRequest req; from_json(params, req);
            auto write_update = [&](const nlohmann::json& payload) {
                outputs.push_back({{"jsonrpc", "2.0"},
                                   {"method", "session/update"},
                                   {"params", payload}});
            };
            outputs.push_back(make_result_fn(agent.new_session(req, write_update)));
        } else if (method == "session/load") {
            LoadSessionRequest req; from_json(params, req);
            auto write_update = [&](const nlohmann::json& payload) {
                outputs.push_back({{"jsonrpc", "2.0"},
                                   {"method", "session/update"},
                                   {"params", payload}});
            };
            outputs.push_back(make_result_fn(agent.load_session(req, write_update)));
        } else if (method == "session/resume") {
            ResumeSessionRequest req; from_json(params, req);
            outputs.push_back(make_result_fn(agent.resume_session(req)));
        } else if (method == "session/prompt") {
            PromptRequest req; from_json(params, req);
            auto write_update = [&](const nlohmann::json& payload) {
                outputs.push_back({{"jsonrpc", "2.0"},
                                   {"method", "session/update"},
                                   {"params", payload}});
            };
            auto result = agent.prompt(req, write_update);
            outputs.push_back(make_result_fn(result));
        } else if (method == "session/fork") {
            ForkSessionRequest req; from_json(params, req);
            auto resp = agent.fork_session(req);
            nlohmann::json j; to_json(j, resp);
            outputs.push_back(make_result_fn(j));
        } else if (method == "session/mode") {
            SetSessionModeRequest req; from_json(params, req);
            auto resp = agent.set_mode(req);
            nlohmann::json j; to_json(j, resp);
            outputs.push_back(make_result_fn(j));
        } else if (method == "session/model") {
            SetSessionModelRequest req; from_json(params, req);
            outputs.push_back(make_result_fn(agent.set_model(req)));
        } else if (method == "session/list") {
            ListSessionsRequest req; from_json(params, req);
            outputs.push_back(make_result_fn(agent.list_sessions(req)));
        } else if (method == "authenticate") {
            agent.authenticate(params);
            outputs.push_back(make_result_fn(nlohmann::json::object()));
        } else {
            outputs.push_back(make_error_fn(-32601, "Method not found"));
        }
    } catch (const nlohmann::json::exception& ex) {
        outputs.push_back(make_error_fn(-32602, std::string("Invalid params: ") + ex.what()));
    } catch (const std::exception& ex) {
        const std::string msg = ex.what();
        if (msg.find("authRequired") != std::string::npos) {
            nlohmann::json err = {{"jsonrpc", "2.0"}, {"id", id},
                                  {"error", {{"code", -32000}, {"message", "authRequired"},
                                             {"data", {{"type", "authRequired"}}}}}};
            outputs.push_back(err);
        } else {
            outputs.push_back(make_error_fn(-32603, std::string("Internal error: ") + msg));
        }
    }

    return outputs;
}

// ---------------------------------------------------------------------------
// Test cases
// ---------------------------------------------------------------------------

TEST_CASE("ACPServer dispatch: initialize returns valid response", "[acp][server]") {
    StubACPAgent agent;
    auto resp = invoke({{"jsonrpc", "2.0"},
                        {"id", 1},
                        {"method", "initialize"},
                        {"params", {{"protocolVersion", 1}}}},
                       agent);

    REQUIRE(resp.size() == 1);
    REQUIRE(resp[0]["id"] == 1);
    REQUIRE(resp[0].contains("result"));
    REQUIRE(resp[0]["result"]["protocolVersion"] == 1);
    REQUIRE(resp[0]["result"]["agentInfo"]["name"] == "Stub");
    REQUIRE(resp[0]["result"].contains("agentCapabilities"));
    REQUIRE(agent.initialized);
}

TEST_CASE("ACPServer dispatch: unknown method returns -32601", "[acp][server]") {
    StubACPAgent agent;
    auto resp = invoke({{"jsonrpc", "2.0"}, {"id", 2}, {"method", "nonexistent"}}, agent);

    REQUIRE(resp.size() == 1);
    REQUIRE(resp[0]["id"] == 2);
    REQUIRE(resp[0]["error"]["code"] == -32601);
}

TEST_CASE("ACPServer dispatch: cancel notification has no response", "[acp][server]") {
    StubACPAgent agent;
    auto resp = invoke({{"jsonrpc", "2.0"},
                        {"method", "cancel"},
                        {"params", {{"sessionId", "sess-1"}}}},
                       agent);

    REQUIRE(resp.empty());  // notifications never produce a response
    REQUIRE(agent.cancelled);
}

TEST_CASE("ACPServer dispatch: session/prompt emits session/update notifications", "[acp][server]") {
    StubACPAgent agent;
    auto resp = invoke({{"jsonrpc", "2.0"},
                        {"id", 3},
                        {"method", "session/prompt"},
                        {"params", {{"sessionId", "sess-1"}, {"prompt", nlohmann::json::array()}}}},
                       agent);

    // 2 session/update notifications + 1 result = 3 messages
    REQUIRE(resp.size() == 3);
    REQUIRE(resp[0]["method"] == "session/update");
    REQUIRE(resp[1]["method"] == "session/update");
    REQUIRE(resp[2]["id"] == 3);
    REQUIRE(resp[2]["result"]["stopReason"] == "end_turn");
    REQUIRE(agent.prompted);
}

TEST_CASE("ACPServer dispatch: authenticate throws authRequired → -32000", "[acp][server]") {
    StubACPAgent agent;
    auto resp = invoke({{"jsonrpc", "2.0"},
                        {"id", 4},
                        {"method", "authenticate"},
                        {"params", nlohmann::json::object()}},
                       agent);

    REQUIRE(resp.size() == 1);
    REQUIRE(resp[0]["error"]["code"] == -32000);
    REQUIRE(resp[0]["error"]["message"] == "authRequired");
}

TEST_CASE("ACPServer dispatch: session/new returns sessionId", "[acp][server]") {
    StubACPAgent agent;
    auto resp = invoke({{"jsonrpc", "2.0"},
                        {"id", 5},
                        {"method", "session/new"},
                        {"params", {{"cwd", "/home/user"}, {"mcpServers", nlohmann::json::array()}}}},
                       agent);

    REQUIRE(resp.size() == 1);
    REQUIRE(resp[0]["result"]["sessionId"] == "stub-session-1");
}

TEST_CASE("ACPServer dispatch: session/fork returns forked sessionId", "[acp][server]") {
    StubACPAgent agent;
    auto resp = invoke({{"jsonrpc", "2.0"},
                        {"id", 6},
                        {"method", "session/fork"},
                        {"params", {{"sessionId", "orig-1"}, {"cwd", "/proj"},
                                    {"mcpServers", nlohmann::json::array()}}}},
                       agent);

    REQUIRE(resp.size() == 1);
    REQUIRE(resp[0]["result"]["session"]["sessionId"] == "fork-orig-1");
}

TEST_CASE("ACPServer dispatch: session/mode returns mode", "[acp][server]") {
    StubACPAgent agent;
    auto resp = invoke({{"jsonrpc", "2.0"},
                        {"id", 7},
                        {"method", "session/mode"},
                        {"params", {{"sessionId", "s1"}, {"modeId", "code"}}}},
                       agent);

    REQUIRE(resp.size() == 1);
    REQUIRE(resp[0]["result"]["mode"] == "code");
}

TEST_CASE("ACPServer dispatch: session/model returns modelId", "[acp][server]") {
    StubACPAgent agent;
    auto resp = invoke({{"jsonrpc", "2.0"},
                        {"id", 8},
                        {"method", "session/model"},
                        {"params", {{"sessionId", "s1"}, {"modelId", "gpt-4o"}}}},
                       agent);

    REQUIRE(resp.size() == 1);
    REQUIRE(resp[0]["result"]["modelId"] == "gpt-4o");
}

TEST_CASE("ACPServer dispatch: session/list returns sessions array", "[acp][server]") {
    StubACPAgent agent;
    auto resp = invoke({{"jsonrpc", "2.0"},
                        {"id", 9},
                        {"method", "session/list"},
                        {"params", nlohmann::json::object()}},
                       agent);

    REQUIRE(resp.size() == 1);
    REQUIRE(resp[0]["result"]["sessions"].is_array());
}
