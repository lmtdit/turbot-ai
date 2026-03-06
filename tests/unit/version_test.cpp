#include <catch2/catch_test_macros.hpp>
#include <turbot/core/version.hpp>

using namespace turbot::core;

TEST_CASE("Version::constants", "[core][version]") {
    SECTION("major version") {
        REQUIRE(Version::major == 0);
    }

    SECTION("minor version") {
        REQUIRE(Version::minor == 1);
    }

    SECTION("patch version") {
        REQUIRE(Version::patch == 0);
    }
}

TEST_CASE("Version::string", "[core][version]") {
    SECTION("version string") {
        REQUIRE(Version::string() == "0.1.0");
    }
}

TEST_CASE("Version::name", "[core][version]") {
    SECTION("project name") {
        REQUIRE(Version::name() == "turbot-ai");
    }
}

TEST_CASE("get_version_string", "[core][version]") {
    SECTION("global version function") {
        REQUIRE(get_version_string() == "0.1.0");
        REQUIRE(get_version_string() == Version::string());
    }
}
