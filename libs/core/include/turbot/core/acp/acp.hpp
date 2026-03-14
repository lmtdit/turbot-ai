#pragma once

#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace turbot::core::acp {

// ---------------------------------------------------------------------------
// Basic capability types
// ---------------------------------------------------------------------------

struct ModelOption {
    std::string model_id;  // "modelId" in ACP wire format
    std::string name;
};

struct ModeOption {
    std::string id;
    std::string name;
    std::optional<std::string> description;
};

struct AgentCapabilities {
    bool load_session = true;  // "loadSession"
    struct McpCaps {
        bool http = true;
        bool sse  = true;
    } mcp_capabilities;
    struct PromptCaps {
        bool embedded_context = true;
        bool image            = true;
    } prompt_capabilities;
    struct SessionCaps {
        bool fork   = true;
        bool list   = true;
        bool resume = true;
    } session_capabilities;
};

struct AuthMethod {
    std::string id;
    std::string name;
    std::string description;
    nlohmann::json meta = nlohmann::json::object();  // terminal-auth metadata
};

struct AgentInfo {
    std::string name;
    std::string version;
};

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

struct InitializeRequest {
    int protocol_version = 1;
    std::optional<std::string> authentication;
    nlohmann::json client_capabilities = nlohmann::json::object();
};

struct InitializeResponse {
    int protocol_version = 1;
    AgentCapabilities agent_capabilities;
    std::vector<AuthMethod> auth_methods;
    AgentInfo agent_info;
};

// ---------------------------------------------------------------------------
// Session types
// ---------------------------------------------------------------------------

struct SessionInfo {
    std::string session_id;
    std::string cwd;
    std::optional<std::string> model_id;
    std::optional<std::string> mode;
    std::vector<nlohmann::json> mcp_servers;
    std::optional<std::string> title;
    std::optional<std::string> updated_at;
};

struct NewSessionRequest {
    std::string cwd;
    std::vector<nlohmann::json> mcp_servers;
    std::optional<std::string> model_id;
};

struct LoadSessionRequest {
    std::string session_id;
    std::string cwd;
    std::vector<nlohmann::json> mcp_servers;
    std::optional<std::string> model_id;
};

struct ResumeSessionRequest {
    std::string session_id;
    std::string cwd;
    std::vector<nlohmann::json> mcp_servers;
};

struct ForkSessionRequest {
    std::string session_id;
    std::string cwd;
    std::vector<nlohmann::json> mcp_servers;
};

struct ForkSessionResponse {
    SessionInfo session;
};

// ---------------------------------------------------------------------------
// Prompt / content types
// ---------------------------------------------------------------------------

struct TextContent {
    std::string type = "text";
    std::string text;
};

struct ResourceContent {
    std::string type = "resource";
    std::string uri;
    std::optional<std::string> mime_type;
};

struct PromptRequest {
    std::string session_id;
    std::vector<nlohmann::json> prompt;  // TextContent | ResourceContent | ...
    std::optional<std::string> mode;
    std::optional<std::string> model;
};

// ---------------------------------------------------------------------------
// Mode / model / cancel
// ---------------------------------------------------------------------------

struct SetSessionModeRequest {
    std::string session_id;
    std::string mode_id;
};

struct SetSessionModeResponse {
    std::string mode;
};

struct SetSessionModelRequest {
    std::string session_id;
    std::string model_id;
};

struct CancelNotification {
    std::string session_id;
};

// ---------------------------------------------------------------------------
// List sessions
// ---------------------------------------------------------------------------

struct ListSessionsRequest {
    std::optional<std::string> cwd;
    std::optional<std::string> cursor;  // Unix timestamp (seconds), pagination cursor
};

// ---------------------------------------------------------------------------
// Stop reason / usage
// ---------------------------------------------------------------------------

enum class StopReason { EndTurn, ToolUse, StopSequence, Error, Cancelled };

struct Usage {
    int total_tokens  = 0;
    int input_tokens  = 0;
    int output_tokens = 0;
    std::optional<int> thought_tokens;
    std::optional<int> cached_read_tokens;
    std::optional<int> cached_write_tokens;
};

// ---------------------------------------------------------------------------
// JSON serialization helpers
// ---------------------------------------------------------------------------

