#include <catch2/catch_test_macros.hpp>
#include <turbot/core/version.hpp>

using namespace turbot::core;

TEST_CASE("Version constants are correct", "[version]") {
    REQUIRE(Version::major == 0);
    REQUIRE(Version::minor == 1);
    REQUIRE(Version::patch == 0);
}

TEST_CASE("Version string is correct", "[version]") {
    REQUIRE(Version::string() == "0.1.0");
    REQUIRE(get_version_string() == "0.1.0");
}

TEST_CASE("Project name is correct", "[version]") {
    REQUIRE(Version::name() == "turbot-ai");
}
