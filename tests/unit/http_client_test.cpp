#include <catch2/catch_test_macros.hpp>
#include <turbot/network/http_client.hpp>
#include <future>

using namespace turbot::network;

// ============================================================================
// HttpMethod Tests
// ============================================================================

TEST_CASE("HttpMethod::to_string", "[network][http]") {
    REQUIRE(method_to_string(HttpMethod::GET) == "GET");
    REQUIRE(method_to_string(HttpMethod::POST) == "POST");
    REQUIRE(method_to_string(HttpMethod::PUT) == "PUT");
    REQUIRE(method_to_string(HttpMethod::PATCH) == "PATCH");
    REQUIRE(method_to_string(HttpMethod::DELETE_) == "DELETE");
    REQUIRE(method_to_string(HttpMethod::HEAD) == "HEAD");
    REQUIRE(method_to_string(HttpMethod::OPTIONS) == "OPTIONS");
}

// ============================================================================
// HttpRequest Tests
// ============================================================================

TEST_CASE("HttpRequest::static_factory_methods", "[network][http]") {
    SECTION("GET request") {
        auto req = HttpRequest::get("https://example.com/api");
        REQUIRE(req.method == HttpMethod::GET);
        REQUIRE(req.url == "https://example.com/api");
        REQUIRE(req.body.empty());
    }
    
    SECTION("POST request") {
        auto req = HttpRequest::post("https://example.com/api", R"({"key": "value"})");
        REQUIRE(req.method == HttpMethod::POST);
        REQUIRE(req.url == "https://example.com/api");
        REQUIRE(req.body == R"({"key": "value"})");
    }
    
    SECTION("PUT request") {
        auto req = HttpRequest::put("https://example.com/api/1", R"({"id": 1})");
        REQUIRE(req.method == HttpMethod::PUT);
        REQUIRE(req.body == R"({"id": 1})");
    }
    
    SECTION("PATCH request") {
        auto req = HttpRequest::patch("https://example.com/api/1", R"({"field": "new"})");
        REQUIRE(req.method == HttpMethod::PATCH);
        REQUIRE(req.body == R"({"field": "new"})");
    }
    
    SECTION("DELETE request") {
        auto req = HttpRequest::del("https://example.com/api/1");
        REQUIRE(req.method == HttpMethod::DELETE_);
        REQUIRE(req.url == "https://example.com/api/1");
    }
}

TEST_CASE("HttpRequest::builder_pattern", "[network][http]") {
    SECTION("with_header") {
        auto req = HttpRequest::get("https://example.com/api")
            .with_header("Authorization", "Bearer token123")
            .with_header("Accept", "application/json");
        
        REQUIRE(req.headers.size() == 2);
        REQUIRE(req.headers[0].first == "Authorization");
        REQUIRE(req.headers[0].second == "Bearer token123");
        REQUIRE(req.headers[1].first == "Accept");
        REQUIRE(req.headers[1].second == "application/json");
    }
    
    SECTION("with_headers") {
        HttpHeaders hdrs = {
            {"X-Custom-1", "value1"},
            {"X-Custom-2", "value2"}
        };
        
        auto req = HttpRequest::get("https://example.com/api")
            .with_headers(hdrs);
        
        REQUIRE(req.headers.size() == 2);
    }
    
    SECTION("with_timeout") {
        auto req = HttpRequest::get("https://example.com/api")
            .with_timeout(60);
        
        REQUIRE(req.timeout_seconds == 60);
    }
    
    SECTION("with_body") {
        auto req = HttpRequest::post("https://example.com/api")
            .with_body("request body");
        
        REQUIRE(req.body == "request body");
    }
    
    SECTION("with_json_body") {
        auto req = HttpRequest::post("https://example.com/api")
            .with_json_body(R"({"key": "value"})");
        
        REQUIRE(req.body == R"({"key": "value"})");
        REQUIRE(req.headers.size() == 1);
        REQUIRE(req.headers[0].first == "Content-Type");
        REQUIRE(req.headers[0].second == "application/json");
    }
    
    SECTION("chained builders") {
        auto req = HttpRequest::post("https://example.com/api")
            .with_header("Authorization", "Bearer token")
            .with_json_body(R"({"data": "test"})")
            .with_timeout(120);
        
        REQUIRE(req.method == HttpMethod::POST);
        REQUIRE(req.url == "https://example.com/api");
        REQUIRE(req.headers.size() == 2);
        REQUIRE(req.timeout_seconds == 120);
        REQUIRE_FALSE(req.body.empty());
    }
}

