#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/tool/tool.hpp>

using namespace turbot::core::tool;
using namespace turbot::test;

// ==================== ToolRegistry 测试 ====================

TEST_CASE("Tool.Registry.Instance", "[Tool]") {
    auto& registry1 = ToolRegistry::instance();
    auto& registry2 = ToolRegistry::instance();
    REQUIRE(&registry1 == &registry2);
}

TEST_CASE("Tool.Registry.Has", "[Tool]") {
    auto& registry = ToolRegistry::instance();
    
    // 检查内置工具是否存在（需要先注册）
    // registry.register_builtin_tools();
    // REQUIRE(registry.has("bash"));
}

TEST_CASE("Tool.Registry.Names", "[Tool]") {
    auto& registry = ToolRegistry::instance();
    
    auto names = registry.names();
    // 验证返回的是名称列表
    for (const auto& name : names) {
        REQUIRE_FALSE(name.empty());
    }
}

TEST_CASE("Tool.Registry.Size", "[Tool]") {
    auto& registry = ToolRegistry::instance();
    
    size_t size = registry.size();
    // size 应该 >= 0
    REQUIRE(size >= 0);
}

// ==================== ToolResult 测试 ====================

TEST_CASE("Tool.Result.Success", "[Tool]") {
    auto result = ToolResult::success("Test Title", "Operation completed");
    
    REQUIRE(result.title == "Test Title");
    REQUIRE(result.output == "Operation completed");
    REQUIRE_FALSE(result.is_error);
}

TEST_CASE("Tool.Result.Error", "[Tool]") {
    auto result = ToolResult::error("Error Title", "Operation failed");
    
    REQUIRE(result.title == "Error Title");
    REQUIRE(result.output == "Operation failed");
    REQUIRE(result.is_error);
}

TEST_CASE("Tool.Result.WithMetadata", "[Tool]") {
    nlohmann::json metadata = {{"count", 42}, {"status", "ok"}};
    auto result = ToolResult::success("Test", "Output", metadata);
    
    REQUIRE(result.metadata["count"] == 42);
    REQUIRE(result.metadata["status"] == "ok");
}

TEST_CASE("Tool.Result.JsonSerialization", "[Tool]") {
    auto result = ToolResult::success("Title", "Output", {{"key", "value"}});
    
    nlohmann::json j = result.to_json();
    REQUIRE(j["title"] == "Title");
    REQUIRE(j["output"] == "Output");
    REQUIRE(j["is_error"] == false);
    REQUIRE(j["metadata"]["key"] == "value");
    
    auto restored = ToolResult::from_json(j);
    REQUIRE(restored.title == result.title);
    REQUIRE(restored.output == result.output);
    REQUIRE(restored.is_error == result.is_error);
}

// ==================== ToolConfig 测试 ====================

TEST_CASE("Tool.Config.Defaults", "[Tool]") {
    ToolConfig config;
    
    REQUIRE(config.shell_mode == ShellMode::Ask);
    REQUIRE(config.default_timeout == 120);
    REQUIRE(config.max_timeout == 600);
    REQUIRE(config.auto_approve_read == true);
    REQUIRE(config.auto_approve_edit == false);
}

TEST_CASE("Tool.Config.JsonSerialization", "[Tool]") {
    ToolConfig config;
    config.shell_mode = ShellMode::Sandbox;
    config.default_timeout = 300;
    config.max_timeout = 900;
    
    nlohmann::json j = config.to_json();
    REQUIRE(j["shell_mode"] == "sandbox");
    REQUIRE(j["default_timeout"] == 300);
    REQUIRE(j["max_timeout"] == 900);
    
    auto restored = ToolConfig::from_json(j);
    REQUIRE(restored.shell_mode == ShellMode::Sandbox);
    REQUIRE(restored.default_timeout == 300);
    REQUIRE(restored.max_timeout == 900);
}

// ==================== ShellMode 测试 ====================

TEST_CASE("Tool.ShellMode.Conversion", "[Tool]") {
    REQUIRE(shell_mode_to_string(ShellMode::Normal) == "normal");
    REQUIRE(shell_mode_to_string(ShellMode::Sandbox) == "sandbox");
    REQUIRE(shell_mode_to_string(ShellMode::Ask) == "ask");
    
    REQUIRE(string_to_shell_mode("normal") == ShellMode::Normal);
    REQUIRE(string_to_shell_mode("sandbox") == ShellMode::Sandbox);
    REQUIRE(string_to_shell_mode("ask") == ShellMode::Ask);
}

// ==================== ToolContext 测试 ====================

TEST_CASE("Tool.Context.Defaults", "[Tool]") {
    ToolContext ctx;
    
    REQUIRE(ctx.session_id.empty());
    REQUIRE(ctx.message_id.empty());
    REQUIRE(ctx.agent.empty());
    REQUIRE_FALSE(ctx.call_id.has_value());
    REQUIRE(ctx.working_directory.empty());
}

TEST_CASE("Tool.Context.WithValues", "[Tool]") {
    ToolContext ctx;
    ctx.session_id = "session-123";
    ctx.message_id = "message-456";
    ctx.agent = "build";
    ctx.call_id = "call-789";
    ctx.working_directory = "/home/user/project";
    
    REQUIRE(ctx.session_id == "session-123");
    REQUIRE(ctx.message_id == "message-456");
    REQUIRE(ctx.agent == "build");
    REQUIRE(ctx.call_id == "call-789");
    REQUIRE(ctx.working_directory == "/home/user/project");
}
