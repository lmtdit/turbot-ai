#include <catch2/catch_test_macros.hpp>
#include <turbot/core/common/version.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/string_utils.hpp>
#include <turbot/network/http_client.hpp>
#include <turbot/network/url.hpp>

using namespace turbot;

TEST_CASE("Integration: All libraries work together", "[integration]") {
    // Test version
    REQUIRE(core::Version::major == 0);

    // Test logger
    REQUIRE_NOTHROW(core::Logger::instance().get_logger());

    // Test utils
    REQUIRE(utils::trim("  test  ") == "test");

    // Test network
    network::Url url("https://example.com/path");
    REQUIRE(url.is_valid());
    REQUIRE(url.host() == "example.com");
}

TEST_CASE("Integration: End-to-end workflow", "[integration]") {
    // 1. Parse a URL
    network::Url url("https://api.example.com/v1/data");
    REQUIRE(url.is_valid());

    // 2. Make HTTP request (mocked)
    network::HttpClient client;
    auto response = client.get(url.to_string());
    REQUIRE(response.status_code == 200);

    // 3. Process response with utils
    auto content_type = utils::to_lower(response.content_type);
    REQUIRE(content_type == "application/json");

    // 4. Log the result
    TURBOT_LOG_INFO("Integration test completed successfully");
}
