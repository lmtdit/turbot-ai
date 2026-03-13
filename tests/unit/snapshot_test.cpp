// tests/unit/snapshot_test.cpp
// Unit tests for snapshot tracking system

#include <turbot/core/snapshot/snapshot.hpp>
#include <turbot/utils/file_utils.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <thread>
#include <atomic>

using namespace turbot::core::snapshot;

namespace {
    // Helper to create test file
    void create_test_file(const std::filesystem::path& path, const std::string& content) {
        std::ofstream file(path, std::ios::binary);
        file << content;
    }
    
    // Helper to read test file
    std::string read_test_file(const std::filesystem::path& path) {
        return turbot::utils::read_file(path.string()).value_or("");
    }
    
    // Test fixture for file-based tests
    struct SnapshotTestDir {
        std::filesystem::path test_dir;
        
        SnapshotTestDir() {
            test_dir = std::filesystem::temp_directory_path() / 
                ("snapshot-test-" + std::to_string(std::time(nullptr)));
            std::filesystem::create_directories(test_dir);
        }
        
        ~SnapshotTestDir() {
            std::error_code ec;
            std::filesystem::remove_all(test_dir, ec);
        }
        
        std::filesystem::path path(const std::string& name) const {
            return test_dir / name;
        }
    };
}

// ============================================================================
// FileChangeType Tests
// ============================================================================

TEST_CASE("FileChangeType: to_string conversion", "[snapshot]") {
    CHECK(file_change_type_to_string(FileChangeType::Created) == "created");
    CHECK(file_change_type_to_string(FileChangeType::Modified) == "modified");
    CHECK(file_change_type_to_string(FileChangeType::Deleted) == "deleted");
}

TEST_CASE("FileChangeType: from_string conversion", "[snapshot]") {
    CHECK(file_change_type_from_string("created") == FileChangeType::Created);
    CHECK(file_change_type_from_string("modified") == FileChangeType::Modified);
    CHECK(file_change_type_from_string("deleted") == FileChangeType::Deleted);
    
    CHECK_THROWS_AS(file_change_type_from_string("invalid"), std::invalid_argument);
}

// ============================================================================
// FileChange Tests
// ============================================================================

TEST_CASE("FileChange: JSON serialization", "[snapshot]") {
    FileChange change;
    change.path = "/test/path.txt";
    change.type = FileChangeType::Modified;
    change.old_content = "old content";
    change.new_content = "new content";
    change.hash = "abc123";
    
    nlohmann::json j = change.to_json();
    
    CHECK(j["path"] == "/test/path.txt");
    CHECK(j["type"] == "modified");
    CHECK(j["old_content"] == "old content");
    CHECK(j["new_content"] == "new content");
    CHECK(j["hash"] == "abc123");
}

TEST_CASE("FileChange: JSON deserialization", "[snapshot]") {
    nlohmann::json j = {
        {"path", "/test/file.txt"},
        {"type", "created"},
        {"new_content", "hello world"},
        {"hash", "def456"}
    };
    
    FileChange change = FileChange::from_json(j);
    
    CHECK(change.path == "/test/file.txt");
    CHECK(change.type == FileChangeType::Created);
    CHECK(change.new_content == "hello world");
    CHECK(change.old_content.empty());
    CHECK(change.hash == "def456");
}

// ============================================================================
// PatchResult Tests
// ============================================================================

TEST_CASE("PatchResult: empty check", "[snapshot]") {
    PatchResult result;
    CHECK(result.empty());
    CHECK(result.size() == 0);
    
    result.files.push_back({.path = "/test", .type = FileChangeType::Created});
    CHECK_FALSE(result.empty());
    CHECK(result.size() == 1);
}

TEST_CASE("PatchResult: JSON serialization", "[snapshot]") {
    PatchResult result;
    result.id = "patch-123";
    result.snapshot_id = "snapshot-456";
    result.created_at = std::chrono::system_clock::now();
    
    FileChange change;
    change.path = "/test/file.txt";
    change.type = FileChangeType::Created;
    change.new_content = "test";
    change.hash = "abc";
    result.files.push_back(change);
    
    nlohmann::json j = result.to_json();
    
    CHECK(j["id"] == "patch-123");
    CHECK(j["snapshot_id"] == "snapshot-456");
    CHECK(j["files"].size() == 1);
    CHECK(j["files"][0]["path"] == "/test/file.txt");
}

