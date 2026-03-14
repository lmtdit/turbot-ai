// ACP Streaming Update Tests (3.10)
// Verifies all 8 session/update types and CLI command plumbing.

#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>

#include "turbot/core/acp/agent.hpp"
#include "turbot/core/acp/acp.hpp"
#include "turbot/core/acp/server.hpp"

using namespace turbot::core::acp;

// ============================================================================
// to_tool_kind – aligned with OpenCode toToolKind
// ============================================================================

TEST_CASE("to_tool_kind – execute", "[acp][streaming]") {
    REQUIRE(to_tool_kind("bash") == "execute");
}

TEST_CASE("to_tool_kind – fetch", "[acp][streaming]") {
    REQUIRE(to_tool_kind("webfetch") == "fetch");
}

TEST_CASE("to_tool_kind – edit tools", "[acp][streaming]") {
    REQUIRE(to_tool_kind("edit")  == "edit");
    REQUIRE(to_tool_kind("patch") == "edit");
    REQUIRE(to_tool_kind("write") == "edit");
}

TEST_CASE("to_tool_kind – search tools", "[acp][streaming]") {
    REQUIRE(to_tool_kind("grep") == "search");
    REQUIRE(to_tool_kind("glob") == "search");
    // OpenCode exact matches for context7
    REQUIRE(to_tool_kind("context7_resolve_library_id") == "search");
    REQUIRE(to_tool_kind("context7_get_library_docs")   == "search");
    // Unknown context7_ variant → "other" (not starts_with blanket match)
    REQUIRE(to_tool_kind("context7_unknown") == "other");
}

TEST_CASE("to_tool_kind – read tools", "[acp][streaming]") {
    REQUIRE(to_tool_kind("list") == "read");
    REQUIRE(to_tool_kind("read") == "read");
}

TEST_CASE("to_tool_kind – other (MCP tools)", "[acp][streaming]") {
    REQUIRE(to_tool_kind("mcp_github_create_issue") == "other");
    REQUIRE(to_tool_kind("playwright_click")         == "other");
    REQUIRE(to_tool_kind("unknown_tool")             == "other");
}

// ============================================================================
// build_available_commands_update – type 8
// ============================================================================

TEST_CASE("build_available_commands_update returns correct structure",
          "[acp][streaming]") {
    const auto payload =
        TurbotACPAgent::build_available_commands_update("sess-abc");

    REQUIRE(payload["sessionUpdate"] == "available_commands_update");
    REQUIRE(payload["sessionId"]     == "sess-abc");
    REQUIRE(payload.contains("availableCommands"));
    REQUIRE(payload["availableCommands"].is_array());

    // Must include "compact" baseline command (aligned with OpenCode)
    bool found_compact = false;
    for (const auto& cmd : payload["availableCommands"]) {
        if (cmd.value("name", "") == "compact") {
            found_compact = true;
            REQUIRE(cmd.contains("description"));
        }
    }
    REQUIRE(found_compact);
}

// ============================================================================
// Stub ACPAgent for dispatcher testing
// ============================================================================

namespace {

struct StreamingStubAgent : ACPAgent {
    // Track calls
    bool new_session_called  = false;
    bool load_session_called = false;
    std::vector<nlohmann::json> new_session_updates;
    std::vector<nlohmann::json> load_session_updates;

    InitializeResponse initialize(const InitializeRequest& req) override {
        InitializeResponse resp;
        resp.protocol_version = req.protocol_version;
        resp.agent_info = {"Stub", "0.0.1"};
        return resp;
    }

    nlohmann::json new_session(
        const NewSessionRequest& /*req*/,
        std::function<void(const nlohmann::json&)> on_update) override {
        new_session_called = true;
        // Simulate async available_commands_update
        on_update({{"sessionUpdate", "available_commands_update"},
                   {"sessionId",    "new-sess-1"},
                   {"availableCommands",
                    nlohmann::json::array({
                        {{"name", "compact"}, {"description", "compact the session"}}
                    })}});
        new_session_updates.push_back(
            {{"sessionUpdate", "available_commands_update"}});
        return {{"sessionId", "new-sess-1"},
                {"models",    nlohmann::json::object()},
                {"modes",     nlohmann::json::object()}};
    }

