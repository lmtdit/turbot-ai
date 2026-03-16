# tests/v2/CMakeLists.txt
# Turbot V2 测试系统（基于 OpenCode 测试能力同步）

find_package(Catch2 REQUIRED)
find_package(nlohmann_json REQUIRED)

# V2 测试源文件
set(V2_TEST_SOURCES
    # fixture
    fixture/test_fixture.cpp
    fixture/fixture_test.cpp
    # permission
    permission/permission_test_v2.cpp
    # session
    session/session_core_test.cpp
    session/session_lifecycle_test.cpp
    session/session_store_persistence_test.cpp
    session/session_loop_test.cpp
    session/session_state_machine_test.cpp
    session/retry_manager_test.cpp
    # config
    config/config_test.cpp
    config/config_enhanced_test.cpp
    config/config_yaml_test.cpp
    config/config_env_test.cpp
    config/config_persistence_test.cpp
    config/config_validation_test.cpp
    # tool
    tool/tool_test.cpp
    tool/tool_registry_test.cpp
    tool/tool_advanced_test.cpp
    # agent
    agent/agent_test.cpp
    agent/agent_enhanced_test.cpp
    agent/agent_flow_test.cpp
    agent/agent_loader_test.cpp
    agent/agent_builtin_test.cpp
    agent/agent_generator_test.cpp
    # file
    file/filesystem_test.cpp
    # provider
    provider/provider_test.cpp
    # mcp
    mcp/mcp_test.cpp
    # skill
    skill/skill_test.cpp
    skill/skill_enhanced_test.cpp
    skill/skill_remote_test.cpp
    skill/skill_registry_advanced_test.cpp
    skill/skill_tool_test.cpp
    # storage
    storage/storage_test.cpp
    # lsp
    lsp/lsp_test.cpp
    # acp
    acp/acp_test.cpp
)

# V2 测试头文件
set(V2_TEST_HEADERS
    fixture/test_fixture.hpp
    fixture/test_macros.hpp
)

# 创建测试可执行文件
add_executable(turbot-v2-tests
    ${V2_TEST_SOURCES}
    ${V2_TEST_HEADERS}
)

# 链接库
target_link_libraries(turbot-v2-tests
    PRIVATE
        turbot::core
        turbot::utils
        turbot::storage
        Catch2::Catch2WithMain
        nlohmann_json::nlohmann_json
)

# 包含目录
target_include_directories(turbot-v2-tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/libs/core/include
        ${CMAKE_SOURCE_DIR}/libs/utils/include
        ${CMAKE_SOURCE_DIR}/libs/storage/include
        ${CMAKE_SOURCE_DIR}/tests  # for mock headers
)

# 编译特性
turbot_configure_target(turbot-v2-tests)
turbot_configure_sanitizers(turbot-v2-tests)
turbot_configure_coverage(turbot-v2-tests)

# C++23 标准（备选 C++20）
set_target_properties(turbot-v2-tests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED OFF
    CXX_EXTENSIONS OFF
)

# 如果 C++23 不可用，降级到 C++20
if(NOT CMAKE_CXX_STANDARD_DEFAULT GREATER_EQUAL 23)
    set_target_properties(turbot-v2-tests PROPERTIES CXX_STANDARD 20)
endif()

# 注册测试
catch_discover_tests(turbot-v2-tests)

# 复制 prompts 目录到测试构建目录
add_dependencies(turbot-v2-tests copy_prompts)
