#pragma once

#include <turbot/core/mcp/mcp.hpp>
#include <turbot/core/mcp/client.hpp>
#include <future>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace turbot::core::mcp {

/// MCPManager 单例：管理多 MCP 服务器连接生命周期，集成 ToolRegistry
/// 对齐 OpenCode MCP namespace（add/connect/disconnect/tools/prompts/resources/auth）
class TURBOT_CORE_API MCPManager {
public:
    /// 获取单例实例
    static MCPManager& instance();

    MCPManager(const MCPManager&) = delete;
    MCPManager& operator=(const MCPManager&) = delete;

    /// 初始化：批量添加配置并连接（对齐 OpenCode Instance.state 初始化）
    void initialize(const std::vector<MCPClientConfig>& configs);

    /// 添加并连接一个 MCP 服务器
    /// 如果同名已存在，先关闭旧连接再重建（对齐 OpenCode add() close existing）
    std::future<MCPStatus> add(const std::string& name, const MCPClientConfig& config);

    /// 重新连接（从存储的 config 重连，对齐 OpenCode connect(name)）
    std::future<void> connect(const std::string& name);

    /// 断开连接（删除 client 对象，保留 config，设置 Disabled，对齐 OpenCode disconnect()）
    std::future<void> disconnect(const std::string& name);

    /// 完全移除（删除 client + config）
    void remove(const std::string& name);

    /// 获取所有服务器状态快照
    [[nodiscard]] std::unordered_map<std::string, MCPStatus> status() const;

    /// 获取所有已连接服务器的工具列表（key = sanitized_client + "_" + sanitized_tool）
    /// 对齐 OpenCode MCP.tools() 命名规则
    std::future<std::unordered_map<std::string, nlohmann::json>> tools();

    /// 将 MCP 工具同步到 ToolRegistry，发布 EventBus mcp.tools.changed 事件
    /// 对齐 OpenCode convertMcpTool（additionalProperties=false, type="object"）
    std::future<void> sync_tools_to_registry(const std::string& server_name = "");

    /// 获取所有资源（key = sanitized_client + ":" + sanitized_resource）
    std::future<std::vector<MCPResource>> all_resources();

    /// 获取所有提示词模板（key = sanitized_client + ":" + sanitized_prompt）
    std::future<std::unordered_map<std::string, MCPPrompt>> all_prompts();

    /// 直接调用指定服务器的工具（供 MCPToolWrapper 内部使用）
    std::future<nlohmann::json> call_tool(
        const std::string& server_name,
        const std::string& tool_name,
        const nlohmann::json& args,
        int timeout_ms = 30000
    );

    // ─── Auth ─────────────────────────────────────────────────────────────────

    /// 开始 OAuth 认证流程，返回授权 URL
    std::future<std::string> start_auth(const std::string& name);

    /// 完整 OAuth 认证流程（打开浏览器，等待 callback）
    std::future<MCPStatus> authenticate(const std::string& name);

    /// 完成 OAuth 认证（传入 code）
    std::future<MCPStatus> finish_auth(const std::string& name, const std::string& code);

    /// 删除已存储的 OAuth 凭据
    std::future<void> remove_auth(const std::string& name);

    /// 检查服务器是否支持 OAuth（remote 且 oauth != false）
    [[nodiscard]] bool supports_oauth(const std::string& name) const;

    /// 关闭所有连接（清理子进程及其后代，对齐 OpenCode Instance.state cleanup）
    void shutdown();

private:
    MCPManager() = default;
    ~MCPManager() = default;

    /// 递归获取后代进程 PID（pgrep -P 语义，对齐 OpenCode descendants()）
    static std::vector<int> descendants(int pid);

    /// 内部创建并连接 MCPClient（不持锁）
    MCPStatus create_and_connect(const std::string& name, const MCPClientConfig& config);

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<MCPClient>> clients_;
    std::unordered_map<std::string, MCPStatus> status_map_;
    std::unordered_map<std::string, MCPClientConfig> configs_;
};

}  // namespace turbot::core::mcp

// ─── Config Bridge (App startup integration) ─────────────────────────────────
// Defined in src/mcp/config_bridge.cpp — no separate header required

namespace turbot::core::mcp {

/// Load MCP server configs from ConfigManager "mcp" key
[[nodiscard]] TURBOT_CORE_API std::vector<MCPClientConfig> load_mcp_configs();

/// Initialize MCPManager from ConfigManager (call once at App startup)
TURBOT_CORE_API void initialize_mcp_from_config();

/// Register config change watch for MCP hot-reload
TURBOT_CORE_API void register_mcp_config_watch();

}  // namespace turbot::core::mcp
