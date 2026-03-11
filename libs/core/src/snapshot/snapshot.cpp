#include <turbot/core/snapshot/snapshot.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/crypto_utils.hpp>
#include <turbot/utils/file_utils.hpp>
#include <fstream>
#include <sstream>
#include <regex>
#include <set>
#if defined(__unix__) || defined(__APPLE__)
#  include <fcntl.h>
#  include <unistd.h>
#endif

namespace turbot::core::snapshot {

// ============================================================================
// FileChangeType utilities
// ============================================================================

std::string file_change_type_to_string(FileChangeType type) noexcept {
    switch (type) {
        case FileChangeType::Created: return "created";
        case FileChangeType::Modified: return "modified";
        case FileChangeType::Deleted: return "deleted";
    }
    return "unknown";
}

FileChangeType file_change_type_from_string(const std::string& str) {
    if (str == "created") return FileChangeType::Created;
    if (str == "modified") return FileChangeType::Modified;
    if (str == "deleted") return FileChangeType::Deleted;
    throw std::invalid_argument("Invalid FileChangeType: " + str);
}

// ============================================================================
// FileChange
// ============================================================================

nlohmann::json FileChange::to_json() const {
    nlohmann::json j;
    j["path"] = path.string();
    j["type"] = file_change_type_to_string(type);
    j["hash"] = hash;
    
    if (!old_content.empty()) {
        j["old_content"] = old_content;
    }
    if (!new_content.empty()) {
        j["new_content"] = new_content;
    }
    
    return j;
}

FileChange FileChange::from_json(const nlohmann::json& j) {
    FileChange change;
    change.path = j.at("path").get<std::string>();
    change.type = file_change_type_from_string(j.at("type").get<std::string>());
    change.hash = j.at("hash").get<std::string>();
    
    if (j.contains("old_content")) {
        change.old_content = j.at("old_content").get<std::string>();
    }
    if (j.contains("new_content")) {
        change.new_content = j.at("new_content").get<std::string>();
    }
    
    return change;
}

// ============================================================================
// PatchResult
// ============================================================================

nlohmann::json PatchResult::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["snapshot_id"] = snapshot_id;
    j["created_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        created_at.time_since_epoch()
    ).count();
    
    nlohmann::json files_json = nlohmann::json::array();
    for (const auto& file : files) {
        files_json.push_back(file.to_json());
    }
    j["files"] = files_json;
    
    return j;
}

PatchResult PatchResult::from_json(const nlohmann::json& j) {
    PatchResult result;
    result.id = j.at("id").get<std::string>();
    result.snapshot_id = j.at("snapshot_id").get<std::string>();
    
    auto timestamp = j.at("created_at").get<int64_t>();
    result.created_at = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(timestamp)
    );
    
    for (const auto& file_json : j.at("files")) {
        result.files.push_back(FileChange::from_json(file_json));
    }
    
    return result;
}

// ============================================================================
// SnapshotOptions
// ============================================================================

std::vector<std::string> SnapshotOptions::default_exclude_patterns() {
    return {
        ".git",
        ".cache",
        "node_modules",
        "build",
        "cmake-build-*",
        "*.o",
        "*.obj",
        "*.a",
        "*.lib",
        "*.so",
        "*.dylib",
        "*.exe",
        "*.dll"
    };
}

// Compile glob patterns to optional<regex> (one-time, outside hot path).
// Returns nullopt for non-glob patterns (they use exact/prefix matching).
static std::vector<std::optional<std::regex>> compile_patterns(const std::vector<std::string>& patterns) {
    std::vector<std::optional<std::regex>> result;
    result.reserve(patterns.size());
    for (const auto& pattern : patterns) {
        if (pattern.find('*') == std::string::npos && pattern.find('?') == std::string::npos) {
            result.emplace_back(std::nullopt);  // non-glob: use exact/prefix match
            continue;
        }
        std::string regex_str;
        for (char c : pattern) {
            switch (c) {
                case '*': regex_str += ".*"; break;
                case '?': regex_str += ".";  break;
                case '.':
                case '+':
                case '[':
                case ']':
                case '(':
                case ')':
                case '{':
                case '}':
                case '^':
                case '$':
                case '|':
                case '\\': regex_str += '\\'; regex_str += c; break;
                default:   regex_str += c; break;
            }
        }
        try {
            result.emplace_back(std::regex{regex_str});
        } catch (const std::regex_error&) {
            result.emplace_back(std::nullopt);  // invalid pattern: fall back to exact match
        }
    }
    return result;
}

// ============================================================================
// SnapshotManager
// ============================================================================

SnapshotManager& SnapshotManager::instance() {
    static SnapshotManager instance;
    return instance;
}

