# Turbot CLI Application CMakeLists.txt

set(APP_NAME turbot-cli)

# Source files
set(CLI_SOURCES
    src/main.cpp
    src/commands.cpp
)

# Create executable
add_executable(${APP_NAME} ${CLI_SOURCES})

# Compiler options
turbot_configure_target(${APP_NAME})
turbot_configure_sanitizers(${APP_NAME})
turbot_configure_coverage(${APP_NAME})

# Dependencies
target_link_libraries(${APP_NAME}
    PRIVATE
        turbot::core
        turbot::utils
        turbot::network
)

find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)

target_link_libraries(${APP_NAME}
    PRIVATE
        fmt::fmt
        nlohmann_json::nlohmann_json
)

# Set output directory
set_target_properties(${APP_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Installation
install(TARGETS ${APP_NAME}
    RUNTIME DESTINATION bin
)
