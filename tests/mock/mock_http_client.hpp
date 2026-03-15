#pragma once

/**
 * @file mock_http_client.hpp
 * @brief HTTP 请求 Mock 测试夹具
 *
 * 用于测试中模拟 HTTP 请求，支持：
 * - 预设响应
 * - 请求验证
 * - 错误模拟
 * - 延迟模拟
 * - 流式响应模拟
 */

#include <turbot/network/http_client.hpp>
#include <turbot/network/url.hpp>
#include <nlohmann/json.hpp>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <unordered_map>

namespace turbot::test {

/// HTTP Mock 错误类型
enum class HttpMockError {
    None,               ///< 无错误
    ConnectionFailed,   ///< 连接失败
    Timeout,            ///< 请求超时
    DnsError,           ///< DNS 解析失败
    SslError,           ///< SSL 错误
    InvalidUrl,         ///< 无效 URL
    NetworkError,       ///< 网络错误
};

/// HTTP 请求匹配器
struct HttpRequestMatcher {
    std::optional<turbot::network::HttpMethod> method;
    std::optional<std::string> url_pattern;      ///< 支持 * 通配符
    std::optional<std::string> body_pattern;     ///< 支持 * 通配符
    std::unordered_map<std::string, std::string> required_headers;

    /// 检查请求是否匹配
    [[nodiscard]] bool matches(const turbot::network::HttpRequest& req) const {
        // 检查方法
        if (method.has_value() && req.method != *method) {
            return false;
        }

        // 检查 URL
        if (url_pattern.has_value() && !pattern_match(req.url, *url_pattern)) {
            return false;
        }

        // 检查 body
        if (body_pattern.has_value() && !pattern_match(req.body, *body_pattern)) {
            return false;
        }

        // 检查必需的 headers
        for (const auto& [name, value] : required_headers) {
            bool found = false;
            for (const auto& header : req.headers) {
                if (case_insensitive_equal(header.first, name) &&
                    (value.empty() || header.second == value)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
            }
        }

        return true;
    }

private:
    /// 简单的模式匹配（支持 * 通配符）
    static bool pattern_match(const std::string& text, const std::string& pattern) {
        if (pattern == "*") return true;
        if (pattern.empty()) return text.empty();

        // 检查前缀和后缀 *
        if (pattern.front() == '*' && pattern.back() == '*') {
            std::string middle = pattern.substr(1, pattern.size() - 2);
            return text.find(middle) != std::string::npos;
        }
        if (pattern.front() == '*') {
            std::string suffix = pattern.substr(1);
            if (text.size() < suffix.size()) return false;
            return text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
        if (pattern.back() == '*') {
            std::string prefix = pattern.substr(0, pattern.size() - 1);
            return text.compare(0, prefix.size(), prefix) == 0;
        }

        return text == pattern;
    }

    /// 大小写不敏感比较
    static bool case_insensitive_equal(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(a[i]) != std::tolower(b[i])) return false;
        }
        return true;
    }
};

/// Mock HTTP 响应配置
struct MockHttpResponse {
    turbot::network::HttpResponse response;
    HttpMockError error = HttpMockError::None;
    int delay_ms = 0;  ///< 模拟延迟（毫秒）

    /// 创建成功响应
    static MockHttpResponse ok(const std::string& body, int status = 200) {
        MockHttpResponse r;
        r.response.status_code = status;
        r.response.body = body;
        r.response.status_message = "OK";
        return r;
    }

    /// 创建 JSON 响应
    static MockHttpResponse json(const nlohmann::json& j, int status = 200) {
        MockHttpResponse r;
        r.response.status_code = status;
        r.response.body = j.dump();
        r.response.content_type = "application/json";
        r.response.headers.push_back({"Content-Type", "application/json"});
        r.response.status_message = "OK";
        return r;
    }

    /// 创建错误响应
    static MockHttpResponse error_response(int status, const std::string& message) {
        MockHttpResponse r;
        r.response.status_code = status;
        r.response.body = message;
        r.response.status_message = message;
        return r;
    }

    /// 创建网络错误
    static MockHttpResponse network_error(HttpMockError err) {
        MockHttpResponse r;
        r.error = err;
        return r;
    }

