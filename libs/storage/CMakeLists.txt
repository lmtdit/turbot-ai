# Storage Library CMakeLists.txt

set(LIBRARY_NAME turbot-storage)

# Source files
set(STORAGE_SOURCES
    src/database.cpp
    src/transaction.cpp
    src/sqlite_database.cpp
    src/migration.cpp
)

# Header files
set(STORAGE_HEADERS
    include/turbot/storage/export.hpp
    include/turbot/storage/database.hpp
    include/turbot/storage/transaction.hpp
    include/turbot/storage/sqlite_database.hpp
    include/turbot/storage/migration.hpp
)

# Create library
add_library(${LIBRARY_NAME} ${STORAGE_SOURCES} ${STORAGE_HEADERS})

# Configure library
add_library(turbot::storage ALIAS ${LIBRARY_NAME})

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
find_package(SQLite3 REQUIRED)

target_link_libraries(${LIBRARY_NAME}
    PUBLIC
        fmt::fmt
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        SQLite::SQLite3
    PRIVATE
        turbot::utils
        turbot::core
)

# Export macros
target_compile_definitions(${LIBRARY_NAME}
    PRIVATE
        TURBOT_STORAGE_EXPORTS
)

# Installation
install(TARGETS ${LIBRARY_NAME}
    EXPORT turbot-storage-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include
)
