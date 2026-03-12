# Tests CMakeLists.txt

find_package(Catch2 REQUIRED)

# Copy prompts directory to build directory for tests
set(PROMPTS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/libs/core/prompts")
set(PROMPTS_BUILD_DIR "${CMAKE_BINARY_DIR}/prompts")
# Also copy to tests subdirectory so unit tests can find prompts via current_path()/prompts
set(PROMPTS_TESTS_DIR "${CMAKE_CURRENT_BINARY_DIR}/prompts")

# Create custom command to copy prompts directory (root build dir)
add_custom_command(
    OUTPUT "${PROMPTS_BUILD_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROMPTS_SOURCE_DIR}" "${PROMPTS_BUILD_DIR}"
    DEPENDS "${PROMPTS_SOURCE_DIR}"
    COMMENT "Copying prompts directory to build directory"
)

# Create custom command to copy prompts to tests subdirectory
add_custom_command(
    OUTPUT "${PROMPTS_TESTS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROMPTS_SOURCE_DIR}" "${PROMPTS_TESTS_DIR}"
    DEPENDS "${PROMPTS_SOURCE_DIR}"
    COMMENT "Copying prompts directory to tests build directory"
)

# Custom target for prompts copy (depends on both locations)
add_custom_target(copy_prompts ALL DEPENDS "${PROMPTS_BUILD_DIR}" "${PROMPTS_TESTS_DIR}")

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
    # v2.0 LLM module
    unit/tool_schema_test.cpp
    unit/message_builder_test.cpp
    unit/stream_event_test.cpp
    unit/system_prompt_test.cpp
    unit/prompt_builder_test.cpp
    unit/llm_test.cpp
    # v2.0 Session module
    unit/usage_tracker_test.cpp
    unit/retry_manager_test.cpp
    unit/session_compaction_test.cpp
    # v2.0 Skill module
    unit/skill_test.cpp
    unit/skill_tool_test.cpp
    # v2.5 Snapshot module
    unit/snapshot_test.cpp
    # v2.8 Question Tool
    unit/question_tool_test.cpp
    # v2.8.9 Plugin System
    unit/plugin_test.cpp
    # v2.8.10 Built-in Agent Presets
    unit/builtin_agents_test.cpp
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

# Ensure prompts are copied before running tests
add_dependencies(turbot-unit-tests copy_prompts)

include(Catch)
catch_discover_tests(config-manager-test)
catch_discover_tests(turbot-unit-tests)

# 集成测试
set(INTEGRATION_TEST_NAME turbot-integration-tests)

set(INTEGRATION_TEST_SOURCES
    test_integration.cpp
    integration/persistence_test.cpp
    integration/agent_collaboration_test.cpp
    integration/error_recovery_test.cpp
)

add_executable(${INTEGRATION_TEST_NAME} ${INTEGRATION_TEST_SOURCES})

target_link_libraries(${INTEGRATION_TEST_NAME}
    PRIVATE
        turbot::core
        turbot::utils
        turbot::network
        turbot::storage
        Catch2::Catch2WithMain
)

turbot_configure_target(${INTEGRATION_TEST_NAME})
turbot_configure_sanitizers(${INTEGRATION_TEST_NAME})
turbot_configure_coverage(${INTEGRATION_TEST_NAME})

catch_discover_tests(${INTEGRATION_TEST_NAME})

# E2E 测试
set(E2E_TEST_NAME turbot-e2e-tests)

set(E2E_TEST_SOURCES
    integration/e2e_test.cpp
)

add_executable(${E2E_TEST_NAME} ${E2E_TEST_SOURCES})

target_include_directories(${E2E_TEST_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(${E2E_TEST_NAME}
    PRIVATE
        turbot::core
        turbot::utils
        turbot::network
        turbot::storage
        Catch2::Catch2WithMain
)

turbot_configure_target(${E2E_TEST_NAME})
turbot_configure_sanitizers(${E2E_TEST_NAME})
turbot_configure_coverage(${E2E_TEST_NAME})

catch_discover_tests(${E2E_TEST_NAME})
