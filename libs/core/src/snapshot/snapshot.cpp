#include <turbot/core/snapshot/snapshot.hpp>
#include <turbot/core/common/logger.hpp>
#include <fstream>
#include <sstream>
#include <random>
#include <openssl/sha.h>
#include <regex>

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

// ============================================================================
// SnapshotManager
// ============================================================================

SnapshotManager& SnapshotManager::instance() {
    static SnapshotManager instance;
    return instance;
}

std::string SnapshotManager::generate_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";
    
    std::string id;
    id.reserve(36);
    
    // Generate UUID v4 format
    for (int i = 0; i < 8; i++) id += hex[dis(gen)];
    id += '-';
    for (int i = 0; i < 4; i++) id += hex[dis(gen)];
    id += "-4";  // Version 4
    for (int i = 0; i < 3; i++) id += hex[dis(gen)];
    id += '-';
    id += hex[8 + dis(gen) % 4];  // Variant
    for (int i = 0; i < 3; i++) id += hex[dis(gen)];
    id += '-';
    for (int i = 0; i < 12; i++) id += hex[dis(gen)];
    
    return id;
}

std::string SnapshotManager::compute_file_hash(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "";
    }
    
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer))) {
        SHA256_Update(&sha256, buffer, file.gcount());
    }
    if (file.gcount() > 0) {
        SHA256_Update(&sha256, buffer, file.gcount());
    }
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);
    
    std::stringstream ss;
    for (unsigned char c : hash) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    
    return ss.str();
}

std::string SnapshotManager::read_file_content(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
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
    return true;
}

bool SnapshotManager::is_excluded(
    const std::filesystem::path& path,
    const std::vector<std::string>& patterns
) {
    std::string path_str = path.string();
    
    for (const auto& pattern : patterns) {
        // Simple glob matching
        if (pattern.find('*') != std::string::npos) {
            // Convert glob to regex
            std::string regex_str;
            for (char c : pattern) {
                if (c == '*') {
                    regex_str += ".*";
                } else if (c == '?') {
                    regex_str += ".";
                } else if (c == '.' || c == '+' || c == '[' || c == ']' ||
                           c == '(' || c == ')' || c == '{' || c == '}' ||
                           c == '^' || c == '$' || c == '|' || c == '\\') {
                    // Escape regex special characters
                    regex_str += "\\";
                    regex_str += c;
                } else {
                    regex_str += c;
                }
            }
            try {
                std::regex re(regex_str);
                if (std::regex_search(path_str, re)) {
                    return true;
                }
            } catch (const std::regex_error&) {
                // Invalid regex, skip this pattern
                continue;
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
        if (is_excluded(path, data.options.exclude_patterns)) {
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
    
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec) {
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
        data.file_contents[path_str] = read_file_content(path);
    }
}

std::string SnapshotManager::start_tracking(const SnapshotOptions& options) {
    std::lock_guard<std::mutex> lock(mutex_);
    
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
    }
    
    // Scan initial file state
    scan_files(data);
    
    snapshots_[id] = std::move(data);
    
    TURBOT_LOG_DEBUG("Started snapshot tracking: {}", id);
    return id;
}

PatchResult SnapshotManager::stop_tracking(const std::string& snapshot_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = snapshots_.find(snapshot_id);
    if (it == snapshots_.end()) {
        TURBOT_LOG_ERROR("Snapshot not found: {}", snapshot_id);
        return {};
    }
    
    PatchResult result;
    result.id = generate_id();
    result.snapshot_id = snapshot_id;
    result.created_at = std::chrono::system_clock::now();
    
    const auto& data = it->second;
    const auto& root = data.options.root_directory;
    
    // Scan current file state
    std::unordered_map<std::string, std::string> current_hashes;
    std::error_code ec;
    
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec || !entry.is_regular_file()) {
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
            result.files.push_back({
                .path = path,
                .type = FileChangeType::Modified,
                .old_content = data.file_contents.count(path_str) > 0 
                    ? data.file_contents.at(path_str) : "",
                .new_content = read_file_content(path),
                .hash = current_hash
            });
        }
    }
    
    // Check for deleted files
    for (const auto& [path_str, hash] : data.file_hashes) {
        if (current_hashes.find(path_str) == current_hashes.end()) {
            result.files.push_back({
                .path = path_str,
                .type = FileChangeType::Deleted,
                .old_content = data.file_contents.count(path_str) > 0 
                    ? data.file_contents.at(path_str) : "",
                .new_content = "",
                .hash = hash
            });
        }
    }
    
    snapshots_.erase(it);
    
    TURBOT_LOG_DEBUG("Stopped snapshot tracking: {}, found {} changes", 
              snapshot_id, result.files.size());
    
    return result;
}

bool SnapshotManager::apply_patch(const PatchResult& patch) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& change : patch.files) {
        switch (change.type) {
            case FileChangeType::Created:
            case FileChangeType::Modified:
                if (!write_file_content(change.path, change.new_content)) {
                    TURBOT_LOG_ERROR("Failed to apply patch for: {}", change.path.string());
                    return false;
                }
                break;
                
            case FileChangeType::Deleted:
                std::error_code ec;
                if (!std::filesystem::remove(change.path, ec) && ec) {
                    TURBOT_LOG_ERROR("Failed to delete file: {}", change.path.string());
                    return false;
                }
                break;
        }
    }
    
    TURBOT_LOG_DEBUG("Applied patch: {}", patch.id);
    return true;
}

bool SnapshotManager::rollback_patch(const PatchResult& patch) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Apply changes in reverse
    for (auto it = patch.files.rbegin(); it != patch.files.rend(); ++it) {
        const auto& change = *it;
        
        switch (change.type) {
            case FileChangeType::Created:
                // Rollback: delete the created file
                {
                    std::error_code ec;
                    std::filesystem::remove(change.path, ec);
                }
                break;
                
            case FileChangeType::Modified:
                // Rollback: restore old content
                if (!write_file_content(change.path, change.old_content)) {
                    TURBOT_LOG_ERROR("Failed to rollback modification: {}", change.path.string());
                    return false;
                }
                break;
                
            case FileChangeType::Deleted:
                // Rollback: restore the deleted file
                if (!write_file_content(change.path, change.old_content)) {
                    TURBOT_LOG_ERROR("Failed to rollback deletion: {}", change.path.string());
                    return false;
                }
                break;
        }
    }
    
    TURBOT_LOG_DEBUG("Rolled back patch: {}", patch.id);
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

