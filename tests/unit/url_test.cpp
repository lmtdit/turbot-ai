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
        // RFC 3986 §3.2.3: default ports (80/443) should be omitted in to_string()
        REQUIRE(url.to_string() == "https://example.com/path");
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

TEST_CASE("Url::scheme_validation", "[network][url]") {
    SECTION("scheme must start with letter - digit scheme invalid") {
        // numeric first char -> invalid scheme
        Url url("1nvalid://example.com/");
        REQUIRE_FALSE(url.is_valid());
    }

    SECTION("scheme with special chars is invalid") {
        Url url("inv@lid://example.com/");
        REQUIRE_FALSE(url.is_valid());
    }

    SECTION("scheme uppercase normalized") {
        Url url("HTTPS://example.com/");
        REQUIRE(url.is_valid());
        REQUIRE(url.scheme() == "https");
    }
}

TEST_CASE("Url::userinfo_stripped", "[network][url]") {
    SECTION("userinfo before @ is stripped") {
        Url url("https://user:pass@example.com/path");
        REQUIRE(url.is_valid());
        REQUIRE(url.host() == "example.com");
        REQUIRE(url.path() == "/path");
    }

    SECTION("userinfo with @ in password (rfind)") {
        Url url("https://user:p@ss@example.com/");
        REQUIRE(url.is_valid());
        REQUIRE(url.host() == "example.com");
    }
}

TEST_CASE("Url::ipv6_addresses", "[network][url]") {
    SECTION("IPv6 address without port") {
        Url url("http://[::1]/path");
        REQUIRE(url.is_valid());
        REQUIRE(url.host() == "::1");
    }

    SECTION("IPv6 address with port") {
        Url url("http://[::1]:8080/path");
        REQUIRE(url.is_valid());
        REQUIRE(url.host() == "::1");
        REQUIRE(url.port().has_value());
        REQUIRE(url.port().value() == 8080);
    }

    SECTION("IPv6 address without closing bracket is invalid") {
        Url url("http://[::1/path");
        REQUIRE_FALSE(url.is_valid());
    }
}

TEST_CASE("Url::port_edge_cases", "[network][url]") {
    SECTION("invalid port with non-digit chars") {
        Url url("http://example.com:80abc/path");
        REQUIRE_FALSE(url.is_valid());
    }

    SECTION("port 0 is valid") {
        Url url("http://example.com:0/");
        REQUIRE(url.is_valid());
        REQUIRE(url.port().has_value());
        REQUIRE(url.port().value() == 0);
    }

    SECTION("port 65535 is valid boundary") {
        Url url("http://example.com:65535/");
        REQUIRE(url.is_valid());
        REQUIRE(url.port().value() == 65535);
    }

    SECTION("negative-like string port invalid") {
        // contains '+' or '-' which are non-digit
        Url url("http://example.com:+80/");
        REQUIRE_FALSE(url.is_valid());
    }

    SECTION("IPv6 invalid port non-digits") {
        Url url("http://[::1]:abc/path");
        REQUIRE_FALSE(url.is_valid());
    }
}

TEST_CASE("Url::path_only", "[network][url]") {
    SECTION("URL without path gets default slash") {
        Url url("https://example.com");
        REQUIRE(url.is_valid());
        REQUIRE(url.path() == "/");
    }
}

TEST_CASE("Url::fragment_before_query", "[network][url]") {
    SECTION("fragment before query treated as path+fragment only") {
        // '/path#frag?not-query' - '#' before '?' means no real query
        Url url("https://example.com/path#frag?not-query");
        REQUIRE(url.is_valid());
        REQUIRE(url.path() == "/path");
        REQUIRE(url.fragment() == "frag?not-query");
        REQUIRE(url.query().empty());
    }
}

TEST_CASE("Url::control_characters_rejected", "[network][url]") {
    SECTION("URL with control char in host is invalid") {
        std::string raw = "https://ex\x01ample.com/path";
        Url url(raw);
        REQUIRE_FALSE(url.is_valid());
    }

    SECTION("URL with DEL (0x7F) in path is invalid") {
        std::string raw = "https://example.com/pa\x7Fth";
        Url url(raw);
        REQUIRE_FALSE(url.is_valid());
    }
}

TEST_CASE("Url::host_lowercased", "[network][url]") {
    SECTION("uppercase host normalized to lowercase") {
        Url url("https://EXAMPLE.COM/");
        REQUIRE(url.is_valid());
        REQUIRE(url.host() == "example.com");
    }
}

TEST_CASE("Url::to_string_non_default_port", "[network][url]") {
    SECTION("default http port 80 omitted") {
        Url url("http://example.com:80/");
        REQUIRE(url.to_string() == "http://example.com/");
    }

    SECTION("default https port 443 omitted") {
        Url url("https://example.com:443/path");
        REQUIRE(url.to_string() == "https://example.com/path");
    }

    SECTION("non-default port included") {
        Url url("https://example.com:8443/path");
        REQUIRE(url.to_string() == "https://example.com:8443/path");
    }

    SECTION("to_string with query and fragment") {
        Url url("https://example.com/page?k=v#sec");
        auto s = url.to_string();
        REQUIRE(s == "https://example.com/page?k=v#sec");
    }
}
