#include <catch2/catch_test_macros.hpp>
#include <turbot/core/tool/builtin/task_tool.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/agent/builtin/build_agent.hpp>
#include <turbot/core/permission/permission.hpp>
#include <memory>

using namespace turbot::core::tool;
using namespace turbot::core::permission;
using namespace turbot::core::agent;

// ============================================================================
// TaskToolParams
// ============================================================================

TEST_CASE("TaskToolParams::from_json required fields", "[core][tool][task_tool]") {
    nlohmann::json j = {
        {"prompt", "Test prompt"},
        {"description", "Test description"},
        {"subagent_type", "build"}
    };
    
    auto params = TaskToolParams::from_json(j);
    REQUIRE(params.prompt == "Test prompt");
    REQUIRE(params.description == "Test description");
    REQUIRE(params.subagent_type == "build");
    REQUIRE_FALSE(params.task_id.has_value());
}

TEST_CASE("TaskToolParams::from_json with task_id", "[core][tool][task_tool]") {
    nlohmann::json j = {
        {"prompt", "Test prompt"},
        {"description", "Test description"},
        {"subagent_type", "plan"},
        {"task_id", "task_123"}
    };
    
    auto params = TaskToolParams::from_json(j);
    REQUIRE(params.prompt == "Test prompt");
    REQUIRE(params.description == "Test description");
    REQUIRE(params.subagent_type == "plan");
    REQUIRE(params.task_id.has_value());
    REQUIRE(params.task_id.value() == "task_123");
}

TEST_CASE("TaskToolParams::from_json null task_id", "[core][tool][task_tool]") {
    nlohmann::json j = {
        {"prompt", "Test"},
        {"description", "Desc"},
        {"subagent_type", "explore"},
        {"task_id", nullptr}
    };
    
    auto params = TaskToolParams::from_json(j);
    REQUIRE_FALSE(params.task_id.has_value());
}

TEST_CASE("TaskToolParams::to_json", "[core][tool][task_tool]") {
    TaskToolParams params;
    params.prompt = "Test prompt";
    params.description = "Test description";
    params.subagent_type = "build";
    params.task_id = "task_456";
    
    auto j = params.to_json();
    REQUIRE(j["prompt"] == "Test prompt");
    REQUIRE(j["description"] == "Test description");
    REQUIRE(j["subagent_type"] == "build");
    REQUIRE(j["task_id"] == "task_456");
}

TEST_CASE("TaskToolParams::to_json without task_id", "[core][tool][task_tool]") {
    TaskToolParams params;
    params.prompt = "Test";
    params.description = "Desc";
    params.subagent_type = "plan";
    
    auto j = params.to_json();
    REQUIRE(j["prompt"] == "Test");
    REQUIRE(j["description"] == "Desc");
    REQUIRE(j["subagent_type"] == "plan");
    REQUIRE_FALSE(j.contains("task_id"));
}

TEST_CASE("TaskToolParams round-trip JSON", "[core][tool][task_tool]") {
    TaskToolParams original;
    original.prompt = "Round trip test";
    original.description = "Testing round trip";
    original.subagent_type = "explore";
    original.task_id = "task_789";
    
    auto restored = TaskToolParams::from_json(original.to_json());
    REQUIRE(restored.prompt == original.prompt);
    REQUIRE(restored.description == original.description);
    REQUIRE(restored.subagent_type == original.subagent_type);
    REQUIRE(restored.task_id == original.task_id);
}

// ============================================================================
// TaskTool
// ============================================================================

TEST_CASE("TaskTool::name", "[core][tool][task_tool]") {
    TaskTool tool;
    REQUIRE(tool.name() == "task");
}

TEST_CASE("TaskTool::description", "[core][tool][task_tool]") {
    TaskTool tool;
    REQUIRE_FALSE(tool.description().empty());
    REQUIRE(tool.description().find("subtask") != std::string::npos);
}

TEST_CASE("TaskTool::input_schema", "[core][tool][task_tool]") {
    TaskTool tool;
    auto schema = tool.input_schema();
    
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].is_object());
    REQUIRE(schema["properties"].contains("prompt"));
    REQUIRE(schema["properties"].contains("description"));
    REQUIRE(schema["properties"].contains("subagent_type"));
    REQUIRE(schema["properties"].contains("task_id"));
    
    // Check required fields
    REQUIRE(schema["required"].is_array());
    bool has_prompt = false, has_description = false, has_subagent_type = false;
    for (const auto& req : schema["required"]) {
        if (req == "prompt") has_prompt = true;
        if (req == "description") has_description = true;
        if (req == "subagent_type") has_subagent_type = true;
    }
    REQUIRE(has_prompt);
    REQUIRE(has_description);
    REQUIRE(has_subagent_type);
}

TEST_CASE("TaskTool::execute with invalid parameters", "[core][tool][task_tool]") {
    TaskTool tool;
    ToolContext ctx;
    ctx.session_id = "test-session";
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    
    // Missing required fields
    nlohmann::json invalid_input = {{"prompt", "test"}};
    auto result = tool.execute(invalid_input, ctx);
    
    REQUIRE(result.is_error);
    REQUIRE(result.output.find("Invalid parameters") != std::string::npos);
}

