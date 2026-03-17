/**
 * @file http_client_test.cpp
 * @brief Tests for HTTP client functionality
 */

#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/network/http_client.hpp>

using namespace turbot::network;
using namespace turbot::test;

// ==================== HttpMethod Tests ====================

TEST_CASE("HttpMethod.ToString", "[Network][Http]") {
    REQUIRE(method_to_string(HttpMethod::GET) == "GET");
    REQUIRE(method_to_string(HttpMethod::POST) == "POST");
    REQUIRE(method_to_string(HttpMethod::PUT) == "PUT");
    REQUIRE(method_to_string(HttpMethod::PATCH) == "PATCH");
    REQUIRE(method_to_string(HttpMethod::DELETE_) == "DELETE");
    REQUIRE(method_to_string(HttpMethod::HEAD) == "HEAD");
    REQUIRE(method_to_string(HttpMethod::OPTIONS) == "OPTIONS");
}

// ==================== HttpRequest Tests ====================

TEST_CASE("HttpRequest.Get", "[Network][Http]") {
    auto req = HttpRequest::get("https://example.com/api");
    
    REQUIRE(req.method == HttpMethod::GET);
    REQUIRE(req.url == "https://example.com/api");
    REQUIRE(req.body.empty());
}

TEST_CASE("HttpRequest.Post", "[Network][Http]") {
    auto req = HttpRequest::post("https://example.com/api", R"({"key": "value"})");
    
    REQUIRE(req.method == HttpMethod::POST);
    REQUIRE(req.url == "https://example.com/api");
    REQUIRE(req.body == R"({"key": "value"})");
}

TEST_CASE("HttpRequest.Put", "[Network][Http]") {
    auto req = HttpRequest::put("https://example.com/api/1", R"({"id": 1})");
    
    REQUIRE(req.method == HttpMethod::PUT);
    REQUIRE(req.body == R"({"id": 1})");
}

TEST_CASE("HttpRequest.Patch", "[Network][Http]") {
    auto req = HttpRequest::patch("https://example.com/api/1", R"({"field": "new"})");
    
    REQUIRE(req.method == HttpMethod::PATCH);
    REQUIRE(req.body == R"({"field": "new"})");
}

TEST_CASE("HttpRequest.Delete", "[Network][Http]") {
    auto req = HttpRequest::del("https://example.com/api/1");
    
    REQUIRE(req.method == HttpMethod::DELETE_);
    REQUIRE(req.url == "https://example.com/api/1");
}

TEST_CASE("HttpRequest.WithHeader", "[Network][Http]") {
    auto req = HttpRequest::get("https://example.com/api")
        .with_header("Authorization", "Bearer token123")
        .with_header("Accept", "application/json");
    
    REQUIRE(req.headers.size() == 2);
    REQUIRE(req.headers[0].first == "Authorization");
    REQUIRE(req.headers[0].second == "Bearer token123");
}

TEST_CASE("HttpRequest.WithHeaders", "[Network][Http]") {
    HttpHeaders hdrs = {
        {"X-Custom-1", "value1"},
        {"X-Custom-2", "value2"}
    };
    
    auto req = HttpRequest::get("https://example.com/api")
        .with_headers(hdrs);
    
    REQUIRE(req.headers.size() == 2);
}

TEST_CASE("HttpRequest.WithTimeout", "[Network][Http]") {
    auto req = HttpRequest::get("https://example.com/api")
        .with_timeout(60);
    
    REQUIRE(req.timeout_seconds == 60);
}

TEST_CASE("HttpRequest.WithBody", "[Network][Http]") {
    auto req = HttpRequest::post("https://example.com/api")
        .with_body("request body");
    
    REQUIRE(req.body == "request body");
}

TEST_CASE("HttpRequest.WithJsonBody", "[Network][Http]") {
    auto req = HttpRequest::post("https://example.com/api")
        .with_json_body(R"({"key": "value"})");
    
    REQUIRE(req.body == R"({"key": "value"})");
    // Check Content-Type header was added
    bool has_json_ct = false;
    for (const auto& h : req.headers) {
        if (h.first == "Content-Type" && h.second == "application/json") {
            has_json_ct = true;
            break;
        }
    }
    REQUIRE(has_json_ct);
}

