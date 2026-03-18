#include <catch2/catch_test_macros.hpp>
#include <turbot/network/url.hpp>

using namespace turbot::network;

// ==================== URL 基础解析测试 ====================

TEST_CASE("Url.BasicParsing", "[Network][Url]") {
    Url url("https://example.com/path");
    REQUIRE(url.is_valid());
    REQUIRE(url.scheme() == "https");
    REQUIRE(url.host() == "example.com");
    REQUIRE(url.path() == "/path");
}

TEST_CASE("Url.ParsingWithPort", "[Network][Url]") {
    Url url("http://localhost:8080/api");
    REQUIRE(url.is_valid());
    REQUIRE(url.scheme() == "http");
    REQUIRE(url.host() == "localhost");
    REQUIRE(url.port().has_value());
    REQUIRE(url.port().value() == 8080);
    REQUIRE(url.path() == "/api");
}

TEST_CASE("Url.ParsingWithQuery", "[Network][Url]") {
    Url url("https://api.example.com/search?q=hello&page=1");
    REQUIRE(url.is_valid());
    REQUIRE(url.host() == "api.example.com");
    REQUIRE(url.query() == "q=hello&page=1");
}

TEST_CASE("Url.ParsingWithFragment", "[Network][Url]") {
    Url url("https://example.com/page#section1");
    REQUIRE(url.is_valid());
    REQUIRE(url.fragment() == "section1");
}

TEST_CASE("Url.ParsingWithQueryAndFragment", "[Network][Url]") {
    Url url("https://example.com/page?key=value#anchor");
    REQUIRE(url.is_valid());
    REQUIRE(url.query() == "key=value");
    REQUIRE(url.fragment() == "anchor");
}

// ==================== 默认端口测试 ====================

TEST_CASE("Url.DefaultPortHttp", "[Network][Url]") {
    Url url("http://example.com/");
    REQUIRE(url.is_valid());
    REQUIRE(url.port().has_value());
    REQUIRE(url.port().value() == 80);
}

TEST_CASE("Url.DefaultPortHttps", "[Network][Url]") {
    Url url("https://example.com/");
    REQUIRE(url.is_valid());
    REQUIRE(url.port().has_value());
    REQUIRE(url.port().value() == 443);
}

// ==================== 无效 URL 测试 ====================

TEST_CASE("Url.InvalidEmpty", "[Network][Url]") {
    Url url("");
    REQUIRE_FALSE(url.is_valid());
}

TEST_CASE("Url.InvalidNoScheme", "[Network][Url]") {
    Url url("example.com/path");
    REQUIRE_FALSE(url.is_valid());
}

TEST_CASE("Url.InvalidNoHost", "[Network][Url]") {
    Url url("https:///path");
    REQUIRE_FALSE(url.is_valid());
}

// ==================== to_string 测试 ====================

TEST_CASE("Url.ToString", "[Network][Url]") {
    Url url("https://example.com/path");
    REQUIRE(url.to_string() == "https://example.com/path");
}

TEST_CASE("Url.ToStringWithPort", "[Network][Url]") {
    Url url("http://localhost:3000/api");
    REQUIRE(url.to_string() == "http://localhost:3000/api");
}

TEST_CASE("Url.ToStringWithQuery", "[Network][Url]") {
    Url url("https://example.com/search?q=test");
    REQUIRE(url.to_string().find("q=test") != std::string::npos);
}

TEST_CASE("Url.ToStringWithFragment", "[Network][Url]") {
    Url url("https://example.com/page#section");
    REQUIRE(url.to_string().find("#section") != std::string::npos);
}

// ==================== 默认构造测试 ====================

TEST_CASE("Url.DefaultConstructor", "[Network][Url]") {
    Url url;
    REQUIRE_FALSE(url.is_valid());
}

// ==================== Scheme 验证测试 ====================

TEST_CASE("Url.SchemeStartWithLetter", "[Network][Url]") {
    Url url("1nvalid://example.com/");
    REQUIRE_FALSE(url.is_valid());
}

TEST_CASE("Url.SchemeWithSpecialChars", "[Network][Url]") {
    Url url("inv@lid://example.com/");
    REQUIRE_FALSE(url.is_valid());
}

TEST_CASE("Url.SchemeUppercaseNormalized", "[Network][Url]") {
    Url url("HTTPS://example.com/");
    REQUIRE(url.is_valid());
    REQUIRE(url.scheme() == "https");
}

// ==================== Userinfo 测试 ====================

