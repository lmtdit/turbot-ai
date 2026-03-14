#pragma once

#include "turbot/core/acp/acp.hpp"

#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace turbot::core::acp {

// ---------------------------------------------------------------------------
// ACPSessionState – in-memory ACP session state mapping
// (aligned with OpenCode types.ts ACPSessionState)
// ---------------------------------------------------------------------------

struct ACPSessionState {
    std::string id;                            // ACP session ID (= Turbot Session ID)
    std::string cwd;
    std::vector<nlohmann::json> mcp_servers;
    std::optional<std::string> provider_id;
    std::optional<std::string> model_id;
    std::optional<std::string> mode_id;
    std::optional<std::string> variant;        // model variant (e.g. "high")
    int64_t created_at = 0;                    // Unix timestamp (seconds)
};

// ---------------------------------------------------------------------------
// ACPSessionManager – maintains ACP session state map
// (aligned with OpenCode ACPSessionManager in session.ts)
// ---------------------------------------------------------------------------

class ACPSessionManager {
public:
    ACPSessionManager() = default;

    // Non-copyable
    ACPSessionManager(const ACPSessionManager&)            = delete;
    ACPSessionManager& operator=(const ACPSessionManager&) = delete;

    /// Create a new ACP session (stores state; caller is responsible for
    /// creating the underlying Turbot Session).
    ACPSessionState create(
        const std::string& session_id,
        const std::string& cwd,
        const std::vector<nlohmann::json>& mcp_servers,
        const std::optional<std::string>& provider_id = {},
        const std::optional<std::string>& model_id    = {});

    /// Load an existing session into the ACP state map.
    ACPSessionState load(
        const std::string& session_id,
        const std::string& cwd,
        const std::vector<nlohmann::json>& mcp_servers,
        const std::optional<std::string>& provider_id = {},
        const std::optional<std::string>& model_id    = {});

    /// Get session state by ID (throws std::invalid_argument if not found).
    [[nodiscard]] ACPSessionState get(const std::string& session_id);

    /// Try to get session state (returns nullptr if not found).
    [[nodiscard]] ACPSessionState* try_get(const std::string& session_id);

    /// Set the model for a session.
    void set_model(const std::string& session_id,
                   const std::string& provider_id,
                   const std::string& model_id);

    /// Set the mode for a session.
    void set_mode(const std::string& session_id, const std::string& mode_id);

    /// Set the model variant for a session.
    void set_variant(const std::string& session_id,
                     const std::optional<std::string>& variant);

    /// Get the model variant for a session (returns "" if unset).
    [[nodiscard]] std::string get_variant(const std::string& session_id);

    /// Get the mode ID for a session (returns "" if unset).
    [[nodiscard]] std::string get_mode_id(const std::string& session_id);

    /// List all session states (snapshot copy).
    [[nodiscard]] std::vector<ACPSessionState> list() const;

    /// Remove a session from the map.
    void remove(const std::string& session_id);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, ACPSessionState> sessions_;
};

}  // namespace turbot::core::acp
