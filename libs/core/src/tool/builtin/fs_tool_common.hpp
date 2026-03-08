#pragma once
/// @file fs_tool_common.hpp
/// Internal shared helpers for filesystem-based builtin tools (glob, grep, list).
/// Not part of the public API.

#include <filesystem>
#include <string>
#include <vector>

namespace turbot::core::tool::builtin {

/// Default directory/file patterns that are ignored during recursive scans.
inline const std::vector<std::string>& default_ignore_patterns() {
    static const std::vector<std::string> patterns = {
        "node_modules", ".git", "__pycache__", "dist", "build",
        "target", "vendor", "bin", "obj", ".idea", ".vscode",
        ".zig-cache", "zig-out", ".coverage", "coverage",
        "tmp", "temp", ".cache", "cache", "logs",
        ".venv", "venv", "env"
    };
    return patterns;
}

/// Returns true if any component of \p path matches a default ignore pattern.
/// Used by glob_tool and grep_tool to skip noise directories.
[[nodiscard]] inline bool should_ignore(const std::filesystem::path& path) {
    for (const auto& part : path) {
        const std::string part_str = part.string();
        for (const auto& pattern : default_ignore_patterns()) {
            if (part_str == pattern) {
                return true;
            }
        }
    }
    return false;
}

} // namespace turbot::core::tool::builtin