std::string SnapshotManager::generate_id() {
    return turbot::utils::crypto::generate_uuid();
}

std::string SnapshotManager::compute_file_hash(const std::filesystem::path& path) {
    auto result = turbot::utils::crypto::sha256_file_ex(path);
    return std::visit([&](auto&& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return v;
        } else {
            switch (v) {
                case turbot::utils::crypto::FileHashError::NotFound:
                    TURBOT_LOG_DEBUG("compute_file_hash: file not found: {}", path.string());
                    break;
                case turbot::utils::crypto::FileHashError::TooLarge:
                    TURBOT_LOG_WARN("compute_file_hash: file too large (>100MB): {}", path.string());
                    break;
                case turbot::utils::crypto::FileHashError::IOError:
                    TURBOT_LOG_ERROR("compute_file_hash: IO error reading: {}", path.string());
                    break;
            }
            return "";
        }
    }, result);
}

std::string SnapshotManager::read_file_content(const std::filesystem::path& path) {
    return turbot::utils::read_file(path.string()).value_or("");
}

bool SnapshotManager::write_file_content(
    const std::filesystem::path& path,
    const std::string& content
) {
    // Create parent directories if needed
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            TURBOT_LOG_ERROR("Failed to create directories for {}: {}", path.string(), ec.message());
            return false;
        }
    }
    
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        TURBOT_LOG_ERROR("Failed to open file for writing: {}", path.string());
        return false;
    }
    
    file << content;
    file.close();
    if (!file.good()) {
        TURBOT_LOG_ERROR("Failed to flush/close file after writing: {}", path.string());
        return false;
    }
    return true;
}

bool SnapshotManager::is_excluded(
    const std::filesystem::path& path,
    const SnapshotData& data
) {
    std::string path_str = path.string();
    const auto& patterns = data.options.exclude_patterns;

    for (size_t i = 0; i < patterns.size(); ++i) {
        const auto& pattern = patterns[i];
        // Use pre-compiled regex for glob patterns (has_value = is glob)
        const bool has_compiled = (i < data.compiled_patterns.size()) &&
                                   data.compiled_patterns[i].has_value();
        if (has_compiled) {
            try {
                if (std::regex_search(path_str, *data.compiled_patterns[i])) {
                    return true;
                }
            } catch (const std::regex_error&) {
                // skip invalid
            }
        } else {
            // Exact match or prefix match for directories
            if (path_str == pattern ||
                path_str.find(pattern + "/") == 0 ||
                path_str.find("/" + pattern + "/") != std::string::npos ||
                path_str.find(pattern + "\\") == 0 ||
                path_str.find("\\" + pattern + "\\") != std::string::npos) {
                return true;
            }
        }
    }

    return false;
}

bool SnapshotManager::should_track(
    const std::filesystem::path& path,
    const SnapshotData& data
) {
    // Skip excluded patterns
    if (!data.options.exclude_patterns.empty()) {
        if (is_excluded(path, data)) {
            return false;
        }
    }
    
    // Apply custom filter
    if (data.options.file_filter) {
        return data.options.file_filter(path);
    }
    
    return true;
}

void SnapshotManager::scan_files(SnapshotData& data) {
    const auto& root = data.options.root_directory;
    if (root.empty()) {
        return;
    }

    // Verify root directory is accessible before iterating
    std::error_code root_ec;
    if (!std::filesystem::is_directory(root, root_ec) || root_ec) {
        TURBOT_LOG_ERROR("scan_files: root directory is not accessible: {}{}", root.string(),
                         root_ec ? fmt::format(" ({})", root_ec.message()) : "");
        return;
    }

    // Calculate cache budget in bytes (0 means hash-only mode)
    const size_t max_cache_bytes = data.options.max_cache_size_mb * 1024ULL * 1024ULL;
    size_t cached_bytes = 0;

    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec) {
            ec.clear();  // reset so subsequent entries are not skipped
            continue;
        }

        if (!entry.is_regular_file()) {
            continue;
        }

        const auto& path = entry.path();

        if (!should_track(path, data)) {
            continue;
        }

        std::string path_str = path.string();
        data.file_hashes[path_str] = compute_file_hash(path);

        // Content caching: only if budget allows.
        // Use content.size() (actual bytes in memory) rather than file_size (disk size) for
        // budget accounting — they may differ (e.g., text newline translation, truncation).
        // Note: empty files (content.size() == 0) are not cached; this is intentional since
        // they contribute nothing to the old_content diff and would clutter file_contents.
        if (max_cache_bytes > 0 && cached_bytes < max_cache_bytes) {
            auto content = read_file_content(path);
            if (!content.empty()) {
                if (cached_bytes + content.size() <= max_cache_bytes) {
                    cached_bytes += content.size();
                    data.file_contents[path_str] = std::move(content);
                } else {
                    TURBOT_LOG_DEBUG("scan_files: skipping content cache for {} "
                                     "(cache budget {}MB reached)", path_str,
                                     data.options.max_cache_size_mb);
                }
            }
            // Empty files are intentionally not cached (content is empty; old_content = "")
        }
    }

    if (cached_bytes > 0) {
        TURBOT_LOG_DEBUG("scan_files: cached {:.1f} MB of file content",
                         static_cast<double>(cached_bytes) / (1024.0 * 1024.0));
    }
}

