# Integration Tests CMakeLists.txt

set(TEST_NAME turbot-integration-tests)

find_package(Catch2 REQUIRED)

set(TEST_SOURCES
    test_integration.cpp
)

add_executable(${TEST_NAME} ${TEST_SOURCES})

target_link_libraries(${TEST_NAME}
    PRIVATE
        turbot::core
        turbot::utils
        turbot::network
        Catch2::Catch2WithMain
)

turbot_configure_target(${TEST_NAME})
turbot_configure_sanitizers(${TEST_NAME})

include(Catch)
catch_discover_tests(${TEST_NAME})
