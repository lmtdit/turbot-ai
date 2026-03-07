#include <catch2/catch_test_macros.hpp>
#include <turbot/core/common/logger.hpp>

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
        // do-while 包装的宏不能直接用于 REQUIRE_NOTHROW
        // 因为它们展开后是语句而不是表达式
        TURBOT_LOG_DEBUG("Test debug message: {}", 42);
        TURBOT_LOG_INFO("Test info message");
        TURBOT_LOG_WARN("Test warn message");
        TURBOT_LOG_ERROR("Test error message");
        REQUIRE(true);  // 如果宏抛出异常，测试会失败
    }
}
