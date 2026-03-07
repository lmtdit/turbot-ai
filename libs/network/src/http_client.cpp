#include <turbot/network/http_client.hpp>
#include <turbot/core/common/logger.hpp>

#include <curl/curl.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace turbot::network {

// ============================================================================
// CURL Global Initialization (Thread-Safe)
// ============================================================================

namespace {

std::once_flag curl_init_flag;

void ensure_curl_initialized() {
    std::call_once(curl_init_flag, []() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    });
}

} // anonymous namespace

// ============================================================================
// Helper Functions
// ============================================================================

std::string_view method_to_string(HttpMethod method) noexcept {
    switch (method) {
        case HttpMethod::GET:     return "GET";
        case HttpMethod::POST:    return "POST";
        case HttpMethod::PUT:     return "PUT";
        case HttpMethod::PATCH:   return "PATCH";
        case HttpMethod::DELETE_: return "DELETE";
        case HttpMethod::HEAD:    return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
        default:                  return "GET";
    }
}

// Case-insensitive string comparison for header names
static std::string to_lower(std::string_view s) {
    std::string result(s);
    std::transform(result.begin(), result.end(), result.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// ============================================================================
// HttpRequest Implementation
// ============================================================================

HttpRequest HttpRequest::get(std::string_view url) {
    HttpRequest req;
    req.method = HttpMethod::GET;
    req.url = url;
    return req;
}

HttpRequest HttpRequest::post(std::string_view url, std::string_view body) {
    HttpRequest req;
    req.method = HttpMethod::POST;
    req.url = url;
    req.body = body;
    return req;
}

HttpRequest HttpRequest::put(std::string_view url, std::string_view body) {
    HttpRequest req;
    req.method = HttpMethod::PUT;
    req.url = url;
    req.body = body;
    return req;
}

HttpRequest HttpRequest::patch(std::string_view url, std::string_view body) {
    HttpRequest req;
    req.method = HttpMethod::PATCH;
    req.url = url;
    req.body = body;
    return req;
}

HttpRequest HttpRequest::del(std::string_view url) {
    HttpRequest req;
    req.method = HttpMethod::DELETE_;
    req.url = url;
    return req;
}

HttpRequest& HttpRequest::with_header(std::string_view name, std::string_view value) {
    headers.emplace_back(std::string(name), std::string(value));
    return *this;
}

HttpRequest& HttpRequest::with_headers(const HttpHeaders& hdrs) {
    headers.insert(headers.end(), hdrs.begin(), hdrs.end());
    return *this;
}

HttpRequest& HttpRequest::with_timeout(int seconds) {
    timeout_seconds = seconds;
    return *this;
}

HttpRequest& HttpRequest::with_body(std::string_view body_) {
    body = body_;
    return *this;
}

HttpRequest& HttpRequest::with_json_body(std::string_view json) {
    body = json;
    headers.emplace_back("Content-Type", "application/json");
    return *this;
}

// ============================================================================
// HttpResponse Implementation
// ============================================================================

std::optional<std::string> HttpResponse::get_header(std::string_view name) const {
    std::string lower_name = to_lower(name);
    for (const auto& [key, value] : headers) {
        if (to_lower(key) == lower_name) {
            return value;
        }
    }
    return std::nullopt;
}

// ============================================================================
// CURL Callbacks
// ============================================================================

struct CurlData {
    std::string body;
    std::string headers;
    StreamCallback* stream_callback = nullptr;
    bool aborted = false;
};

static size_t write_body_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    auto* data = static_cast<CurlData*>(userp);
    
    if (data->stream_callback) {
        // Streaming mode
        if (!(*data->stream_callback)(std::string_view(static_cast<const char*>(contents), total_size))) {
            data->aborted = true;
            return 0;  // Abort transfer
        }
    } else {
        // Buffer mode
        data->body.append(static_cast<const char*>(contents), total_size);
    }
    return total_size;
}

static size_t write_header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t total_size = size * nitems;
    auto* data = static_cast<CurlData*>(userdata);
    data->headers.append(buffer, total_size);
    return total_size;
}