// AgentCapabilities
inline void to_json(nlohmann::json& j, const AgentCapabilities& v) {
    j = {
        {"loadSession", v.load_session},
        {"mcpCapabilities", {{"http", v.mcp_capabilities.http}, {"sse", v.mcp_capabilities.sse}}},
        {"promptCapabilities",
         {{"embeddedContext", v.prompt_capabilities.embedded_context},
          {"image", v.prompt_capabilities.image}}},
        {"sessionCapabilities",
         {{"fork", nlohmann::json::object()},
          {"list", nlohmann::json::object()},
          {"resume", nlohmann::json::object()}}},
    };
}

inline void from_json(const nlohmann::json& j, AgentCapabilities& v) {
    if (j.contains("loadSession")) j.at("loadSession").get_to(v.load_session);
    if (j.contains("mcpCapabilities")) {
        auto& mc = j["mcpCapabilities"];
        if (mc.contains("http")) mc.at("http").get_to(v.mcp_capabilities.http);
        if (mc.contains("sse")) mc.at("sse").get_to(v.mcp_capabilities.sse);
    }
    if (j.contains("promptCapabilities")) {
        auto& pc = j["promptCapabilities"];
        if (pc.contains("embeddedContext"))
            pc.at("embeddedContext").get_to(v.prompt_capabilities.embedded_context);
        if (pc.contains("image")) pc.at("image").get_to(v.prompt_capabilities.image);
    }
    if (j.contains("sessionCapabilities")) {
        auto& sc = j["sessionCapabilities"];
        v.session_capabilities.fork   = sc.contains("fork");
        v.session_capabilities.list   = sc.contains("list");
        v.session_capabilities.resume = sc.contains("resume");
    }
}

// AuthMethod
inline void to_json(nlohmann::json& j, const AuthMethod& v) {
    j = {{"id", v.id}, {"name", v.name}, {"description", v.description}};
    if (!v.meta.empty()) j["_meta"] = v.meta;
}

inline void from_json(const nlohmann::json& j, AuthMethod& v) {
    j.at("id").get_to(v.id);
    j.at("name").get_to(v.name);
    if (j.contains("description")) j.at("description").get_to(v.description);
    if (j.contains("_meta")) v.meta = j["_meta"];
}

// AgentInfo
inline void to_json(nlohmann::json& j, const AgentInfo& v) {
    j = {{"name", v.name}, {"version", v.version}};
}

inline void from_json(const nlohmann::json& j, AgentInfo& v) {
    j.at("name").get_to(v.name);
    j.at("version").get_to(v.version);
}

// InitializeRequest
inline void to_json(nlohmann::json& j, const InitializeRequest& v) {
    j = {{"protocolVersion", v.protocol_version},
         {"clientCapabilities", v.client_capabilities}};
    if (v.authentication) j["authentication"] = *v.authentication;
}

inline void from_json(const nlohmann::json& j, InitializeRequest& v) {
    if (j.contains("protocolVersion")) j.at("protocolVersion").get_to(v.protocol_version);
    if (j.contains("authentication") && j["authentication"].is_string())
        v.authentication = j["authentication"].get<std::string>();
    if (j.contains("clientCapabilities"))
        v.client_capabilities = j["clientCapabilities"];
}

// InitializeResponse
inline void to_json(nlohmann::json& j, const InitializeResponse& v) {
    j = {{"protocolVersion", v.protocol_version},
         {"agentCapabilities", v.agent_capabilities},
         {"authMethods", v.auth_methods},
         {"agentInfo", v.agent_info}};
}

inline void from_json(const nlohmann::json& j, InitializeResponse& v) {
    if (j.contains("protocolVersion")) j.at("protocolVersion").get_to(v.protocol_version);
    if (j.contains("agentCapabilities")) from_json(j["agentCapabilities"], v.agent_capabilities);
    if (j.contains("authMethods")) j.at("authMethods").get_to(v.auth_methods);
    if (j.contains("agentInfo")) from_json(j["agentInfo"], v.agent_info);
}

