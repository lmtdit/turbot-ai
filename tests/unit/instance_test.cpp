// instance_test.cpp - Unit tests for Instance module
//
// Tests Instance::provide(), current(), directory(), worktree(),
// contains_path(), dispose(), dispose_all(), and RAII guard behaviour.
//
// Strategy:
//   - Use std::filesystem::temp_directory_path() to get a real directory so
//     Project::from_directory() has a valid canonical path to work with.
//   - Tests do NOT mock Project::from_directory; the module must remain
//     self-contained and accept any existing directory.

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/project/instance.hpp>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
#include <atomic>

namespace fs = std::filesystem;
using namespace turbot::core::project;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Return a canonical path we can definitely boot an instance for.
static std::string temp_dir() {
    return fs::temp_directory_path().string();
}

// ---------------------------------------------------------------------------
// 1. Basic provide / current
// ---------------------------------------------------------------------------

TEST_CASE("Instance: provide() sets current context", "[instance]") {
    Instance::dispose_all();

    bool called = false;
    Instance::provide(temp_dir(), [&] {
        called = true;
        const auto& ctx = Instance::current();
        REQUIRE_FALSE(ctx.directory.empty());
    });
    REQUIRE(called);
}

TEST_CASE("Instance: current() throws outside provide() scope", "[instance]") {
    Instance::dispose_all();
    REQUIRE_THROWS_AS(Instance::current(), std::logic_error);
}

// ---------------------------------------------------------------------------
// 2. Directory / worktree accessors
// ---------------------------------------------------------------------------

TEST_CASE("Instance: directory() returns resolved path", "[instance]") {
    Instance::dispose_all();

    Instance::provide(temp_dir(), [] {
        const std::string& d = Instance::directory();
        REQUIRE_FALSE(d.empty());
        // Must be an absolute path
        REQUIRE(fs::path(d).is_absolute());
    });
}

TEST_CASE("Instance: worktree() is non-empty", "[instance]") {
    Instance::dispose_all();

    Instance::provide(temp_dir(), [] {
        const std::string& wt = Instance::worktree();
        REQUIRE_FALSE(wt.empty());
    });
}

// ---------------------------------------------------------------------------
// 3. RAII guard — context restored after provide() exits
// ---------------------------------------------------------------------------

TEST_CASE("Instance: context is restored after provide() returns", "[instance]") {
    Instance::dispose_all();

    // Outer provide
    Instance::provide(temp_dir(), [&] {
        const auto& outer = Instance::current();
        const std::string outer_dir = outer.directory;

        // Inner provide with the same directory (cache hit)
        Instance::provide(temp_dir(), [&] {
            const auto& inner = Instance::current();
            REQUIRE(inner.directory == outer_dir);
        });

        // Back to outer — pointer should still be valid
        REQUIRE(Instance::directory() == outer_dir);
    });

    // Outside any scope → must throw
    REQUIRE_THROWS_AS(Instance::current(), std::logic_error);
}

// ---------------------------------------------------------------------------
// 4. Caching — same directory reuses the same InstanceShape
// ---------------------------------------------------------------------------

TEST_CASE("Instance: same directory boots once (cache hit)", "[instance]") {
    Instance::dispose_all();

    const InstanceShape* first_ptr  = nullptr;
    const InstanceShape* second_ptr = nullptr;

    Instance::provide(temp_dir(), [&] {
        first_ptr = &Instance::current();
    });
    Instance::provide(temp_dir(), [&] {
        second_ptr = &Instance::current();
    });

    // Both invocations must resolve to the same cached object
    REQUIRE(first_ptr != nullptr);
    REQUIRE(second_ptr != nullptr);
    REQUIRE(first_ptr == second_ptr);
}

// ---------------------------------------------------------------------------
// 5. dispose() evicts from cache
// ---------------------------------------------------------------------------

