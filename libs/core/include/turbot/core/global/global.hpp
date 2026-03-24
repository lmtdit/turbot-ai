// global.hpp - Global application paths singleton
//
// Mirrors: opencode/packages/opencode/src/global/index.ts (Global.Path)
//
// Provides XDG-compliant data/config/cache/state directories for the
// application. All paths are created on first access via Global::init().
//
// Environment overrides:
//   OPENCODE_TEST_HOME   — Override home directory (used in tests)
//   XDG_DATA_HOME        — Override XDG data root
//   XDG_CONFIG_HOME      — Override XDG config root
//   XDG_CACHE_HOME       — Override XDG cache root
//   XDG_STATE_HOME       — Override XDG state root

#pragma once

#include <string>

namespace turbot::core::global {

/// Global path singleton — call init() once at startup to create directories.
struct Global {
    struct Path {
        std::string home;    ///< User home directory (env OPENCODE_TEST_HOME or $HOME)
        std::string data;    ///< XDG_DATA_HOME/opencode
        std::string config;  ///< XDG_CONFIG_HOME/opencode
        std::string cache;   ///< XDG_CACHE_HOME/opencode
        std::string state;   ///< XDG_STATE_HOME/opencode
        std::string bin;     ///< cache/bin  (downloaded binaries)
        std::string log;     ///< data/log
    };

    /// Initialize and return the global path singleton.
    /// Creates all required directories; safe to call multiple times.
    static const Path& init();

    /// Return the already-initialized path singleton.
    /// Must call init() first; throws std::logic_error if not initialized.
    static const Path& path();

    /// Cache version string — bump to invalidate cached artifacts.
    /// Mirrors: OpenCode CACHE_VERSION = "21"
    static constexpr const char* CACHE_VERSION = "21";
};

} // namespace turbot::core::global
