#include <catch2/catch_test_macros.hpp>
#include <turbot/core/tool/builtin/todo_tool.hpp>
#include <turbot/core/tool/tool_registry.hpp>

using namespace turbot::core::tool;

// Helper to create a basic context
static ToolContext make_ctx(const std::string& session_id = "test-session") {
    ToolContext ctx;
    ctx.session_id = session_id;
    ctx.message_id = "test-message";
    ctx.agent = "test-agent";
    ctx.working_directory = "/tmp";
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    return ctx;
}

// ============================================================================
// TodoStatus Tests
// ============================================================================

TEST_CASE("TodoStatus conversion", "[tool][todo]") {
    SECTION("to_string") {
        REQUIRE(todo_status_to_string(TodoStatus::Pending) == "pending");
        REQUIRE(todo_status_to_string(TodoStatus::InProgress) == "in_progress");
        REQUIRE(todo_status_to_string(TodoStatus::Completed) == "completed");
        REQUIRE(todo_status_to_string(TodoStatus::Cancelled) == "cancelled");
    }
    
    SECTION("from_string") {
        REQUIRE(string_to_todo_status("pending") == TodoStatus::Pending);
        REQUIRE(string_to_todo_status("in_progress") == TodoStatus::InProgress);
        REQUIRE(string_to_todo_status("completed") == TodoStatus::Completed);
        REQUIRE(string_to_todo_status("cancelled") == TodoStatus::Cancelled);
        REQUIRE(string_to_todo_status("unknown") == TodoStatus::Pending);  // default
    }
}

// ============================================================================
// TodoItem Tests
// ============================================================================

TEST_CASE("TodoItem JSON serialization", "[tool][todo]") {
    TodoItem item;
    item.id = "todo-123";
    item.content = "Test task";
    item.status = TodoStatus::InProgress;
    item.priority = "high";
    item.position = 1;
    
    nlohmann::json j = item.to_json();
    
    REQUIRE(j["id"] == "todo-123");
    REQUIRE(j["content"] == "Test task");
    REQUIRE(j["status"] == "in_progress");
    REQUIRE(j["priority"] == "high");
    REQUIRE(j["position"] == 1);
}

TEST_CASE("TodoItem JSON deserialization", "[tool][todo]") {
    nlohmann::json j = {
        {"id", "todo-456"},
        {"content", "Another task"},
        {"status", "completed"},
        {"priority", "low"},
        {"position", 2}
    };
    
    TodoItem item = TodoItem::from_json(j);
    
    REQUIRE(item.id == "todo-456");
    REQUIRE(item.content == "Another task");
    REQUIRE(item.status == TodoStatus::Completed);
    REQUIRE(item.priority == "low");
    REQUIRE(item.position == 2);
}

TEST_CASE("TodoItem equality", "[tool][todo]") {
    TodoItem item1;
    item1.id = "todo-1";
    item1.content = "Task 1";
    item1.status = TodoStatus::Pending;
    item1.priority = "medium";
    item1.position = 0;
    
    TodoItem item2 = item1;
    REQUIRE(item1 == item2);
    
    item2.status = TodoStatus::Completed;
    REQUIRE_FALSE(item1 == item2);
}

// ============================================================================
// TodoManager Tests
// ============================================================================

TEST_CASE("TodoManager basic operations", "[tool][todo]") {
    TodoManager::instance().clear_all();
    
    SECTION("empty initially") {
        auto todos = TodoManager::instance().get_todos("session-1");
        REQUIRE(todos.empty());
    }
    
    SECTION("set and get") {
        std::vector<TodoItem> items;
        TodoItem item;
        item.id = "todo-1";
        item.content = "Task 1";
        item.status = TodoStatus::Pending;
        items.push_back(item);
        
        TodoManager::instance().set_todos("session-1", items);
        
        auto retrieved = TodoManager::instance().get_todos("session-1");
        REQUIRE(retrieved.size() == 1);
        REQUIRE(retrieved[0].content == "Task 1");
    }
    
    SECTION("different sessions have different todos") {
        std::vector<TodoItem> items1;
        items1.push_back(TodoItem{.id = "1", .content = "Session 1 task"});
        
        std::vector<TodoItem> items2;
        items2.push_back(TodoItem{.id = "2", .content = "Session 2 task"});
        
        TodoManager::instance().set_todos("session-1", items1);
        TodoManager::instance().set_todos("session-2", items2);
        
        auto retrieved1 = TodoManager::instance().get_todos("session-1");
        auto retrieved2 = TodoManager::instance().get_todos("session-2");
        
        REQUIRE(retrieved1[0].content == "Session 1 task");
        REQUIRE(retrieved2[0].content == "Session 2 task");
    }
    
    SECTION("clear session") {
        std::vector<TodoItem> items;
        items.push_back(TodoItem{.id = "1", .content = "Task"});
        TodoManager::instance().set_todos("session-to-clear", items);
        
        TodoManager::instance().clear_todos("session-to-clear");
        
        auto retrieved = TodoManager::instance().get_todos("session-to-clear");
        REQUIRE(retrieved.empty());
    }
    
    TodoManager::instance().clear_all();
}

