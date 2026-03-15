#pragma once

#include <turbot/core/tool/tool.hpp>
#include <string>
#include <optional>

namespace turbot::core::tool {

/// Kind of external path being accessed
enum class ExternalPathKind {
    File,      ///< Single file access
    Directory  ///< Directory access
};

/// Options for external directory assertion
struct ExternalDirectoryOptions {
    bool bypass = false;                   ///< Skip permission check
    ExternalPathKind kind = ExternalPathKind::File;  ///< Kind of path
};

/// Assert that access to an external directory is permitted.
/// If the target path is outside the project directory, this function
/// will request permission from the user via the ToolContext.
///
/// @param target The target path to check (absolute or relative)
/// @param project_dir The project root directory
/// @param ctx Tool execution context for permission requests
/// @param options Options for the assertion
/// @return true if access is permitted, false if denied
[[nodiscard]] TURBOT_CORE_API bool assert_external_directory(
    const std::string& target,
    const std::string& project_dir,
    ToolContext& ctx,
    const ExternalDirectoryOptions& options = {}
);

/// Check if a path is inside the project directory.
/// @param target The target path to check
/// @param project_dir The project root directory
/// @return true if target is inside project_dir
[[nodiscard]] TURBOT_CORE_API bool is_inside_project(
    const std::string& target,
    const std::string& project_dir
);

/// Get the parent directory of a path.
/// For files, returns the containing directory.
/// For directories, returns the directory itself.
/// @param path The path to process
/// @param kind Whether the path is a file or directory
/// @return The parent directory path
[[nodiscard]] TURBOT_CORE_API std::string get_parent_directory(
    const std::string& path,
    ExternalPathKind kind
);

} // namespace turbot::core::tool
