#include <turbot/core/tool/builtin/todo_tool.hpp>
#include <turbot/core/common/logger.hpp>
#include <fmt/format.h>
#include <random>
#include <sstream>

namespace turbot::core::tool {

// ============================================================================
// TodoStatus helpers
// ============================================================================

std::string todo_status_to_string(TodoStatus status) {
    switch (status) {
        case TodoStatus::Pending:    return "pending";
        case TodoStatus::InProgress: return "in_progress";
        case TodoStatus::Completed:  return "completed";
        case TodoStatus::Cancelled:  return "cancelled";
        default:                     return "pending";
    }
}

TodoStatus string_to_todo_status(const std::string& str) {
    if (str == "in_progress") return TodoStatus::InProgress;
    if (str == "completed")   return TodoStatus::Completed;
    if (str == "cancelled")   return TodoStatus::Cancelled;
    return TodoStatus::Pending;  // default
}

// ============================================================================
// TodoItem
// ============================================================================

nlohmann::json TodoItem::to_json() const {
    return {
        {"id", id},
        {"content", content},
        {"status", todo_status_to_string(status)},
        {"priority", priority},
        {"position", position}
    };
}

TodoItem TodoItem::from_json(const nlohmann::json& j) {
    TodoItem item;
    item.id = j.value("id", "");
    item.content = j.value("content", "");
    item.status = string_to_todo_status(j.value("status", "pending"));
    item.priority = j.value("priority", "medium");
    item.position = j.value("position", 0);
    return item;
}

bool TodoItem::operator==(const TodoItem& other) const noexcept {
    return id == other.id &&
           content == other.content &&
           status == other.status &&
           priority == other.priority &&
           position == other.position;
}

// ============================================================================
// TodoManager
// ============================================================================

TodoManager& TodoManager::instance() {
    static TodoManager instance;
    return instance;
}

std::vector<TodoItem> TodoManager::get_todos(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = todos_.find(session_id);
    if (it == todos_.end()) {
        return {};
    }
    return it->second;
}

void TodoManager::set_todos(const std::string& session_id, const std::vector<TodoItem>& todos) {
    std::lock_guard<std::mutex> lock(mutex_);
    todos_[session_id] = todos;
    TURBOT_LOG_DEBUG("TodoManager: set {} todos for session {}", todos.size(), session_id);
}

void TodoManager::clear_todos(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    todos_.erase(session_id);
}

void TodoManager::clear_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    todos_.clear();
}

// ============================================================================
// Helper: generate unique ID
// ============================================================================

static std::string generate_todo_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "todo_";
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

// ============================================================================
// TodoReadTool
// ============================================================================

std::string TodoReadTool::description() const {
    return "Use this tool to read the current todo list for the session. "
           "Returns an array of todo items with their content, status, and priority.";
}

nlohmann::json TodoReadTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", nlohmann::json::object()},
        {"required", nlohmann::json::array()}
    };
}

ToolResult TodoReadTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    // Check abort flag
    if (ctx.should_abort()) {
        return ToolResult::error("todoread", "Operation aborted", {{"aborted", true}});
    }

    // Get todos for this session
    auto todos = TodoManager::instance().get_todos(ctx.session_id);
    
    // Build result
    nlohmann::json result = nlohmann::json::array();
    for (const auto& todo : todos) {
        result.push_back(todo.to_json());
    }
    
    // Count incomplete todos
    size_t incomplete = 0;
    for (const auto& todo : todos) {
        if (todo.status != TodoStatus::Completed) {
            incomplete++;
        }
    }
    
    return ToolResult::success(
        fmt::format("{} todos ({} incomplete)", todos.size(), incomplete),
        result.dump(2),
        {{"todos", result}}
    );
}

// ============================================================================
// TodoWriteTool
// ============================================================================

std::string TodoWriteTool::description() const {
    return "Use this tool to update the todo list for the session. "
           "Provide an array of todo items with content, status, and priority. "
           "Status can be: pending, in_progress, completed, cancelled. "
           "Priority can be: high, medium, low.";
}

nlohmann::json TodoWriteTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"todos", {
                {"type", "array"},
                {"description", "The updated todo list"},
                {"items", {
                    {"type", "object"},
                    {"properties", {
                        {"content", {
                            {"type", "string"},
                            {"description", "Brief description of the task"}
                        }},
                        {"status", {
                            {"type", "string"},
                            {"enum", {"pending", "in_progress", "completed", "cancelled"}},
                            {"description", "Current status of the task"}
                        }},
                        {"priority", {
                            {"type", "string"},
                            {"enum", {"high", "medium", "low"}},
                            {"description", "Priority level of the task"}
                        }}
                    }},
                    {"required", {"content"}}
                }}
            }}
        }},
        {"required", {"todos"}}
    };
}

bool TodoWriteTool::validate_input(const nlohmann::json& input) const {
    if (!input.is_object()) return false;
    if (!input.contains("todos")) return false;
    if (!input["todos"].is_array()) return false;
    
    for (const auto& item : input["todos"]) {
        if (!item.is_object()) return false;
        if (!item.contains("content")) return false;
        if (!item["content"].is_string()) return false;
        
        // Validate status if present
        if (item.contains("status")) {
            std::string status = item["status"];
            if (status != "pending" && status != "in_progress" && 
                status != "completed" && status != "cancelled") {
                return false;
            }
        }
        
        // Validate priority if present
        if (item.contains("priority")) {
            std::string priority = item["priority"];
            if (priority != "high" && priority != "medium" && priority != "low") {
                return false;
            }
        }
    }
    
    return true;
}

ToolResult TodoWriteTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    // Check abort flag
    if (ctx.should_abort()) {
        return ToolResult::error("todowrite", "Operation aborted", {{"aborted", true}});
    }

    // Parse todos
    std::vector<TodoItem> todos;
    int position = 0;
    
    for (const auto& item : input["todos"]) {
        TodoItem todo;
        todo.id = generate_todo_id();
        todo.content = item["content"];
        todo.status = string_to_todo_status(item.value("status", "pending"));
        todo.priority = item.value("priority", "medium");
        todo.position = position++;
        todos.push_back(todo);
    }
    
    // Store todos
    TodoManager::instance().set_todos(ctx.session_id, todos);
    
    // Build result
    nlohmann::json result = nlohmann::json::array();
    for (const auto& todo : todos) {
        result.push_back(todo.to_json());
    }
    
    // Count incomplete todos
    size_t incomplete = 0;
    for (const auto& todo : todos) {
        if (todo.status != TodoStatus::Completed) {
            incomplete++;
        }
    }
    
    TURBOT_LOG_INFO("TodoWriteTool: saved {} todos for session {}", todos.size(), ctx.session_id);
    
    return ToolResult::success(
        fmt::format("{} todos ({} incomplete)", todos.size(), incomplete),
        result.dump(2),
        {{"todos", result}}
    );
}

} // namespace turbot::core::tool
