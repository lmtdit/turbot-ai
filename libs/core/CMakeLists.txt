# Core Library CMakeLists.txt

set(LIBRARY_NAME turbot-core)

# Source files
set(CORE_SOURCES
    src/version.cpp
    src/logger.cpp
)

# Header files
set(CORE_HEADERS
    include/turbot/core/version.hpp
    include/turbot/core/logger.hpp
    include/turbot/core/export.hpp
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

target_link_libraries(${LIBRARY_NAME}
    PUBLIC
        fmt::fmt
        spdlog::spdlog
)

# Export macros
target_compile_definitions(${LIBRARY_NAME}
    PRIVATE
        TURBOT_CORE_EXPORTS
)

# Tests
if(TURBOT_BUILD_TESTS)
    add_subdirectory(tests)
endif()

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
