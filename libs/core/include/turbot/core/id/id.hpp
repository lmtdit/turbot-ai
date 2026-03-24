// id.hpp - Centralized ID generation matching OpenCode's Identifier namespace
//
// Mirrors: opencode/packages/opencode/src/id/id.ts
//
// All ID formats: "<prefix>_<12-hex-timestamp><14-base62-random>"
// Total length: prefix(3-4) + "_" + 26 chars = 30-31 chars
//
// Supported prefixes:
//   session    -> "ses"    (descending order: newest first)
//   message    -> "msg"    (ascending order:  oldest first)
//   part       -> "prt"    (ascending)
//   permission -> "per"    (ascending)
//   question   -> "que"    (ascending)
//   user       -> "usr"    (ascending)
//   workspace  -> "wrk"    (ascending)
//   pty        -> "pty"    (ascending)
//   tool       -> "tool"   (ascending)

#pragma once

#include <string>
#include <cstdint>

namespace turbot::core::id {

/// Generate an ascending ID (oldest first, suitable for messages/parts/etc.)
/// @param prefix  The identifier prefix (e.g. "msg", "prt")
/// @param ts_ms   Optional timestamp override (milliseconds since epoch); 0 = use now
std::string ascending(const std::string& prefix, int64_t ts_ms = 0);

/// Generate a descending ID (newest first, suitable for sessions)
/// @param prefix  The identifier prefix (e.g. "ses")
/// @param ts_ms   Optional timestamp override (milliseconds since epoch); 0 = use now
std::string descending(const std::string& prefix, int64_t ts_ms = 0);

/// Extract the millisecond timestamp encoded in an ascending ID.
/// Returns 0 if the ID format is invalid.
int64_t timestamp(const std::string& id);

/// Convenience generators — mirror OpenCode's Identifier namespace

/// Session ID: descending order (newest sessions sort first)
inline std::string session_id() { return descending("ses"); }

/// Message ID: ascending order
inline std::string message_id() { return ascending("msg"); }

/// Part ID: ascending order
inline std::string part_id() { return ascending("prt"); }

/// Permission ID: ascending order
inline std::string permission_id() { return ascending("per"); }

/// Question ID: ascending order
inline std::string question_id() { return ascending("que"); }

/// User ID: ascending order
inline std::string user_id() { return ascending("usr"); }

/// Workspace ID: ascending order
inline std::string workspace_id() { return ascending("wrk"); }

/// Pty ID: ascending order
inline std::string pty_id() { return ascending("pty"); }

/// Tool call ID: ascending order
inline std::string tool_call_id() { return ascending("tool"); }

} // namespace turbot::core::id
