# Network Library CMakeLists.txt

set(LIBRARY_NAME turbot-network)

# Source files
set(NETWORK_SOURCES
    src/http_client.cpp
    src/url.cpp
)

# Header files
set(NETWORK_HEADERS
    include/turbot/network/http_client.hpp
    include/turbot/network/url.hpp
    include/turbot/network/export.hpp
)

# Create library
add_library(${LIBRARY_NAME} ${NETWORK_SOURCES} ${NETWORK_HEADERS})

# Configure library
add_library(turbot::network ALIAS ${LIBRARY_NAME})

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
target_link_libraries(${LIBRARY_NAME}
    PUBLIC
        turbot::core
        turbot::utils
)

# Boost dependency removed - not currently needed for placeholder implementation
# find_package(Boost REQUIRED COMPONENTS system)
# target_link_libraries(${LIBRARY_NAME}
#     PUBLIC
#         Boost::system
# )

# Export macros
target_compile_definitions(${LIBRARY_NAME}
    PRIVATE
        TURBOT_NETWORK_EXPORTS
)

# Tests
if(TURBOT_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS ${LIBRARY_NAME}
    EXPORT turbot-network-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
)