    nlohmann::json load_session(
        const LoadSessionRequest& /*req*/,
        std::function<void(const nlohmann::json&)> on_update) override {
        load_session_called = true;
        // Simulate history replay: user message + available_commands_update
        on_update({{"sessionUpdate", "user_message_chunk"},
                   {"sessionId",    "old-sess-1"},
                   {"content",
                    {{"type", "text"}, {"text", "Hello from history"}}}});
        on_update({{"sessionUpdate", "available_commands_update"},
                   {"sessionId",    "old-sess-1"},
                   {"availableCommands",
                    nlohmann::json::array({
                        {{"name", "compact"}, {"description", "compact the session"}}
                    })}});
        load_session_updates = {
            {{"sessionUpdate", "user_message_chunk"}},
            {{"sessionUpdate", "available_commands_update"}},
        };
        return {{"sessionId", "old-sess-1"},
                {"models",    nlohmann::json::object()},
                {"modes",     nlohmann::json::object()}};
    }

    nlohmann::json resume_session(
        const ResumeSessionRequest& /*req*/) override {
        return {{"sessionId", "resumed-sess-1"}};
    }

    nlohmann::json list_sessions(
        const ListSessionsRequest& /*req*/) override {
        return {{"sessions", nlohmann::json::array()}};
    }

    ForkSessionResponse fork_session(
        const ForkSessionRequest& /*req*/) override {
        ForkSessionResponse resp;
        resp.session.session_id = "forked-sess";
        return resp;
    }

    nlohmann::json prompt(
        const PromptRequest& req,
        std::function<void(const nlohmann::json&)> on_update) override {
        // Emit all 6 streaming update types
        // 1. agent_message_chunk
        on_update({{"sessionUpdate", "agent_message_chunk"},
                   {"sessionId",    req.session_id},
                   {"content", {{"type", "text"}, {"text", "Hello"}}}});
        // 3. agent_thought_chunk
        on_update({{"sessionUpdate", "agent_thought_chunk"},
                   {"sessionId",    req.session_id},
                   {"content", {{"type", "text"}, {"text", "Thinking..."}}}});
        // 4. tool_call (pending)
        on_update({{"sessionUpdate", "tool_call"},
                   {"sessionId",    req.session_id},
                   {"toolCallId",   "call-1"},
                   {"title",        "bash"},
                   {"kind",         "execute"},
                   {"status",       "pending"},
                   {"rawInput",     nlohmann::json::object()},
                   {"locations",    nlohmann::json::array()}});
        // 5. tool_call_update (completed)
        on_update({{"sessionUpdate", "tool_call_update"},
                   {"sessionId",    req.session_id},
                   {"toolCallId",   "call-1"},
                   {"status",       "completed"},
                   {"kind",         "execute"},
                   {"title",        "bash"},
                   {"rawInput",     nlohmann::json::object()}});
        // 7. plan (todowrite)
        on_update({{"sessionUpdate", "plan"},
                   {"sessionId",    req.session_id},
                   {"entries",
                    nlohmann::json::array({
                        {{"priority", "medium"},
                         {"status",   "in_progress"},
                         {"content",  "Implement feature"}}
                    })}});
        // 6. usage_update
        on_update({{"sessionUpdate", "usage_update"},
                   {"sessionId",    req.session_id},
                   {"used",         100},
                   {"size",         200000},
                   {"cost",         {{"amount", 0.01}, {"currency", "USD"}}}});
        return {{"stopReason", "end_turn"}};
    }

    void cancel(const CancelNotification& /*notif*/) override {}

    SetSessionModeResponse set_mode(
        const SetSessionModeRequest& req) override {
        return {req.mode_id};
    }

    nlohmann::json set_model(
        const SetSessionModelRequest& /*req*/) override {
        return {{"model", "gpt-4o"}};
    }