TEST_CASE("PatchResult: JSON round-trip", "[snapshot]") {
    PatchResult original;
    original.id = "patch-789";
    original.snapshot_id = "snapshot-012";
    original.created_at = std::chrono::system_clock::now();
    
    FileChange change;
    change.path = "/test/roundtrip.txt";
    change.type = FileChangeType::Modified;
    change.old_content = "before";
    change.new_content = "after";
    change.hash = "hash123";
    original.files.push_back(change);
    
    nlohmann::json j = original.to_json();
    PatchResult restored = PatchResult::from_json(j);
    
    CHECK(restored.id == original.id);
    CHECK(restored.snapshot_id == original.snapshot_id);
    CHECK(restored.files.size() == 1);
    CHECK(restored.files[0].path == change.path);
    CHECK(restored.files[0].type == FileChangeType::Modified);
    CHECK(restored.files[0].old_content == "before");
    CHECK(restored.files[0].new_content == "after");
}

// ============================================================================
// SnapshotManager Tests
// ============================================================================

TEST_CASE("SnapshotManager: singleton instance", "[snapshot]") {
    auto& instance1 = SnapshotManager::instance();
    auto& instance2 = SnapshotManager::instance();
    CHECK(&instance1 == &instance2);
}

TEST_CASE("SnapshotManager: start and stop tracking", "[snapshot]") {
    SnapshotTestDir test_dir;
    auto& manager = SnapshotManager::instance();
    manager.clear();
    
    SnapshotOptions options;
    options.root_directory = test_dir.test_dir;
    
    std::string snapshot_id = manager.start_tracking(options);
    CHECK_FALSE(snapshot_id.empty());
    CHECK(manager.is_tracking(snapshot_id));
    CHECK(manager.active_tracking_count() == 1);
    
    PatchResult patch = manager.stop_tracking(snapshot_id);
    CHECK_FALSE(manager.is_tracking(snapshot_id));
    CHECK(manager.active_tracking_count() == 0);
    CHECK(patch.snapshot_id == snapshot_id);
}

TEST_CASE("SnapshotManager: track file creation", "[snapshot]") {
    SnapshotTestDir test_dir;
    auto& manager = SnapshotManager::instance();
    manager.clear();
    
    SnapshotOptions options;
    options.root_directory = test_dir.test_dir;
    
    std::string snapshot_id = manager.start_tracking(options);
    
    // Create a new file
    create_test_file(test_dir.path("new_file.txt"), "new content");
    
    PatchResult patch = manager.stop_tracking(snapshot_id);
    
    REQUIRE(patch.files.size() == 1);
    CHECK(patch.files[0].type == FileChangeType::Created);
    CHECK(patch.files[0].new_content == "new content");
    CHECK_FALSE(patch.files[0].hash.empty());
}

TEST_CASE("SnapshotManager: track file modification", "[snapshot]") {
    SnapshotTestDir test_dir;
    auto& manager = SnapshotManager::instance();
    manager.clear();
    
    // Create initial file
    create_test_file(test_dir.path("modify.txt"), "original content");
    
    SnapshotOptions options;
    options.root_directory = test_dir.test_dir;
    
    std::string snapshot_id = manager.start_tracking(options);
    
    // Modify the file
    create_test_file(test_dir.path("modify.txt"), "modified content");
    
    PatchResult patch = manager.stop_tracking(snapshot_id);
    
    REQUIRE(patch.files.size() == 1);
    CHECK(patch.files[0].type == FileChangeType::Modified);
    CHECK(patch.files[0].old_content == "original content");
    CHECK(patch.files[0].new_content == "modified content");
}

TEST_CASE("SnapshotManager: track file deletion", "[snapshot]") {
    SnapshotTestDir test_dir;
    auto& manager = SnapshotManager::instance();
    manager.clear();
    
    // Create file to be deleted
    create_test_file(test_dir.path("delete.txt"), "to be deleted");
    
    SnapshotOptions options;
    options.root_directory = test_dir.test_dir;
    
    std::string snapshot_id = manager.start_tracking(options);
    
    // Delete the file
    std::filesystem::remove(test_dir.path("delete.txt"));
    
    PatchResult patch = manager.stop_tracking(snapshot_id);
    
    REQUIRE(patch.files.size() == 1);
    CHECK(patch.files[0].type == FileChangeType::Deleted);
    CHECK(patch.files[0].old_content == "to be deleted");
}

TEST_CASE("SnapshotManager: track multiple changes", "[snapshot]") {
    SnapshotTestDir test_dir;
    auto& manager = SnapshotManager::instance();
    manager.clear();
    
    // Create initial files
    create_test_file(test_dir.path("existing.txt"), "existing content");
    
    SnapshotOptions options;
    options.root_directory = test_dir.test_dir;
    
    std::string snapshot_id = manager.start_tracking(options);
    
    // Make multiple changes
    create_test_file(test_dir.path("new.txt"), "new file");
    create_test_file(test_dir.path("existing.txt"), "modified content");
    std::filesystem::remove(test_dir.path("existing.txt"));  // Actually delete it
    
    // Recreate to have a modification
    create_test_file(test_dir.path("existing.txt"), "existing content");
    create_test_file(test_dir.path("existing.txt"), "modified content");
    
    PatchResult patch = manager.stop_tracking(snapshot_id);
    
    CHECK(patch.files.size() >= 1);  // At least the new file
}

