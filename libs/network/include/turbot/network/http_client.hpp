#pragma once

#include <string>
#include <string_view>
#include <future>
#include <turbot/network/url.hpp>
#include <turbot/network/export.hpp>

namespace turbot::network {

struct HttpResponse {
    int status_code = 0;
    std::string body;
    std::string content_type;
};

class TURBOT_NETWORK_API HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Delete copy, allow move
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) noexcept;
    HttpClient& operator=(HttpClient&&) noexcept;

    // Synchronous requests
    [[nodiscard]] HttpResponse get(std::string_view url);
    [[nodiscard]] HttpResponse post(std::string_view url, std::string_view body);

    // Asynchronous requests
    [[nodiscard]] std::future<HttpResponse> get_async(std::string_view url);
    [[nodiscard]] std::future<HttpResponse> post_async(std::string_view url, std::string_view body);

    void set_timeout(int seconds);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace turbot::network
