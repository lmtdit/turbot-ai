#pragma once

/**
 * @file truncation_dir.hpp
 * @brief Truncation output directory constant.
 *
 * C++ port of OpenCode packages/opencode/src/tool/truncation-dir.ts:
 *
 *   import path from "path"
 *   import { Global } from "../global"
 *   export const TRUNCATION_DIR = path.join(Global.Path.data, "tool-output")
 *
 * Returns the absolute path to the directory where Truncate::output() persists
 * full tool output when it exceeds line/byte limits.
 *
 * The directory is lazily created by Truncate::output(); callers only need
 * this header to reference the canonical path.
 */

#include <turbot/core/common/export.hpp>
#include <turbot/core/global/global.hpp>
#include <filesystem>
#include <string>

namespace turbot::core::tool {

/// Return the absolute path to the truncation output directory.
///
/// Mirrors TRUNCATION_DIR = path.join(Global.Path.data, "tool-output")
/// in OpenCode truncation-dir.ts.
///
/// @note Global::init() must have been called before this function is used.
[[nodiscard]] inline std::string truncation_dir() {
    return (std::filesystem::path(turbot::core::global::Global::path().data) / "tool-output").string();
}

/// Compile-time name of the subdirectory within Global.Path.data.
/// Useful for static/test contexts that build the path independently.
inline constexpr const char* TRUNCATION_SUBDIR_NAME = "tool-output";

} // namespace turbot::core::tool
