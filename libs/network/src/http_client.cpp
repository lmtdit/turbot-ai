#include <turbot/network/http_client.hpp>
#include <turbot/core/logger.hpp>

namespace turbot::network {

class HttpClient::Impl {
public:
    int timeout_seconds_ = 30;
};

HttpClient::HttpClient() : impl_(std::make_unique<Impl>()) {
    TURBOT_LOG_INFO("HttpClient initialized");
}

HttpClient::~HttpClient() = default;

HttpClient::HttpClient(HttpClient&&) noexcept = default;
HttpClient& HttpClient::operator=(HttpClient&&) noexcept = default;

HttpResponse HttpClient::get(std::string_view url) {
    TURBOT_LOG_DEBUG("GET request to: {}", url);

    // Placeholder implementation
    HttpResponse response;
    response.status_code = 200;
    response.body = R"({"status": "ok"})";
    response.content_type = "application/json";

    return response;
}

HttpResponse HttpClient::post(std::string_view url, std::string_view body) {
    TURBOT_LOG_DEBUG("POST request to: {}, body size: {}", url, body.size());

    // Placeholder implementation
    HttpResponse response;
    response.status_code = 201;
    response.body = R"({"id": 123, "status": "created"})";
    response.content_type = "application/json";

    return response;
}

std::future<HttpResponse> HttpClient::get_async(std::string_view url) {
    return std::async(std::launch::async, [this, url_str = std::string(url)]() {
        return this->get(url_str);
    });
}

std::future<HttpResponse> HttpClient::post_async(std::string_view url, std::string_view body) {
    return std::async(std::launch::async, [this, url_str = std::string(url), body_str = std::string(body)]() {
        return this->post(url_str, body_str);
    });
}

void HttpClient::set_timeout(int seconds) {
    impl_->timeout_seconds_ = seconds;
    TURBOT_LOG_DEBUG("HTTP timeout set to {} seconds", seconds);
}

} // namespace turbot::network
