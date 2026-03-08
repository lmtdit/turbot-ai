#pragma once
/// @file fs_tool_common.hpp
/// Internal shared helpers for filesystem-based builtin tools (glob, grep, list).
/// Not part of the public API.

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
#include <fmt/format.h>

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

/// Determine whether \p path should be treated as a binary file.
/// Checks known binary extensions first, then samples up to 8 KB for null bytes
/// and high non-printable-character ratio (> 30%).
/// Used by both grep_tool and read_file_tool to ensure consistent behaviour.
[[nodiscard]] inline bool is_binary_file(const std::filesystem::path& path) noexcept {
    static const std::vector<std::string> binary_exts = {
        ".zip", ".tar", ".gz", ".exe", ".dll", ".so", ".class", ".jar",
        ".war", ".7z", ".bin", ".dat", ".obj", ".o", ".a",
        ".lib", ".wasm", ".pyc", ".pyo", ".png", ".jpg", ".jpeg", ".gif",
        ".ico", ".pdf", ".mp3", ".mp4", ".avi", ".mov", ".wav",
        ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx",
        ".odt", ".ods", ".odp", ".bmp", ".webp"
    };

    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (std::find(binary_exts.begin(), binary_exts.end(), ext) != binary_exts.end()) {
        return true;
    }

    // Content-based check: sample first 8 KB
    std::ifstream file(path, std::ios::binary);
    if (!file) return true;

    char buffer[8192];
    file.read(buffer, sizeof(buffer));
    std::streamsize bytes_read = file.gcount();
    if (bytes_read == 0) return false;

    // Null byte → definitely binary
    for (std::streamsize i = 0; i < bytes_read; ++i) {
        if (buffer[i] == '\0') return true;
    }

    // High non-printable ratio → likely binary
    int non_printable = 0;
    for (std::streamsize i = 0; i < bytes_read; ++i) {
        unsigned char c = static_cast<unsigned char>(buffer[i]);
        if (c < 9 || (c > 13 && c < 32)) ++non_printable;
    }
    return static_cast<double>(non_printable) / static_cast<double>(bytes_read) > 0.30;
}

/// Check that \p file_path stays within \p working_directory (workspace boundary).
/// Returns an error message string if the path escapes the workspace, std::nullopt if OK.
/// When working_directory is empty, the check is skipped (no boundary enforced).
/// Uses fail-CLOSED policy: if canonicalization fails, access is denied.
[[nodiscard]] inline std::optional<std::string>
check_workspace_boundary(
    const std::filesystem::path& file_path,
    const std::string& working_directory)
{
    if (working_directory.empty()) return std::nullopt;
    std::error_code ec1, ec2;
    auto canonical = std::filesystem::weakly_canonical(file_path, ec1);
    auto root = std::filesystem::weakly_canonical(
                    std::filesystem::path(working_directory), ec2);
    // Fail-closed: if we cannot verify the path is safe, deny access.
    if (ec1 || ec2)
        return fmt::format("Cannot verify path safety for '{}': {}",
                           file_path.string(),
                           (ec1 ? ec1 : ec2).message());
    const auto can  = canonical.string();
    const auto base = root.string();
    const bool ok = (can == base) ||
                    (can.size() > base.size() &&
                     can[base.size()] == '/' &&
                     can.substr(0, base.size()) == base);
    if (!ok)
        return fmt::format("Path '{}' escapes workspace boundary", file_path.string());
    return std::nullopt;
}

} // namespace turbot::core::tool::builtin
