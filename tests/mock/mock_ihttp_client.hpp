#pragma once

/**
 * @file mock_ihttp_client.hpp
 * @brief Mock IHttpClient for testing
 */

#include <turbot/network/ihttp_client.hpp>
#include <turbot/network/http_client.hpp>
#include <nlohmann/json.hpp>
#include <queue>
#include <mutex>
#include <string>
#include <unordered_map>

namespace turbot::test {

/// Mock HTTP client for testing (implements IHttpClient)
class MockIHttpClient : public turbot::network::IHttpClient {
public:
    MockIHttpClient() = default;

    // === IHttpClient interface implementation ===
    
    [[nodiscard]] turbot::network::HttpResponse get(
        std::string_view url, 
        const turbot::network::HttpHeaders& headers = {}) override {
        return handle_request("GET", std::string(url), "", headers);
    }
    
    [[nodiscard]] turbot::network::HttpResponse post(
        std::string_view url, 
        std::string_view body,
        const turbot::network::HttpHeaders& headers = {}) override {
        return handle_request("POST", std::string(url), std::string(body), headers);
    }
    
    [[nodiscard]] turbot::network::HttpResponse put(
        std::string_view url, 
        std::string_view body,
        const turbot::network::HttpHeaders& headers = {}) override {
        return handle_request("PUT", std::string(url), std::string(body), headers);
    }
    
    [[nodiscard]] turbot::network::HttpResponse patch(
        std::string_view url, 
        std::string_view body,
        const turbot::network::HttpHeaders& headers = {}) override {
        return handle_request("PATCH", std::string(url), std::string(body), headers);
    }
    
    [[nodiscard]] turbot::network::HttpResponse del(
        std::string_view url, 
        const turbot::network::HttpHeaders& headers = {}) override {
        return handle_request("DELETE", std::string(url), "", headers);
    }
    
    [[nodiscard]] turbot::network::HttpResponse request(
        const turbot::network::HttpRequest& req) override {
        return handle_request(
            std::string(turbot::network::method_to_string(req.method)),
            req.url,
            req.body,
            req.headers
        );
    }
    
    void set_timeout(int seconds) override {
        timeout_seconds_ = seconds;
    }
    
    void set_default_header(std::string_view name, std::string_view value) override {
        default_headers_[std::string(name)] = std::string(value);
    }
    
    void set_proxy(std::string_view proxy) override {
        proxy_ = std::string(proxy);
    }
    
    void set_ssl_verify(bool verify) override {
        ssl_verify_ = verify;
    }
    
    void set_user_agent(std::string_view user_agent) override {
        user_agent_ = std::string(user_agent);
    }

    // === Mock configuration methods ===
    
    /// Set response for a specific URL pattern
    MockIHttpClient& when_get(const std::string& url_pattern, 
                               turbot::network::HttpResponse response) {
        std::lock_guard<std::mutex> lock(mutex_);
        get_responses_[url_pattern] = std::move(response);
        return *this;
    }
    
    /// Set response for POST requests
    MockIHttpClient& when_post(const std::string& url_pattern,
                                turbot::network::HttpResponse response) {
        std::lock_guard<std::mutex> lock(mutex_);
        post_responses_[url_pattern] = std::move(response);
        return *this;
    }
    
    /// Set default response for any unmatched request
    MockIHttpClient& set_default_response(turbot::network::HttpResponse response) {
        std::lock_guard<std::mutex> lock(mutex_);
        default_response_ = std::move(response);
        return *this;
    }
    
    /// Add response to queue (for sequential responses)
    void enqueue_response(turbot::network::HttpResponse response) {
        std::lock_guard<std::mutex> lock(mutex_);
        response_queue_.push(std::move(response));
    }
    
    /// Get request count
    [[nodiscard]] size_t request_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return request_count_;
    }
    
    /// Get last request URL
    [[nodiscard]] const std::string& last_url() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_url_;
    }
    
    /// Get last request body
    [[nodiscard]] const std::string& last_body() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_body_;
    }
    
    /// Clear all recorded requests
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        request_count_ = 0;
        last_url_.clear();
        last_body_.clear();
        response_queue_ = {};
    }

private:
    [[nodiscard]] turbot::network::HttpResponse handle_request(
        const std::string& method,
        const std::string& url,
        const std::string& body,
        const turbot::network::HttpHeaders& /*headers*/) {
        
        std::lock_guard<std::mutex> lock(mutex_);
        request_count_++;
        last_url_ = url;
        last_body_ = body;
        
        // Check queue first
        if (!response_queue_.empty()) {
            auto response = response_queue_.front();
            response_queue_.pop();
            return response;
        }
        
        // Check method-specific responses
        if (method == "GET") {
            for (const auto& [pattern, response] : get_responses_) {
                if (url.find(pattern) != std::string::npos || pattern == "*") {
                    return response;
                }
            }
        } else if (method == "POST") {
            for (const auto& [pattern, response] : post_responses_) {
                if (url.find(pattern) != std::string::npos || pattern == "*") {
                    return response;
                }
            }
        }
        
        // Return default response
        return default_response_;
    }

    mutable std::mutex mutex_;
    int timeout_seconds_ = 30;
    std::unordered_map<std::string, std::string> default_headers_;
    std::string proxy_;
    bool ssl_verify_ = true;
    std::string user_agent_;
    
    // Mock state
    std::unordered_map<std::string, turbot::network::HttpResponse> get_responses_;
    std::unordered_map<std::string, turbot::network::HttpResponse> post_responses_;
    turbot::network::HttpResponse default_response_;
    std::queue<turbot::network::HttpResponse> response_queue_;
    
    size_t request_count_ = 0;
    std::string last_url_;
    std::string last_body_;
};

} // namespace turbot::test
