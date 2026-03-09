#pragma once

#include <turbot/core/common/export.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <mutex>
#include <regex>

namespace turbot::core::snapshot {

/// File change type
enum class FileChangeType {
    Created,   ///< File was created
    Modified,  ///< File was modified
    Deleted    ///< File was deleted
};

/// Convert FileChangeType to string
[[nodiscard]] TURBOT_CORE_API std::string file_change_type_to_string(FileChangeType type) noexcept;

/// Parse FileChangeType from string
[[nodiscard]] TURBOT_CORE_API FileChangeType file_change_type_from_string(const std::string& str);

/// File change record
struct TURBOT_CORE_API FileChange {
    std::filesystem::path path;       ///< Path to the file
    FileChangeType type;              ///< Type of change
    std::string old_content;          ///< Original content (for Modified/Deleted)
    std::string new_content;          ///< New content (for Created/Modified)
    std::string hash;                 ///< Content hash

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static FileChange from_json(const nlohmann::json& j);
};

/// Patch result containing all file changes
struct TURBOT_CORE_API PatchResult {
    std::string id;                   ///< Unique patch identifier
    std::string snapshot_id;          ///< Associated snapshot ID
    std::vector<FileChange> files;    ///< List of file changes
    std::chrono::system_clock::time_point created_at;  ///< Creation timestamp

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static PatchResult from_json(const nlohmann::json& j);

    /// Check if patch is empty
    [[nodiscard]] bool empty() const noexcept { return files.empty(); }

    /// Get number of changed files
    [[nodiscard]] size_t size() const noexcept { return files.size(); }
};

/// File filter function type
using FileFilter = std::function<bool(const std::filesystem::path&)>;

/// Snapshot options
struct TURBOT_CORE_API SnapshotOptions {
    std::filesystem::path root_directory;  ///< Root directory to track
    std::vector<std::string> exclude_patterns;  ///< Glob patterns to exclude
    FileFilter file_filter;  ///< Custom file filter

    /// Default exclude patterns
    static std::vector<std::string> default_exclude_patterns();
};

/// Snapshot data for tracking
struct SnapshotData {
    std::unordered_map<std::string, std::string> file_hashes;  ///< Path -> hash mapping
    std::unordered_map<std::string, std::string> file_contents;  ///< Path -> content backup
    std::chrono::system_clock::time_point start_time;  ///< Tracking start time
    SnapshotOptions options;  ///< Snapshot options
    /// Pre-compiled glob patterns (parallel to exclude_patterns).
    /// nullopt = non-glob pattern (use exact/prefix match instead).
    std::vector<std::optional<std::regex>> compiled_patterns;
};

/// Snapshot manager for tracking file changes
class TURBOT_CORE_API SnapshotManager {
public:
    /// Get singleton instance
    static SnapshotManager& instance();

    // Non-copyable, non-movable
    SnapshotManager(const SnapshotManager&) = delete;
    SnapshotManager& operator=(const SnapshotManager&) = delete;
    SnapshotManager(SnapshotManager&&) = delete;
    SnapshotManager& operator=(SnapshotManager&&) = delete;

    /// Start tracking file changes
    /// @param options Snapshot options
    /// @return Snapshot ID for later reference
    std::string start_tracking(const SnapshotOptions& options = {});

    /// Stop tracking and generate patch
    /// @param snapshot_id The snapshot ID returned by start_tracking
    /// @return Patch result containing all changes
    PatchResult stop_tracking(const std::string& snapshot_id);

    /// Apply a patch (for rollback purposes)
    /// @param patch The patch to apply
    /// @return true if successful
    bool apply_patch(const PatchResult& patch);

    /// Rollback a patch
    /// @param patch The patch to rollback
    /// @return true if successful
    bool rollback_patch(const PatchResult& patch);

    /// Check if a snapshot is being tracked
    [[nodiscard]] bool is_tracking(const std::string& snapshot_id) const;

    /// Cancel a tracking session without generating patch
    void cancel_tracking(const std::string& snapshot_id);

    /// Get number of active tracking sessions
    [[nodiscard]] size_t active_tracking_count() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return snapshots_.size();
    }

    /// Clear all tracking sessions
    void clear();

private:
    SnapshotManager() = default;
    ~SnapshotManager() = default;

    /// Generate unique snapshot ID
    [[nodiscard]] static std::string generate_id();

    /// Compute file hash (SHA-256)
    [[nodiscard]] static std::string compute_file_hash(const std::filesystem::path& path);

    /// Read file content
    [[nodiscard]] static std::string read_file_content(const std::filesystem::path& path);

    /// Write file content
    static bool write_file_content(const std::filesystem::path& path, const std::string& content);

    /// Check if path matches any exclude pattern (uses pre-compiled patterns from SnapshotData)
    [[nodiscard]] static bool is_excluded(
        const std::filesystem::path& path,
        const SnapshotData& data
    );

    /// Should track this file
    [[nodiscard]] static bool should_track(
        const std::filesystem::path& path,
        const SnapshotData& data
    );

    /// Scan directory and record file states
    void scan_files(SnapshotData& data);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, SnapshotData> snapshots_;
};

} // namespace turbot::core::snapshot
