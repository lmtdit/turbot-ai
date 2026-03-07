cmake_minimum_required(VERSION 3.28)

# Project definition
project(turbot-ai
    VERSION 0.1.0
    DESCRIPTION "Turbot AI - A modern C++23 project"
    LANGUAGES CXX
)

# C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Position independent code for shared libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Include cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(Sanitizers)
include(Coverage)

# Conan auto-generated dependencies (if using Conan)
if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/conan_toolchain.cmake")
    include("${CMAKE_CURRENT_BINARY_DIR}/conan_toolchain.cmake")
endif()

# Project options
option(TURBOT_BUILD_TESTS "Build tests" ON)
option(TURBOT_BUILD_EXAMPLES "Build examples" ON)
option(TURBOT_ENABLE_SANITIZERS "Enable sanitizers" OFF)
option(TURBOT_ENABLE_COVERAGE "Enable code coverage" OFF)
option(TURBOT_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)

# Add subdirectories for libraries (order matters for dependencies)
add_subdirectory(libs/utils)
add_subdirectory(libs/network)
add_subdirectory(libs/storage)
add_subdirectory(libs/core)

# Add subdirectories for applications
add_subdirectory(apps/turbot-cli)
add_subdirectory(apps/turbot-server)

# Add integration tests
if(TURBOT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "=== Turbot AI Configuration Summary ===")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:     ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build Tests:      ${TURBOT_BUILD_TESTS}")
message(STATUS "  Build Examples:   ${TURBOT_BUILD_EXAMPLES}")
message(STATUS "  Sanitizers:       ${TURBOT_ENABLE_SANITIZERS}")
message(STATUS "  Coverage:         ${TURBOT_ENABLE_COVERAGE}")
message(STATUS "  Clang-Tidy:       ${TURBOT_ENABLE_CLANG_TIDY}")
message(STATUS "========================================")
message(STATUS "")
