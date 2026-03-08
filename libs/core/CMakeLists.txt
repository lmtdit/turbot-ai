# Core Library CMakeLists.txt

set(LIBRARY_NAME turbot-core)

# Source files - organized by module
set(CORE_SOURCES
    # Common module
    src/common/version.cpp
    src/common/logger.cpp
    # Config module
    src/config/config.cpp
    src/config/config_manager.cpp
    # Event module
    src/event/event_bus.cpp
    # Message module
    src/message/part.cpp
    src/message/message.cpp
    # Provider module
    src/provider/provider.cpp
    src/provider/impl/openai_provider.cpp
    src/provider/impl/bailian_provider.cpp
    src/provider/impl/zhipu_provider.cpp
    src/provider/impl/kimi_provider.cpp
    src/provider/impl/iflow_provider.cpp
    # Permission module
    src/permission/permission.cpp
    # Tool module
    src/tool/tool.cpp
    src/tool/tool_registry.cpp
    src/tool/skill_tool.cpp
    src/tool/builtin/read_file_tool.cpp
    src/tool/builtin/write_file_tool.cpp
    src/tool/builtin/bash_tool.cpp
    src/tool/builtin/edit_tool.cpp
    src/tool/builtin/glob_tool.cpp
    src/tool/builtin/grep_tool.cpp
    src/tool/builtin/list_tool.cpp
    src/tool/builtin/task_tool.cpp
    # Agent module
    src/agent/agent.cpp
    src/agent/agent_loader.cpp
    src/agent/builtin/build_agent.cpp
    src/agent/builtin/plan_agent.cpp
    src/agent/builtin/explore_agent.cpp
    # Session module
    src/session/session.cpp
    src/session/session_state_machine.cpp
    src/session/session_loop.cpp
    src/session/usage_tracker.cpp
    src/session/retry_manager.cpp
    src/session/session_compaction.cpp
    # LLM module (v2.0)
    src/llm/tool_schema.cpp
    src/llm/message_builder.cpp
    src/llm/stream_event.cpp
    src/llm/system_prompt.cpp
    src/llm/prompt_builder.cpp
    src/llm/llm.cpp
    src/llm/provider_adapter.cpp
    # Skill module (v2.0)
    src/skill/skill.cpp
)

# Header files - organized by module
set(CORE_HEADERS
    # Common module
    include/turbot/core/common/export.hpp
    include/turbot/core/common/version.hpp
    include/turbot/core/common/logger.hpp
    # Config module
    include/turbot/core/config/config.hpp
    include/turbot/core/config/config_manager.hpp
    # Event module
    include/turbot/core/event/event_bus.hpp
    # Message module
    include/turbot/core/message/token_usage.hpp
    include/turbot/core/message/part.hpp
    include/turbot/core/message/message.hpp
    # Provider module
    include/turbot/core/provider/provider.hpp
    include/turbot/core/provider/provider_manager.hpp
    include/turbot/core/provider/impl/openai_provider.hpp
    include/turbot/core/provider/impl/bailian_provider.hpp
    include/turbot/core/provider/impl/zhipu_provider.hpp
    include/turbot/core/provider/impl/kimi_provider.hpp
    include/turbot/core/provider/impl/iflow_provider.hpp
    # Permission module
    include/turbot/core/permission/permission.hpp
    # Tool module
    include/turbot/core/tool/tool.hpp
    include/turbot/core/tool/tool_registry.hpp
    include/turbot/core/tool/builtin/read_file_tool.hpp
    include/turbot/core/tool/builtin/write_file_tool.hpp
    include/turbot/core/tool/builtin/bash_tool.hpp
    include/turbot/core/tool/builtin/edit_tool.hpp
    include/turbot/core/tool/builtin/glob_tool.hpp
    include/turbot/core/tool/builtin/grep_tool.hpp
    include/turbot/core/tool/builtin/list_tool.hpp
    include/turbot/core/tool/builtin/task_tool.hpp
    # Agent module
    include/turbot/core/agent/agent.hpp
    include/turbot/core/agent/builtin/build_agent.hpp
    include/turbot/core/agent/builtin/plan_agent.hpp
    include/turbot/core/agent/builtin/explore_agent.hpp
    # Session module
    include/turbot/core/session/session.hpp
    include/turbot/core/session/session_state_machine.hpp
    include/turbot/core/session/session_loop.hpp
    # LLM module (v2.0)
    include/turbot/core/llm/tool_schema.hpp
    include/turbot/core/llm/message_builder.hpp
    include/turbot/core/llm/stream_event.hpp
    include/turbot/core/llm/system_prompt.hpp
    include/turbot/core/llm/prompt_builder.hpp
    include/turbot/core/llm/llm.hpp
    include/turbot/core/llm/provider_adapter.hpp
    # Skill module (v2.0)
    include/turbot/core/skill/skill.hpp
)

# Create library
add_library(${LIBRARY_NAME} ${CORE_SOURCES} ${CORE_HEADERS})

# Configure library
add_library(turbot::core ALIAS ${LIBRARY_NAME})

# Include directories
target_include_directories(${LIBRARY_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Compiler options
turbot_configure_target(${LIBRARY_NAME})
turbot_configure_sanitizers(${LIBRARY_NAME})
turbot_configure_coverage(${LIBRARY_NAME})

# Dependencies
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)

target_link_libraries(${LIBRARY_NAME}
    PUBLIC
        fmt::fmt
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        turbot::network
    PRIVATE
        turbot::utils
        turbot::storage
)

# Export macros
target_compile_definitions(${LIBRARY_NAME}
    PRIVATE
        TURBOT_CORE_EXPORTS
)

# Installation
install(TARGETS ${LIBRARY_NAME}
    EXPORT turbot-core-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
)
