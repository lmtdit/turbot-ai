#include <turbot/core/mcp/http_transport.hpp>
#include <turbot/core/mcp/transport.hpp>
#include <turbot/core/common/logger.hpp>

#include <stdexcept>
#include <sstream>

namespace turbot::core::mcp {

HttpTransport::HttpTransport(
    std::string url,
    std::unordered_map<std::string, std::string> headers
) : url_(std::move(url)), headers_(std::move(headers)) {
    if (!url_.starts_with("http://") && !url_.starts_with("https://")) {
        throw std::invalid_argument("HttpTransport: url must start with http:// or https://");
    }
}

HttpTransport::~HttpTransport() {
    if (!closed_.load()) {
        try {
            close().get();
        } catch (...) {}
    }
}

std::future<void> HttpTransport::connect() {
    return std::async(std::launch::async, [this]() {
        // Attempt Streamable HTTP first (POST /mcp/v1 with Content-Type: application/json)
        // If server responds with Content-Type: text/event-stream → fallback to SSE
        // If timeout (30s) → fallback to SSE
        try {
            connect_streamable_http().get();
        } catch (const std::exception& e) {
            TURBOT_LOG_WARN("HttpTransport: StreamableHTTP failed ({}), falling back to SSE", e.what());
            setup_sse_fallback();
            sse_delegate_->connect().get();
        }
    });
}

std::future<void> HttpTransport::connect_streamable_http() {
    return std::async(std::launch::async, [this]() {
        turbot::network::HttpClient probe_client;

        // Probe the endpoint: send a POST with an initialization request
        auto probe_req = turbot::network::HttpRequest::post(url_, "");
        probe_req.with_header("Content-Type", "application/json")
                 .with_header("Accept", "application/json, text/event-stream")
                 .with_timeout(30);
        for (const auto& [k, v] : headers_) {
            probe_req.with_header(k, v);
        }
        // Send a JSON-RPC initialize probe (no body yet, just check connectivity)
        JsonRpcRequest init_req;
        init_req.id = 0;
        init_req.method = "initialize";
        init_req.params = {
            {"protocolVersion", "2024-11-05"},
            {"capabilities", nlohmann::json::object()},
            {"clientInfo", {{"name", "turbot"}, {"version", "1.0"}}}
        };
        probe_req.with_body(init_req.to_json().dump());

        auto resp = probe_client.request(probe_req);

        if (resp.is_server_error()) {
            throw std::runtime_error(
                "HttpTransport: server error " + std::to_string(resp.status_code));
        }
        if (resp.status_code == 404 || resp.status_code == 401 || resp.status_code == 403) {
            throw std::runtime_error(
                "HttpTransport: endpoint error " + std::to_string(resp.status_code)
                + " - not falling back");
        }

        // Check if server returned SSE content-type → force SSE mode
        bool is_sse = resp.content_type.find("text/event-stream") != std::string::npos;
        if (is_sse) {
            TURBOT_LOG_INFO("HttpTransport: server returned text/event-stream, switching to SSE mode");
            mode_.store(Mode::SSE);
            mode_determined_.store(true);
            setup_sse_fallback();
            sse_delegate_->connect().get();
            return;
        }

        // Success: Streamable HTTP mode confirmed
        mode_.store(Mode::StreamableHTTP);
        mode_determined_.store(true);
        TURBOT_LOG_INFO("HttpTransport: StreamableHTTP mode confirmed for {}", url_);
    });
}

void HttpTransport::setup_sse_fallback() {
    // Forward custom headers to SSE delegate
    sse_delegate_ = std::make_unique<SSETransport>(url_, headers_);

    // Forward notification handlers
    std::lock_guard<std::mutex> lock(notification_mutex_);
    for (auto& [method, handler] : notification_handlers_) {
        sse_delegate_->on_notification(method, handler);
    }
}

std::future<JsonRpcResponse> HttpTransport::send_request(
    const std::string& method,
    const nlohmann::json& params
) {
    // If SSE mode, delegate
    if (mode_.load() == Mode::SSE && sse_delegate_) {
        return sse_delegate_->send_request(method, params);
    }

    // StreamableHTTP: POST request, parse response body directly
    return std::async(std::launch::async, [this, method, params]() -> JsonRpcResponse {
        int64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);

        JsonRpcRequest rpc_req;
        rpc_req.id = id;
        rpc_req.method = method;
        rpc_req.params = params;

        turbot::network::HttpClient http;
        auto headers = to_http_headers(headers_);
        headers.emplace_back("Content-Type", "application/json");
        headers.emplace_back("Accept", "application/json");

        auto resp = http.post(url_, rpc_req.to_json().dump(), headers);

        if (!resp.is_success()) {
            return JsonRpcResponse::make_error(
                id, -32000,
                "HttpTransport: request failed with status " + std::to_string(resp.status_code));
        }

        try {
            auto j = nlohmann::json::parse(resp.body);
            return JsonRpcResponse::from_json(j);
        } catch (const nlohmann::json::parse_error& e) {
            return JsonRpcResponse::make_error(id, -32700,
                std::string("HttpTransport: JSON parse error: ") + e.what());
        }
    });
}

void HttpTransport::send_notification(const std::string& method, const nlohmann::json& params) {
    if (mode_.load() == Mode::SSE && sse_delegate_) {
        sse_delegate_->send_notification(method, params);
        return;
    }

    // StreamableHTTP: fire-and-forget POST
    JsonRpcRequest rpc_req;
    // notification has no id
    rpc_req.method = method;
    rpc_req.params = params;

    try {
        turbot::network::HttpClient http;
        auto headers = to_http_headers(headers_);
        headers.emplace_back("Content-Type", "application/json");
        [[maybe_unused]] auto post_result = http.post(url_, rpc_req.to_json().dump(), headers);
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("HttpTransport: send_notification failed: {}", e.what());
    }
}

void HttpTransport::on_notification(
    const std::string& method,
    std::function<void(const nlohmann::json&)> handler
) {
    {
        std::lock_guard<std::mutex> lock(notification_mutex_);
        notification_handlers_[method] = handler;
    }
    // If SSE delegate already exists, forward immediately
    if (sse_delegate_) {
        sse_delegate_->on_notification(method, std::move(handler));
    }
}

std::future<void> HttpTransport::close() {
    return std::async(std::launch::async, [this]() {
        if (closed_.exchange(true)) return;

        if (sse_delegate_) {
            try {
                sse_delegate_->close().get();
            } catch (...) {}
        }

        // Fail all pending StreamableHTTP requests
        std::lock_guard<std::mutex> lock(pending_mutex_);
        for (auto& [id, req] : pending_) {
            try {
                req.promise.set_value(
                    JsonRpcResponse::make_error(id, -32000, "Transport closed"));
            } catch (...) {}
        }
        pending_.clear();
    });
}

}  // namespace turbot::core::mcp