// SessionInfo
inline void to_json(nlohmann::json& j, const SessionInfo& v) {
    j = {{"sessionId", v.session_id}, {"cwd", v.cwd}, {"mcpServers", v.mcp_servers}};
    if (v.model_id) j["modelId"]   = *v.model_id;
    if (v.mode) j["mode"]          = *v.mode;
    if (v.title) j["title"]        = *v.title;
    if (v.updated_at) j["updatedAt"] = *v.updated_at;
}

inline void from_json(const nlohmann::json& j, SessionInfo& v) {
    j.at("sessionId").get_to(v.session_id);
    j.at("cwd").get_to(v.cwd);
    if (j.contains("mcpServers")) v.mcp_servers = j["mcpServers"].get<std::vector<nlohmann::json>>();
    if (j.contains("modelId") && j["modelId"].is_string()) v.model_id = j["modelId"].get<std::string>();
    if (j.contains("mode") && j["mode"].is_string()) v.mode = j["mode"].get<std::string>();
    if (j.contains("title") && j["title"].is_string()) v.title = j["title"].get<std::string>();
    if (j.contains("updatedAt") && j["updatedAt"].is_string())
        v.updated_at = j["updatedAt"].get<std::string>();
}

// NewSessionRequest
inline void to_json(nlohmann::json& j, const NewSessionRequest& v) {
    j = {{"cwd", v.cwd}, {"mcpServers", v.mcp_servers}};
    if (v.model_id) j["modelId"] = *v.model_id;
}

inline void from_json(const nlohmann::json& j, NewSessionRequest& v) {
    j.at("cwd").get_to(v.cwd);
    if (j.contains("mcpServers")) v.mcp_servers = j["mcpServers"].get<std::vector<nlohmann::json>>();
    if (j.contains("modelId") && j["modelId"].is_string()) v.model_id = j["modelId"].get<std::string>();
}

// LoadSessionRequest
inline void to_json(nlohmann::json& j, const LoadSessionRequest& v) {
    j = {{"sessionId", v.session_id}, {"cwd", v.cwd}, {"mcpServers", v.mcp_servers}};
    if (v.model_id) j["modelId"] = *v.model_id;
}

inline void from_json(const nlohmann::json& j, LoadSessionRequest& v) {
    j.at("sessionId").get_to(v.session_id);
    j.at("cwd").get_to(v.cwd);
    if (j.contains("mcpServers")) v.mcp_servers = j["mcpServers"].get<std::vector<nlohmann::json>>();
    if (j.contains("modelId") && j["modelId"].is_string()) v.model_id = j["modelId"].get<std::string>();
}

// ResumeSessionRequest
inline void to_json(nlohmann::json& j, const ResumeSessionRequest& v) {
    j = {{"sessionId", v.session_id}, {"cwd", v.cwd}, {"mcpServers", v.mcp_servers}};
}

inline void from_json(const nlohmann::json& j, ResumeSessionRequest& v) {
    j.at("sessionId").get_to(v.session_id);
    j.at("cwd").get_to(v.cwd);
    if (j.contains("mcpServers")) v.mcp_servers = j["mcpServers"].get<std::vector<nlohmann::json>>();
}

// ForkSessionRequest
inline void to_json(nlohmann::json& j, const ForkSessionRequest& v) {
    j = {{"sessionId", v.session_id}, {"cwd", v.cwd}, {"mcpServers", v.mcp_servers}};
}

inline void from_json(const nlohmann::json& j, ForkSessionRequest& v) {
    j.at("sessionId").get_to(v.session_id);
    j.at("cwd").get_to(v.cwd);
    if (j.contains("mcpServers")) v.mcp_servers = j["mcpServers"].get<std::vector<nlohmann::json>>();
}

// ForkSessionResponse
inline void to_json(nlohmann::json& j, const ForkSessionResponse& v) {
    j = {{"session", v.session}};
}

inline void from_json(const nlohmann::json& j, ForkSessionResponse& v) {
    from_json(j.at("session"), v.session);
}

// PromptRequest
inline void to_json(nlohmann::json& j, const PromptRequest& v) {
    j = {{"sessionId", v.session_id}, {"prompt", v.prompt}};
    if (v.mode) j["mode"]   = *v.mode;
    if (v.model) j["model"] = *v.model;
}

