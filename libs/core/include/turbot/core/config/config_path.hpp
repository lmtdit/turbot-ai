#pragma once

#include <turbot/core/common/export.hpp>
#include <string>

namespace turbot::core::config {

/// Get the default config file path (.turbot/turbot.json)
/// This function should be used by all CLI commands that need to access config
[[nodiscard]] TURBOT_CORE_API std::string get_default_config_path();

/// Get the default config directory path (.turbot/)
[[nodiscard]] TURBOT_CORE_API std::string get_default_config_dir();

/// Get the absolute config path for a given directory
/// @param directory The base directory
/// @return Absolute path to .turbot/turbot.json
[[nodiscard]] TURBOT_CORE_API std::string get_config_path_for_dir(const std::string& directory);

} // namespace turbot::core::config