// ============================================================================
// TodoReadTool Tests
// ============================================================================

TEST_CASE("TodoReadTool metadata", "[tool][todo]") {
    TodoReadTool tool;
    
    REQUIRE(tool.name() == "todoread");
    REQUIRE_FALSE(tool.description().empty());
    
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
}

TEST_CASE("TodoReadTool execute", "[tool][todo]") {
    TodoManager::instance().clear_all();
    TodoReadTool tool;
    auto ctx = make_ctx("read-test-session");
    
    SECTION("empty list") {
        auto result = tool.execute(nlohmann::json::object(), ctx);
        REQUIRE(result.is_error == false);
        REQUIRE_FALSE(result.output.empty());
    }
    
    SECTION("with items") {
        std::vector<TodoItem> items;
        items.push_back(TodoItem{.id = "1", .content = "Task 1", .status = TodoStatus::Pending});
        items.push_back(TodoItem{.id = "2", .content = "Task 2", .status = TodoStatus::Completed});
        TodoManager::instance().set_todos("read-test-session", items);
        
        auto result = tool.execute(nlohmann::json::object(), ctx);
        REQUIRE(result.is_error == false);
        REQUIRE(result.metadata["todos"].size() == 2);
    }
    
    TodoManager::instance().clear_all();
}

// ============================================================================
// TodoWriteTool Tests
// ============================================================================

TEST_CASE("TodoWriteTool metadata", "[tool][todo]") {
    TodoWriteTool tool;
    
    REQUIRE(tool.name() == "todowrite");
    REQUIRE_FALSE(tool.description().empty());
    
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["required"].size() == 1);
    REQUIRE(schema["required"][0] == "todos");
}

TEST_CASE("TodoWriteTool validation", "[tool][todo]") {
    TodoWriteTool tool;
    
    SECTION("valid input") {
        nlohmann::json input = {
            {"todos", {
                {{"content", "Task 1"}, {"status", "pending"}},
                {{"content", "Task 2"}, {"status", "in_progress"}}
            }}
        };
        REQUIRE(tool.validate_input(input));
    }
    
    SECTION("missing todos") {
        nlohmann::json input = {{"other", "value"}};
        REQUIRE_FALSE(tool.validate_input(input));
    }
    
    SECTION("invalid status") {
        nlohmann::json input = {
            {"todos", {
                {{"content", "Task"}, {"status", "invalid"}}
            }}
        };
        REQUIRE_FALSE(tool.validate_input(input));
    }
    
    SECTION("missing content") {
        nlohmann::json input = {
            {"todos", {
                {{"status", "pending"}}
            }}
        };
        REQUIRE_FALSE(tool.validate_input(input));
    }
    
    SECTION("valid with optional fields") {
        nlohmann::json input = {
            {"todos", {
                {{"content", "Task"}, {"status", "pending"}, {"priority", "high"}}
            }}
        };
        REQUIRE(tool.validate_input(input));
    }
}

TEST_CASE("TodoWriteTool execute", "[tool][todo]") {
    TodoManager::instance().clear_all();
    TodoWriteTool tool;
    auto ctx = make_ctx("write-test-session");
    
    nlohmann::json input = {
        {"todos", {
            {{"content", "Task 1"}, {"status", "pending"}},
            {{"content", "Task 2"}, {"status", "in_progress"}},
            {{"content", "Task 3"}, {"status", "completed"}}
        }}
    };
    
    auto result = tool.execute(input, ctx);
    REQUIRE(result.is_error == false);
    REQUIRE(result.metadata["todos"].size() == 3);
    
    // Verify stored
    auto stored = TodoManager::instance().get_todos("write-test-session");
    REQUIRE(stored.size() == 3);
    REQUIRE(stored[0].content == "Task 1");
    REQUIRE(stored[1].content == "Task 2");
    REQUIRE(stored[2].content == "Task 3");
    
    TodoManager::instance().clear_all();
}

// ============================================================================
// ToolRegistry Integration Tests
// ============================================================================

TEST_CASE("Todo tools registered in ToolRegistry", "[tool][todo][registry]") {
    ToolRegistry::instance().clear();
    ToolRegistry::instance().register_builtin_tools();
    
    REQUIRE(ToolRegistry::instance().has("todoread"));
    REQUIRE(ToolRegistry::instance().has("todowrite"));
    
    auto read_tool = ToolRegistry::instance().get("todoread");
    auto write_tool = ToolRegistry::instance().get("todowrite");
    
    REQUIRE(read_tool != nullptr);
    REQUIRE(write_tool != nullptr);
    REQUIRE(read_tool->name() == "todoread");
    REQUIRE(write_tool->name() == "todowrite");
    
    ToolRegistry::instance().clear();
}