TEST_CASE("Url.UserinfoStripped", "[Network][Url]") {
    Url url("https://user:pass@example.com/path");
    REQUIRE(url.is_valid());
    REQUIRE(url.host() == "example.com");
    REQUIRE(url.path() == "/path");
}

TEST_CASE("Url.UserinfoWithAtInPassword", "[Network][Url]") {
    Url url("https://user:p@ss@example.com/");
    REQUIRE(url.is_valid());
    REQUIRE(url.host() == "example.com");
}

// ==================== IPv6 测试 ====================

TEST_CASE("Url.IPv6WithoutPort", "[Network][Url]") {
    Url url("http://[::1]/path");
    REQUIRE(url.is_valid());
    REQUIRE(url.host() == "::1");
}

TEST_CASE("Url.IPv6WithPort", "[Network][Url]") {
    Url url("http://[::1]:8080/path");
    REQUIRE(url.is_valid());
    REQUIRE(url.host() == "::1");
    REQUIRE(url.port().has_value());
    REQUIRE(url.port().value() == 8080);
}

TEST_CASE("Url.IPv6NoClosingBracket", "[Network][Url]") {
    Url url("http://[::1/path");
    REQUIRE_FALSE(url.is_valid());
}

// ==================== 端口边界测试 ====================

TEST_CASE("Url.PortNonDigitChars", "[Network][Url]") {
    Url url("http://example.com:80abc/path");
    REQUIRE_FALSE(url.is_valid());
}

TEST_CASE("Url.PortZero", "[Network][Url]") {
    Url url("http://example.com:0/");
    REQUIRE(url.is_valid());
    REQUIRE(url.port().has_value());
    REQUIRE(url.port().value() == 0);
}

TEST_CASE("Url.PortMaxValid", "[Network][Url]") {
    Url url("http://example.com:65535/");
    REQUIRE(url.is_valid());
    REQUIRE(url.port().value() == 65535);
}

TEST_CASE("Url.PortInvalidChars", "[Network][Url]") {
    Url url("http://example.com:+80/");
    REQUIRE_FALSE(url.is_valid());
}

TEST_CASE("Url.IPv6PortNonDigits", "[Network][Url]") {
    Url url("http://[::1]:abc/path");
    REQUIRE_FALSE(url.is_valid());
}

// ==================== 路径测试 ====================

TEST_CASE("Url.DefaultPathSlash", "[Network][Url]") {
    Url url("https://example.com");
    REQUIRE(url.is_valid());
    REQUIRE(url.path() == "/");
}

// ==================== Fragment/Query 顺序测试 ====================

TEST_CASE("Url.FragmentBeforeQuery", "[Network][Url]") {
    Url url("https://example.com/path#frag?not-query");
    REQUIRE(url.is_valid());
    REQUIRE(url.path() == "/path");
    REQUIRE(url.fragment() == "frag?not-query");
    REQUIRE(url.query().empty());
}

// ==================== 控制字符测试 ====================

TEST_CASE("Url.ControlCharInHost", "[Network][Url]") {
    std::string raw = "https://ex\x01ample.com/path";
    Url url(raw);
    REQUIRE_FALSE(url.is_valid());
}

TEST_CASE("Url.DelCharInPath", "[Network][Url]") {
    std::string raw = "https://example.com/pa\x7Fth";
    Url url(raw);
    REQUIRE_FALSE(url.is_valid());
}

// ==================== Host 小写化测试 ====================

TEST_CASE("Url.HostLowercased", "[Network][Url]") {
    Url url("https://EXAMPLE.COM/");
    REQUIRE(url.is_valid());
    REQUIRE(url.host() == "example.com");
}

// ==================== to_string 默认端口省略测试 ====================

TEST_CASE("Url.ToStringOmitDefaultHttpPort", "[Network][Url]") {
    Url url("http://example.com:80/");
    REQUIRE(url.to_string() == "http://example.com/");
}

TEST_CASE("Url.ToStringOmitDefaultHttpsPort", "[Network][Url]") {
    Url url("https://example.com:443/path");
    REQUIRE(url.to_string() == "https://example.com/path");
}

TEST_CASE("Url.ToStringIncludeNonDefaultPort", "[Network][Url]") {
    Url url("https://example.com:8443/path");
    REQUIRE(url.to_string() == "https://example.com:8443/path");
}

TEST_CASE("Url.ToStringWithQueryAndFragment", "[Network][Url]") {
    Url url("https://example.com/page?k=v#sec");
    auto s = url.to_string();
    REQUIRE(s == "https://example.com/page?k=v#sec");
}
