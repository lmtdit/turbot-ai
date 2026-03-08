#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <future>
#include <functional>
#include <memory>
#include <turbot/network/url.hpp>
#include <turbot/network/export.hpp>

namespace turbot::network {

/// HTTP request methods
enum class HttpMethod {
    GET,
    POST,
    PUT,
    PATCH,
    DELETE_,
    HEAD,
    OPTIONS
};

/// Convert HttpMethod to string
std::string_view method_to_string(HttpMethod method) noexcept;

/// HTTP header as key-value pair
using HttpHeader = std::pair<std::string, std::string>;
using HttpHeaders = std::vector<HttpHeader>;

/// HTTP request configuration
struct TURBOT_NETWORK_API HttpRequest {
    HttpMethod method = HttpMethod::GET;
    std::string url;
    HttpHeaders headers;
    std::string body;
    int timeout_seconds = 0;  ///< 0 means use the HttpClient-level default (set via set_timeout())
    bool follow_redirects = true;
    int max_redirects = 5;
    
    // Convenience constructors
    static HttpRequest get(std::string_view url);
    static HttpRequest post(std::string_view url, std::string_view body = "");
    static HttpRequest put(std::string_view url, std::string_view body = "");
    static HttpRequest patch(std::string_view url, std::string_view body = "");
    static HttpRequest del(std::string_view url);
    
    // Builder pattern for headers
    HttpRequest& with_header(std::string_view name, std::string_view value);
    HttpRequest& with_headers(const HttpHeaders& headers);
    HttpRequest& with_timeout(int seconds);
    HttpRequest& with_body(std::string_view body);
    HttpRequest& with_json_body(std::string_view json);
};

/// HTTP response
struct TURBOT_NETWORK_API HttpResponse {
    int status_code = 0;
    std::string status_message;
    std::string body;
    HttpHeaders headers;
    std::string content_type;
    int64_t response_time_ms = 0;  ///< Response time in milliseconds (int64_t avoids 32-bit truncation on Windows)
    
    /// Check if response indicates success (2xx status code)
    [[nodiscard]] bool is_success() const noexcept {
        return status_code >= 200 && status_code < 300;
    }
    
    /// Check if response is a client error (4xx)
    [[nodiscard]] bool is_client_error() const noexcept {
        return status_code >= 400 && status_code < 500;
    }
    
    /// Check if response is a server error (5xx)
    [[nodiscard]] bool is_server_error() const noexcept {
        return status_code >= 500 && status_code < 600;
    }
    
    /// Get a specific header value by name (case-insensitive)
    [[nodiscard]] std::optional<std::string> get_header(std::string_view name) const;
};

/// Stream chunk callback type
using StreamCallback = std::function<bool(std::string_view chunk)>;

/// HTTP client for making HTTP requests
class TURBOT_NETWORK_API HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Delete copy, allow move
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) noexcept;
    HttpClient& operator=(HttpClient&&) noexcept;

    // === High-level API ===
    
    /// Perform a GET request
    [[nodiscard]] HttpResponse get(std::string_view url, const HttpHeaders& headers = {});
    
    /// Perform a POST request with JSON body
    [[nodiscard]] HttpResponse post(std::string_view url, std::string_view body, 
                                    const HttpHeaders& headers = {});
    
    /// Perform a POST request with JSON body (nlohmann::json style)
    [[nodiscard]] HttpResponse post_json(std::string_view url, std::string_view json_body,
                                         const HttpHeaders& headers = {});
    
    /// Perform a PUT request
    [[nodiscard]] HttpResponse put(std::string_view url, std::string_view body,
                                   const HttpHeaders& headers = {});
    
    /// Perform a PATCH request
    [[nodiscard]] HttpResponse patch(std::string_view url, std::string_view body,
                                     const HttpHeaders& headers = {});
    
    /// Perform a DELETE request
    [[nodiscard]] HttpResponse del(std::string_view url, const HttpHeaders& headers = {});

    // === Low-level API ===
    
    /// Perform an HTTP request with full control
    [[nodiscard]] HttpResponse request(const HttpRequest& req);
    
    /// Perform a streaming request (for SSE or large responses)
    /// The callback receives each chunk and returns true to continue, false to abort
    [[nodiscard]] HttpResponse request_stream(const HttpRequest& req, StreamCallback callback);

    // === Async API ===
    
    /// Async GET request.
    /// Lifetime-safe: the internal Impl object is reference-counted (shared_ptr)
    /// and stays alive until the returned future resolves, even if this HttpClient
    /// instance is destroyed first.
    [[nodiscard]] std::future<HttpResponse> get_async(std::string_view url, 
                                                      const HttpHeaders& headers = {});
    
    /// Async POST request.
    /// Lifetime-safe: the internal Impl object is reference-counted (shared_ptr)
    /// and stays alive until the returned future resolves, even if this HttpClient
    /// instance is destroyed first.
    [[nodiscard]] std::future<HttpResponse> post_async(std::string_view url, 
                                                       std::string_view body,
                                                       const HttpHeaders& headers = {});

    // === Configuration ===
    
    /// Set default timeout for all requests
    void set_timeout(int seconds);
    
    /// Set default headers for all requests
    void set_default_header(std::string_view name, std::string_view value);
    
    /// Set proxy URL (e.g., "http://proxy:8080")
    void set_proxy(std::string_view proxy);
    
    /// Enable/disable SSL verification (use with caution)
    void set_ssl_verify(bool verify);
    
    /// Set user agent string
    void set_user_agent(std::string_view user_agent);

private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace turbot::network
