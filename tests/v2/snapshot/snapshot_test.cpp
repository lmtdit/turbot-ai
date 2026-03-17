/**
 * @file snapshot_test.cpp
 * @brief Tests for Snapshot module
 */

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/snapshot/snapshot.hpp>
#include <filesystem>
#include <fstream>

using namespace turbot::core::snapshot;
namespace fs = std::filesystem;

// ==================== FileChangeType Tests ====================

TEST_CASE("FileChangeType.ToString", "[Snapshot]") {
    REQUIRE(file_change_type_to_string(FileChangeType::Created) == "created");
    REQUIRE(file_change_type_to_string(FileChangeType::Modified) == "modified");
    REQUIRE(file_change_type_to_string(FileChangeType::Deleted) == "deleted");
}

TEST_CASE("FileChangeType.FromString", "[Snapshot]") {
    REQUIRE(file_change_type_from_string("created") == FileChangeType::Created);
    REQUIRE(file_change_type_from_string("modified") == FileChangeType::Modified);
    REQUIRE(file_change_type_from_string("deleted") == FileChangeType::Deleted);
}

TEST_CASE("FileChangeType.FromString.Invalid", "[Snapshot]") {
    REQUIRE_THROWS_AS(file_change_type_from_string("invalid"), std::invalid_argument);
}

// ==================== FileChange Tests ====================

TEST_CASE("FileChange.ToJson", "[Snapshot]") {
    FileChange change;
    change.path = "/path/to/file.cpp";
    change.type = FileChangeType::Modified;
    change.hash = "abc123";
    change.old_content = "old content";
    change.new_content = "new content";
    
    auto j = change.to_json();
    REQUIRE(j["path"] == "/path/to/file.cpp");
    REQUIRE(j["type"] == "modified");
    REQUIRE(j["hash"] == "abc123");
    REQUIRE(j["old_content"] == "old content");
    REQUIRE(j["new_content"] == "new content");
}

TEST_CASE("FileChange.ToJson.WithoutContent", "[Snapshot]") {
    FileChange change;
    change.path = "/path/to/file.h";
    change.type = FileChangeType::Created;
    change.hash = "def456";
    
    auto j = change.to_json();
    REQUIRE(j["path"] == "/path/to/file.h");
    REQUIRE(j["type"] == "created");
    REQUIRE_FALSE(j.contains("old_content"));
    REQUIRE_FALSE(j.contains("new_content"));
}

TEST_CASE("FileChange.FromJson", "[Snapshot]") {
    nlohmann::json j = {
        {"path", "/test/file.txt"},
        {"type", "deleted"},
        {"hash", "xyz789"},
        {"old_content", "deleted content"}
    };
    
    auto change = FileChange::from_json(j);
    REQUIRE(change.path == "/test/file.txt");
    REQUIRE(change.type == FileChangeType::Deleted);
    REQUIRE(change.hash == "xyz789");
    REQUIRE(change.old_content == "deleted content");
    REQUIRE(change.new_content.empty());
}

TEST_CASE("FileChange.RoundTrip", "[Snapshot]") {
    FileChange original;
    original.path = "/round/trip.cpp";
    original.type = FileChangeType::Modified;
    original.hash = "hash123";
    original.old_content = "old";
    original.new_content = "new";
    
    auto j = original.to_json();
    auto restored = FileChange::from_json(j);
    
    REQUIRE(restored.path == original.path);
    REQUIRE(restored.type == original.type);
    REQUIRE(restored.hash == original.hash);
    REQUIRE(restored.old_content == original.old_content);
    REQUIRE(restored.new_content == original.new_content);
}

// ==================== PatchResult Tests ====================

TEST_CASE("PatchResult.ToJson", "[Snapshot]") {
    PatchResult result;
    result.id = "patch-123";
    result.snapshot_id = "snap-456";
    result.created_at = std::chrono::system_clock::now();
    
    FileChange change;
    change.path = "/changed/file.cpp";
    change.type = FileChangeType::Modified;
    change.hash = "hash";
    result.files.push_back(change);
    
    auto j = result.to_json();
    REQUIRE(j["id"] == "patch-123");
    REQUIRE(j["snapshot_id"] == "snap-456");
    REQUIRE(j["files"].size() == 1);
}

