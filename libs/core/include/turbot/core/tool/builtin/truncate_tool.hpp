#pragma once

#include <turbot/core/common/export.hpp>
#include <optional>
#include <string>

namespace turbot::core::tool::builtin {

/// Truncation limits — mirrors OpenCode truncate.ts constants.
static constexpr int TRUNCATE_MAX_LINES = 2000;
static constexpr int TRUNCATE_MAX_BYTES = 50 * 1024;  // 50 KiB

/// Directory where truncated output files are stored.
/// Path is relative to the project working directory.
static constexpr const char* TRUNCATION_SUBDIR = ".turbot/truncation";

/// Options controlling how text is truncated.
struct TruncateOptions {
    int max_lines = TRUNCATE_MAX_LINES;
    int max_bytes = TRUNCATE_MAX_BYTES;
    /// "head" keeps the beginning; "tail" keeps the end.
    enum class Direction { Head, Tail } direction = Direction::Head;
};

/// Result of a truncation operation.
struct TruncateResult {
    std::string content;         ///< (Possibly truncated) content returned to the LLM
    bool truncated = false;      ///< Whether output was truncated
    std::string output_path;     ///< Full path of the saved file (only when truncated)
};

/// Truncation service — mirrors OpenCode Truncate namespace in truncate.ts.
///
/// If the text fits within the limits it is returned as-is.
/// Otherwise the full text is written to a timestamped file inside
/// TRUNCATION_SUBDIR and a short preview plus a hint is returned.
///
/// This is a free-function interface, not a Tool sub-class, because truncation
/// is invoked internally by other tools (bash, grep, etc.) and not directly
/// by the LLM.
namespace Truncate {

/// Apply truncation to @p text with the given @p options.
///
/// @param text           The full text to (possibly) truncate.
/// @param options        Limits and direction.
/// @param working_dir    Project root used to resolve the truncation directory.
/// @param has_task_tool  When true, the hint will recommend the Task tool.
[[nodiscard]] TURBOT_CORE_API TruncateResult output(
    const std::string& text,
    const TruncateOptions& options    = {},
    const std::string& working_dir    = {},
    bool has_task_tool                = false
);

/// Delete truncated-output files older than @p retention_days days.
/// Silent on errors.
TURBOT_CORE_API void cleanup(const std::string& working_dir, int retention_days = 7);

} // namespace Truncate

} // namespace turbot::core::tool::builtin
