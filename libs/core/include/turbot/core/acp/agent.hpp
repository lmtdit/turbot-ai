#pragma once

#include "turbot/core/acp/acp.hpp"
#include "turbot/core/acp/server.hpp"
#include "turbot/core/acp/session.hpp"

#include "turbot/core/agent/agent.hpp"
#include "turbot/core/common/version.hpp"
#include "turbot/core/llm/stream_event.hpp"
#include "turbot/core/session/session.hpp"
#include "turbot/core/session/session_loop.hpp"
#include "turbot/core/tool/tool.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace turbot::core::acp {

// ---------------------------------------------------------------------------
// Tool kind mapping (aligned with OpenCode toToolKind)
// ---------------------------------------------------------------------------

/// Map a Turbot tool name to an ACP tool kind string.
[[nodiscard]] std::string to_tool_kind(const std::string& tool_name);

// ---------------------------------------------------------------------------
// TurbotACPAgent – bridges ACP protocol to the Turbot Session system
// (implements ACPAgent from server.hpp)
// ---------------------------------------------------------------------------

class TurbotACPAgent : public ACPAgent {
public:
    explicit TurbotACPAgent(const std::string& default_cwd = ".");
    ~TurbotACPAgent() override = default;

    // Non-copyable, non-movable
    TurbotACPAgent(const TurbotACPAgent&)            = delete;
    TurbotACPAgent& operator=(const TurbotACPAgent&) = delete;

    // -----------------------------------------------------------------------
    // ACPAgent interface
    // -----------------------------------------------------------------------

    InitializeResponse initialize(const InitializeRequest& req) override;

    nlohmann::json new_session(
        const NewSessionRequest& req,
        std::function<void(const nlohmann::json& update_payload)> on_update) override;

    nlohmann::json load_session(
        const LoadSessionRequest& req,
        std::function<void(const nlohmann::json& update_payload)> on_update) override;

    nlohmann::json resume_session(const ResumeSessionRequest& req) override;

    nlohmann::json list_sessions(const ListSessionsRequest& req) override;

    ForkSessionResponse fork_session(const ForkSessionRequest& req) override;

    /// Execute a prompt. on_update is called for each streaming update.
    nlohmann::json prompt(
        const PromptRequest& req,
        std::function<void(const nlohmann::json& update_payload)> on_update) override;

    void cancel(const CancelNotification& notif) override;

    SetSessionModeResponse set_mode(const SetSessionModeRequest& req) override;

    nlohmann::json set_model(const SetSessionModelRequest& req) override;

    /// Always throws authRequired (aligned with OpenCode behavior).
    void authenticate(const nlohmann::json& req) override;

    /// Build an available_commands_update notification payload.
    /// Public to allow unit testing and reuse in server.cpp session/new/load.
    [[nodiscard]] static nlohmann::json build_available_commands_update(
        const std::string& session_id);

private:
    std::string default_cwd_;
    ACPSessionManager session_manager_;

    // -----------------------------------------------------------------------
    // Active loop registry – used by cancel() to stop a running prompt()
    // -----------------------------------------------------------------------
    mutable std::mutex            active_loops_mutex_;
    /// Maps session_id → raw pointer to the SessionLoop currently executing
    /// prompt() for that session.  Protected by active_loops_mutex_.
    /// The pointer is valid only while prompt() is on the call stack for the
    /// corresponding session; cancel() must hold the mutex long enough to call
    /// loop->stop() and then release it before the loop object is destroyed.
    std::unordered_map<std::string, session::SessionLoop*> active_loops_;

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    /// Build available modes list from registered Turbot agents.
    [[nodiscard]] nlohmann::json get_available_modes() const;

    /// Build available models list from registered providers.
    [[nodiscard]] nlohmann::json get_available_models(
        const std::string& cwd) const;

    /// Replay a single Turbot session message as ACP session/update notifications.
    void process_message(
        const nlohmann::json& msg_json,
        const std::function<void(const nlohmann::json&)>& on_update);

    /// Compute usage summary from a set of session messages.
    [[nodiscard]] nlohmann::json compute_usage(
        const std::vector<nlohmann::json>& messages) const;

    /// Build a SessionInfo JSON object from an ACPSessionState.
    [[nodiscard]] static nlohmann::json session_info_from_state(
        const ACPSessionState& state,
        const session::Session& sess);

    /// Parse "providerID/modelID[/variant]" into (provider, model, variant).
    static void parse_model_string(
        const std::string& model_str,
        std::string& provider_id,
        std::string& model_id,
        std::optional<std::string>& variant);
};

}  // namespace turbot::core::acp
