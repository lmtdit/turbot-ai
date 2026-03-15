#include <catch2/catch_test_macros.hpp>
#include "test_fixture.hpp"
#include "test_macros.hpp"
#include <fstream>

using namespace turbot::test;

// ==================== TmpDir 测试 ====================

TEST_CASE("Fixture.TmpDir.Creation", "[Fixture]") {
    SECTION("CreatesTempDirectory") {
        TURBOT_TEST_TMPDIR(tmp, false);
        REQUIRE(std::filesystem::exists(tmp.path()));
        REQUIRE(std::filesystem::is_directory(tmp.path()));
    }
    
    SECTION("CreatesUniqueDirectories") {
        TURBOT_TEST_TMPDIR(tmp1, false);
        TURBOT_TEST_TMPDIR(tmp2, false);
        REQUIRE(tmp1.path() != tmp2.path());
    }
    
    SECTION("CreatesWithGitInit") {
        TURBOT_TEST_TMPDIR(tmp, true);
        REQUIRE(std::filesystem::exists(tmp.path() / ".git"));
    }
}

TEST_CASE("Fixture.TmpDir.AutoCleanup", "[Fixture]") {
    std::filesystem::path saved_path;
    
    {
        TURBOT_TEST_TMPDIR(tmp, false);
        saved_path = tmp.path();
        REQUIRE(std::filesystem::exists(saved_path));
    }
    
    // 离开作用域后应该自动清理
    REQUIRE_FALSE(std::filesystem::exists(saved_path));
}

TEST_CASE("Fixture.TmpDir.CreateSubdir", "[Fixture]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto subdir = tmp.create_subdir("src");
    REQUIRE(std::filesystem::exists(subdir));
    REQUIRE(std::filesystem::is_directory(subdir));
}

TEST_CASE("Fixture.TmpDir.CreateFile", "[Fixture]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto filepath = tmp.create_file("test.txt", "Hello World");
    REQUIRE(std::filesystem::exists(filepath));
    
    std::ifstream file(filepath);
    std::string content;
    std::getline(file, content);
    REQUIRE(content == "Hello World");
}

TEST_CASE("Fixture.TmpDir.Init", "[Fixture]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    tmp.init([](const std::filesystem::path& p) {
        std::ofstream file(p / "init.txt");
        file << "initialized";
    });
    
    REQUIRE(std::filesystem::exists(tmp.path() / "init.txt"));
}

// ==================== TestContext 测试 ====================

TEST_CASE("Fixture.TestContext.Default", "[Fixture]") {
    auto ctx = TestContext::create_default();
    
    REQUIRE_FALSE(ctx.session_id.empty());
    REQUIRE_FALSE(ctx.message_id.empty());
    REQUIRE(ctx.agent == "build");
}

TEST_CASE("Fixture.TestContext.Ask", "[Fixture]") {
    auto ctx = TestContext::create_default();
    
    REQUIRE(ctx.permission_requests.empty());
    
    ctx.ask({{"permission", "bash"}, {"pattern", "rm -rf /"}});
    REQUIRE(ctx.permission_requests.size() == 1);
    REQUIRE(ctx.permission_requests[0]["permission"] == "bash");
    
    ctx.ask({{"permission", "edit"}, {"pattern", "/etc/passwd"}});
    REQUIRE(ctx.permission_requests.size() == 2);
}

TEST_CASE("Fixture.TestContext.ClearRequests", "[Fixture]") {
    auto ctx = TestContext::create_default();
    
    ctx.ask({{"permission", "bash"}});
    REQUIRE_FALSE(ctx.permission_requests.empty());
    
    ctx.clear_requests();
    REQUIRE(ctx.permission_requests.empty());
}

// ==================== TestInstance 测试 ====================

TEST_CASE("Fixture.TestInstance.Provide", "[Fixture]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    TestInstance::instance().provide(tmp.path());
    REQUIRE(TestInstance::instance().directory() == std::filesystem::weakly_canonical(tmp.path()));
    
    TestInstance::instance().clear();
    REQUIRE(TestInstance::instance().directory().empty());
}

TEST_CASE("Fixture.TestInstance.ContainsPath", "[Fixture]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    TURBOT_TEST_PROVIDE(tmp.path());
    
    SECTION("ContainsSubpath") {
        auto subpath = tmp.path() / "src" / "file.cpp";
        REQUIRE(TestInstance::instance().contains_path(subpath));
    }
    
    SECTION("ContainsSelf") {
        REQUIRE(TestInstance::instance().contains_path(tmp.path()));
    }
    
    SECTION("NotContainsParent") {
        auto parent = tmp.path().parent_path();
        REQUIRE_FALSE(TestInstance::instance().contains_path(parent));
    }
    
    SECTION("NotContainsSibling") {
        auto sibling = tmp.path().parent_path() / "other-project";
        REQUIRE_FALSE(TestInstance::instance().contains_path(sibling));
    }
    
    TestInstance::instance().clear();
}

// ==================== 宏测试 ====================

TEST_CASE("Fixture.Macros.TmpDir", "[Fixture]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    REQUIRE(std::filesystem::exists(tmp.path()));
}

TEST_CASE("Fixture.Macros.Provide", "[Fixture]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    TURBOT_TEST_PROVIDE(tmp.path());
    
    REQUIRE(TestInstance::instance().directory() == std::filesystem::weakly_canonical(tmp.path()));
    
    TestInstance::instance().clear();
}

TEST_CASE("Fixture.Macros.Context", "[Fixture]") {
    auto ctx = TURBOT_TEST_CONTEXT();
    REQUIRE_FALSE(ctx.session_id.empty());
}

TEST_CASE("Fixture.Macros.AssertPermission", "[Fixture]") {
    auto ctx = TURBOT_TEST_CONTEXT();
    ctx.ask({{"permission", "bash"}});
    
    TURBOT_ASSERT_PERMISSION_ASKED(ctx, "bash");
}

TEST_CASE("Fixture.Macros.PathContains", "[Fixture]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    TURBOT_TEST_PROVIDE(tmp.path());
    
    auto inside = tmp.path() / "inside.txt";
    auto outside = tmp.path().parent_path() / "outside.txt";
    
    TURBOT_ASSERT_PATH_CONTAINS(tmp.path(), inside);
    TURBOT_ASSERT_PATH_NOT_CONTAINS(tmp.path(), outside);
    
    TestInstance::instance().clear();
}

// ==================== fs_utils 测试 ====================

TEST_CASE("Fixture.FsUtils.CanonicalPath", "[Fixture]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto canonical = fs_utils::canonical_path(tmp.path());
    REQUIRE(canonical.is_absolute());
}

TEST_CASE("Fixture.FsUtils.IsSubpath", "[Fixture]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto base = tmp.path();
    auto subpath = tmp.path() / "src" / "file.cpp";
    auto not_subpath = tmp.path().parent_path() / "other";
    
    REQUIRE(fs_utils::is_subpath(base, subpath));
    REQUIRE_FALSE(fs_utils::is_subpath(base, not_subpath));
}
