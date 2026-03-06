#include <catch2/catch_test_macros.hpp>
#include <turbot/core/logger.hpp>

using namespace turbot::core;

TEST_CASE("Logger::instance", "[core][logger]") {
    SECTION("singleton pattern") {
        auto& logger1 = Logger::instance();
        auto& logger2 = Logger::instance();

        REQUIRE(&logger1 == &logger2);
    }
}

TEST_CASE("Logger::get_logger", "[core][logger]") {
    SECTION("get logger returns valid logger") {
        auto& logger = Logger::instance();

        REQUIRE_NOTHROW(logger.get_logger());
        REQUIRE(logger.get_logger() != nullptr);
    }
}

TEST_CASE("Logger::set_level", "[core][logger]") {
    SECTION("set debug level") {
        auto& logger = Logger::instance();
        REQUIRE_NOTHROW(logger.set_level(spdlog::level::debug));
    }

    SECTION("set info level") {
        auto& logger = Logger::instance();
        REQUIRE_NOTHROW(logger.set_level(spdlog::level::info));
    }

    SECTION("set warn level") {
        auto& logger = Logger::instance();
        REQUIRE_NOTHROW(logger.set_level(spdlog::level::warn));
    }

    SECTION("set error level") {
        auto& logger = Logger::instance();
        REQUIRE_NOTHROW(logger.set_level(spdlog::level::err));
    }
}

TEST_CASE("Logger macros", "[core][logger]") {
    SECTION("log macros do not throw") {
        REQUIRE_NOTHROW(TURBOT_LOG_DEBUG("Test debug message: {}", 42));
        REQUIRE_NOTHROW(TURBOT_LOG_INFO("Test info message"));
        REQUIRE_NOTHROW(TURBOT_LOG_WARN("Test warn message"));
        REQUIRE_NOTHROW(TURBOT_LOG_ERROR("Test error message"));
    }
}