TEST_CASE("TaskTool::execute with non-existent agent", "[core][tool][task_tool]") {
    // Clear registry to ensure clean state
    AgentRegistry::instance().clear();
    
    TaskTool tool;
    ToolContext ctx;
    ctx.session_id = "test-session";
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    
    nlohmann::json input = {
        {"prompt", "Test prompt"},
        {"description", "Test description"},
        {"subagent_type", "non_existent_agent"}
    };
    
    auto result = tool.execute(input, ctx);
    
    REQUIRE(result.is_error);
    REQUIRE(result.output.find("Agent not found") != std::string::npos);
}

TEST_CASE("TaskTool::execute when aborted", "[core][tool][task_tool]") {
    TaskTool tool;
    ToolContext ctx;
    ctx.session_id = "test-session";
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(true);  // Already aborted
    
    nlohmann::json input = {
        {"prompt", "Test"},
        {"description", "Desc"},
        {"subagent_type", "build"}
    };
    
    auto result = tool.execute(input, ctx);
    
    REQUIRE(result.is_error);
    REQUIRE(result.output.find("aborted") != std::string::npos);
}

TEST_CASE("TaskTool::execute with valid agent", "[core][tool][task_tool]") {
    // Register a build agent
    AgentRegistry::instance().clear();
    AgentRegistry::instance().register_agent(std::make_shared<BuildAgent>());
    
    TaskTool tool;
    ToolContext ctx;
    ctx.session_id = "test-session";
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    
    nlohmann::json input = {
        {"prompt", "Build the project"},
        {"description", "Build task"},
        {"subagent_type", "build"}
    };
    
    auto result = tool.execute(input, ctx);
    
    // Should succeed (agent is found and executed)
    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.title == "Build task");
    REQUIRE(result.metadata.contains("session_id"));
    REQUIRE(result.metadata.contains("subagent"));
    REQUIRE(result.metadata["subagent"] == "build");
}

TEST_CASE("TaskTool::execute with permission rejection", "[core][tool][task_tool]") {
    AgentRegistry::instance().clear();
    AgentRegistry::instance().register_agent(std::make_shared<BuildAgent>());
    
    TaskTool tool;
    ToolContext ctx;
    ctx.session_id = "test-session";
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    
    // Set up a permission callback that rejects
    ctx.ask_permission = [](const PermissionRequest&) -> PermissionReply {
        return PermissionReply::reject("User rejected");
    };
    
    nlohmann::json input = {
        {"prompt", "Test"},
        {"description", "Desc"},
        {"subagent_type", "build"}
    };
    
    auto result = tool.execute(input, ctx);
    
    REQUIRE(result.is_error);
    REQUIRE(result.output.find("rejected") != std::string::npos);
}

TEST_CASE("TaskTool::execute with permission approval", "[core][tool][task_tool]") {
    AgentRegistry::instance().clear();
    AgentRegistry::instance().register_agent(std::make_shared<BuildAgent>());
    
    TaskTool tool;
    ToolContext ctx;
    ctx.session_id = "test-session";
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    
    // Set up a permission callback that approves
    ctx.ask_permission = [](const PermissionRequest&) -> PermissionReply {
        return PermissionReply::once();
    };
    
    nlohmann::json input = {
        {"prompt", "Build task"},
        {"description", "Build the project"},
        {"subagent_type", "build"}
    };
    
    auto result = tool.execute(input, ctx);
    
    REQUIRE_FALSE(result.is_error);
}

TEST_CASE("TaskTool registered in ToolRegistry", "[core][tool][task_tool]") {
    // Register builtin tools first
    ToolRegistry::instance().register_builtin_tools();
    
    // The task tool should be registered
    auto tool = ToolRegistry::instance().get("task");
    REQUIRE(tool != nullptr);
    REQUIRE(tool->name() == "task");
}

// ============================================================================
// TaskTool edge cases
// ============================================================================

TEST_CASE("TaskTool::execute with empty prompt", "[core][tool][task_tool]") {
    AgentRegistry::instance().clear();
    AgentRegistry::instance().register_agent(std::make_shared<BuildAgent>());
    
    TaskTool tool;
    ToolContext ctx;
    ctx.session_id = "test-session";
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    
    nlohmann::json input = {
        {"prompt", ""},
        {"description", "Empty prompt test"},
        {"subagent_type", "build"}
    };
    
    auto result = tool.execute(input, ctx);
    
    // Should still work (empty prompt is valid, though not useful)
    REQUIRE_FALSE(result.is_error);
}

TEST_CASE("TaskTool::execute with long description", "[core][tool][task_tool]") {
    AgentRegistry::instance().clear();
    AgentRegistry::instance().register_agent(std::make_shared<BuildAgent>());
    
    TaskTool tool;
    ToolContext ctx;
    ctx.session_id = "test-session";
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    
    std::string long_desc(1000, 'a');
    
    nlohmann::json input = {
        {"prompt", "Test prompt"},
        {"description", long_desc},
        {"subagent_type", "build"}
    };
    
    auto result = tool.execute(input, ctx);
    
    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.title == long_desc);
}
