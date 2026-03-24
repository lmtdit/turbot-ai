#include "turbot/core/acp/server.hpp"
#include "turbot/core/common/logger.hpp"

#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>

namespace turbot::core::acp {

// Static member definition
std::atomic<bool> ACPServer::running_{false};

// Output mutex to prevent interleaved writes on stdout
static std::mutex g_write_mutex;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void write_line(const std::string& line) {
    std::lock_guard<std::mutex> lk(g_write_mutex);
    std::cout << line << '\n';
    std::cout.flush();
}

void ACPServer::write_response(
    const nlohmann::json& response,
    const std::function<void(const nlohmann::json&)>& write_fn) {
    write_fn(response);
}

nlohmann::json ACPServer::make_result(
    const nlohmann::json& id, const nlohmann::json& result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

nlohmann::json ACPServer::make_error(
    const nlohmann::json& id, int code, const std::string& message,
    const nlohmann::json& data) {
    nlohmann::json err = {{"code", code}, {"message", message}};
    if (!data.is_null()) err["data"] = data;
    return {{"jsonrpc", "2.0"}, {"id", id}, {"error", err}};
}

// ---------------------------------------------------------------------------
// Dispatcher – maps JSON-RPC method → ACPAgent virtual call
// ---------------------------------------------------------------------------

void ACPServer::dispatch(
    const nlohmann::json& request,
    std::unique_ptr<ACPAgent>& agent,
    const std::function<void(const nlohmann::json&)>& write_fn) {

    // id may be absent for notifications
    nlohmann::json id = nlohmann::json();  // null
    if (request.contains("id")) id = request["id"];

    const std::string method = request.value("method", "");
    const nlohmann::json params =
        request.contains("params") ? request["params"] : nlohmann::json::object();

    // ---------------------------------------------------------------------------
    // cancel – notification (no id, no response)
    // ---------------------------------------------------------------------------
    if (method == "cancel") {
        try {
            CancelNotification notif;
            from_json(params, notif);
            agent->cancel(notif);
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("cancel notification failed: {}", e.what());
        } catch (...) {
            TURBOT_LOG_DEBUG("cancel notification failed with unknown error");
        }
        return;
    }

    // All remaining methods require an id
    if (id.is_null()) {
        // Unknown notification – ignore silently
        return;
    }

    try {
        // -----------------------------------------------------------------------
        // initialize
        // -----------------------------------------------------------------------
        if (method == "initialize") {
            InitializeRequest req;
            from_json(params, req);
            auto resp = agent->initialize(req);
            nlohmann::json j_resp;
            to_json(j_resp, resp);
            write_fn(make_result(id, j_resp));
            return;
        }

        // -----------------------------------------------------------------------
        // session/new
        // -----------------------------------------------------------------------
        if (method == "session/new") {
            NewSessionRequest req;
            from_json(params, req);

            // on_update for available_commands_update notification
            auto write_update = [&](const nlohmann::json& payload) {
                write_fn({{"jsonrpc", "2.0"},
                          {"method", "session/update"},
                          {"params", payload}});
            };

            auto result = agent->new_session(req, write_update);
            write_fn(make_result(id, result));
            return;
        }

        // -----------------------------------------------------------------------
        // session/load
        // -----------------------------------------------------------------------
        if (method == "session/load") {
            LoadSessionRequest req;
            from_json(params, req);

            // on_update for history replay + available_commands_update
            auto write_update = [&](const nlohmann::json& payload) {
                write_fn({{"jsonrpc", "2.0"},
                          {"method", "session/update"},
                          {"params", payload}});
            };

            auto result = agent->load_session(req, write_update);
            write_fn(make_result(id, result));
            return;
        }

        // -----------------------------------------------------------------------
        // session/resume
        // -----------------------------------------------------------------------
        if (method == "session/resume") {
            ResumeSessionRequest req;
            from_json(params, req);
            auto result = agent->resume_session(req);
            write_fn(make_result(id, result));
            return;
        }

        // -----------------------------------------------------------------------
        // session/prompt – streaming: write session/update notifications inline
        // -----------------------------------------------------------------------
        if (method == "session/prompt") {
            PromptRequest req;
            from_json(params, req);

            auto write_update = [&](const nlohmann::json& payload) {
                nlohmann::json notif = {
                    {"jsonrpc", "2.0"},
                    {"method", "session/update"},
                    {"params", payload}};
                write_fn(notif);
            };

            auto result = agent->prompt(req, write_update);
            write_fn(make_result(id, result));
            return;
        }

        // -----------------------------------------------------------------------
        // session/fork
        // -----------------------------------------------------------------------
        if (method == "session/fork") {
            ForkSessionRequest req;
            from_json(params, req);
            auto resp = agent->fork_session(req);
            nlohmann::json j_resp;
            to_json(j_resp, resp);
            write_fn(make_result(id, j_resp));
            return;
        }

        // -----------------------------------------------------------------------
        // session/mode
        // -----------------------------------------------------------------------
        if (method == "session/mode") {
            SetSessionModeRequest req;
            from_json(params, req);
            auto resp = agent->set_mode(req);
            nlohmann::json j_resp;
            to_json(j_resp, resp);
            write_fn(make_result(id, j_resp));
            return;
        }

        // -----------------------------------------------------------------------
        // session/model
        // -----------------------------------------------------------------------
        if (method == "session/model") {
            SetSessionModelRequest req;
            from_json(params, req);
            auto result = agent->set_model(req);
            write_fn(make_result(id, result));
            return;
        }

        // -----------------------------------------------------------------------
        // session/list (unstable_listSessions in OpenCode)
        // -----------------------------------------------------------------------
        if (method == "session/list") {
            ListSessionsRequest req;
            from_json(params, req);
            auto result = agent->list_sessions(req);
            write_fn(make_result(id, result));
            return;
        }

        // -----------------------------------------------------------------------
        // authenticate
        // -----------------------------------------------------------------------
        if (method == "authenticate") {
            agent->authenticate(params);
            write_fn(make_result(id, nlohmann::json::object()));
            return;
        }

        // -----------------------------------------------------------------------
        // Method not found
        // -----------------------------------------------------------------------
        write_fn(make_error(id, -32601, "Method not found"));

    } catch (const nlohmann::json::exception& ex) {
        // JSON parse / key access failure → Invalid params
        write_fn(make_error(id, -32602, std::string("Invalid params: ") + ex.what()));
    } catch (const std::exception& ex) {
        const std::string msg = ex.what();

        // ACP authRequired error convention
        if (msg.find("authRequired") != std::string::npos ||
            msg.find("auth_required") != std::string::npos) {
            write_fn(make_error(id, -32000, "authRequired",
                                nlohmann::json{{"type", "authRequired"}}));
        } else {
            write_fn(make_error(id, -32603, std::string("Internal error: ") + msg));
        }
    } catch (...) {
        write_fn(make_error(id, -32603, "Internal error: unknown exception"));
    }
}

// ---------------------------------------------------------------------------
// ACPServer::stop
// ---------------------------------------------------------------------------

void ACPServer::stop() {
    running_.store(false);
}

// ---------------------------------------------------------------------------
// ACPServer::start – main loop (blocking)
// ---------------------------------------------------------------------------

void ACPServer::start(
    std::function<std::unique_ptr<ACPAgent>()> agent_factory,
    const std::string& /* cwd */) {

    running_.store(true);

    // Create the agent instance via factory
    std::unique_ptr<ACPAgent> agent;
    try {
        agent = agent_factory();
    } catch (const std::exception& ex) {
        // Fatal: cannot create agent
        nlohmann::json err = {
            {"jsonrpc", "2.0"},
            {"id", nullptr},
            {"error", {{"code", -32603}, {"message",
                std::string("Failed to create agent: ") + ex.what()}}}};
        write_line(err.dump());
        return;
    }

    // Wire the write function to stdout (ndJSON – one JSON per line)
    auto write_fn = [](const nlohmann::json& msg) {
        write_line(msg.dump());
    };

    std::string line;
    while (running_.load() && std::getline(std::cin, line)) {
        if (line.empty()) continue;  // skip blank lines

        // Parse JSON-RPC message
        nlohmann::json request;
        try {
            request = nlohmann::json::parse(line);
        } catch (const nlohmann::json::exception& ex) {
            nlohmann::json err = {
                {"jsonrpc", "2.0"},
                {"id", nullptr},
                {"error", {{"code", -32700},
                           {"message", std::string("Parse error: ") + ex.what()}}}};
            write_fn(err);
            continue;
        }

        if (!request.is_object()) {
            nlohmann::json err = {
                {"jsonrpc", "2.0"},
                {"id", nullptr},
                {"error", {{"code", -32600}, {"message", "Invalid Request"}}}};
            write_fn(err);
            continue;
        }

        dispatch(request, agent, write_fn);
    }

    running_.store(false);
}

}  // namespace turbot::core::acp

// ---------------------------------------------------------------------------
// ACPServer::TestAccess - exposes private methods for unit testing
// ---------------------------------------------------------------------------

void turbot::core::acp::ACPServer::TestAccess::dispatch(
    const nlohmann::json& request,
    std::unique_ptr<ACPAgent>& agent,
    const std::function<void(const nlohmann::json&)>& write_fn) {
    ACPServer::dispatch(request, agent, write_fn);
}

nlohmann::json turbot::core::acp::ACPServer::TestAccess::make_error(
    const nlohmann::json& id, int code, const std::string& message,
    const nlohmann::json& data) {
    return ACPServer::make_error(id, code, message, data);
}

nlohmann::json turbot::core::acp::ACPServer::TestAccess::make_result(
    const nlohmann::json& id, const nlohmann::json& result) {
    return ACPServer::make_result(id, result);
}

void turbot::core::acp::ACPServer::TestAccess::write_response(
    const nlohmann::json& response,
    const std::function<void(const nlohmann::json&)>& write_fn) {
    ACPServer::write_response(response, write_fn);
}
