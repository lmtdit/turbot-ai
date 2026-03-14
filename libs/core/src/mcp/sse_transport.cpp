#include <turbot/core/mcp/sse_transport.hpp>
#include <turbot/core/mcp/transport.hpp>
#include <turbot/core/common/logger.hpp>

#include <stdexcept>
#include <sstream>

namespace turbot::core::mcp {

SSETransport::SSETransport(
    std::string url,
    std::unordered_map<std::string, std::string> headers
) : url_(std::move(url)), headers_(std::move(headers)), http_client_() {}

SSETransport::~SSETransport() {
    if (!closed_.load()) {
        try { close().get(); } catch (...) {}
    }
    if (sse_thread_.joinable()) {
        sse_thread_.join();
    }
}

std::future<void> SSETransport::connect() {
    return std::async(std::launch::async, [this]() {
        connected_.store(false);

        // Start SSE read loop in background thread
        sse_thread_ = std::thread([this]() { sse_read_loop(); });

        // Wait briefly for SSE connection to establish
        for (int i = 0; i < 50 && !connected_.load() && !closed_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!connected_.load() && !closed_.load()) {
            throw std::runtime_error("SSETransport: failed to connect to " + url_);
        }
    });
}

void SSETransport::sse_read_loop() {
    try {
        // Build SSE request
        auto sse_req = turbot::network::HttpRequest::get(url_);
        sse_req.with_header("Accept", "text/event-stream")
               .with_header("Cache-Control", "no-cache")
               .with_timeout(0);  // no timeout for long-lived SSE
        // Append custom headers
        for (const auto& [k, v] : headers_) {
            sse_req.with_header(k, v);
        }

        // SSE accumulates incomplete lines across chunks
        std::string line_buf;

        [[maybe_unused]] auto stream_result = http_client_.request_stream(sse_req, [this, &line_buf](std::string_view chunk) -> bool {
            // Accumulate chunk into line buffer and process complete lines
            line_buf.append(chunk.data(), chunk.size());

            std::string::size_type pos = 0;
            while (true) {
                auto nl = line_buf.find('\n', pos);
                if (nl == std::string::npos) break;
                std::string line = line_buf.substr(pos, nl - pos);
                // Strip trailing \r
                if (!line.empty() && line.back() == '\r') line.pop_back();
                pos = nl + 1;

                // SSE line parsing: "data: {...}"
                if (line.starts_with("data: ")) {
                    connected_.store(true);
                    std::string json_str = line.substr(6);
                    if (json_str.empty() || json_str == "[DONE]") continue;
                    try {
                        auto msg = nlohmann::json::parse(json_str);
                        dispatch_message(msg);
                    } catch (const nlohmann::json::parse_error& e) {
                        TURBOT_LOG_ERROR("SSETransport: JSON parse error: {}", e.what());
                    }
                } else if (line.starts_with(":")) {
                    // SSE keepalive comment
                    connected_.store(true);
                }
                // empty line = SSE event separator, ignore
            }
            // Keep remainder in buffer
            line_buf.erase(0, pos);
            return !closed_.load();  // false = abort stream
        });
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SSETransport: SSE read loop error: {}", e.what());
    }

    // Connection ended - fail all pending requests
    std::lock_guard<std::mutex> lock(pending_mutex_);
    for (auto& [id, req] : pending_) {
        try {
            req.promise.set_value(JsonRpcResponse::make_error(id, -32000, "SSE connection closed"));
        } catch (...) {}
    }
    pending_.clear();
}

void SSETransport::dispatch_message(const nlohmann::json& msg) {
    if (msg.contains("id") && !msg["id"].is_null()) {
        // Response: dispatch to pending promise
        int64_t id = msg["id"].get<int64_t>();
        std::lock_guard<std::mutex> lock(pending_mutex_);
        auto it = pending_.find(id);
        if (it != pending_.end()) {
            it->second.promise.set_value(JsonRpcResponse::from_json(msg));
            pending_.erase(it);
        }
    } else if (msg.contains("method")) {
        // Notification: dispatch to handler
        std::string method = msg["method"].get<std::string>();
        std::function<void(const nlohmann::json&)> handler;
        {
            std::lock_guard<std::mutex> lock(notification_mutex_);
            auto it = notification_handlers_.find(method);
            if (it != notification_handlers_.end()) {
                handler = it->second;
            }
        }
        if (handler) {
            try {
                handler(msg.value("params", nlohmann::json::object()));
            } catch (const std::exception& e) {
                TURBOT_LOG_ERROR("SSETransport: notification handler threw: {}", e.what());
            }
        }
    }
}

void SSETransport::post_message(const nlohmann::json& msg) {
    auto headers = to_http_headers(headers_);
    headers.emplace_back("Content-Type", "application/json");
    auto resp = http_client_.post(url_, msg.dump(), headers);
    if (!resp.is_success()) {
        throw std::runtime_error(
            "SSETransport: POST failed with status " + std::to_string(resp.status_code));
    }
}

std::future<JsonRpcResponse> SSETransport::send_request(
    const std::string& method,
    const nlohmann::json& params
) {
    int64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);

    std::promise<JsonRpcResponse> promise;
    auto future = promise.get_future();

    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_[id] = PendingRequest{std::move(promise)};
    }

    JsonRpcRequest req;
    req.id = id;
    req.method = method;
    req.params = params;

    try {
        post_message(req.to_json());
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        auto it = pending_.find(id);
        if (it != pending_.end()) {
            it->second.promise.set_value(
                JsonRpcResponse::make_error(id, -32000, std::string("POST failed: ") + e.what())
            );
            pending_.erase(it);
        }
    }

    return future;
}

void SSETransport::send_notification(const std::string& method, const nlohmann::json& params) {
    JsonRpcRequest req;
    req.method = method;
    req.params = params;
    try {
        post_message(req.to_json());
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("SSETransport: send_notification failed: {}", e.what());
    }
}

void SSETransport::on_notification(
    const std::string& method,
    std::function<void(const nlohmann::json&)> handler
) {
    std::lock_guard<std::mutex> lock(notification_mutex_);
    notification_handlers_[method] = std::move(handler);
}

std::future<void> SSETransport::close() {
    return std::async(std::launch::async, [this]() {
        if (closed_.exchange(true)) return;
        connected_.store(false);

        // Fail all pending requests
        std::lock_guard<std::mutex> lock(pending_mutex_);
        for (auto& [id, req] : pending_) {
            try {
                req.promise.set_value(JsonRpcResponse::make_error(id, -32000, "Transport closed"));
            } catch (...) {}
        }
        pending_.clear();
    });
}

}  // namespace turbot::core::mcp
