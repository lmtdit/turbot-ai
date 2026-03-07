# Tests CMakeLists.txt

find_package(Catch2 REQUIRED)

# Config Manager 单元测试（独立，不依赖 network）
add_executable(config-manager-test unit/config_manager_test.cpp)

target_link_libraries(config-manager-test
    PRIVATE
        turbot::core
        turbot::utils
        Catch2::Catch2WithMain
)

turbot_configure_target(config-manager-test)
turbot_configure_sanitizers(config-manager-test)
turbot_configure_coverage(config-manager-test)

# 单元测试
set(UNIT_TEST_SOURCES
    unit/json_utils_test.cpp
    unit/string_utils_test.cpp
    unit/crypto_utils_test.cpp
    unit/config_test.cpp
    unit/config_manager_test.cpp
    unit/event_bus_test.cpp
    unit/file_utils_test.cpp
    unit/url_test.cpp
    unit/http_client_test.cpp
    unit/logger_test.cpp
    unit/version_test.cpp
    unit/storage_test.cpp
    unit/message_test.cpp
    unit/provider_test.cpp
    unit/permission_test.cpp
    unit/tool_test.cpp
    unit/agent_test.cpp
    unit/session_test.cpp
    unit/task_tool_test.cpp
    unit/session_loop_test.cpp
)

add_executable(turbot-unit-tests ${UNIT_TEST_SOURCES})

target_link_libraries(turbot-unit-tests
    PRIVATE
        turbot::core
        turbot::utils
        turbot::network
        turbot::storage
        Catch2::Catch2WithMain
)

turbot_configure_target(turbot-unit-tests)
turbot_configure_sanitizers(turbot-unit-tests)
turbot_configure_coverage(turbot-unit-tests)

include(Catch)
catch_discover_tests(config-manager-test)
catch_discover_tests(turbot-unit-tests)

# 集成测试
set(INTEGRATION_TEST_NAME turbot-integration-tests)

set(INTEGRATION_TEST_SOURCES
    test_integration.cpp
)

add_executable(${INTEGRATION_TEST_NAME} ${INTEGRATION_TEST_SOURCES})

target_link_libraries(${INTEGRATION_TEST_NAME}
    PRIVATE
        turbot::core
        turbot::utils
        turbot::network
        Catch2::Catch2WithMain
)

turbot_configure_target(${INTEGRATION_TEST_NAME})
turbot_configure_sanitizers(${INTEGRATION_TEST_NAME})
turbot_configure_coverage(${INTEGRATION_TEST_NAME})

catch_discover_tests(${INTEGRATION_TEST_NAME})