TEST_CASE("HttpRequest.HeaderInjection", "[Network][Http]") {
    // Test that CRLF injection is blocked
    REQUIRE_THROWS_AS(
        HttpRequest::get("https://example.com").with_header("X-Test\r\nInjected", "value"),
        std::invalid_argument
    );
    
    REQUIRE_THROWS_AS(
        HttpRequest::get("https://example.com").with_header("X-Test", "value\r\nInjected: bad"),
        std::invalid_argument
    );
}

// ==================== HttpResponse Tests ====================

TEST_CASE("HttpResponse.IsSuccess", "[Network][Http]") {
    HttpResponse resp;
    
    resp.status_code = 200;
    REQUIRE(resp.is_success());
    
    resp.status_code = 201;
    REQUIRE(resp.is_success());
    
    resp.status_code = 204;
    REQUIRE(resp.is_success());
    
    resp.status_code = 400;
    REQUIRE_FALSE(resp.is_success());
    
    resp.status_code = 500;
    REQUIRE_FALSE(resp.is_success());
}

TEST_CASE("HttpResponse.IsClientError", "[Network][Http]") {
    HttpResponse resp;
    
    resp.status_code = 400;
    REQUIRE(resp.is_client_error());
    
    resp.status_code = 404;
    REQUIRE(resp.is_client_error());
    
    resp.status_code = 499;
    REQUIRE(resp.is_client_error());
    
    resp.status_code = 500;
    REQUIRE_FALSE(resp.is_client_error());
}

TEST_CASE("HttpResponse.IsServerError", "[Network][Http]") {
    HttpResponse resp;
    
    resp.status_code = 500;
    REQUIRE(resp.is_server_error());
    
    resp.status_code = 502;
    REQUIRE(resp.is_server_error());
    
    resp.status_code = 503;
    REQUIRE(resp.is_server_error());
    
    resp.status_code = 400;
    REQUIRE_FALSE(resp.is_server_error());
}

TEST_CASE("HttpResponse.GetHeader", "[Network][Http]") {
    HttpResponse resp;
    resp.headers = {
        {"Content-Type", "application/json"},
        {"X-Custom", "value"},
        {"Content-Length", "1234"}
    };
    
    REQUIRE(resp.get_header("Content-Type") == "application/json");
    REQUIRE(resp.get_header("content-type") == "application/json");  // case-insensitive
    REQUIRE(resp.get_header("X-Custom") == "value");
    REQUIRE(resp.get_header("X-NONEXISTENT") == std::nullopt);
}

// ==================== HttpClient Configuration Tests ====================

TEST_CASE("HttpClient.SetTimeout", "[Network][Http]") {
    HttpClient client;
    client.set_timeout(60);
    // No exception means success
    REQUIRE(true);
}

TEST_CASE("HttpClient.SetUserAgent", "[Network][Http]") {
    HttpClient client;
    client.set_user_agent("MyApp/1.0");
    REQUIRE(true);
}

TEST_CASE("HttpClient.SetProxy", "[Network][Http]") {
    HttpClient client;
    client.set_proxy("http://proxy:8080");
    REQUIRE(true);
}

TEST_CASE("HttpClient.SetSslVerify", "[Network][Http]") {
    HttpClient client;
    client.set_ssl_verify(false);
    REQUIRE(true);
}

TEST_CASE("HttpClient.SetDefaultHeader", "[Network][Http]") {
    HttpClient client;
    client.set_default_header("Authorization", "Bearer token");
    REQUIRE(true);
}

// ==================== HttpClient Move Tests ====================

TEST_CASE("HttpClient.MoveConstructor", "[Network][Http]") {
    HttpClient client1;
    client1.set_timeout(60);
    
    HttpClient client2 = std::move(client1);
    // No exception means success
    REQUIRE(true);
}

TEST_CASE("HttpClient.MoveAssignment", "[Network][Http]") {
    HttpClient client1;
    client1.set_timeout(60);
    
    HttpClient client2;
    client2 = std::move(client1);
    // No exception means success
    REQUIRE(true);
}
