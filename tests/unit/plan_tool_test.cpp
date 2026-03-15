#include <catch2/catch_test_macros.hpp>
#include <turbot/core/tool/builtin/plan_tool.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <filesystem>
#include <fstream>

using namespace turbot::core::tool;

// Helper to create a basic context
static ToolContext make_ctx(const std::string& session_id = "test-session") {
    ToolContext ctx;
    ctx.session_id = session_id;
    ctx.message_id = "test-message";
    ctx.agent = "test-agent";
    ctx.working_directory = "/tmp/turbot-plan-test";
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    return ctx;
}

// ============================================================================
// PlanMode Tests
// ============================================================================

TEST_CASE("PlanMode conversion", "[tool][plan]") {
    SECTION("to_string") {
        REQUIRE(plan_mode_to_string(PlanMode::Normal) == "normal");
        REQUIRE(plan_mode_to_string(PlanMode::Planning) == "planning");
    }
    
    SECTION("from_string") {
        REQUIRE(string_to_plan_mode("normal") == PlanMode::Normal);
        REQUIRE(string_to_plan_mode("planning") == PlanMode::Planning);
        REQUIRE(string_to_plan_mode("unknown") == PlanMode::Normal);  // default
    }
}

// ============================================================================
// PlanManager Tests
// ============================================================================

TEST_CASE("PlanManager basic operations", "[tool][plan]") {
    PlanManager::instance().clear_all();
    
    SECTION("default mode is normal") {
        auto mode = PlanManager::instance().get_mode("new-session");
        REQUIRE(mode == PlanMode::Normal);
    }
    
    SECTION("set and get mode") {
        PlanManager::instance().set_mode("session-1", PlanMode::Planning);
        REQUIRE(PlanManager::instance().get_mode("session-1") == PlanMode::Planning);
        
        PlanManager::instance().set_mode("session-1", PlanMode::Normal);
        REQUIRE(PlanManager::instance().get_mode("session-1") == PlanMode::Normal);
    }
    
    SECTION("set and get plan") {
        std::string plan = "# My Plan\n\n1. Step 1\n2. Step 2";
        PlanManager::instance().set_plan("session-1", plan);
        
        auto retrieved = PlanManager::instance().get_plan("session-1");
        REQUIRE(retrieved == plan);
    }
    
    SECTION("clear session") {
        PlanManager::instance().set_mode("session-to-clear", PlanMode::Planning);
        PlanManager::instance().set_plan("session-to-clear", "Plan content");
        
        PlanManager::instance().clear_session("session-to-clear");
        
        REQUIRE(PlanManager::instance().get_mode("session-to-clear") == PlanMode::Normal);
        REQUIRE(PlanManager::instance().get_plan("session-to-clear").empty());
    }
    
    PlanManager::instance().clear_all();
}

TEST_CASE("PlanManager plan path", "[tool][plan]") {
    PlanManager::instance().clear_all();
    
    std::string path = PlanManager::instance().get_plan_path("sess123", "/workspace");
    
    // Should contain the working directory
    REQUIRE(path.find("/workspace/.opencode/plans/") != std::string::npos);
    // Should contain session id prefix
    REQUIRE(path.find("sess123") != std::string::npos);
    // Should end with .md
    REQUIRE(path.ends_with(".md"));
    
    PlanManager::instance().clear_all();
}

// ============================================================================
// PlanEnterTool Tests
// ============================================================================

TEST_CASE("PlanEnterTool metadata", "[tool][plan]") {
    PlanEnterTool tool;
    
    REQUIRE(tool.name() == "plan_enter");
    REQUIRE_FALSE(tool.description().empty());
    
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
}

TEST_CASE("PlanEnterTool execute", "[tool][plan]") {
    PlanManager::instance().clear_all();
    PlanEnterTool tool;
    auto ctx = make_ctx("plan-enter-session");
    
    auto result = tool.execute(nlohmann::json::object(), ctx);
    REQUIRE(result.is_error == false);
    REQUIRE(result.metadata["mode"] == "planning");
    
    // Verify mode was set
    REQUIRE(PlanManager::instance().get_mode("plan-enter-session") == PlanMode::Planning);
    
    PlanManager::instance().clear_all();
}

// ============================================================================
// PlanExitTool Tests
// ============================================================================

TEST_CASE("PlanExitTool metadata", "[tool][plan]") {
    PlanExitTool tool;
    
    REQUIRE(tool.name() == "plan_exit");
    REQUIRE_FALSE(tool.description().empty());
    
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["required"].size() == 1);
    REQUIRE(schema["required"][0] == "plan");
}

TEST_CASE("PlanExitTool validation", "[tool][plan]") {
    PlanExitTool tool;
    
    SECTION("valid input") {
        nlohmann::json input = {{"plan", "# My Plan\n\nContent here"}};
        REQUIRE(tool.validate_input(input));
    }
    
    SECTION("missing plan") {
        nlohmann::json input = {{"other", "value"}};
        REQUIRE_FALSE(tool.validate_input(input));
    }
    
    SECTION("plan not string") {
        nlohmann::json input = {{"plan", 123}};
        REQUIRE_FALSE(tool.validate_input(input));
    }
}

TEST_CASE("PlanExitTool execute", "[tool][plan]") {
    PlanManager::instance().clear_all();
    PlanExitTool tool;
    auto ctx = make_ctx("plan-exit-session");
    
    // First enter plan mode
    PlanManager::instance().set_mode("plan-exit-session", PlanMode::Planning);
    
    std::string plan_content = "# Test Plan\n\n1. Step 1\n2. Step 2\n3. Step 3";
    nlohmann::json input = {{"plan", plan_content}};
    
    auto result = tool.execute(input, ctx);
    REQUIRE(result.is_error == false);
    REQUIRE(result.metadata["mode"] == "normal");
    
    // Verify mode was reset
    REQUIRE(PlanManager::instance().get_mode("plan-exit-session") == PlanMode::Normal);
    
    // Verify plan was stored
    auto stored_plan = PlanManager::instance().get_plan("plan-exit-session");
    REQUIRE(stored_plan == plan_content);
    
    // Verify file was created
    std::string plan_path = result.metadata["plan_path"];
    REQUIRE(std::filesystem::exists(plan_path));
    
    // Clean up
    std::filesystem::remove_all("/tmp/turbot-plan-test");
    PlanManager::instance().clear_all();
}

// ============================================================================
// ToolRegistry Integration Tests
// ============================================================================

TEST_CASE("Plan tools registered in ToolRegistry", "[tool][plan][registry]") {
    ToolRegistry::instance().clear();
    ToolRegistry::instance().register_builtin_tools();
    
    REQUIRE(ToolRegistry::instance().has("plan_enter"));
    REQUIRE(ToolRegistry::instance().has("plan_exit"));
    
    auto enter_tool = ToolRegistry::instance().get("plan_enter");
    auto exit_tool = ToolRegistry::instance().get("plan_exit");
    
    REQUIRE(enter_tool != nullptr);
    REQUIRE(exit_tool != nullptr);
    REQUIRE(enter_tool->name() == "plan_enter");
    REQUIRE(exit_tool->name() == "plan_exit");
    
    ToolRegistry::instance().clear();
}
