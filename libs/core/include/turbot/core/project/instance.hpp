// instance.hpp - Project instance context
//
// Mirrors: opencode/packages/opencode/src/project/instance.ts (Instance)
//
// Provides the current "instance" context for a running Turbot session:
//   - directory: resolved project working directory
//   - worktree:  sandbox/worktree path (fallback to directory)
//   - project:   loaded ProjectInfo
//
// Usage:
//   Instance::provide("/path/to/project", []{ /* work here */ });
//   Instance::directory()      -> current directory
//   Instance::worktree()       -> current worktree/sandbox
//   Instance::project()        -> current ProjectInfo
//   Instance::contains_path(p) -> true if p is within dir or worktree

#pragma once

#include <turbot/core/project/project.hpp>
#include <functional>
#include <optional>
#include <string>

namespace turbot::core::project {

/// Shape of a resolved instance context
struct InstanceShape {
    std::string  directory;  ///< Resolved absolute working directory
    std::string  worktree;   ///< Sandbox / worktree path (may equal directory)
    ProjectInfo  project;    ///< Loaded project info
};

/// Project instance singleton registry
class Instance {
public:
    // ----- Context Scoped Execution -----

    /// Boot or reuse an instance for `directory`, then call `fn` within its context.
    /// @param directory  Working directory (will be canonicalized)
    /// @param init       Optional init callback (called once on boot)
    /// @param fn         Function to execute within the instance context
    template<typename Fn>
    static auto provide(const std::string& directory,
                        std::function<void()> init,
                        Fn&& fn) -> decltype(fn()) {
        const InstanceShape& shape = boot(directory, std::move(init));
        const InstanceShape* prev  = current_ptr();
        push(&shape);
        struct Guard {
            const InstanceShape* prev_;
            ~Guard() { Instance::pop(prev_); }
        } guard{prev};
        return fn();
    }

    /// Overload without init callback
    template<typename Fn>
    static auto provide(const std::string& directory, Fn&& fn) -> decltype(fn()) {
        return provide(directory, std::function<void()>{}, std::forward<Fn>(fn));
    }

    // ----- Current Context Accessors -----

    /// Access the current InstanceShape.
    /// Throws std::logic_error if called outside a provide() scope.
    static const InstanceShape& current();

    static const std::string& directory();
    static const std::string& worktree();
    static const ProjectInfo& project();

    /// Returns true if `filepath` is within Instance::directory() or Instance::worktree().
    /// Non-git projects set worktree to "/" — skips worktree check in that case.
    static bool contains_path(const std::string& filepath);

    // ----- Lifecycle -----

    /// Dispose the instance for `directory` (remove from cache).
    /// If directory is empty, uses the current context's directory.
    static void dispose(const std::string& directory = "");

    /// Dispose all cached instances.
    static void dispose_all();

    /// Reload (dispose + re-boot) an instance.
    static InstanceShape reload(const std::string& directory,
                                std::function<void()> init = {});

private:
    Instance() = delete;

    // Internal helpers called by the template
    static const InstanceShape& boot(const std::string& directory,
                                     std::function<void()> init);
    static void push(const InstanceShape* shape);
    static void pop(const InstanceShape* prev);
    static const InstanceShape* current_ptr();
};

} // namespace turbot::core::project