// Parse response headers from string
static HttpHeaders parse_headers(const std::string& header_string) {
    HttpHeaders headers;
    std::istringstream stream(header_string);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // Skip status line and empty lines
        if (line.empty() || line.substr(0, 5) == "HTTP/") {
            continue;
        }
        
        // Find colon separator
        auto colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            // Trim leading whitespace from value
            size_t start = value.find_first_not_of(" \t");
            if (start != std::string::npos) {
                value = value.substr(start);
            }
            
            headers.emplace_back(std::move(key), std::move(value));
        }
    }
    
    return headers;
}

// ============================================================================
// HttpClient::Impl
// ============================================================================

class HttpClient::Impl {
public:
    int default_timeout_ = 30;
    bool ssl_verify_ = true;
    std::string user_agent_ = "TurbotAI/1.0";
    std::string proxy_;
    std::unordered_map<std::string, std::string> default_headers_;
    
    Impl() {
        // Initialize libcurl globally (thread-safe using std::call_once)
        ensure_curl_initialized();
    }
    
    ~Impl() {
        // Note: We don't call curl_global_cleanup() here because other instances
        // might still be using libcurl. It should be called at program exit.
    }
    
    HttpResponse do_request(const HttpRequest& req, StreamCallback* stream_callback = nullptr) {
        auto start_time = std::chrono::steady_clock::now();
        
        HttpResponse response;
        CurlData curl_data;
        curl_data.stream_callback = stream_callback;
        
        // Create CURL handle
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize CURL handle");
        }
        
        try {
            // Set URL
            curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
            
            // Set method
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, std::string(method_to_string(req.method)).c_str());
            
            // Set timeout
            int timeout = req.timeout_seconds > 0 ? req.timeout_seconds : default_timeout_;
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout);
            
            // Set redirects
            if (req.follow_redirects) {
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_MAXREDIRS, req.max_redirects);
            }
            
            // Build headers list
            struct curl_slist* curl_headers = nullptr;
            
            // Add request headers
            for (const auto& [key, value] : req.headers) {
                std::string header = key + ": " + value;
                curl_headers = curl_slist_append(curl_headers, header.c_str());
            }
            
            // Add default headers (if not already present)
            for (const auto& [key, value] : default_headers_) {
                bool found = false;
                for (const auto& [k, v] : req.headers) {
                    if (to_lower(k) == to_lower(key)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    std::string header = key + ": " + value;
                    curl_headers = curl_slist_append(curl_headers, header.c_str());
                }
            }
            
            // Set User-Agent
            curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent_.c_str());
            
            // Set headers
            if (curl_headers) {
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
            }
            
            // Set body for POST/PUT/PATCH
            if (!req.body.empty() && (req.method == HttpMethod::POST || 
                                       req.method == HttpMethod::PUT ||
                                       req.method == HttpMethod::PATCH)) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.c_str());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, req.body.size());
            }
            
            // Set proxy
            if (!proxy_.empty()) {
                curl_easy_setopt(curl, CURLOPT_PROXY, proxy_.c_str());
            }
            
            // Set SSL verification
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ssl_verify_ ? 1L : 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, ssl_verify_ ? 2L : 0L);
            
            // Set callbacks
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curl_data);
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header_callback);
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, &curl_data);
            
            // Perform request
            CURLcode res = curl_easy_perform(curl);
            
            // Clean up headers
            if (curl_headers) {
                curl_slist_free_all(curl_headers);
            }
            
            // Handle errors
            if (res != CURLE_OK) {
                if (curl_data.aborted) {
                    response.status_code = 0;
                    response.status_message = "Request aborted by callback";
                } else {
                    curl_easy_cleanup(curl);
                    throw std::runtime_error(std::string("CURL error: ") + curl_easy_strerror(res));
                }
            } else {
                // Get response code
                long status_code;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
                response.status_code = static_cast<int>(status_code);
                
                // Parse headers
                response.headers = parse_headers(curl_data.headers);
                
                // Get content type
                char* content_type = nullptr;
                curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type);
                if (content_type) {
                    response.content_type = content_type;
                }
                
                // Set body (empty for streaming mode)
                if (!stream_callback) {
                    response.body = std::move(curl_data.body);
                }
            }
            
            curl_easy_cleanup(curl);
        } catch (...) {
            curl_easy_cleanup(curl);
            throw;
        }
        
        // Calculate response time
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        response.response_time_ms = duration.count();
        
        TURBOT_LOG_DEBUG("HTTP {} {} -> {} ({}ms)", 
                         method_to_string(req.method), req.url, 
                         response.status_code, response.response_time_ms);
        
        return response;
    }
};

