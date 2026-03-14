#pragma once

#include <turbot/core/mcp/mcp.hpp>
#include <turbot/network/http_client.hpp>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace turbot::core::mcp {

/// SSETransport：通过 SSE 长连接接收服务器推送，通过独立 POST 发送请求
/// 对齐 OpenCode SSEClientTransport
class TURBOT_CORE_API SSETransport : public ITransport {
public:
    /// @param url      SSE 服务端 URL（GET 建立长连接）
    /// @param headers  自定义请求头（Authorization 等）
    explicit SSETransport(
        std::string url,
        std::unordered_map<std::string, std::string> headers = {}
    );
    ~SSETransport() override;

    SSETransport(const SSETransport&) = delete;
    SSETransport& operator=(const SSETransport&) = delete;

    /// 建立 SSE 长连接（GET {url} Accept: text/event-stream），启动 SSE 读循环
    std::future<void> connect() override;

    /// 通过 POST {url} 发送 JSON-RPC 请求，等待 SSE 推送响应
    std::future<JsonRpcResponse> send_request(
        const std::string& method,
        const nlohmann::json& params = nlohmann::json::object()
    ) override;

    /// 发送通知（POST，无 id）
    void send_notification(
        const std::string& method,
        const nlohmann::json& params = nlohmann::json::object()
    ) override;

    void on_notification(
        const std::string& method,
        std::function<void(const nlohmann::json&)> handler
    ) override;

    /// 关闭 SSE 连接，清理所有 pending futures
    std::future<void> close() override;

private:
    void sse_read_loop();  // 解析 SSE data: 行，分发到 pending futures / notification handlers
    void dispatch_message(const nlohmann::json& msg);
    void post_message(const nlohmann::json& msg);  // HTTP POST

    std::string url_;
    std::unordered_map<std::string, std::string> headers_;
    turbot::network::HttpClient http_client_;

    std::atomic<int64_t> next_id_{1};

    struct PendingRequest {
        std::promise<JsonRpcResponse> promise;
    };
    std::mutex pending_mutex_;
    std::unordered_map<int64_t, PendingRequest> pending_;

    std::mutex notification_mutex_;
    std::unordered_map<std::string, std::function<void(const nlohmann::json&)>> notification_handlers_;

    std::thread sse_thread_;
    std::atomic<bool> closed_{false};
    std::atomic<bool> connected_{false};
};

}  // namespace turbot::core::mcp