std::string SnapshotManager::start_tracking(const SnapshotOptions& options) {
    // Step 1: Prepare data outside the lock (no shared state access needed)
    std::string id = generate_id();

    SnapshotData data;
    data.start_time = std::chrono::system_clock::now();
    data.options = options;

    // Set default exclude patterns if not specified
    if (data.options.exclude_patterns.empty()) {
        data.options.exclude_patterns = SnapshotOptions::default_exclude_patterns();
    }

    // Set default root directory
    if (data.options.root_directory.empty()) {
        data.options.root_directory = std::filesystem::current_path();
        TURBOT_LOG_WARN("start_tracking: root_directory not specified; "
                        "falling back to CWD: {}. "
                        "Pass an explicit root_directory to avoid scanning unexpected paths.",
                        data.options.root_directory.string());
    }

    // Step 2: Pre-compile glob patterns (once, before scanning)
    data.compiled_patterns = compile_patterns(data.options.exclude_patterns);

    // Step 3: IO scan outside the lock (potentially slow)
    scan_files(data);

    // Step 4: Only hold lock for map insertion (nanoseconds)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshots_[id] = std::move(data);
    }

    TURBOT_LOG_DEBUG("Started snapshot tracking: {}", id);
    return id;
}

PatchResult SnapshotManager::stop_tracking(const std::string& snapshot_id) {
    // Step 1: Hold lock only to extract data and remove from map
    SnapshotData data;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = snapshots_.find(snapshot_id);
        if (it == snapshots_.end()) {
            TURBOT_LOG_ERROR("Snapshot not found: {}", snapshot_id);
            return {};
        }
        data = std::move(it->second);
        snapshots_.erase(it);
    }
    // Lock released — IO runs outside critical section

    PatchResult result;
    result.id = generate_id();
    result.snapshot_id = snapshot_id;
    result.created_at = std::chrono::system_clock::now();

    const auto& root = data.options.root_directory;
    
    // Scan current file state
    std::unordered_map<std::string, std::string> current_hashes;
    std::error_code ec;
    
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec) {
            ec.clear();  // reset so subsequent entries are not skipped
            continue;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        
        const auto& path = entry.path();
        std::string path_str = path.string();
        
        if (!should_track(path, data)) {
            continue;
        }
        
        std::string current_hash = compute_file_hash(path);
        current_hashes[path_str] = current_hash;
        
        auto old_it = data.file_hashes.find(path_str);
        if (old_it == data.file_hashes.end()) {
            // New file created
            result.files.push_back({
                .path = path,
                .type = FileChangeType::Created,
                .old_content = "",
                .new_content = read_file_content(path),
                .hash = current_hash
            });
        } else if (old_it->second != current_hash) {
            // File modified
            auto content_it = data.file_contents.find(path_str);
            if (content_it == data.file_contents.end()) {
                TURBOT_LOG_DEBUG("stop_tracking: old_content unavailable for {} "
                                 "(file not cached, may exceed size limit)", path_str);
            }
            result.files.push_back({
                .path = path,
                .type = FileChangeType::Modified,
                .old_content = (content_it != data.file_contents.end()) ? content_it->second : "",
                .new_content = read_file_content(path),
                .hash = current_hash
            });
        }
    }
    
    // Check for deleted files
    for (const auto& [path_str, hash] : data.file_hashes) {
        if (current_hashes.find(path_str) == current_hashes.end()) {
            auto content_it = data.file_contents.find(path_str);
            if (content_it == data.file_contents.end()) {
                TURBOT_LOG_DEBUG("stop_tracking: old_content unavailable for deleted file {} "
                                 "(file not cached, may exceed size limit)", path_str);
            }
            result.files.push_back({
                .path = path_str,
                .type = FileChangeType::Deleted,
                .old_content = (content_it != data.file_contents.end()) ? content_it->second : "",
                .new_content = "",
                .hash = hash
            });
        }
    }

    TURBOT_LOG_DEBUG("Stopped snapshot tracking: {}, found {} changes",
              snapshot_id, result.files.size());

    return result;
}