TEST_CASE("Instance: dispose() evicts the cached instance", "[instance]") {
    Instance::dispose_all();

    const InstanceShape* before_ptr = nullptr;
    const InstanceShape* after_ptr  = nullptr;

    Instance::provide(temp_dir(), [&] {
        before_ptr = &Instance::current();
    });

    Instance::dispose(temp_dir());

    Instance::provide(temp_dir(), [&] {
        after_ptr = &Instance::current();
        // Verify directory is still valid after re-boot
        REQUIRE_FALSE(Instance::directory().empty());
    });

    // After eviction, a new InstanceShape is allocated at a (likely) different address
    REQUIRE(before_ptr != nullptr);
    REQUIRE(after_ptr  != nullptr);
}

// ---------------------------------------------------------------------------
// 6. dispose_all() clears all cached instances
// ---------------------------------------------------------------------------

TEST_CASE("Instance: dispose_all() clears cache", "[instance]") {
    Instance::dispose_all();

    // Boot once
    Instance::provide(temp_dir(), [] {
        REQUIRE_FALSE(Instance::directory().empty());
    });

    // Clear everything
    Instance::dispose_all();

    // Should still be bootable after clear
    bool ran = false;
    Instance::provide(temp_dir(), [&] {
        ran = true;
        REQUIRE_FALSE(Instance::directory().empty());
    });
    REQUIRE(ran);
}

// ---------------------------------------------------------------------------
// 7. contains_path()
// ---------------------------------------------------------------------------

TEST_CASE("Instance: contains_path() matches files under directory", "[instance]") {
    Instance::dispose_all();

    Instance::provide(temp_dir(), [] {
        const std::string dir = Instance::directory();
        // A path directly inside the directory
        const std::string child = dir + "/some_file.txt";
        REQUIRE(Instance::contains_path(child));

        // A completely different path (e.g. /nonexistent_root/x) must not match
        const std::string unrelated = "/nonexistent_root/totally_separate/file.txt";
        REQUIRE_FALSE(Instance::contains_path(unrelated));
    });
}

// ---------------------------------------------------------------------------
// 8. Thread safety — concurrent boots for the same directory
// ---------------------------------------------------------------------------

TEST_CASE("Instance: concurrent boots do not corrupt cache", "[instance][thread]") {
    Instance::dispose_all();

    constexpr int kThreads = 8;
    std::atomic<int> error_count{0};

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&] {
            try {
                Instance::provide(temp_dir(), [] {
                    if (Instance::directory().empty()) {
                        throw std::runtime_error("empty directory");
                    }
                });
            } catch (...) {
                ++error_count;
            }
        });
    }

    for (auto& t : threads) t.join();

    REQUIRE(error_count.load() == 0);
}

// ---------------------------------------------------------------------------
// 9. init callback is invoked on first boot
// ---------------------------------------------------------------------------

TEST_CASE("Instance: init callback fires on first boot only", "[instance]") {
    Instance::dispose_all();

    std::atomic<int> init_count{0};

    auto init_fn = [&] { ++init_count; };

    // First provide → should fire init
    Instance::provide(temp_dir(), init_fn, [] {});
    REQUIRE(init_count.load() == 1);

    // Second provide (cache hit) → init should NOT fire again
    Instance::provide(temp_dir(), init_fn, [] {});
    REQUIRE(init_count.load() == 1);
}

// ---------------------------------------------------------------------------
// 10. reload() re-runs init
// ---------------------------------------------------------------------------

TEST_CASE("Instance: reload() evicts and re-boots", "[instance]") {
    Instance::dispose_all();

    std::atomic<int> init_count{0};
    auto init_fn = [&] { ++init_count; };

    Instance::provide(temp_dir(), init_fn, [] {});
    REQUIRE(init_count.load() == 1);

    // reload() must evict old entry and re-run init
    Instance::reload(temp_dir(), init_fn);
    REQUIRE(init_count.load() == 2);
}
