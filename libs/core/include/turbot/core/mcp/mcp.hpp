#pragma once

#include <turbot/core/common/export.hpp>
#include <nlohmann/json.hpp>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace turbot::core::mcp {

/// MCP 连接状态（对齐 OpenCode MCP.Status discriminatedUnion）
enum class TURBOT_CORE_API MCPStatus {
    Connected,
    Disabled,
    Failed,
    NeedsAuth,
    NeedsClientRegistration,
};

/// Convert MCPStatus to string
[[nodiscard]] TURBOT_CORE_API std::string mcp_status_to_string(MCPStatus status);

/// MCP 工具定义（对齐 MCP 协议 tools/list 响应）
struct TURBOT_CORE_API MCPTool {
    std::string name;
    std::string description;
    nlohmann::json input_schema;  // JSON Schema object，type 固定为 "object"

    [[nodiscard]] nlohmann::json to_json() const;
    static MCPTool from_json(const nlohmann::json& j);
};

/// MCP 资源（对齐 OpenCode MCP.Resource zod schema）
struct TURBOT_CORE_API MCPResource {
    std::string name;
    std::string uri;
    std::optional<std::string> description;
    std::optional<std::string> mime_type;
    std::string client;  // 所属 server 名称

    [[nodiscard]] nlohmann::json to_json() const;
    static MCPResource from_json(const nlohmann::json& j);
};

/// MCP 提示词模板
struct TURBOT_CORE_API MCPPrompt {
    std::string name;
    std::string description;
    std::vector<std::string> arguments;

    [[nodiscard]] nlohmann::json to_json() const;
    static MCPPrompt from_json(const nlohmann::json& j);
};

/// MCP 客户端配置（对齐 OpenCode Config.Mcp type）
struct TURBOT_CORE_API MCPClientConfig {
    std::string name;
    std::string type;  // "local" | "remote"

    // type=local
    std::vector<std::string> command;  // [cmd, ...args]
    std::unordered_map<std::string, std::string> environment;

    // type=remote
    std::optional<std::string> url;
    std::optional<nlohmann::json> oauth;   // false(json bool) | oauth-config-object
    std::optional<std::unordered_map<std::string, std::string>> headers;

    bool enabled = true;
    int timeout_ms = 30000;  // 对齐 OpenCode DEFAULT_TIMEOUT = 30_000
};

/// Sanitize a string for use as MCP tool name component
/// replace(/[^a-zA-Z0-9_-]/g, "_") — 对齐 OpenCode sanitizedClientName 规则
[[nodiscard]] TURBOT_CORE_API std::string sanitize_mcp_name(const std::string& name);

/// JSON-RPC 2.0 基础消息
struct TURBOT_CORE_API JsonRpcRequest {
    std::string jsonrpc = "2.0";
    std::optional<int64_t> id;   // nullopt => notification
    std::string method;
    nlohmann::json params = nlohmann::json::object();

    [[nodiscard]] nlohmann::json to_json() const;
};

struct TURBOT_CORE_API JsonRpcResponse {
    std::string jsonrpc = "2.0";
    int64_t id = 0;
    std::optional<nlohmann::json> result;
    std::optional<nlohmann::json> error;

    [[nodiscard]] bool is_error() const noexcept { return error.has_value(); }
    static JsonRpcResponse from_json(const nlohmann::json& j);
    static JsonRpcResponse make_error(int64_t id, int code, const std::string& message);
};

/// 传输层抽象接口
class TURBOT_CORE_API ITransport {
public:
    virtual ~ITransport() = default;

    /// 连接（启动子进程或建立 HTTP 连接）
    virtual std::future<void> connect() = 0;

    /// 发送请求并等待响应
    virtual std::future<JsonRpcResponse> send_request(
        const std::string& method,
        const nlohmann::json& params = nlohmann::json::object()
    ) = 0;

    /// 发送通知（无响应，no-id request）
    virtual void send_notification(
        const std::string& method,
        const nlohmann::json& params = nlohmann::json::object()
    ) = 0;

    /// 注册服务器通知处理器（server-to-client）
    virtual void on_notification(
        const std::string& method,
        std::function<void(const nlohmann::json&)> handler
    ) = 0;

    /// 断开连接
    virtual std::future<void> close() = 0;

    /// 子进程 PID（仅 StdioTransport 返回有效值，其余返回 -1）
    virtual int pid() const noexcept { return -1; }
};

}  // namespace turbot::core::mcp