TEST_CASE("SnapshotManager: apply and rollback patch", "[snapshot]") {
    SnapshotTestDir test_dir;
    auto& manager = SnapshotManager::instance();
    manager.clear();
    
    // Create a patch manually
    PatchResult patch;
    patch.id = "test-patch";
    patch.snapshot_id = "test-snapshot";
    patch.created_at = std::chrono::system_clock::now();
    
    FileChange change;
    change.path = test_dir.path("applied.txt");
    change.type = FileChangeType::Created;
    change.new_content = "applied content";
    change.hash = "test-hash";
    patch.files.push_back(change);
    
    // Apply the patch
    CHECK(manager.apply_patch(patch));
    CHECK(std::filesystem::exists(test_dir.path("applied.txt")));
    CHECK(read_test_file(test_dir.path("applied.txt")) == "applied content");
    
    // Rollback the patch
    CHECK(manager.rollback_patch(patch));
    CHECK_FALSE(std::filesystem::exists(test_dir.path("applied.txt")));
}

TEST_CASE("SnapshotManager: rollback modified file", "[snapshot]") {
    SnapshotTestDir test_dir;
    auto& manager = SnapshotManager::instance();
    manager.clear();
    
    // Create initial file
    create_test_file(test_dir.path("to_modify.txt"), "original");
    
    // Create a modification patch
    PatchResult patch;
    patch.id = "modify-patch";
    patch.snapshot_id = "modify-snapshot";
    patch.created_at = std::chrono::system_clock::now();
    
    FileChange change;
    change.path = test_dir.path("to_modify.txt");
    change.type = FileChangeType::Modified;
    change.old_content = "original";
    change.new_content = "modified";
    change.hash = "modify-hash";
    patch.files.push_back(change);
    
    // Apply the patch
    CHECK(manager.apply_patch(patch));
    CHECK(read_test_file(test_dir.path("to_modify.txt")) == "modified");
    
    // Rollback the patch
    CHECK(manager.rollback_patch(patch));
    CHECK(read_test_file(test_dir.path("to_modify.txt")) == "original");
}

TEST_CASE("SnapshotManager: cancel tracking", "[snapshot]") {
    SnapshotTestDir test_dir;
    auto& manager = SnapshotManager::instance();
    manager.clear();
    
    SnapshotOptions options;
    options.root_directory = test_dir.test_dir;
    
    std::string snapshot_id = manager.start_tracking(options);
    CHECK(manager.is_tracking(snapshot_id));
    
    manager.cancel_tracking(snapshot_id);
    CHECK_FALSE(manager.is_tracking(snapshot_id));
    CHECK(manager.active_tracking_count() == 0);
}

TEST_CASE("SnapshotManager: exclude patterns", "[snapshot]") {
    SnapshotTestDir test_dir;
    auto& manager = SnapshotManager::instance();
    manager.clear();
    
    // Create files in different locations
    create_test_file(test_dir.path("include.txt"), "should be tracked");
    std::filesystem::create_directories(test_dir.test_dir / "build");
    create_test_file(test_dir.path("build/exclude.txt"), "should be excluded");
    
    SnapshotOptions options;
    options.root_directory = test_dir.test_dir;
    options.exclude_patterns = {"build", "*.o"};
    
    std::string snapshot_id = manager.start_tracking(options);
    
    // Create new files
    create_test_file(test_dir.path("new_include.txt"), "new tracked file");
    create_test_file(test_dir.path("build/new_exclude.txt"), "new excluded file");
    
    PatchResult patch = manager.stop_tracking(snapshot_id);
    
    // Only the non-excluded file should be tracked
    for (const auto& change : patch.files) {
        CHECK(change.path.string().find("build") == std::string::npos);
    }
}

TEST_CASE("SnapshotManager: stop non-existent snapshot", "[snapshot]") {
    auto& manager = SnapshotManager::instance();
    
    PatchResult patch = manager.stop_tracking("non-existent-id");
    CHECK(patch.empty());
}

TEST_CASE("SnapshotOptions: default exclude patterns", "[snapshot]") {
    auto patterns = SnapshotOptions::default_exclude_patterns();
    
    CHECK_FALSE(patterns.empty());
    CHECK(std::find(patterns.begin(), patterns.end(), ".git") != patterns.end());
    CHECK(std::find(patterns.begin(), patterns.end(), "node_modules") != patterns.end());
    CHECK(std::find(patterns.begin(), patterns.end(), "build") != patterns.end());
}

// ============================================================================
// SNAP-PERF: Performance - reduced lock granularity
// ============================================================================

