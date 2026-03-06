# Core Library Tests

set(TEST_NAME turbot-core-tests)

find_package(Catch2 REQUIRED)

set(TEST_SOURCES
    test_version.cpp
    test_logger.cpp
)

add_executable(${TEST_NAME} ${TEST_SOURCES})

target_link_libraries(${TEST_NAME}
    PRIVATE
        turbot::core
        Catch2::Catch2WithMain
)

turbot_configure_target(${TEST_NAME})
turbot_configure_sanitizers(${TEST_NAME})

include(Catch)
catch_discover_tests(${TEST_NAME})
