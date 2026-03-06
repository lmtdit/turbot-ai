# Utils Library CMakeLists.txt

set(LIBRARY_NAME turbot-utils)

# Source files
set(UTILS_SOURCES
    src/string_utils.cpp
    src/file_utils.cpp
    src/json_utils.cpp
    src/crypto_utils.cpp
)

# Header files
set(UTILS_HEADERS
    include/turbot/utils/string_utils.hpp
    include/turbot/utils/file_utils.hpp
    include/turbot/utils/json_utils.hpp
    include/turbot/utils/crypto_utils.hpp
    include/turbot/utils/export.hpp
)

# Create library
add_library(${LIBRARY_NAME} ${UTILS_SOURCES} ${UTILS_HEADERS})

# Configure library
add_library(turbot::utils ALIAS ${LIBRARY_NAME})

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
)

find_package(fmt REQUIRED)
target_link_libraries(${LIBRARY_NAME}
    PUBLIC
        fmt::fmt
)

# OpenSSL for crypto utilities
find_package(OpenSSL REQUIRED)
target_link_libraries(${LIBRARY_NAME}
    PUBLIC
        OpenSSL::Crypto
        OpenSSL::SSL
)

# Export macros
target_compile_definitions(${LIBRARY_NAME}
    PRIVATE
        TURBOT_UTILS_EXPORTS
)

# Installation
install(TARGETS ${LIBRARY_NAME}
    EXPORT turbot-utils-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
)
