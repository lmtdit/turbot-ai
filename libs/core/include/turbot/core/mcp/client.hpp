#pragma once

#include <turbot/core/mcp/mcp.hpp>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace turbot::core::mcp {

/// MCPClient：封装 ITransport，实现 MCP initialize 握手和工具/资源/提示词 API
/// 对齐 OpenCode MCP.create() + Client 生命周期
class TURBOT_CORE_API MCPClient {
public:
    explicit MCPClient(std::unique_ptr<ITransport> transport);
    ~MCPClient();

    MCPClient(const MCPClient&) = delete;
    MCPClient& operator=(const MCPClient&) = delete;

    /// 连接并执行 MCP initialize 握手
    /// 成功后 status() == MCPStatus::Connected
    std::future<MCPStatus> connect(
        const std::string& client_name = "turbot",
        const std::string& version = "1.0.0"
    );

    /// 断开连接
    std::future<void> close();

    /// 当前连接状态
    [[nodiscard]] MCPStatus status() const noexcept;

    /// 列出服务器提供的所有工具
    std::future<std::vector<MCPTool>> list_tools();

    /// 调用工具
    /// @param timeout_ms  硬超时（默认 30000ms，对齐 OpenCode DEFAULT_TIMEOUT）
    ///                    收到 progress 通知时重置（resetTimeoutOnProgress = true）
    std::future<nlohmann::json> call_tool(
        const std::string& name,
        const nlohmann::json& args,
        int timeout_ms = 30000
    );

    /// 列出所有资源
    std::future<std::vector<MCPResource>> list_resources();

    /// 读取资源内容
    std::future<nlohmann::json> read_resource(const std::string& uri);

    /// 列出所有提示词模板
    std::future<std::vector<MCPPrompt>> list_prompts();

    /// 获取提示词内容
    std::future<std::string> get_prompt(
        const std::string& name,
        const nlohmann::json& args = nlohmann::json::object()
    );

    /// 注册工具列表变更回调（对应 notifications/tools/list_changed）
    void on_tools_changed(std::function<void()> callback);

    /// 获取子进程 PID（仅 StdioTransport 有效，其余返回 -1）
    [[nodiscard]] int pid() const noexcept;

private:
    std::unique_ptr<ITransport> transport_;
    std::atomic<MCPStatus> status_{MCPStatus::Failed};
    std::function<void()> tools_changed_callback_;
    mutable std::mutex callback_mutex_;
};

}  // namespace turbot::core::mcp
