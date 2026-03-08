#pragma once
/// @file fs_tool_common.hpp
/// Internal shared helpers for filesystem-based builtin tools (glob, grep, list).
/// Not part of the public API.

#include <chrono>
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

/// Convert std::filesystem::file_time_type to a Unix timestamp (seconds since epoch).
/// This is a portable helper for glob_tool / grep_tool mtime fields; the conversion
/// trick avoids the clock_cast API that requires C++20 on some toolchains.
[[nodiscard]] inline int64_t file_time_to_unix_sec(
    std::filesystem::file_time_type ftime) noexcept {
    using namespace std::chrono;
    using fft = std::filesystem::file_time_type;
    auto sctp = time_point_cast<system_clock::duration>(
        ftime - fft::clock::now() + system_clock::now());
    return duration_cast<seconds>(sctp.time_since_epoch()).count();
}

} // namespace turbot::core::tool::builtin