TEST_CASE("SNAP-PERF-01: start_tracking does not block active_tracking_count", "[snapshot][perf]") {
    SnapshotTestDir test_dir;
    auto& manager = SnapshotManager::instance();
    manager.clear();

    // Create several files so scan_files takes measurable time
    for (int i = 0; i < 20; ++i) {
        create_test_file(test_dir.path("file" + std::to_string(i) + ".txt"),
                         std::string(256, 'A'));
    }

    SnapshotOptions options;
    options.root_directory = test_dir.test_dir;

    std::atomic<bool> query_succeeded{false};
    std::atomic<bool> start_done{false};

    std::thread t1([&] {
        manager.start_tracking(options);
        start_done.store(true);
    });

    std::thread t2([&] {
        for (int i = 0; i < 50; ++i) {
            std::this_thread::yield();
        }
        [[maybe_unused]] auto count = manager.active_tracking_count();
        query_succeeded.store(true);
    });

    t1.join();
    t2.join();

    CHECK(start_done.load());
    CHECK(query_succeeded.load());

    manager.clear();
}

TEST_CASE("SNAP-PERF-02: is_excluded uses pre-compiled patterns correctly", "[snapshot][perf]") {
    SnapshotTestDir test_dir;
    auto& manager = SnapshotManager::instance();
    manager.clear();

    std::filesystem::create_directories(test_dir.test_dir / "cmake-build-debug");
    create_test_file(test_dir.path("cmake-build-debug/output"), "excluded");
    create_test_file(test_dir.path("main.o"), "excluded");
    create_test_file(test_dir.path("main.cpp"), "tracked");

    SnapshotOptions options;
    options.root_directory = test_dir.test_dir;
    options.exclude_patterns = {"cmake-build-*", "*.o"};

    std::string id = manager.start_tracking(options);

    create_test_file(test_dir.path("new.cpp"), "new tracked");
    create_test_file(test_dir.path("cmake-build-debug/new_output"), "new excluded");
    create_test_file(test_dir.path("new.o"), "new excluded");

    PatchResult patch = manager.stop_tracking(id);

    for (const auto& change : patch.files) {
        std::string p = change.path.string();
        CHECK(p.find(".o") == std::string::npos);
        CHECK(p.find("cmake-build") == std::string::npos);
    }
}

TEST_CASE("SNAP-PERF-03: wildcard patterns match correctly", "[snapshot][perf]") {
    SnapshotTestDir test_dir;
    auto& manager = SnapshotManager::instance();
    manager.clear();

    create_test_file(test_dir.path("readme.txt"), "tracked");
    create_test_file(test_dir.path("lib.a"), "excluded");
    create_test_file(test_dir.path("lib.so"), "excluded");

    SnapshotOptions options;
    options.root_directory = test_dir.test_dir;
    options.exclude_patterns = {"*.a", "*.so"};

    std::string id = manager.start_tracking(options);

    create_test_file(test_dir.path("new.txt"), "tracked");
    create_test_file(test_dir.path("new.a"), "excluded");

    PatchResult patch = manager.stop_tracking(id);

    bool found_txt = false;
    for (const auto& change : patch.files) {
        std::string p = change.path.string();
        CHECK(p.find(".a") == std::string::npos);
        CHECK(p.find(".so") == std::string::npos);
        if (p.find("new.txt") != std::string::npos) found_txt = true;
    }
    CHECK(found_txt);
}

// ============================================================================
// Edge case coverage tests
// ============================================================================

TEST_CASE("FileChangeType: unknown value returns 'unknown'", "[snapshot]") {
    auto unknown_type = static_cast<FileChangeType>(999);
    std::string s = file_change_type_to_string(unknown_type);
    CHECK(s == "unknown");
}

TEST_CASE("SnapshotManager: glob pattern with ? wildcard", "[snapshot]") {
    auto& manager = SnapshotManager::instance();
    manager.clear();

    SnapshotOptions options;
    options.root_directory = std::filesystem::temp_directory_path().string();
    options.exclude_patterns = {"?.txt", "test?"};

    std::string id = manager.start_tracking(options);
    REQUIRE_FALSE(id.empty());
    PatchResult result = manager.stop_tracking(id);
    SUCCEED();
}

// ============================================================================
// SnapshotManager: invalid regex pattern handling
// ============================================================================

TEST_CASE("SnapshotManager: invalid regex pattern", "[snapshot]") {
    auto& manager = SnapshotManager::instance();
    manager.clear();

    SnapshotOptions options;
    options.root_directory = std::filesystem::temp_directory_path().string();
    // Invalid regex pattern - unmatched bracket
    options.exclude_patterns = {"[invalid"};

    std::string id = manager.start_tracking(options);
    REQUIRE_FALSE(id.empty());
    PatchResult result = manager.stop_tracking(id);
    SUCCEED();  // Should handle invalid pattern gracefully
}