    /// 设置延迟
    MockHttpResponse& with_delay(int ms) {
        delay_ms = ms;
        return *this;
    }

    /// 添加 header
    MockHttpResponse& with_header(const std::string& name, const std::string& value) {
        response.headers.push_back({name, value});
        return *this;
    }
};

/// Mock HTTP 客户端
///
/// 完整模拟 HTTP 客户端行为，支持预设响应、请求验证和错误模拟。
///
/// @example
/// ```cpp
/// TEST_CASE("HTTP client test") {
///     turbot::test::MockHttpClient mock;
///
///     // 预设响应
///     mock.when(turbot::network::HttpMethod::GET, "https://api.example.com/*")
///         .respond(MockHttpResponse::json({{"status", "ok"}}));
///
///     // 使用 mock 进行测试
///     auto response = mock.get("https://api.example.com/users");
///     REQUIRE(response.status_code == 200);
///
///     // 验证请求
///     REQUIRE(mock.request_count() == 1);
///     REQUIRE(mock.last_request().url == "https://api.example.com/users");
/// }
/// ```
class MockHttpClient {
public:
    MockHttpClient() = default;

    // === 高级 API（与 turbot::network::HttpClient 兼容）===

    /// 执行 GET 请求
    [[nodiscard]] turbot::network::HttpResponse get(
        std::string_view url,
        const turbot::network::HttpHeaders& headers = {}
    ) {
        turbot::network::HttpRequest req = turbot::network::HttpRequest::get(std::string(url));
        req.headers = headers;
        return request(req);
    }

    /// 执行 POST 请求
    [[nodiscard]] turbot::network::HttpResponse post(
        std::string_view url,
        std::string_view body,
        const turbot::network::HttpHeaders& headers = {}
    ) {
        turbot::network::HttpRequest req = turbot::network::HttpRequest::post(std::string(url), body);
        req.headers = headers;
        return request(req);
    }

    /// 执行 POST JSON 请求
    [[nodiscard]] turbot::network::HttpResponse post_json(
        std::string_view url,
        std::string_view json_body,
        const turbot::network::HttpHeaders& headers = {}
    ) {
        turbot::network::HttpRequest req = turbot::network::HttpRequest::post(std::string(url), json_body);
        req.headers = headers;
        req.with_header("Content-Type", "application/json");
        return request(req);
    }

    /// 执行 PUT 请求
    [[nodiscard]] turbot::network::HttpResponse put(
        std::string_view url,
        std::string_view body,
        const turbot::network::HttpHeaders& headers = {}
    ) {
        turbot::network::HttpRequest req = turbot::network::HttpRequest::put(std::string(url), body);
        req.headers = headers;
        return request(req);
    }

    /// 执行 PATCH 请求
    [[nodiscard]] turbot::network::HttpResponse patch(
        std::string_view url,
        std::string_view body,
        const turbot::network::HttpHeaders& headers = {}
    ) {
        turbot::network::HttpRequest req = turbot::network::HttpRequest::patch(std::string(url), body);
        req.headers = headers;
        return request(req);
    }

    /// 执行 DELETE 请求
    [[nodiscard]] turbot::network::HttpResponse del(
        std::string_view url,
        const turbot::network::HttpHeaders& headers = {}
    ) {
        turbot::network::HttpRequest req = turbot::network::HttpRequest::del(std::string(url));
        req.headers = headers;
        return request(req);
    }

    /// 执行请求
    [[nodiscard]] turbot::network::HttpResponse request(const turbot::network::HttpRequest& req) {
        std::lock_guard<std::mutex> lock(mutex_);

        // 记录请求
        request_count_++;
        last_request_ = req;
        requests_.push_back(req);

        // 检查是否应该抛出错误
        if (!error_sequence_.empty()) {
            auto error = error_sequence_.front();
            error_sequence_.pop();
            if (error != HttpMockError::None) {
                return make_error_response(error);
            }
        }

        // 查找匹配的响应
        for (auto& rule : response_rules_) {
            if (rule.matcher.matches(req)) {
                // 应用延迟
                if (rule.response.delay_ms > 0) {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(rule.response.delay_ms)
                    );
                }

                // 检查错误
                if (rule.response.error != HttpMockError::None) {
                    return make_error_response(rule.response.error);
                }

                return rule.response.response;
            }
        }