TEST_CASE("PatchResult.FromJson", "[Snapshot]") {
    nlohmann::json j = {
        {"id", "patch-abc"},
        {"snapshot_id", "snap-def"},
        {"created_at", 1234567890000},
        {"files", nlohmann::json::array({
            {{"path", "/a.cpp"}, {"type", "created"}, {"hash", "h1"}},
            {{"path", "/b.cpp"}, {"type", "modified"}, {"hash", "h2"}}
        })}
    };
    
    auto result = PatchResult::from_json(j);
    REQUIRE(result.id == "patch-abc");
    REQUIRE(result.snapshot_id == "snap-def");
    REQUIRE(result.files.size() == 2);
    REQUIRE(result.files[0].type == FileChangeType::Created);
    REQUIRE(result.files[1].type == FileChangeType::Modified);
}

// ==================== SnapshotOptions Tests ====================

TEST_CASE("SnapshotOptions.DefaultExcludePatterns", "[Snapshot]") {
    auto patterns = SnapshotOptions::default_exclude_patterns();
    
    REQUIRE_FALSE(patterns.empty());
    // Should exclude common patterns
    REQUIRE(std::find(patterns.begin(), patterns.end(), ".git") != patterns.end());
    REQUIRE(std::find(patterns.begin(), patterns.end(), "node_modules") != patterns.end());
    REQUIRE(std::find(patterns.begin(), patterns.end(), "build") != patterns.end());
}

// ==================== Snapshot Basic Tests ====================

TEST_CASE("SnapshotManager.StartTracking", "[Snapshot]") {
    // Create temp directory
    std::string dir = "/tmp/turbot_snapshot_test_" + std::to_string(std::time(nullptr));
    fs::create_directory(dir);
    
    auto& manager = SnapshotManager::instance();
    manager.clear();
    
    SnapshotOptions options;
    options.root_directory = dir;
    
    auto snap_id = manager.start_tracking(options);
    REQUIRE_FALSE(snap_id.empty());
    REQUIRE(manager.is_tracking(snap_id));
    
    manager.clear();
    fs::remove_all(dir);
}

TEST_CASE("SnapshotManager.StopTracking", "[Snapshot]") {
    // Create temp directory with files
    std::string dir = "/tmp/turbot_snapshot_files_" + std::to_string(std::time(nullptr));
    fs::create_directory(dir);
    std::ofstream(dir + "/test.txt") << "Hello World";
    std::ofstream(dir + "/test.cpp") << "int main() {}";
    
    auto& manager = SnapshotManager::instance();
    manager.clear();
    
    SnapshotOptions options;
    options.root_directory = dir;
    
    auto snap_id = manager.start_tracking(options);
    
    // Modify files
    std::ofstream(dir + "/test.txt") << "Modified content";
    
    auto patch = manager.stop_tracking(snap_id);
    REQUIRE_FALSE(patch.empty());
    
    // Cleanup
    fs::remove_all(dir);
}

TEST_CASE("SnapshotManager.CancelTracking", "[Snapshot]") {
    std::string dir = "/tmp/turbot_snapshot_cancel_" + std::to_string(std::time(nullptr));
    fs::create_directory(dir);
    
    auto& manager = SnapshotManager::instance();
    manager.clear();
    
    SnapshotOptions options;
    options.root_directory = dir;
    
    auto snap_id = manager.start_tracking(options);
    REQUIRE(manager.is_tracking(snap_id));
    
    manager.cancel_tracking(snap_id);
    REQUIRE_FALSE(manager.is_tracking(snap_id));
    
    fs::remove_all(dir);
}

