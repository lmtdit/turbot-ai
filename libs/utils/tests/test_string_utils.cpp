#include <catch2/catch_test_macros.hpp>
#include <turbot/utils/string_utils.hpp>

using namespace turbot::utils;

TEST_CASE("String trim works correctly", "[string_utils]") {
    REQUIRE(trim("  hello  ") == "hello");
    REQUIRE(trim("hello") == "hello");
    REQUIRE(trim("   ") == "");
    REQUIRE(trim("") == "");
}

TEST_CASE("String case conversion works", "[string_utils]") {
    REQUIRE(to_lower("HELLO") == "hello");
    REQUIRE(to_lower("Hello") == "hello");
    REQUIRE(to_upper("hello") == "HELLO");
    REQUIRE(to_upper("Hello") == "HELLO");
}

TEST_CASE("String split works correctly", "[string_utils]") {
    auto result = split("a,b,c", ',');
    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == "a");
    REQUIRE(result[1] == "b");
    REQUIRE(result[2] == "c");

    result = split("single", ',');
    REQUIRE(result.size() == 1);
    REQUIRE(result[0] == "single");
}

TEST_CASE("String prefix/suffix checks work", "[string_utils]") {
    REQUIRE(starts_with("hello world", "hello"));
    REQUIRE(!starts_with("hello world", "world"));
    REQUIRE(ends_with("hello world", "world"));
    REQUIRE(!ends_with("hello world", "hello"));
}
