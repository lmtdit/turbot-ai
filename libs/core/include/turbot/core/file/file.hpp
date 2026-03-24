#pragma once

#include <turbot/core/common/export.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace turbot::core::file {

/// File-ignore logic — mirrors OpenCode file/ignore.ts.
///
/// Provides a fast in-process matcher that skips well-known build artefact
/// directories and temporary files without spawning an external process.
namespace FileIgnore {

/// Returns true when @p filepath should be ignored by default.
///
/// Matching rules (in order):
///   1. Whitelist: any path matching an entry in @p whitelist is NOT ignored.
///   2. Folder components: any path component that is in the built-in FOLDERS
///      set (node_modules, .git, dist, build, target, …) → ignored.
///   3. File glob patterns: **/*.pyc, **/.DS_Store, **/logs/**, … → ignored.
///   4. Extra glob patterns supplied by the caller.
[[nodiscard]] TURBOT_CORE_API bool match(
    const std::string& filepath,
    const std::vector<std::string>& extra     = {},
    const std::vector<std::string>& whitelist = {}
);

/// Return the built-in list of ignored folder names (e.g. "node_modules").
[[nodiscard]] TURBOT_CORE_API const std::vector<std::string>& default_folders();

/// Return the built-in list of ignored file glob patterns (e.g. "**/*.pyc").
[[nodiscard]] TURBOT_CORE_API const std::vector<std::string>& default_file_patterns();

} // namespace FileIgnore

// ─── FileInfo ─────────────────────────────────────────────────────────────────

/// Change-set information for a single file (from git diff).
struct TURBOT_CORE_API FileInfo {
    std::string path;           ///< Project-relative path
    int added   = 0;            ///< Lines added
    int removed = 0;            ///< Lines removed
    std::string status;         ///< "added" | "modified" | "deleted"
};

// ─── FileNode ─────────────────────────────────────────────────────────────────

/// A directory entry returned by FileService::list().
struct TURBOT_CORE_API FileNode {
    std::string name;           ///< Basename
    std::string path;           ///< Project-relative path
    std::string absolute;       ///< Absolute path
    std::string type;           ///< "file" | "directory"
    bool ignored = false;       ///< Whether this entry is gitignored
};

// ─── FileContent ──────────────────────────────────────────────────────────────

/// File content returned by FileService::read().
struct TURBOT_CORE_API FileContent {
    std::string type;           ///< "text" | "binary"
    std::string content;        ///< Text content, or base64 for images
    std::string diff;           ///< Unified diff against HEAD (may be empty)
    std::string mime_type;      ///< MIME type (may be empty)
    std::string encoding;       ///< "base64" when content is base64-encoded
};

// ─── FileService ──────────────────────────────────────────────────────────────

/// File service — mirrors OpenCode File namespace in file/index.ts.
///
/// Provides:
///   - list()   — directory listing with gitignore status
///   - read()   — file content (text or base64) plus git diff
///   - status() — git change set (added/modified/deleted files)
///   - search() — fuzzy filename search using the cached file list
class TURBOT_CORE_API FileService {
public:
    explicit FileService(std::string working_dir);

    // Non-copyable, movable
    FileService(const FileService&) = delete;
    FileService& operator=(const FileService&) = delete;
    FileService(FileService&&) = default;
    FileService& operator=(FileService&&) = default;

    ~FileService() = default;

    // ─── Directory listing ────────────────────────────────────────────────────

    /// List directory entries under @p dir (relative to working_dir).
    /// Entries are sorted: directories first, then files, both alphabetically.
    [[nodiscard]] std::vector<FileNode> list(const std::string& dir = {}) const;

    // ─── File reading ─────────────────────────────────────────────────────────

    /// Read file content, performing binary / image detection.
    /// Returns FileContent with type "text" or "binary".
    [[nodiscard]] FileContent read(const std::string& relative_path) const;

    // ─── Git status ───────────────────────────────────────────────────────────

    /// Return the git change set (files modified/added/deleted since HEAD).
    /// Returns an empty vector when the project is not a git repository.
    [[nodiscard]] std::vector<FileInfo> status() const;

    // ─── File search ──────────────────────────────────────────────────────────

    struct SearchInput {
        std::string query;
        int         limit = 100;
        bool        dirs  = false;
        std::string type;   ///< "file" | "directory" | "" (both)
    };

    /// Fuzzy-search file paths from the scanned file list.
    /// Calls scan() on first use to populate the internal cache.
    [[nodiscard]] std::vector<std::string> search(const SearchInput& input);

    // ─── Cache management ─────────────────────────────────────────────────────

    /// (Re-)scan the project directory using ripgrep (if available) or
    /// std::filesystem::recursive_directory_iterator as fallback.
    void scan();

    /// Return the working directory.
    [[nodiscard]] const std::string& working_dir() const noexcept { return working_dir_; }

private:
    std::string working_dir_;

    // Scanned file cache
    mutable bool     cache_valid_ = false;
    mutable std::vector<std::string> cached_files_;
    mutable std::vector<std::string> cached_dirs_;

    // Helpers
    [[nodiscard]] bool is_git_repo() const;
    [[nodiscard]] std::string run_git(const std::vector<std::string>& args) const;
    void scan_impl() const;  // const because lazy init from search()

    static bool is_image_extension(const std::string& ext);
    static bool is_text_extension(const std::string& ext);
    static bool is_binary_extension(const std::string& ext);
    static std::string get_mime_type(const std::string& ext);
};

} // namespace turbot::core::file
