// instance.cpp - Project instance context implementation
//
// Mirrors: opencode/packages/opencode/src/project/instance.ts
//
// Design:
//   - A global cache (map<directory, InstanceShape>) holds booted instances.
//   - A thread_local pointer tracks the "current" instance for the call stack.
//   - boot()     -> loads-or-returns cached InstanceShape
//   - push/pop   -> scopes the thread_local pointer (RAII in the template header)
//   - dispose()  -> evicts from cache
//   - dispose_all() -> clears cache

#include <turbot/core/project/instance.hpp>
#include <filesystem>
#include <map>
#include <mutex>
#include <stdexcept>

namespace turbot::core::project {

namespace {

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Global cache: resolved_directory -> InstanceShape
// ---------------------------------------------------------------------------
std::mutex                           g_cache_mutex;
std::map<std::string, InstanceShape> g_cache;

// ---------------------------------------------------------------------------
// Thread-local context pointer (mirrors Effect AsyncLocalStorage)
// ---------------------------------------------------------------------------
thread_local const InstanceShape* t_current = nullptr;

// ---------------------------------------------------------------------------
// Path helpers
// ---------------------------------------------------------------------------

std::string resolve_dir(const std::string& directory) {
    std::error_code ec;
    auto canonical = fs::canonical(directory, ec);
    if (ec) return fs::absolute(directory).string();
    return canonical.string();
}

bool path_contains(const std::string& parent, const std::string& child) {
    auto p = fs::path(parent).lexically_normal();
    auto c = fs::path(child).lexically_normal();
    auto [pi, ci] = std::mismatch(p.begin(), p.end(), c.begin(), c.end());
    return pi == p.end();
}

// ---------------------------------------------------------------------------
// Build an InstanceShape from a directory
// Called while holding g_cache_mutex (so Project::from_directory must not re-acquire it)
// ---------------------------------------------------------------------------
InstanceShape make_shape(const std::string& resolved_dir, std::function<void()> init_fn) {
    // LoadResult is defined in the project namespace, not as Project::LoadResult
    LoadResult result = Project::from_directory(resolved_dir);

    InstanceShape shape;
    shape.directory = resolved_dir;
    shape.worktree  = result.sandbox.empty() ? resolved_dir : result.sandbox;
    shape.project   = result.project;

    if (init_fn) init_fn();

    return shape;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Private static helpers (called from the template in the header)
// ---------------------------------------------------------------------------

const InstanceShape& Instance::boot(const std::string& directory,
                                     std::function<void()> init) {
    const std::string resolved = resolve_dir(directory);

    // Fast path: already cached
    {
        std::lock_guard<std::mutex> lock(g_cache_mutex);
        auto it = g_cache.find(resolved);
        if (it != g_cache.end()) {
            return it->second;
        }
    }

    // Slow path: build the shape OUTSIDE the lock to avoid:
    //   1. Holding the lock during slow I/O (Project::from_directory → SQLite + fs)
    //   2. Leaving a partially-constructed InstanceShape in the cache on exception
    //
    // Concurrent boots for the same directory are harmless — the last writer wins
    // and both produce equivalent results.
    InstanceShape shape = make_shape(resolved, std::move(init));

    {
        std::lock_guard<std::mutex> lock(g_cache_mutex);
        // Use try_emplace so the first writer wins if two threads raced here
        auto [it, inserted] = g_cache.try_emplace(resolved, std::move(shape));
        return it->second;
    }
}

void Instance::push(const InstanceShape* shape) {
    t_current = shape;
}

void Instance::pop(const InstanceShape* prev) {
    t_current = prev;
}

const InstanceShape* Instance::current_ptr() {
    return t_current;
}

// ---------------------------------------------------------------------------
// Public accessors
// ---------------------------------------------------------------------------

const InstanceShape& Instance::current() {
    if (!t_current) {
        throw std::logic_error(
            "Instance::current() called outside an Instance::provide() scope");
    }
    return *t_current;
}

const std::string& Instance::directory() { return current().directory; }
const std::string& Instance::worktree()  { return current().worktree; }
const ProjectInfo& Instance::project()   { return current().project; }

bool Instance::contains_path(const std::string& filepath) {
    const auto& ctx = current();
    if (path_contains(ctx.directory, filepath)) return true;
    // Non-git projects set worktree to "/" — skip worktree check in that case
    // (mirrors OpenCode: Instance.worktree === "/" guard in containsPath)
    if (ctx.worktree == "/") return false;
    return path_contains(ctx.worktree, filepath);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void Instance::dispose(const std::string& directory) {
    std::string resolved;
    if (directory.empty()) {
        if (!t_current) return;
        resolved = t_current->directory;
    } else {
        resolved = resolve_dir(directory);
    }
    std::lock_guard<std::mutex> lock(g_cache_mutex);
    g_cache.erase(resolved);
}

void Instance::dispose_all() {
    std::lock_guard<std::mutex> lock(g_cache_mutex);
    g_cache.clear();
}

InstanceShape Instance::reload(const std::string& directory, std::function<void()> init) {
    const std::string resolved = resolve_dir(directory);
    {
        std::lock_guard<std::mutex> lock(g_cache_mutex);
        g_cache.erase(resolved);
    }
    return boot(resolved, std::move(init));
}

} // namespace turbot::core::project