TEST_CASE("SnapshotManager.ActiveTrackingCount", "[Snapshot]") {
    std::string dir = "/tmp/turbot_snapshot_count_" + std::to_string(std::time(nullptr));
    fs::create_directory(dir);
    
    auto& manager = SnapshotManager::instance();
    manager.clear();
    
    REQUIRE(manager.active_tracking_count() == 0);
    
    SnapshotOptions options;
    options.root_directory = dir;
    
    auto id1 = manager.start_tracking(options);
    REQUIRE(manager.active_tracking_count() == 1);
    
    auto id2 = manager.start_tracking(options);
    REQUIRE(manager.active_tracking_count() == 2);
    
    manager.cancel_tracking(id1);
    REQUIRE(manager.active_tracking_count() == 1);
    
    manager.clear();
    REQUIRE(manager.active_tracking_count() == 0);
    
    fs::remove_all(dir);
}

TEST_CASE("SnapshotManager.Clear", "[Snapshot]") {
    std::string dir = "/tmp/turbot_snapshot_clear_" + std::to_string(std::time(nullptr));
    fs::create_directory(dir);
    
    auto& manager = SnapshotManager::instance();
    
    SnapshotOptions options;
    options.root_directory = dir;
    
    manager.start_tracking(options);
    manager.start_tracking(options);
    manager.start_tracking(options);
    
    REQUIRE(manager.active_tracking_count() == 3);
    
    manager.clear();
    REQUIRE(manager.active_tracking_count() == 0);
    
    fs::remove_all(dir);
}

TEST_CASE("SnapshotManager.WithExcludePatterns", "[Snapshot]") {
    std::string dir = "/tmp/turbot_snapshot_exclude_" + std::to_string(std::time(nullptr));
    fs::create_directory(dir);
    fs::create_directory(dir + "/.git");
    fs::create_directory(dir + "/src");
    std::ofstream(dir + "/.git/config") << "git config";
    std::ofstream(dir + "/src/main.cpp") << "int main() {}";
    
    auto& manager = SnapshotManager::instance();
    manager.clear();
    
    SnapshotOptions options;
    options.root_directory = dir;
    options.exclude_patterns = {".git", "*.o"};
    
    auto snap_id = manager.start_tracking(options);
    REQUIRE(manager.is_tracking(snap_id));
    
    manager.clear();
    fs::remove_all(dir);
}

TEST_CASE("SnapshotManager.ApplyPatch", "[Snapshot]") {
    std::string dir = "/tmp/turbot_snapshot_patch_" + std::to_string(std::time(nullptr));
    fs::create_directory(dir);
    
    auto& manager = SnapshotManager::instance();
    manager.clear();
    
    // Create a patch manually
    PatchResult patch;
    patch.id = "patch-test";
    patch.snapshot_id = "snap-test";
    patch.created_at = std::chrono::system_clock::now();
    
    FileChange change;
    change.path = dir + "/new_file.txt";
    change.type = FileChangeType::Created;
    change.new_content = "Hello World";
    change.hash = "test-hash";
    patch.files.push_back(change);
    
    bool result = manager.apply_patch(patch);
    REQUIRE(result);
    REQUIRE(fs::exists(dir + "/new_file.txt"));
    
    manager.clear();
    fs::remove_all(dir);
}

TEST_CASE("SnapshotManager.RollbackPatch", "[Snapshot]") {
    std::string dir = "/tmp/turbot_snapshot_rollback_" + std::to_string(std::time(nullptr));
    fs::create_directory(dir);
    
    auto& manager = SnapshotManager::instance();
    manager.clear();
    
    // First create a file via patch
    PatchResult patch;
    patch.id = "patch-rollback";
    patch.snapshot_id = "snap-rollback";
    patch.created_at = std::chrono::system_clock::now();
    
    FileChange change;
    change.path = dir + "/to_rollback.txt";
    change.type = FileChangeType::Created;
    change.new_content = "Content to rollback";
    change.hash = "hash";
    patch.files.push_back(change);
    
    manager.apply_patch(patch);
    REQUIRE(fs::exists(dir + "/to_rollback.txt"));
    
    // Rollback
    bool result = manager.rollback_patch(patch);
    REQUIRE(result);
    REQUIRE_FALSE(fs::exists(dir + "/to_rollback.txt"));
    
    manager.clear();
    fs::remove_all(dir);
}
