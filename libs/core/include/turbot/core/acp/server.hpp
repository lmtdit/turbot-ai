#pragma once

#include "turbot/core/acp/acp.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace turbot::core::acp {

// ---------------------------------------------------------------------------
// ACPAgent interface (pure virtual) – implemented in 3.9 TurbotACPAgent
// ---------------------------------------------------------------------------

class ACPAgent {
public:
    virtual ~ACPAgent() = default;

    virtual InitializeResponse initialize(const InitializeRequest& req)    = 0;

    /// Create a new session. on_update is called for async notifications
    /// (e.g., available_commands_update) aligned with OpenCode setTimeout(0).
    virtual nlohmann::json     new_session(
        const NewSessionRequest& req,
        std::function<void(const nlohmann::json& update_payload)> on_update) = 0;

    /// Load an existing session. on_update is called for history replay
    /// notifications and async available_commands_update.
    virtual nlohmann::json     load_session(
        const LoadSessionRequest& req,
        std::function<void(const nlohmann::json& update_payload)> on_update) = 0;

    virtual nlohmann::json     resume_session(const ResumeSessionRequest& req) = 0;
    virtual nlohmann::json     list_sessions(const ListSessionsRequest& req) = 0;
    virtual ForkSessionResponse fork_session(const ForkSessionRequest& req) = 0;

    /// Execute a prompt. on_update is called for each session/update notification.
    virtual nlohmann::json prompt(
        const PromptRequest& req,
        std::function<void(const nlohmann::json& update_payload)> on_update) = 0;

    virtual void                   cancel(const CancelNotification& notif) = 0;
    virtual SetSessionModeResponse set_mode(const SetSessionModeRequest& req) = 0;
    virtual nlohmann::json         set_model(const SetSessionModelRequest& req) = 0;
    virtual void                   authenticate(const nlohmann::json& req) = 0;
};

// ---------------------------------------------------------------------------
// ACPServer – JSON-RPC 2.0 dispatcher over stdin/stdout (ndJSON)
// ---------------------------------------------------------------------------

class ACPServer {
public:
    ACPServer() = delete;

    /// Start the server (blocking until stdin EOF or stop() is called).
    static void start(
        std::function<std::unique_ptr<ACPAgent>()> agent_factory,
        const std::string& cwd = ".");

    /// Stop the server (thread-safe; callable from signal handlers).
    static void stop();

private:
    static void dispatch(
        const nlohmann::json& request,
        std::unique_ptr<ACPAgent>& agent,
        const std::function<void(const nlohmann::json&)>& write_fn);

    static nlohmann::json make_error(
        const nlohmann::json& id, int code, const std::string& message,
        const nlohmann::json& data = nlohmann::json());

    static nlohmann::json make_result(
        const nlohmann::json& id, const nlohmann::json& result);

    static void write_response(
        const nlohmann::json& response,
        const std::function<void(const nlohmann::json&)>& write_fn);

    static std::atomic<bool> running_;
};

}  // namespace turbot::core::acp
