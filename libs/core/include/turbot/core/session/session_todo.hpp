#pragma once

#include <turbot/core/common/export.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace turbot::core::session {

// ---------------------------------------------------------------------------
// TodoInfo — mirrors OpenCode Todo.Info schema
// ---------------------------------------------------------------------------
struct TURBOT_CORE_API TodoInfo {
    std::string content;   ///< Brief description of the task
    std::string status;    ///< "pending" | "in_progress" | "completed" | "cancelled"
    std::string priority;  ///< "high" | "medium" | "low"

    [[nodiscard]] nlohmann::json to_json() const;
    static TodoInfo from_json(const nlohmann::json& j);
};

// ---------------------------------------------------------------------------
// Todo — session-scoped todo list persistent storage
// Mirrors OpenCode session/todo.ts  Todo namespace
// ---------------------------------------------------------------------------
namespace Todo {

/// Replace the full todo list for a session (all-or-nothing transaction).
/// Publishes a "todo.updated" event via EventBus after persistence.
/// Mirrors OpenCode Todo.update().
///
/// @param session_id  The session whose todo list is replaced.
/// @param todos       The new list of todos (empty = clear all).
TURBOT_CORE_API void update(const std::string& session_id, const std::vector<TodoInfo>& todos);

/// Retrieve the current todo list for a session, ordered by position.
/// Returns an empty vector when the session has no todos or the store is
/// not initialised.
/// Mirrors OpenCode Todo.get().
[[nodiscard]] TURBOT_CORE_API std::vector<TodoInfo> get(const std::string& session_id);

} // namespace Todo

// ---------------------------------------------------------------------------
// TodoUpdatedEvent — published by Todo::update()
// ---------------------------------------------------------------------------
struct TURBOT_CORE_API TodoUpdatedEvent {
    static constexpr const char* kEventName = "todo.updated";
    std::string           session_id;
    std::vector<TodoInfo> todos;
};

} // namespace turbot::core::session