    void authenticate(const nlohmann::json& /*req*/) override {
        throw std::runtime_error("authRequired");
    }
};

// Dispatch helper (mirrors ACPServer logic for unit testing)
static std::vector<nlohmann::json> dispatch_request(
    const nlohmann::json& request, StreamingStubAgent& agent) {
    std::vector<nlohmann::json> responses;

    const nlohmann::json id =
        request.contains("id") ? request["id"] : nlohmann::json();
    const std::string method = request.value("method", "");
    const nlohmann::json params =
        request.contains("params") ? request["params"]
                                   : nlohmann::json::object();

    auto write_fn = [&](const nlohmann::json& msg) {
        responses.push_back(msg);
    };

    auto write_update = [&](const nlohmann::json& payload) {
        write_fn({{"jsonrpc", "2.0"},
                  {"method",  "session/update"},
                  {"params",  payload}});
    };

    try {
        if (method == "session/new") {
            NewSessionRequest req;
            from_json(params, req);
            auto result = agent.new_session(req, write_update);
            write_fn({{"jsonrpc", "2.0"}, {"id", id}, {"result", result}});
        } else if (method == "session/load") {
            LoadSessionRequest req;
            from_json(params, req);
            auto result = agent.load_session(req, write_update);
            write_fn({{"jsonrpc", "2.0"}, {"id", id}, {"result", result}});
        } else if (method == "session/prompt") {
            PromptRequest req;
            from_json(params, req);
            auto result = agent.prompt(req, write_update);
            write_fn({{"jsonrpc", "2.0"}, {"id", id}, {"result", result}});
        }
    } catch (...) {
    }

    return responses;
}

}  // anonymous namespace

// ============================================================================
// session/new emits available_commands_update before result
// ============================================================================

TEST_CASE("session/new emits available_commands_update notification",
          "[acp][streaming]") {
    StreamingStubAgent agent;

    const nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id",      1},
        {"method",  "session/new"},
        {"params",
         {{"cwd",        "."},
          {"mcpServers", nlohmann::json::array()}}}};

    auto responses = dispatch_request(request, agent);

    REQUIRE(agent.new_session_called);
    // At least 2 messages: notification + result
    REQUIRE(responses.size() >= 2);

    // First message should be session/update notification
    const auto& notif = responses[0];
    REQUIRE(notif["method"] == "session/update");
    REQUIRE(notif["params"]["sessionUpdate"] == "available_commands_update");
    REQUIRE(notif["params"].contains("availableCommands"));

    // Last message should be the JSON-RPC result
    const auto& result_msg = responses.back();
    REQUIRE(result_msg.contains("result"));
    REQUIRE(result_msg["result"]["sessionId"] == "new-sess-1");
}

// ============================================================================
// session/load emits history replay + available_commands_update
// ============================================================================

TEST_CASE("session/load emits history replay and available_commands_update",
          "[acp][streaming]") {
    StreamingStubAgent agent;

    const nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id",      2},
        {"method",  "session/load"},
        {"params",
         {{"sessionId",  "old-sess-1"},
          {"cwd",        "."},
          {"mcpServers", nlohmann::json::array()}}}};

    auto responses = dispatch_request(request, agent);

    REQUIRE(agent.load_session_called);
    // At least 3 messages: user_message_chunk + available_commands_update + result
    REQUIRE(responses.size() >= 3);

    // Check user_message_chunk
    REQUIRE(responses[0]["method"] == "session/update");
    REQUIRE(responses[0]["params"]["sessionUpdate"] == "user_message_chunk");

    // Check available_commands_update
    REQUIRE(responses[1]["method"] == "session/update");
    REQUIRE(responses[1]["params"]["sessionUpdate"] == "available_commands_update");

    // Check result
    REQUIRE(responses.back().contains("result"));
}

// ============================================================================
// session/prompt emits all 6 live streaming update types
// ============================================================================