        // 检查默认响应队列
        if (!default_responses_.empty()) {
            auto response = default_responses_.front();
            default_responses_.pop();

            if (response.delay_ms > 0) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(response.delay_ms)
                );
            }

            if (response.error != HttpMockError::None) {
                return make_error_response(response.error);
            }

            return response.response;
        }

        // 返回默认响应
        return make_default_response(req);
    }

    /// 执行流式请求
    [[nodiscard]] turbot::network::HttpResponse request_stream(
        const turbot::network::HttpRequest& req,
        turbot::network::StreamCallback callback
    ) {
        std::lock_guard<std::mutex> lock(mutex_);

        request_count_++;
        last_request_ = req;
        requests_.push_back(req);

        // 查找匹配的流式响应
        for (auto& rule : stream_rules_) {
            if (rule.matcher.matches(req)) {
                for (const auto& chunk : rule.chunks) {
                    if (!callback(chunk)) {
                        break;  // 客户端中止
                    }
                }
                return rule.final_response;
            }
        }

        // 默认行为：返回普通响应
        return request(req);
    }

    // === Mock 配置 API ===

    /// 配置响应规则
    class ResponseBuilder {
    public:
        ResponseBuilder(MockHttpClient& client, HttpRequestMatcher matcher)
            : client_(client), matcher_(std::move(matcher)) {}

        /// 设置响应
        ResponseBuilder& respond(const MockHttpResponse& response) {
            client_.add_response_rule(matcher_, response);
            return *this;
        }

        /// 设置 JSON 响应
        ResponseBuilder& respond_json(const nlohmann::json& j, int status = 200) {
            return respond(MockHttpResponse::json(j, status));
        }

        /// 设置错误响应
        ResponseBuilder& respond_error(int status, const std::string& message) {
            return respond(MockHttpResponse::error_response(status, message));
        }

        /// 设置网络错误
        ResponseBuilder& respond_network_error(HttpMockError error) {
            return respond(MockHttpResponse::network_error(error));
        }

    private:
        MockHttpClient& client_;
        HttpRequestMatcher matcher_;
    };

    /// 设置响应规则（流式）
    class StreamBuilder {
    public:
        StreamBuilder(MockHttpClient& client, HttpRequestMatcher matcher)
            : client_(client), matcher_(std::move(matcher)) {}

        /// 设置流式数据块
        StreamBuilder& with_chunks(const std::vector<std::string>& chunks) {
            chunks_ = chunks;
            return *this;
        }

        /// 设置最终响应
        StreamBuilder& with_final_response(const turbot::network::HttpResponse& response) {
            final_response_ = response;
            return *this;
        }

        /// 完成配置
        void build() {
            client_.add_stream_rule(matcher_, chunks_, final_response_);
        }

    private:
        MockHttpClient& client_;
        HttpRequestMatcher matcher_;
        std::vector<std::string> chunks_;
        turbot::network::HttpResponse final_response_;
    };

    /// 设置请求匹配规则
    ResponseBuilder when(turbot::network::HttpMethod method, const std::string& url_pattern) {
        HttpRequestMatcher matcher;
        matcher.method = method;
        matcher.url_pattern = url_pattern;
        return ResponseBuilder(*this, matcher);
    }

    /// 设置任意请求的默认响应
    void set_default_response(const MockHttpResponse& response) {
        std::lock_guard<std::mutex> lock(mutex_);
        default_responses_.push(response);
    }

    /// 设置响应序列
    void set_response_sequence(const std::vector<MockHttpResponse>& responses) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& r : responses) {
            default_responses_.push(r);
        }
    }

    /// 设置错误序列
    void set_error_sequence(const std::vector<HttpMockError>& errors) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& e : errors) {
            error_sequence_.push(e);
        }
    }

    // === 请求验证 API ===

    /// 获取请求次数
    [[nodiscard]] int request_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return request_count_;
    }

    /// 获取最后的请求
    [[nodiscard]] turbot::network::HttpRequest last_request() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_request_;
    }

    /// 获取所有请求
    [[nodiscard]] std::vector<turbot::network::HttpRequest> all_requests() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return requests_;
    }

    /// 验证请求是否匹配
    [[nodiscard]] bool verify_request(
        turbot::network::HttpMethod method,
        const std::string& url_pattern
    ) const {
        std::lock_guard<std::mutex> lock(mutex_);
        HttpRequestMatcher matcher;
        matcher.method = method;
        matcher.url_pattern = url_pattern;

        for (const auto& req : requests_) {
            if (matcher.matches(req)) {
                return true;
            }
        }
        return false;
    }

    /// 验证请求次数
    [[nodiscard]] int count_requests(
        turbot::network::HttpMethod method,
        const std::string& url_pattern
    ) const {
        std::lock_guard<std::mutex> lock(mutex_);
        HttpRequestMatcher matcher;
        matcher.method = method;
        matcher.url_pattern = url_pattern;

        int count = 0;
        for (const auto& req : requests_) {
            if (matcher.matches(req)) {
                count++;
            }
        }
        return count;
    }

    // === 状态管理 ===

    /// 重置所有状态
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        request_count_ = 0;
        last_request_ = {};
        requests_.clear();
        response_rules_.clear();
        stream_rules_.clear();
        default_responses_ = {};
        error_sequence_ = {};
    }