inline void from_json(const nlohmann::json& j, PromptRequest& v) {
    j.at("sessionId").get_to(v.session_id);
    if (j.contains("prompt")) v.prompt = j["prompt"].get<std::vector<nlohmann::json>>();
    if (j.contains("mode") && j["mode"].is_string()) v.mode = j["mode"].get<std::string>();
    if (j.contains("model") && j["model"].is_string()) v.model = j["model"].get<std::string>();
}

// SetSessionModeRequest
inline void to_json(nlohmann::json& j, const SetSessionModeRequest& v) {
    j = {{"sessionId", v.session_id}, {"modeId", v.mode_id}};
}

inline void from_json(const nlohmann::json& j, SetSessionModeRequest& v) {
    j.at("sessionId").get_to(v.session_id);
    j.at("modeId").get_to(v.mode_id);
}

// SetSessionModeResponse
inline void to_json(nlohmann::json& j, const SetSessionModeResponse& v) {
    j = {{"mode", v.mode}};
}

inline void from_json(const nlohmann::json& j, SetSessionModeResponse& v) {
    j.at("mode").get_to(v.mode);
}

// SetSessionModelRequest
inline void to_json(nlohmann::json& j, const SetSessionModelRequest& v) {
    j = {{"sessionId", v.session_id}, {"modelId", v.model_id}};
}

inline void from_json(const nlohmann::json& j, SetSessionModelRequest& v) {
    j.at("sessionId").get_to(v.session_id);
    j.at("modelId").get_to(v.model_id);
}

// CancelNotification
inline void to_json(nlohmann::json& j, const CancelNotification& v) {
    j = {{"sessionId", v.session_id}};
}

inline void from_json(const nlohmann::json& j, CancelNotification& v) {
    j.at("sessionId").get_to(v.session_id);
}

// ListSessionsRequest
inline void to_json(nlohmann::json& j, const ListSessionsRequest& v) {
    j = nlohmann::json::object();
    if (v.cwd) j["cwd"]       = *v.cwd;
    if (v.cursor) j["cursor"] = *v.cursor;
}

inline void from_json(const nlohmann::json& j, ListSessionsRequest& v) {
    if (j.contains("cwd") && j["cwd"].is_string()) v.cwd = j["cwd"].get<std::string>();
    if (j.contains("cursor") && j["cursor"].is_string())
        v.cursor = j["cursor"].get<std::string>();
}

// StopReason
inline std::string stop_reason_to_string(StopReason r) {
    switch (r) {
        case StopReason::EndTurn:      return "end_turn";
        case StopReason::ToolUse:      return "tool_use";
        case StopReason::StopSequence: return "stop_sequence";
        case StopReason::Error:        return "error";
        case StopReason::Cancelled:    return "cancelled";
        default:                       return "end_turn";
    }
}

inline void to_json(nlohmann::json& j, StopReason r) {
    j = stop_reason_to_string(r);
}

// Usage
inline void to_json(nlohmann::json& j, const Usage& v) {
    j = {{"totalTokens", v.total_tokens},
         {"inputTokens", v.input_tokens},
         {"outputTokens", v.output_tokens}};
    if (v.thought_tokens) j["thoughtTokens"]         = *v.thought_tokens;
    if (v.cached_read_tokens) j["cachedReadTokens"]   = *v.cached_read_tokens;
    if (v.cached_write_tokens) j["cachedWriteTokens"] = *v.cached_write_tokens;
}

inline void from_json(const nlohmann::json& j, Usage& v) {
    if (j.contains("totalTokens")) j.at("totalTokens").get_to(v.total_tokens);
    if (j.contains("inputTokens")) j.at("inputTokens").get_to(v.input_tokens);
    if (j.contains("outputTokens")) j.at("outputTokens").get_to(v.output_tokens);
    if (j.contains("thoughtTokens") && j["thoughtTokens"].is_number())
        v.thought_tokens = j["thoughtTokens"].get<int>();
    if (j.contains("cachedReadTokens") && j["cachedReadTokens"].is_number())
        v.cached_read_tokens = j["cachedReadTokens"].get<int>();
    if (j.contains("cachedWriteTokens") && j["cachedWriteTokens"].is_number())
        v.cached_write_tokens = j["cachedWriteTokens"].get<int>();
}

}  // namespace turbot::core::acp