bool SnapshotManager::apply_patch(const PatchResult& patch) {
    // No lock needed: patch is a self-contained value type; IO runs outside the critical section
    // Collect affected parent directories for a single fsync pass after all writes.
    std::set<std::filesystem::path> dirty_dirs;

    for (const auto& change : patch.files) {
        switch (change.type) {
            case FileChangeType::Created:
            case FileChangeType::Modified:
                if (!write_file_content(change.path, change.new_content)) {
                    TURBOT_LOG_ERROR("Failed to apply patch for: {}", change.path.string());
                    return false;
                }
                if (change.path.has_parent_path()) {
                    dirty_dirs.insert(change.path.parent_path());
                }
                break;
                
            case FileChangeType::Deleted:
                std::error_code ec;
                if (!std::filesystem::remove(change.path, ec) && ec) {
                    TURBOT_LOG_ERROR("Failed to delete file: {}", change.path.string());
                    return false;
                }
                if (change.path.has_parent_path()) {
                    dirty_dirs.insert(change.path.parent_path());
                }
                break;
        }
    }

    // Batch fsync: flush all dirty parent directories once (POSIX only).
    // This ensures directory entries are persisted even if the process crashes
    // immediately after apply_patch returns.
#if defined(__unix__) || defined(__APPLE__)
    for (const auto& dir : dirty_dirs) {
        int fd = ::open(dir.c_str(), O_RDONLY);
        if (fd >= 0) {
            if (::fsync(fd) != 0) {
                TURBOT_LOG_WARN("apply_patch: fsync failed for directory: {}", dir.string());
            }
            ::close(fd);
        }
    }
#endif

    TURBOT_LOG_DEBUG("Applied patch: {} ({} file(s) changed)", patch.id, patch.files.size());
    return true;
}

bool SnapshotManager::rollback_patch(const PatchResult& patch) {
    // No lock needed: patch is a self-contained value type; IO runs outside the critical section
    // Apply changes in reverse. Collect affected parent directories for batch fsync.
    std::set<std::filesystem::path> dirty_dirs;

    for (auto it = patch.files.rbegin(); it != patch.files.rend(); ++it) {
        const auto& change = *it;
        
        switch (change.type) {
            case FileChangeType::Created:
                // Rollback: delete the created file
                {
                    std::error_code ec;
                    if (!std::filesystem::remove(change.path, ec) && ec) {
                        TURBOT_LOG_ERROR("Failed to rollback creation (delete): {}: {}",
                                         change.path.string(), ec.message());
                        return false;
                    }
                    if (change.path.has_parent_path()) {
                        dirty_dirs.insert(change.path.parent_path());
                    }
                }
                break;
                
            case FileChangeType::Modified:
                // Rollback: restore old content
                if (!write_file_content(change.path, change.old_content)) {
                    TURBOT_LOG_ERROR("Failed to rollback modification: {}", change.path.string());
                    return false;
                }
                if (change.path.has_parent_path()) {
                    dirty_dirs.insert(change.path.parent_path());
                }
                break;
                
            case FileChangeType::Deleted:
                // Rollback: restore the deleted file
                if (!write_file_content(change.path, change.old_content)) {
                    TURBOT_LOG_ERROR("Failed to rollback deletion: {}", change.path.string());
                    return false;
                }
                if (change.path.has_parent_path()) {
                    dirty_dirs.insert(change.path.parent_path());
                }
                break;
        }
    }

    // Batch fsync: flush all dirty parent directories once (POSIX only).
#if defined(__unix__) || defined(__APPLE__)
    for (const auto& dir : dirty_dirs) {
        int fd = ::open(dir.c_str(), O_RDONLY);
        if (fd >= 0) {
            if (::fsync(fd) != 0) {
                TURBOT_LOG_WARN("rollback_patch: fsync failed for directory: {}", dir.string());
            }
            ::close(fd);
        }
    }
#endif
    
    TURBOT_LOG_DEBUG("Rolled back patch: {} ({} file(s) restored)", patch.id, patch.files.size());
    return true;
}

bool SnapshotManager::is_tracking(const std::string& snapshot_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshots_.find(snapshot_id) != snapshots_.end();
}

void SnapshotManager::cancel_tracking(const std::string& snapshot_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshots_.erase(snapshot_id);
    TURBOT_LOG_DEBUG("Cancelled snapshot tracking: {}", snapshot_id);
}

void SnapshotManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshots_.clear();
    TURBOT_LOG_DEBUG("Cleared all snapshot tracking sessions");
}

} // namespace turbot::core::snapshot