private:
    mutable std::mutex mutex_;
    int request_count_ = 0;
    turbot::network::HttpRequest last_request_;
    std::vector<turbot::network::HttpRequest> requests_;

    /// 响应规则
    struct ResponseRule {
        HttpRequestMatcher matcher;
        MockHttpResponse response;
    };
    std::vector<ResponseRule> response_rules_;

    /// 流式响应规则
    struct StreamRule {
        HttpRequestMatcher matcher;
        std::vector<std::string> chunks;
        turbot::network::HttpResponse final_response;
    };
    std::vector<StreamRule> stream_rules_;

    /// 默认响应队列
    std::queue<MockHttpResponse> default_responses_;

    /// 错误序列
    std::queue<HttpMockError> error_sequence_;

    /// 添加响应规则
    void add_response_rule(const HttpRequestMatcher& matcher, const MockHttpResponse& response) {
        std::lock_guard<std::mutex> lock(mutex_);
        response_rules_.push_back({matcher, response});
    }

    /// 添加流式响应规则
    void add_stream_rule(
        const HttpRequestMatcher& matcher,
        const std::vector<std::string>& chunks,
        const turbot::network::HttpResponse& final_response
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        stream_rules_.push_back({matcher, chunks, final_response});
    }

    /// 创建错误响应
    [[nodiscard]] turbot::network::HttpResponse make_error_response(HttpMockError error) const {
        turbot::network::HttpResponse response;
        switch (error) {
            case HttpMockError::ConnectionFailed:
                response.status_code = 0;
                response.status_message = "Connection failed";
                break;
            case HttpMockError::Timeout:
                response.status_code = 0;
                response.status_message = "Request timeout";
                break;
            case HttpMockError::DnsError:
                response.status_code = 0;
                response.status_message = "DNS resolution failed";
                break;
            case HttpMockError::SslError:
                response.status_code = 0;
                response.status_message = "SSL error";
                break;
            case HttpMockError::InvalidUrl:
                response.status_code = 0;
                response.status_message = "Invalid URL";
                break;
            case HttpMockError::NetworkError:
                response.status_code = 0;
                response.status_message = "Network error";
                break;
            default:
                response.status_code = 500;
                response.status_message = "Unknown error";
                break;
        }
        return response;
    }

    /// 创建默认响应
    [[nodiscard]] turbot::network::HttpResponse make_default_response(
        const turbot::network::HttpRequest& req
    ) const {
        turbot::network::HttpResponse response;
        response.status_code = 200;
        response.status_message = "OK";
        response.body = R"({"mock": true, "url": ")" + req.url + R"("})";
        response.content_type = "application/json";
        return response;
    }
};

} // namespace turbot::test