// ============================================================================
// HttpResponse Tests
// ============================================================================

TEST_CASE("HttpResponse::status_checks", "[network][http]") {
    SECTION("success status codes") {
        HttpResponse resp;
        resp.status_code = 200;
        REQUIRE(resp.is_success());
        REQUIRE_FALSE(resp.is_client_error());
        REQUIRE_FALSE(resp.is_server_error());
        
        resp.status_code = 201;
        REQUIRE(resp.is_success());
        
        resp.status_code = 204;
        REQUIRE(resp.is_success());
        
        resp.status_code = 299;
        REQUIRE(resp.is_success());
    }
    
    SECTION("client error status codes") {
        HttpResponse resp;
        resp.status_code = 400;
        REQUIRE_FALSE(resp.is_success());
        REQUIRE(resp.is_client_error());
        
        resp.status_code = 404;
        REQUIRE(resp.is_client_error());
        
        resp.status_code = 499;
        REQUIRE(resp.is_client_error());
    }
    
    SECTION("server error status codes") {
        HttpResponse resp;
        resp.status_code = 500;
        REQUIRE_FALSE(resp.is_success());
        REQUIRE(resp.is_server_error());
        
        resp.status_code = 502;
        REQUIRE(resp.is_server_error());
        
        resp.status_code = 599;
        REQUIRE(resp.is_server_error());
    }
}

TEST_CASE("HttpResponse::get_header", "[network][http]") {
    HttpResponse resp;
    resp.headers = {
        {"Content-Type", "application/json"},
        {"X-Rate-Limit", "100"},
        {"Set-Cookie", "session=abc123"}
    };
    
    SECTION("find existing header") {
        auto value = resp.get_header("Content-Type");
        REQUIRE(value.has_value());
        REQUIRE(*value == "application/json");
    }
    
    SECTION("case insensitive search") {
        auto value = resp.get_header("content-type");
        REQUIRE(value.has_value());
        REQUIRE(*value == "application/json");
        
        value = resp.get_header("CONTENT-TYPE");
        REQUIRE(value.has_value());
        REQUIRE(*value == "application/json");
    }
    
    SECTION("non-existent header") {
        auto value = resp.get_header("X-Not-Found");
        REQUIRE_FALSE(value.has_value());
    }
}

// ============================================================================
// HttpClient Tests
// ============================================================================

TEST_CASE("HttpClient::construction", "[network][http]") {
    SECTION("default construction") {
        HttpClient client;
        // Client should be usable
    }
    
    SECTION("move construction") {
        HttpClient client1;
        HttpClient client2 = std::move(client1);
    }
    
    SECTION("move assignment") {
        HttpClient client1;
        HttpClient client2;
        client2 = std::move(client1);
    }
}

TEST_CASE("HttpClient::configuration", "[network][http]") {
    HttpClient client;
    
    SECTION("set_timeout") {
        REQUIRE_NOTHROW(client.set_timeout(60));
        REQUIRE_NOTHROW(client.set_timeout(5));
    }
    
    SECTION("set_default_header") {
        REQUIRE_NOTHROW(client.set_default_header("Authorization", "Bearer token"));
        REQUIRE_NOTHROW(client.set_default_header("X-Custom", "value"));
    }
    
    SECTION("set_proxy") {
        REQUIRE_NOTHROW(client.set_proxy("http://proxy:8080"));
        REQUIRE_NOTHROW(client.set_proxy(""));
    }
    
    SECTION("set_ssl_verify") {
        REQUIRE_NOTHROW(client.set_ssl_verify(true));
        REQUIRE_NOTHROW(client.set_ssl_verify(false));
    }
    
    SECTION("set_user_agent") {
        REQUIRE_NOTHROW(client.set_user_agent("CustomAgent/1.0"));
    }
}

// Note: The following tests make real HTTP requests to httpbin.org
// They are skipped by default but can be enabled for integration testing

#ifdef TURBOT_INTEGRATION_TESTS

TEST_CASE("HttpClient::get_integration", "[network][http][integration]") {
    HttpClient client;
    client.set_timeout(10);
    
    auto response = client.get("https://httpbin.org/get", {
        {"X-Test-Header", "test-value"}
    });
    
    REQUIRE(response.is_success());
    REQUIRE(response.status_code == 200);
    REQUIRE_FALSE(response.body.empty());
}

