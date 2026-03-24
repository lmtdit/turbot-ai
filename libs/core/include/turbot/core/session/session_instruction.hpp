// session_instruction.hpp - InstructionPrompt namespace
//
// Mirrors: opencode/packages/opencode/src/session/instruction.ts (InstructionPrompt)
//
// Collects instruction files (AGENTS.md, CLAUDE.md, CONTEXT.md) from:
//   1. Project directory tree (glob-up from Instance::directory() to Instance::worktree())
//   2. Global config directory (Global::init().config / AGENTS.md)
//   3. ~/.claude/CLAUDE.md (unless OPENCODE_DISABLE_CLAUDE_CODE_PROMPT is set)
//   4. Config::instructions list (file paths or https:// URLs)
//
// The system() function returns a list of "<header>\n<content>" strings ready to
// be injected into the system prompt.

#pragma once

#include <turbot/core/common/export.hpp>
#include <set>
#include <string>
#include <vector>

namespace turbot::core::session {

/// InstructionPrompt — collects and caches per-message instruction file content.
///
/// Per-message "claim" state is NOT thread-safe by design: it is intended to be
/// used within a single session loop iteration (one message at a time).
namespace InstructionPrompt {

    /// Return the set of canonical file paths that supply system-level instructions.
    /// Searches upward from Instance::directory() and includes global config paths.
    /// Safe to call multiple times (no side effects).
    [[nodiscard]] TURBOT_CORE_API std::set<std::string> system_paths();

    /// Return instruction content strings ready for system prompt injection.
    /// Each element is "Instructions from: <path>\n<content>" (or URL equivalent).
    /// HTTP URLs from config.instructions are fetched synchronously (5s timeout).
    [[nodiscard]] TURBOT_CORE_API std::vector<std::string> system();

    /// Return the set of file paths that have been loaded via read tool in `messages`.
    /// Mirrors OpenCode InstructionPrompt.loaded().
    [[nodiscard]] TURBOT_CORE_API std::set<std::string> loaded(
        const std::vector<std::string>& read_tool_paths);

    /// Resolve contextual instruction files for a file read at `filepath`.
    /// Searches upward from filepath's directory to Instance::directory() for
    /// AGENTS.md / CLAUDE.md / CONTEXT.md, skipping already-loaded and claimed files.
    ///
    /// Returns list of {filepath, content} pairs for injection.
    struct ResolvedInstruction {
        std::string filepath;
        std::string content;
    };
    [[nodiscard]] TURBOT_CORE_API std::vector<ResolvedInstruction> resolve(
        const std::set<std::string>& already_loaded,
        const std::string& filepath,
        const std::string& message_id);

    /// Clear per-message claim state for `message_id`.
    TURBOT_CORE_API void clear(const std::string& message_id);

    /// Find an instruction file in `dir` (AGENTS.md / CLAUDE.md / CONTEXT.md).
    /// Returns empty string if none found.
    [[nodiscard]] TURBOT_CORE_API std::string find(const std::string& dir);

} // namespace InstructionPrompt

} // namespace turbot::core::session
