#pragma once

#include <turbot/core/mcp/mcp.hpp>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace turbot::core::mcp {

/// StdioTransport：fork 子进程，通过 stdin/stdout pipe 进行 newline-delimited JSON 通信
/// 对齐 OpenCode StdioClientTransport
class TURBOT_CORE_API StdioTransport : public ITransport {
public:
    /// @param command  子进程命令（[cmd, arg1, arg2...]）
    /// @param env      附加环境变量（合并到当前进程环境）
    explicit StdioTransport(
        std::vector<std::string> command,
        std::unordered_map<std::string, std::string> env = {}
    );
    ~StdioTransport() override;

    StdioTransport(const StdioTransport&) = delete;
    StdioTransport& operator=(const StdioTransport&) = delete;

    /// fork + execvp 启动子进程，建立 stdin/stdout pipe，启动 io 读线程
    std::future<void> connect() override;

    /// 发送请求（持有 write_mutex_ 后写入 stdin pipe）
    /// 超时返回 error JsonRpcResponse（不抛异常）
    std::future<JsonRpcResponse> send_request(
        const std::string& method,
        const nlohmann::json& params = nlohmann::json::object()
    ) override;

    /// 发送通知（无 id，持有 write_mutex_）
    void send_notification(
        const std::string& method,
        const nlohmann::json& params = nlohmann::json::object()
    ) override;

    /// 注册服务器通知处理器
    void on_notification(
        const std::string& method,
        std::function<void(const nlohmann::json&)> handler
    ) override;

    /// 关闭 stdin fd，发 SIGTERM，等待子进程退出，清理所有 pending futures
    std::future<void> close() override;

    /// 返回子进程 PID（用于孙进程清理）
    int pid() const noexcept override;

private:
    void io_read_loop();  // 独立线程：持续从 stdout 读取，分发消息
    void cleanup_pending(const std::string& error_message);  // 以 error 完成所有 pending futures
    void write_line(const std::string& line);  // 持有 write_mutex_ 后写入 stdin pipe

    std::vector<std::string> command_;
    std::unordered_map<std::string, std::string> env_;

    int child_pid_ = -1;
    int stdin_fd_ = -1;
    int stdout_fd_ = -1;
    int stderr_fd_ = -1;

    std::atomic<int64_t> next_id_{1};
    std::mutex write_mutex_;  // 保护 stdin pipe 写入，防止多线程消息帧交错

    struct PendingRequest {
        std::promise<JsonRpcResponse> promise;
    };
    std::mutex pending_mutex_;
    std::unordered_map<int64_t, PendingRequest> pending_;

    std::mutex notification_mutex_;
    std::unordered_map<std::string, std::function<void(const nlohmann::json&)>> notification_handlers_;

    std::thread read_thread_;
    std::thread stderr_thread_;  // Joined on close to avoid use-after-close on stderr_fd_
    std::atomic<bool> closed_{false};
};

}  // namespace turbot::core::mcp
