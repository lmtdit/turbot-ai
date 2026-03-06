# Utils Library Tests

set(TEST_NAME turbot-utils-tests)

find_package(Catch2 REQUIRED)

set(TEST_SOURCES
    test_string_utils.cpp
    test_file_utils.cpp
)

add_executable(${TEST_NAME} ${TEST_SOURCES})

target_link_libraries(${TEST_NAME}
    PRIVATE
        turbot::utils
        Catch2::Catch2WithMain
)

turbot_configure_target(${TEST_NAME})
turbot_configure_sanitizers(${TEST_NAME})

include(Catch)
catch_discover_tests(${TEST_NAME})
