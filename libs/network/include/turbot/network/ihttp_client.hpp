#pragma once

#include <turbot/network/http_client.hpp>
#include <memory>

namespace turbot::network {

/// HTTP client interface for dependency injection
class TURBOT_NETWORK_API IHttpClient {
public:
    virtual ~IHttpClient() = default;

    /// Perform a GET request
    [[nodiscard]] virtual HttpResponse get(std::string_view url, const HttpHeaders& headers = {}) = 0;
    
    /// Perform a POST request
    [[nodiscard]] virtual HttpResponse post(std::string_view url, std::string_view body, 
                                            const HttpHeaders& headers = {}) = 0;
    
    /// Perform a PUT request
    [[nodiscard]] virtual HttpResponse put(std::string_view url, std::string_view body,
                                           const HttpHeaders& headers = {}) = 0;
    
    /// Perform a PATCH request
    [[nodiscard]] virtual HttpResponse patch(std::string_view url, std::string_view body,
                                             const HttpHeaders& headers = {}) = 0;
    
    /// Perform a DELETE request
    [[nodiscard]] virtual HttpResponse del(std::string_view url, const HttpHeaders& headers = {}) = 0;

    /// Perform an HTTP request with full control
    [[nodiscard]] virtual HttpResponse request(const HttpRequest& req) = 0;

    /// Set default timeout for all requests
    virtual void set_timeout(int seconds) = 0;
    
    /// Set default headers for all requests
    virtual void set_default_header(std::string_view name, std::string_view value) = 0;
    
    /// Set proxy URL
    virtual void set_proxy(std::string_view proxy) = 0;
    
    /// Enable/disable SSL verification
    virtual void set_ssl_verify(bool verify) = 0;
    
    /// Set user agent string
    virtual void set_user_agent(std::string_view user_agent) = 0;
};

/// HttpClient implementation of IHttpClient
class TURBOT_NETWORK_API HttpClientWrapper : public IHttpClient {
public:
    HttpClientWrapper() = default;
    explicit HttpClientWrapper(HttpClient client) : client_(std::move(client)) {}
    
    [[nodiscard]] HttpResponse get(std::string_view url, const HttpHeaders& headers = {}) override {
        return client_.get(url, headers);
    }
    
    [[nodiscard]] HttpResponse post(std::string_view url, std::string_view body, 
                                    const HttpHeaders& headers = {}) override {
        return client_.post(url, body, headers);
    }
    
    [[nodiscard]] HttpResponse put(std::string_view url, std::string_view body,
                                   const HttpHeaders& headers = {}) override {
        return client_.put(url, body, headers);
    }
    
    [[nodiscard]] HttpResponse patch(std::string_view url, std::string_view body,
                                     const HttpHeaders& headers = {}) override {
        return client_.patch(url, body, headers);
    }
    
    [[nodiscard]] HttpResponse del(std::string_view url, const HttpHeaders& headers = {}) override {
        return client_.del(url, headers);
    }
    
    [[nodiscard]] HttpResponse request(const HttpRequest& req) override {
        return client_.request(req);
    }
    
    void set_timeout(int seconds) override {
        client_.set_timeout(seconds);
    }
    
    void set_default_header(std::string_view name, std::string_view value) override {
        client_.set_default_header(name, value);
    }
    
    void set_proxy(std::string_view proxy) override {
        client_.set_proxy(proxy);
    }
    
    void set_ssl_verify(bool verify) override {
        client_.set_ssl_verify(verify);
    }
    
    void set_user_agent(std::string_view user_agent) override {
        client_.set_user_agent(user_agent);
    }
    
private:
    HttpClient client_;
};

} // namespace turbot::network