// ============================================================================
// HttpClient Implementation
// ============================================================================

HttpClient::HttpClient() : impl_(std::make_unique<Impl>()) {
    TURBOT_LOG_INFO("HttpClient initialized");
}

HttpClient::~HttpClient() = default;

HttpClient::HttpClient(HttpClient&&) noexcept = default;
HttpClient& HttpClient::operator=(HttpClient&&) noexcept = default;

HttpResponse HttpClient::get(std::string_view url, const HttpHeaders& headers) {
    return request(HttpRequest::get(url).with_headers(headers));
}

HttpResponse HttpClient::post(std::string_view url, std::string_view body, const HttpHeaders& headers) {
    return request(HttpRequest::post(url, body).with_headers(headers));
}

HttpResponse HttpClient::post_json(std::string_view url, std::string_view json_body, const HttpHeaders& headers) {
    return request(HttpRequest::post(url, json_body)
                   .with_header("Content-Type", "application/json")
                   .with_headers(headers));
}

HttpResponse HttpClient::put(std::string_view url, std::string_view body, const HttpHeaders& headers) {
    return request(HttpRequest::put(url, body).with_headers(headers));
}

HttpResponse HttpClient::patch(std::string_view url, std::string_view body, const HttpHeaders& headers) {
    return request(HttpRequest::patch(url, body).with_headers(headers));
}

HttpResponse HttpClient::del(std::string_view url, const HttpHeaders& headers) {
    return request(HttpRequest::del(url).with_headers(headers));
}

HttpResponse HttpClient::request(const HttpRequest& req) {
    return impl_->do_request(req);
}

HttpResponse HttpClient::request_stream(const HttpRequest& req, StreamCallback callback) {
    return impl_->do_request(req, &callback);
}

std::future<HttpResponse> HttpClient::get_async(std::string_view url, const HttpHeaders& headers) {
    return std::async(std::launch::async, [this, url_str = std::string(url), headers]() {
        return this->get(url_str, headers);
    });
}

std::future<HttpResponse> HttpClient::post_async(std::string_view url, std::string_view body, const HttpHeaders& headers) {
    return std::async(std::launch::async, [this, url_str = std::string(url), 
                                           body_str = std::string(body), headers]() {
        return this->post(url_str, body_str, headers);
    });
}

void HttpClient::set_timeout(int seconds) {
    impl_->default_timeout_ = seconds;
    TURBOT_LOG_DEBUG("HTTP timeout set to {} seconds", seconds);
}

void HttpClient::set_default_header(std::string_view name, std::string_view value) {
    impl_->default_headers_[std::string(name)] = std::string(value);
}

void HttpClient::set_proxy(std::string_view proxy) {
    impl_->proxy_ = std::string(proxy);
    TURBOT_LOG_DEBUG("HTTP proxy set to: {}", proxy);
}

void HttpClient::set_ssl_verify(bool verify) {
    impl_->ssl_verify_ = verify;
    TURBOT_LOG_DEBUG("SSL verification set to: {}", verify);
}

void HttpClient::set_user_agent(std::string_view user_agent) {
    impl_->user_agent_ = std::string(user_agent);
}

} // namespace turbot::network
