#include <catch2/catch_test_macros.hpp>
#include <turbot/network/url.hpp>

using namespace turbot::network;

TEST_CASE("Url::basic_parsing", "[network][url]") {
    SECTION("parse simple URL") {
        Url url("https://example.com/path");

        REQUIRE(url.is_valid());
        REQUIRE(url.scheme() == "https");
        REQUIRE(url.host() == "example.com");
        REQUIRE(url.path() == "/path");
    }

    SECTION("parse URL with port") {
        Url url("http://localhost:8080/api");

        REQUIRE(url.is_valid());
        REQUIRE(url.scheme() == "http");
        REQUIRE(url.host() == "localhost");
        REQUIRE(url.port().has_value());
        REQUIRE(url.port().value() == 8080);
        REQUIRE(url.path() == "/api");
    }

    SECTION("parse URL with query string") {
        Url url("https://api.example.com/search?q=hello&page=1");

        REQUIRE(url.is_valid());
        REQUIRE(url.host() == "api.example.com");
        REQUIRE(url.query() == "q=hello&page=1");
    }

    SECTION("parse URL with fragment") {
        Url url("https://example.com/page#section1");

        REQUIRE(url.is_valid());
        REQUIRE(url.fragment() == "section1");
    }

    SECTION("parse URL with query and fragment") {
        Url url("https://example.com/page?key=value#anchor");

        REQUIRE(url.is_valid());
        REQUIRE(url.query() == "key=value");
        REQUIRE(url.fragment() == "anchor");
    }
}

TEST_CASE("Url::default_ports", "[network][url]") {
    SECTION("http default port") {
        Url url("http://example.com/");

        REQUIRE(url.is_valid());
        REQUIRE(url.port().has_value());
        REQUIRE(url.port().value() == 80);
    }

    SECTION("https default port") {
        Url url("https://example.com/");

        REQUIRE(url.is_valid());
        REQUIRE(url.port().has_value());
        REQUIRE(url.port().value() == 443);
    }
}

TEST_CASE("Url::invalid_urls", "[network][url]") {
    SECTION("empty URL") {
        Url url("");
        REQUIRE_FALSE(url.is_valid());
    }

    SECTION("URL without scheme") {
        Url url("example.com/path");
        REQUIRE_FALSE(url.is_valid());
    }

    SECTION("URL without host") {
        Url url("https:///path");
        REQUIRE_FALSE(url.is_valid());
    }
}

TEST_CASE("Url::to_string", "[network][url]") {
    SECTION("reconstruct simple URL") {
        Url url("https://example.com/path");
        REQUIRE(url.to_string() == "https://example.com:443/path");
    }

    SECTION("reconstruct URL with port") {
        Url url("http://localhost:3000/api");
        REQUIRE(url.to_string() == "http://localhost:3000/api");
    }

    SECTION("reconstruct URL with query") {
        Url url("https://example.com/search?q=test");
        REQUIRE(url.to_string().find("q=test") != std::string::npos);
    }

    SECTION("reconstruct URL with fragment") {
        Url url("https://example.com/page#section");
        REQUIRE(url.to_string().find("#section") != std::string::npos);
    }
}

TEST_CASE("Url::default_constructor", "[network][url]") {
    SECTION("default constructed URL is invalid") {
        Url url;
        REQUIRE_FALSE(url.is_valid());
    }
}
