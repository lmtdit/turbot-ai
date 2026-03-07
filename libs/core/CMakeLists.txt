# Core Library CMakeLists.txt

set(LIBRARY_NAME turbot-core)

# Source files - organized by module
set(CORE_SOURCES
    # Common module
    src/common/version.cpp
    src/common/logger.cpp
    # Config module
    src/config/config.cpp
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
    src/tool/builtin/read_file_tool.cpp
    src/tool/builtin/write_file_tool.cpp
    src/tool/builtin/bash_tool.cpp
)

# Header files - organized by module
set(CORE_HEADERS
    # Common module
    include/turbot/core/common/export.hpp
    include/turbot/core/common/version.hpp
    include/turbot/core/common/logger.hpp
    # Config module
    include/turbot/core/config/config.hpp
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
