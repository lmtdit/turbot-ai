#include <catch2/catch_test_macros.hpp>
#include <turbot/core/logger.hpp>

using namespace turbot::core;

TEST_CASE("Logger instance is accessible", "[logger]") {
    auto& logger = Logger::instance();
    REQUIRE_NOTHROW(logger.get_logger());
}

TEST_CASE("Logger level can be changed", "[logger]") {
    auto& logger = Logger::instance();
    REQUIRE_NOTHROW(logger.set_level(spdlog::level::debug));
    REQUIRE_NOTHROW(logger.set_level(spdlog::level::info));
}