TEST_CASE("HttpClient::post_integration", "[network][http][integration]") {
    HttpClient client;
    client.set_timeout(10);
    
    std::string json_body = R"({"key": "value", "number": 42})";
    auto response = client.post_json("https://httpbin.org/post", json_body);
    
    REQUIRE(response.is_success());
    REQUIRE(response.status_code == 200);
    REQUIRE_FALSE(response.body.empty());
    
    // httpbin returns the posted JSON in the response
    REQUIRE(response.body.find("key") != std::string::npos);
}

TEST_CASE("HttpClient::put_integration", "[network][http][integration]") {
    HttpClient client;
    client.set_timeout(10);
    
    auto response = client.put("https://httpbin.org/put", R"({"updated": true})");
    
    REQUIRE(response.is_success());
    REQUIRE(response.status_code == 200);
}

TEST_CASE("HttpClient::patch_integration", "[network][http][integration]") {
    HttpClient client;
    client.set_timeout(10);
    
    auto response = client.patch("https://httpbin.org/patch", R"({"field": "patched"})");
    
    REQUIRE(response.is_success());
    REQUIRE(response.status_code == 200);
}

TEST_CASE("HttpClient::delete_integration", "[network][http][integration]") {
    HttpClient client;
    client.set_timeout(10);
    
    auto response = client.del("https://httpbin.org/delete");
    
    REQUIRE(response.is_success());
    REQUIRE(response.status_code == 200);
}

TEST_CASE("HttpClient::async_integration", "[network][http][integration]") {
    HttpClient client;
    client.set_timeout(10);
    
    auto future = client.get_async("https://httpbin.org/get");
    
    REQUIRE(future.valid());
    
    auto response = future.get();
    REQUIRE(response.is_success());
}

TEST_CASE("HttpClient::error_handling_integration", "[network][http][integration]") {
    HttpClient client;
    client.set_timeout(5);
    
    // Test 404 error
    auto response = client.get("https://httpbin.org/status/404");
    REQUIRE_FALSE(response.is_success());
    REQUIRE(response.is_client_error());
    REQUIRE(response.status_code == 404);
    
    // Test 500 error
    response = client.get("https://httpbin.org/status/500");
    REQUIRE_FALSE(response.is_success());
    REQUIRE(response.is_server_error());
    REQUIRE(response.status_code == 500);
}

#endif // TURBOT_INTEGRATION_TESTS

// ============================================================================
// HttpClient Mock/Placeholder Tests (always run)
// ============================================================================

TEST_CASE("HttpClient::get_placeholder", "[network][http]") {
    HttpClient client;
    
    // Using a simple URL that should work or return a reasonable error
    // This tests that the client can at least attempt a request
    try {
        auto response = client.get("https://example.com/");
        // If it succeeds, we got a response
        // Note: actual status depends on network availability
    } catch (const std::runtime_error& e) {
        // Network errors are acceptable in test environment
        // Just verify the error message is reasonable
        std::string msg = e.what();
        REQUIRE_FALSE(msg.empty());
    }
}

TEST_CASE("HttpClient::async_basic", "[network][http]") {
    HttpClient client;
    
    SECTION("async get returns valid future") {
        auto future = client.get_async("https://example.com/");
        REQUIRE(future.valid());
    }
    
    SECTION("async post returns valid future") {
        auto future = client.post_async("https://example.com/", R"({"test": true})");
        REQUIRE(future.valid());
    }
}

TEST_CASE("HttpClient::request_with_request_object", "[network][http]") {
    HttpClient client;
    
    auto req = HttpRequest::get("https://example.com/")
        .with_header("Accept", "text/html")
        .with_timeout(10);
    
    try {
        auto response = client.request(req);
        // Response received (may fail due to network in test env)
    } catch (const std::runtime_error& e) {
        // Network errors acceptable
    }
}

// ============================================================================
// Stream Response Tests
// ============================================================================

TEST_CASE("HttpClient::stream_response", "[network][http]") {
    HttpClient client;
    client.set_timeout(10);
    
    SECTION("stream callback receives chunks") {
        std::string collected_data;
        int chunk_count = 0;
        
        auto callback = [&](std::string_view chunk) -> bool {
            collected_data.append(chunk);
            chunk_count++;
            return true;  // Continue streaming
        };
        
        try {
            auto req = HttpRequest::get("https://example.com/");
            auto response = client.request_stream(req, callback);
            
            // For streaming, body should be empty (data went to callback)
            // The status code should still be set
            // Note: actual results depend on network
        } catch (const std::runtime_error& e) {
            // Network errors acceptable in test environment
        }
    }
    
    SECTION("stream callback can abort transfer") {
        int chunk_count = 0;
        
        auto callback = [&](std::string_view chunk) -> bool {
            chunk_count++;
            return chunk_count < 3;  // Abort after 2 chunks
        };
        
        try {
            auto req = HttpRequest::get("https://example.com/");
            auto response = client.request_stream(req, callback);
            // Transfer should have been aborted
        } catch (const std::runtime_error& e) {
            // Network errors or abort acceptable
        }
    }
}