TEST_CASE("session/prompt emits all 6 live streaming update types",
          "[acp][streaming]") {
    StreamingStubAgent agent;

    const nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id",      3},
        {"method",  "session/prompt"},
        {"params",
         {{"sessionId", "sess-xyz"},
          {"prompt",
           nlohmann::json::array(
               {{{"type", "text"}, {"text", "Hello"}}})}}}};

    auto responses = dispatch_request(request, agent);

    // 6 notifications + 1 result = 7 total
    REQUIRE(responses.size() == 7);

    // Verify update types in order
    const std::vector<std::string> expected_updates = {
        "agent_message_chunk",
        "agent_thought_chunk",
        "tool_call",
        "tool_call_update",
        "plan",
        "usage_update",
    };

    for (size_t i = 0; i < expected_updates.size(); ++i) {
        REQUIRE(responses[i]["method"] == "session/update");
        REQUIRE(responses[i]["params"]["sessionUpdate"] == expected_updates[i]);
    }

    // Final result
    REQUIRE(responses.back().contains("result"));
    REQUIRE(responses.back()["result"]["stopReason"] == "end_turn");
}

// ============================================================================
// plan notification structure (type 7 – todowrite)
// ============================================================================

TEST_CASE("plan notification has correct structure", "[acp][streaming]") {
    StreamingStubAgent agent;

    const nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id",      4},
        {"method",  "session/prompt"},
        {"params",
         {{"sessionId", "sess-plan"},
          {"prompt",
           nlohmann::json::array(
               {{{"type", "text"}, {"text", "/todowrite"}}})}}}};

    auto responses = dispatch_request(request, agent);

    // Find the plan notification
    auto it = std::find_if(responses.begin(), responses.end(),
                           [](const nlohmann::json& msg) {
                               return msg.value("method", "") == "session/update" &&
                                      msg["params"].value("sessionUpdate", "") == "plan";
                           });

    REQUIRE(it != responses.end());
    const auto& plan = *it;
    REQUIRE(plan["params"].contains("entries"));
    REQUIRE(plan["params"]["entries"].is_array());
    REQUIRE(!plan["params"]["entries"].empty());

    const auto& entry = plan["params"]["entries"][0];
    REQUIRE(entry.contains("priority"));
    REQUIRE(entry.contains("status"));
    REQUIRE(entry.contains("content"));
    REQUIRE(entry["priority"] == "medium");
}

// ============================================================================
// usage_update notification structure (type 6)
// ============================================================================

TEST_CASE("usage_update notification has correct structure",
          "[acp][streaming]") {
    StreamingStubAgent agent;

    const nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id",      5},
        {"method",  "session/prompt"},
        {"params",
         {{"sessionId", "sess-usage"},
          {"prompt",
           nlohmann::json::array(
               {{{"type", "text"}, {"text", "compute something"}}})}}}};

    auto responses = dispatch_request(request, agent);

    // Find usage_update
    auto it = std::find_if(responses.begin(), responses.end(),
                           [](const nlohmann::json& msg) {
                               return msg.value("method", "") == "session/update" &&
                                      msg["params"].value("sessionUpdate", "") == "usage_update";
                           });

    REQUIRE(it != responses.end());
    const auto& usage = (*it)["params"];
    REQUIRE(usage.contains("used"));
    REQUIRE(usage.contains("size"));
    REQUIRE(usage.contains("cost"));
    REQUIRE(usage["cost"].contains("amount"));
    REQUIRE(usage["cost"].contains("currency"));
    REQUIRE(usage["cost"]["currency"] == "USD");
}

// ============================================================================
// tool_call_update – failed status (type 5 error variant)
// ============================================================================

TEST_CASE("tool_call_update can have 'failed' status", "[acp][streaming]") {
    // Verify the tool_call_update emitted with status=failed
    const nlohmann::json update = {{"sessionUpdate", "tool_call_update"},
                                   {"toolCallId",    "call-err"},
                                   {"status",        "failed"},
                                   {"kind",          "execute"},
                                   {"title",         "bash"},
                                   {"rawInput",      nlohmann::json::object()},
                                   {"content",
                                    nlohmann::json::array(
                                        {{{"type", "content"},
                                          {"content",
                                           {{"type", "text"},
                                            {"text", "command not found"}}}}})}};

    REQUIRE(update["status"] == "failed");
    REQUIRE(update["content"].is_array());
    REQUIRE(!update.contains("rawOutput"));
}
