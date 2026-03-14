#pragma once

#include <turbot/core/mcp/mcp.hpp>
#include <turbot/core/mcp/sse_transport.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace turbot::core::mcp {

/// HttpTransport：Streamable HTTP 优先，fallback 到 SSE
/// 对齐 OpenCode：先尝试 StreamableHTTPClientTransport，若收到 text/event-stream 响应则降级到 SSEClientTransport
///
/// 降级触发条件：
///   1. connect() 时收到 Content-Type: text/event-stream 响应 → 切换 SSE 模式
///   2. connect() 超时（30s）→ 降级到 SSE
///   3. 收到非 200 且非 text/event-stream 的响应（如 404/401）→ 报错，不降级
///   4. 收到 5xx 响应 → 报错，不降级
class TURBOT_CORE_API HttpTransport : public ITransport {
public:
    /// @param url      服务端 URL
    /// @param headers  自定义请求头
    explicit HttpTransport(
        std::string url,
        std::unordered_map<std::string, std::string> headers = {}
    );
    ~HttpTransport() override;

    HttpTransport(const HttpTransport&) = delete;
    HttpTransport& operator=(const HttpTransport&) = delete;

    /// 尝试 StreamableHTTP 连接，按条件决定是否降级到 SSE
    std::future<void> connect() override;

    /// 根据已确定的协议模式路由请求
    std::future<JsonRpcResponse> send_request(
        const std::string& method,
        const nlohmann::json& params = nlohmann::json::object()
    ) override;

    void send_notification(
        const std::string& method,
        const nlohmann::json& params = nlohmann::json::object()
    ) override;

    void on_notification(
        const std::string& method,
        std::function<void(const nlohmann::json&)> handler
    ) override;

    std::future<void> close() override;

    enum class Mode { StreamableHTTP, SSE };
    [[nodiscard]] Mode mode() const noexcept { return mode_; }

private:
    std::future<void> connect_streamable_http();
    void setup_sse_fallback();

    std::string url_;
    std::unordered_map<std::string, std::string> headers_;

    std::atomic<Mode> mode_{Mode::StreamableHTTP};
    std::atomic<bool> mode_determined_{false};  // 一旦确定协议类型后固定，不再重试

    // SSE fallback delegate（降级后使用）
    std::unique_ptr<SSETransport> sse_delegate_;

    // StreamableHTTP pending requests
    std::atomic<int64_t> next_id_{1};
    struct PendingRequest {
        std::promise<JsonRpcResponse> promise;
    };
    std::mutex pending_mutex_;
    std::unordered_map<int64_t, PendingRequest> pending_;

    std::mutex notification_mutex_;
    std::unordered_map<std::string, std::function<void(const nlohmann::json&)>> notification_handlers_;

    std::atomic<bool> closed_{false};
};

}  // namespace turbot::core::mcp
