#include <catch2/catch_test_macros.hpp>
#include <turbot/network/url.hpp>

using namespace turbot::network;

TEST_CASE("URL parsing works correctly", "[url]") {
    Url url("https://example.com:8080/path/to/resource?key=value#section");

    REQUIRE(url.is_valid());
    REQUIRE(url.scheme() == "https");
    REQUIRE(url.host() == "example.com");
    REQUIRE(url.port().has_value());
    REQUIRE(*url.port() == 8080);
    REQUIRE(url.path() == "/path/to/resource");
    REQUIRE(url.query() == "key=value");
    REQUIRE(url.fragment() == "section");
}

TEST_CASE("URL with default port works", "[url]") {
    Url url("http://example.com/path");

    REQUIRE(url.is_valid());
    REQUIRE(url.scheme() == "http");
    REQUIRE(url.host() == "example.com");
    REQUIRE(url.port().has_value());
    REQUIRE(*url.port() == 80);
    REQUIRE(url.path() == "/path");
}

TEST_CASE("Invalid URL is detected", "[url]") {
    Url url("not-a-valid-url");
    REQUIRE(!url.is_valid());
}

TEST_CASE("URL to_string reconstructs correctly", "[url]") {
    std::string original = "https://example.com/path?query=1";
    Url url(original);
    REQUIRE(url.to_string() == original);
}
