#include <turbot/core/mcp/mcp.hpp>
#include <turbot/core/common/logger.hpp>
#include <algorithm>
#include <stdexcept>

namespace turbot::core::mcp {

// ─── MCPStatus ────────────────────────────────────────────────────────────────

std::string mcp_status_to_string(MCPStatus status) {
    switch (status) {
        case MCPStatus::Connected:               return "connected";
        case MCPStatus::Disabled:                return "disabled";
        case MCPStatus::Failed:                  return "failed";
        case MCPStatus::NeedsAuth:               return "needs_auth";
        case MCPStatus::NeedsClientRegistration: return "needs_client_registration";
    }
    return "unknown";
}

// ─── sanitize_mcp_name ────────────────────────────────────────────────────────

std::string sanitize_mcp_name(const std::string& name) {
    std::string result = name;
    for (char& c : result) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            c = '_';
        }
    }
    return result;
}

// ─── MCPTool ──────────────────────────────────────────────────────────────────

nlohmann::json MCPTool::to_json() const {
    nlohmann::json j;
    j["name"] = name;
    j["description"] = description;
    j["inputSchema"] = input_schema;
    return j;
}

MCPTool MCPTool::from_json(const nlohmann::json& j) {
    MCPTool tool;
    tool.name = j.value("name", "");
    tool.description = j.value("description", "");
    if (j.contains("inputSchema")) {
        tool.input_schema = j["inputSchema"];
    } else {
        tool.input_schema = {{"type", "object"}, {"properties", nlohmann::json::object()}};
    }
    // 强制 type = "object"（对齐 OpenCode convertMcpTool）
    if (!tool.input_schema.is_object() || !tool.input_schema.contains("type")) {
        tool.input_schema["type"] = "object";
    }
    return tool;
}

// ─── MCPResource ──────────────────────────────────────────────────────────────

nlohmann::json MCPResource::to_json() const {
    nlohmann::json j;
    j["name"] = name;
    j["uri"] = uri;
    if (description) j["description"] = *description;
    if (mime_type) j["mimeType"] = *mime_type;
    j["client"] = client;
    return j;
}

MCPResource MCPResource::from_json(const nlohmann::json& j) {
    MCPResource res;
    res.name = j.value("name", "");
    res.uri = j.value("uri", "");
    if (j.contains("description") && !j["description"].is_null())
        res.description = j["description"].get<std::string>();
    if (j.contains("mimeType") && !j["mimeType"].is_null())
        res.mime_type = j["mimeType"].get<std::string>();
    res.client = j.value("client", "");
    return res;
}

// ─── MCPPrompt ────────────────────────────────────────────────────────────────

nlohmann::json MCPPrompt::to_json() const {
    nlohmann::json j;
    j["name"] = name;
    j["description"] = description;
    j["arguments"] = arguments;
    return j;
}

MCPPrompt MCPPrompt::from_json(const nlohmann::json& j) {
    MCPPrompt prompt;
    prompt.name = j.value("name", "");
    prompt.description = j.value("description", "");
    if (j.contains("arguments") && j["arguments"].is_array()) {
        for (const auto& arg : j["arguments"]) {
            if (arg.is_string()) {
                prompt.arguments.push_back(arg.get<std::string>());
            } else if (arg.is_object() && arg.contains("name")) {
                prompt.arguments.push_back(arg["name"].get<std::string>());
            }
        }
    }
    return prompt;
}

// ─── JsonRpcRequest ───────────────────────────────────────────────────────────

nlohmann::json JsonRpcRequest::to_json() const {
    nlohmann::json j;
    j["jsonrpc"] = jsonrpc;
    if (id.has_value()) j["id"] = *id;
    j["method"] = method;
    j["params"] = params;
    return j;
}

// ─── JsonRpcResponse ─────────────────────────────────────────────────────────

JsonRpcResponse JsonRpcResponse::from_json(const nlohmann::json& j) {
    JsonRpcResponse resp;
    resp.jsonrpc = j.value("jsonrpc", "2.0");
    if (j.contains("id") && !j["id"].is_null()) {
        resp.id = j["id"].get<int64_t>();
    }
    if (j.contains("result")) {
        resp.result = j["result"];
    }
    if (j.contains("error")) {
        resp.error = j["error"];
    }
    return resp;
}

JsonRpcResponse JsonRpcResponse::make_error(int64_t id, int code, const std::string& message) {
    JsonRpcResponse resp;
    resp.id = id;
    resp.error = {{"code", code}, {"message", message}};
    return resp;
}

}  // namespace turbot::core::mcp