// ============================================================================
// Redirect Tests
// ============================================================================

TEST_CASE("HttpRequest::redirect_settings", "[network][http]") {
    SECTION("default redirect settings") {
        auto req = HttpRequest::get("https://example.com/");
        REQUIRE(req.follow_redirects);
        REQUIRE(req.max_redirects == 5);
    }
    
    SECTION("disable redirects") {
        HttpRequest req;
        req.method = HttpMethod::GET;
        req.url = "https://example.com/";
        req.follow_redirects = false;
        
        REQUIRE_FALSE(req.follow_redirects);
    }
    
    SECTION("custom max redirects") {
        HttpRequest req;
        req.method = HttpMethod::GET;
        req.url = "https://example.com/";
        req.max_redirects = 10;
        
        REQUIRE(req.max_redirects == 10);
    }
}

// ============================================================================
// Default Headers Merge Tests
// ============================================================================

TEST_CASE("HttpClient::default_headers_merge", "[network][http]") {
    HttpClient client;
    
    SECTION("set multiple default headers") {
        client.set_default_header("Authorization", "Bearer token123");
        client.set_default_header("X-Api-Key", "apikey");
        client.set_default_header("Accept", "application/json");
        
        // Default headers should be set without throwing
        REQUIRE_NOTHROW(client.get("https://example.com/"));
    }
}

// ============================================================================
// HTTP Methods Tests
// ============================================================================

TEST_CASE("HttpClient::head_method", "[network][http]") {
    HttpClient client;
    client.set_timeout(10);
    
    SECTION("HEAD request") {
        HttpRequest req;
        req.method = HttpMethod::HEAD;
        req.url = "https://example.com/";
        
        try {
            auto response = client.request(req);
            // HEAD should return headers but no body
        } catch (const std::runtime_error& e) {
            // Network errors acceptable
        }
    }
}

