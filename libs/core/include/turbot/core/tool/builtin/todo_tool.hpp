#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/tool/tool.hpp>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace turbot::core::tool {

/// Todo item status
enum class TURBOT_CORE_API TodoStatus {
    Pending,      ///< Task is pending
    InProgress,   ///< Task is in progress
    Completed,    ///< Task is completed
    Cancelled     ///< Task is cancelled
};

/// Convert TodoStatus to string
[[nodiscard]] TURBOT_CORE_API std::string todo_status_to_string(TodoStatus status);

/// Convert string to TodoStatus
[[nodiscard]] TURBOT_CORE_API TodoStatus string_to_todo_status(const std::string& str);

/// Todo item structure
struct TURBOT_CORE_API TodoItem {
    std::string id;                           ///< Unique identifier
    std::string content;                      ///< Task description
    TodoStatus status = TodoStatus::Pending;  ///< Current status
    std::string priority = "medium";          ///< Priority: high, medium, low
    int position = 0;                         ///< Position in list

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static TodoItem from_json(const nlohmann::json& j);

    /// Equality comparison
    bool operator==(const TodoItem& other) const noexcept;
};

/// Todo manager - manages todo lists per session
class TURBOT_CORE_API TodoManager {
public:
    /// Get the singleton instance
    static TodoManager& instance();

    /// Get todos for a session
    /// @param session_id Session ID
    /// @return Vector of todo items
    [[nodiscard]] std::vector<TodoItem> get_todos(const std::string& session_id) const;

    /// Update todos for a session
    /// @param session_id Session ID
    /// @param todos New todo list
    void set_todos(const std::string& session_id, const std::vector<TodoItem>& todos);

    /// Clear todos for a session
    /// @param session_id Session ID
    void clear_todos(const std::string& session_id);

    /// Clear all todos (mainly for testing)
    void clear_all();

private:
    TodoManager() = default;
    ~TodoManager() = default;

    // Non-copyable, non-movable
    TodoManager(const TodoManager&) = delete;
    TodoManager& operator=(const TodoManager&) = delete;
    TodoManager(TodoManager&&) = delete;
    TodoManager& operator=(TodoManager&&) = delete;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::vector<TodoItem>> todos_;
};

/// TodoRead tool - reads the todo list for the current session
class TURBOT_CORE_API TodoReadTool : public Tool {
public:
    TodoReadTool() = default;

    [[nodiscard]] std::string name() const override { return "todoread"; }
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] nlohmann::json input_schema() const override;
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;
};

/// TodoWrite tool - writes/updates the todo list for the current session
class TURBOT_CORE_API TodoWriteTool : public Tool {
public:
    TodoWriteTool() = default;

    [[nodiscard]] std::string name() const override { return "todowrite"; }
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] nlohmann::json input_schema() const override;
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;
    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;
};

} // namespace turbot::core::tool