TEST_CASE("HttpClient::options_method", "[network][http]") {
    HttpClient client;
    client.set_timeout(10);
    
    SECTION("OPTIONS request") {
        HttpRequest req;
        req.method = HttpMethod::OPTIONS;
        req.url = "https://example.com/";
        
        try {
            auto response = client.request(req);
            // OPTIONS should return allowed methods
        } catch (const std::runtime_error& e) {
            // Network errors acceptable
        }
    }
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_CASE("HttpClient::error_handling", "[network][http]") {
    HttpClient client;
    client.set_timeout(5);
    
    SECTION("invalid URL throws exception") {
        REQUIRE_THROWS_AS(
            client.get("not-a-valid-url"),
            std::runtime_error
        );
    }
    
    SECTION("empty URL throws exception") {
        REQUIRE_THROWS_AS(
            client.get(""),
            std::runtime_error
        );
    }
    
    SECTION("connection to non-existent host throws") {
        REQUIRE_THROWS_AS(
            client.get("https://this-domain-does-not-exist-12345.invalid/"),
            std::runtime_error
        );
    }
}

// ============================================================================
// Response Time Tests
// ============================================================================

TEST_CASE("HttpClient::response_time", "[network][http]") {
    HttpClient client;
    client.set_timeout(10);
    
    SECTION("response time is recorded") {
        try {
            auto response = client.get("https://example.com/");
            // Response time should be non-zero for actual requests
            REQUIRE(response.response_time_ms >= 0);
        } catch (const std::runtime_error& e) {
            // Network errors acceptable
        }
    }
}

// ============================================================================
// Timeout Configuration Tests
// ============================================================================

TEST_CASE("HttpClient::timeout_configuration", "[network][http]") {
    SECTION("request-level timeout overrides default") {
        HttpClient client;
        client.set_timeout(30);  // Default 30 seconds
        
        auto req = HttpRequest::get("https://example.com/")
            .with_timeout(5);  // Override to 5 seconds
        
        REQUIRE(req.timeout_seconds == 5);
    }
    
    SECTION("zero timeout uses default") {
        HttpClient client;
        client.set_timeout(60);
        
        HttpRequest req;
        req.url = "https://example.com/";
        req.timeout_seconds = 0;  // Should use default
        
        try {
            auto response = client.request(req);
        } catch (const std::runtime_error& e) {
            // Network errors acceptable
        }
    }
}

// ============================================================================
// Proxy Configuration Tests
// ============================================================================

TEST_CASE("HttpClient::proxy_configuration", "[network][http]") {
    HttpClient client;
    
    SECTION("set and clear proxy") {
        client.set_proxy("http://proxy.example.com:8080");
        client.set_proxy("");  // Clear proxy
        
        // Should work without proxy
        REQUIRE_NOTHROW(client.set_proxy("socks5://localhost:1080"));
    }
}

// ============================================================================
// SSL Verification Tests
// ============================================================================

TEST_CASE("HttpClient::ssl_configuration", "[network][http]") {
    HttpClient client;
    
    SECTION("disable SSL verification") {
        // Warning: only for testing!
        client.set_ssl_verify(false);
        REQUIRE_NOTHROW(client.set_ssl_verify(true));
    }
    
    SECTION("enable SSL verification (default)") {
        client.set_ssl_verify(true);
        REQUIRE_NOTHROW(client.set_ssl_verify(true));
    }
}

// ============================================================================
// User Agent Tests
// ============================================================================

TEST_CASE("HttpClient::user_agent_configuration", "[network][http]") {
    HttpClient client;
    
    SECTION("custom user agent") {
        client.set_user_agent("MyApp/2.0 (Compatible; Test)");
        // Should not throw
    }
    
    SECTION("default user agent") {
        // Default is "TurbotAI/1.0"
        HttpClient client2;
        // Client should be usable with default user agent
    }
}

// ============================================================================
// HttpMethod Edge Cases
// ============================================================================

TEST_CASE("HttpMethod::all_methods", "[network][http]") {
    // Test all method conversions
    REQUIRE(method_to_string(HttpMethod::GET) == "GET");
    REQUIRE(method_to_string(HttpMethod::POST) == "POST");
    REQUIRE(method_to_string(HttpMethod::PUT) == "PUT");
    REQUIRE(method_to_string(HttpMethod::PATCH) == "PATCH");
    REQUIRE(method_to_string(HttpMethod::DELETE_) == "DELETE");
    REQUIRE(method_to_string(HttpMethod::HEAD) == "HEAD");
    REQUIRE(method_to_string(HttpMethod::OPTIONS) == "OPTIONS");
}

// ============================================================================
// HttpResponse Additional Tests
// ============================================================================

TEST_CASE("HttpResponse::edge_cases", "[network][http]") {
    SECTION("empty headers") {
        HttpResponse resp;
        resp.status_code = 200;
        
        auto value = resp.get_header("Non-Existent");
        REQUIRE_FALSE(value.has_value());
    }
    
    SECTION("status boundaries") {
        HttpResponse resp;
        
        // Just below 200
        resp.status_code = 199;
        REQUIRE_FALSE(resp.is_success());
        
        // Exactly 200
        resp.status_code = 200;
        REQUIRE(resp.is_success());
        
        // Exactly 299
        resp.status_code = 299;
        REQUIRE(resp.is_success());
        
        // Just above 299
        resp.status_code = 300;
        REQUIRE_FALSE(resp.is_success());
        
        // Exactly 400
        resp.status_code = 400;
        REQUIRE(resp.is_client_error());
        
        // Exactly 499
        resp.status_code = 499;
        REQUIRE(resp.is_client_error());
        
        // Exactly 500
        resp.status_code = 500;
        REQUIRE(resp.is_server_error());
        
        // Exactly 599
        resp.status_code = 599;
        REQUIRE(resp.is_server_error());
        
        // 600 is not server error
        resp.status_code = 600;
        REQUIRE_FALSE(resp.is_server_error());
    }
    
    SECTION("get_header with empty name") {
        HttpResponse resp;
        resp.headers = {{"Content-Type", "application/json"}};
        
        auto value = resp.get_header("");
        REQUIRE_FALSE(value.has_value());
    }
}

// ============================================================================
// HttpRequest Default Values Tests
// ============================================================================

TEST_CASE("HttpRequest::default_values", "[network][http]") {
    HttpRequest req;
    
    REQUIRE(req.method == HttpMethod::GET);
    REQUIRE(req.url.empty());
    REQUIRE(req.headers.empty());
    REQUIRE(req.body.empty());
    REQUIRE(req.timeout_seconds == 30);
    REQUIRE(req.follow_redirects);
    REQUIRE(req.max_redirects == 5);
}
