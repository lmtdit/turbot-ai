/**
 * @file tool_advanced_test.cpp
 * @brief Advanced tool tests for Tool class functionality
 *
 * Tests for:
 * - Tool::to_tool_definition() - JSON tool definition
 * - Tool::validate_input() - input validation
 * - ToolResult serialization/deserialization
 * - ToolConfig edge cases
 * - ToolContext functionality
 */

#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/tool/tool.hpp>
#include <turbot/core/tool/tool_registry.hpp>

using namespace turbot::core::tool;
using namespace turbot::test;

// ==================== Mock Tool for Advanced Testing ====================

class AdvancedMockTool : public Tool {
public:
    AdvancedMockTool(const std::string& name, const nlohmann::json& schema)
        : name_(name), schema_(schema) {}
    
    [[nodiscard]] std::string name() const override { return name_; }
    [[nodiscard]] std::string description() const override { return "Advanced mock tool"; }
    [[nodiscard]] nlohmann::json input_schema() const override { return schema_; }
    
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override {
        if (ctx.should_abort()) {
            return ToolResult::error("Aborted", "Execution was aborted");
        }
        return ToolResult::success(name_ + " executed", input.dump());
    }

private:
    std::string name_;
    nlohmann::json schema_;
};

// ==================== Tool Definition Tests ====================

TEST_CASE("Tool.Advanced.ToToolDefinition.BasicStructure", "[Tool][Advanced]") {
    nlohmann::json schema = {
        {"type", "object"},
        {"properties", {
            {"path", {{"type", "string"}, {"description", "File path"}}}
        }},
        {"required", {"path"}}
    };
    
    AdvancedMockTool tool("file_reader", schema);
    auto def = tool.to_tool_definition();
    
    REQUIRE(def.contains("type"));
    REQUIRE(def.contains("function"));
    REQUIRE(def["type"] == "function");
    REQUIRE(def["function"].contains("name"));
    REQUIRE(def["function"].contains("description"));
    REQUIRE(def["function"].contains("parameters"));
    REQUIRE(def["function"]["name"] == "file_reader");
    REQUIRE(def["function"]["description"] == "Advanced mock tool");
}

TEST_CASE("Tool.Advanced.ToToolDefinition.WithComplexSchema", "[Tool][Advanced]") {
    nlohmann::json schema = {
        {"type", "object"},
        {"properties", {
            {"files", {
                {"type", "array"},
                {"items", {{"type", "string"}}}
            }},
            {"options", {
                {"type", "object"},
                {"properties", {
                    {"recursive", {{"type", "boolean"}}},
                    {"depth", {{"type", "integer"}}}
                }}
            }}
        }}
    };
    
    AdvancedMockTool tool("complex_tool", schema);
    auto def = tool.to_tool_definition();
    
    REQUIRE(def["function"]["parameters"]["properties"]["files"]["type"] == "array");
    REQUIRE(def["function"]["parameters"]["properties"]["options"]["type"] == "object");
}

// ==================== Input Validation Tests ====================

TEST_CASE("Tool.Advanced.ValidateInput.ValidInput", "[Tool][Advanced]") {
    nlohmann::json schema = {
        {"type", "object"},
        {"properties", {
            {"name", {{"type", "string"}}}
        }}
    };
    
    AdvancedMockTool tool("validator", schema);
    nlohmann::json input = {{"name", "test"}};
    
    // Base Tool::validate_input does basic validation
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("Tool.Advanced.ValidateInput.EmptyInput", "[Tool][Advanced]") {
    nlohmann::json schema = {{"type", "object"}};
    
    AdvancedMockTool tool("empty_validator", schema);
    nlohmann::json input = nlohmann::json::object();
    
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("Tool.Advanced.FormatValidationError", "[Tool][Advanced]") {
    AdvancedMockTool tool("error_formatter", {{"type", "object"}});
    
    std::string error_msg = tool.format_validation_error("Missing required field");
    REQUIRE_FALSE(error_msg.empty());
    REQUIRE(error_msg.find("error_formatter") != std::string::npos);
}

// ==================== ToolResult Advanced Tests ====================

TEST_CASE("Tool.Advanced.Result.SuccessWithComplexMetadata", "[Tool][Advanced]") {
    nlohmann::json metadata = {
        {"files_processed", 42},
        {"errors", nlohmann::json::array({"error1", "error2"})},
        {"timing", {
            {"start", "2026-03-17T00:00:00Z"},
            {"end", "2026-03-17T00:01:00Z"},
            {"duration_ms", 60000}
        }}
    };
    
    auto result = ToolResult::success("Complex Operation", "Processed files", metadata);
    
    REQUIRE(result.title == "Complex Operation");
    REQUIRE(result.metadata["files_processed"] == 42);
    REQUIRE(result.metadata["errors"].size() == 2);
    REQUIRE(result.metadata["timing"]["duration_ms"] == 60000);
}

TEST_CASE("Tool.Advanced.Result.ErrorWithMetadata", "[Tool][Advanced]") {
    nlohmann::json metadata = {{"error_code", "E001"}, {"retryable", true}};
    
    auto result = ToolResult::error("Operation Failed", "Detailed error message", metadata);
    
    REQUIRE(result.is_error);
    REQUIRE(result.title == "Operation Failed");
    REQUIRE(result.output == "Detailed error message");
    REQUIRE(result.metadata["error_code"] == "E001");
    REQUIRE(result.metadata["retryable"] == true);
}

TEST_CASE("Tool.Advanced.Result.JsonRoundTrip", "[Tool][Advanced]") {
    auto original = ToolResult::success("Round Trip", "Test output", {{"key", "value"}});
    
    nlohmann::json j = original.to_json();
    auto restored = ToolResult::from_json(j);
    
    REQUIRE(restored.title == original.title);
    REQUIRE(restored.output == original.output);
    REQUIRE(restored.is_error == original.is_error);
    REQUIRE(restored.metadata["key"] == "value");
}

TEST_CASE("Tool.Advanced.Result.ErrorJsonRoundTrip", "[Tool][Advanced]") {
    auto original = ToolResult::error("Error Round Trip", "Error output", {{"code", 500}});
    
    nlohmann::json j = original.to_json();
    auto restored = ToolResult::from_json(j);
    
    REQUIRE(restored.is_error);
    REQUIRE(restored.title == "Error Round Trip");
    REQUIRE(restored.metadata["code"] == 500);
}

// ==================== ToolConfig Advanced Tests ====================

TEST_CASE("Tool.Advanced.Config.ExtremeTimeouts", "[Tool][Advanced]") {
    ToolConfig config;
    config.default_timeout = 0;
    config.max_timeout = 3600;  // 1 hour
    
    nlohmann::json j = config.to_json();
    auto restored = ToolConfig::from_json(j);
    
    REQUIRE(restored.default_timeout == 0);
    REQUIRE(restored.max_timeout == 3600);
}

TEST_CASE("Tool.Advanced.Config.AllFlagsEnabled", "[Tool][Advanced]") {
    ToolConfig config;
    config.shell_mode = ShellMode::Normal;
    config.auto_approve_read = true;
    config.auto_approve_edit = true;
    
    nlohmann::json j = config.to_json();
    auto restored = ToolConfig::from_json(j);
    
    REQUIRE(restored.shell_mode == ShellMode::Normal);
    REQUIRE(restored.auto_approve_read == true);
    REQUIRE(restored.auto_approve_edit == true);
}

TEST_CASE("Tool.Advanced.Config.AllFlagsDisabled", "[Tool][Advanced]") {
    ToolConfig config;
    config.shell_mode = ShellMode::Sandbox;
    config.auto_approve_read = false;
    config.auto_approve_edit = false;
    
    nlohmann::json j = config.to_json();
    auto restored = ToolConfig::from_json(j);
    
    REQUIRE(restored.shell_mode == ShellMode::Sandbox);
    REQUIRE(restored.auto_approve_read == false);
    REQUIRE(restored.auto_approve_edit == false);
}

// ==================== ToolContext Advanced Tests ====================

TEST_CASE("Tool.Advanced.Context.AbortFlag", "[Tool][Advanced]") {
    ToolContext ctx;
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    
    REQUIRE_FALSE(ctx.should_abort());
    
    ctx.abort_flag->store(true);
    REQUIRE(ctx.should_abort());
}

TEST_CASE("Tool.Advanced.Context.WithAllFields", "[Tool][Advanced]") {
    ToolContext ctx;
    ctx.session_id = "session-advanced";
    ctx.message_id = "message-advanced";
    ctx.agent = "build";
    ctx.call_id = "call-advanced";
    ctx.working_directory = "/path/to/project";
    
    REQUIRE(ctx.session_id == "session-advanced");
    REQUIRE(ctx.message_id == "message-advanced");
    REQUIRE(ctx.agent == "build");
    REQUIRE(ctx.call_id == "call-advanced");
    REQUIRE(ctx.working_directory == "/path/to/project");
}

TEST_CASE("Tool.Advanced.Context.WithToolConfig", "[Tool][Advanced]") {
    ToolContext ctx;
    ctx.tool_config.default_timeout = 300;
    ctx.tool_config.shell_mode = ShellMode::Sandbox;
    
    REQUIRE(ctx.tool_config.default_timeout == 300);
    REQUIRE(ctx.tool_config.shell_mode == ShellMode::Sandbox);
}

// ==================== ShellMode Edge Cases ====================

TEST_CASE("Tool.Advanced.ShellMode.AllModes", "[Tool][Advanced]") {
    REQUIRE(shell_mode_to_string(ShellMode::Normal) == "normal");
    REQUIRE(shell_mode_to_string(ShellMode::Sandbox) == "sandbox");
    REQUIRE(shell_mode_to_string(ShellMode::Ask) == "ask");
    
    REQUIRE(string_to_shell_mode("normal") == ShellMode::Normal);
    REQUIRE(string_to_shell_mode("sandbox") == ShellMode::Sandbox);
    REQUIRE(string_to_shell_mode("ask") == ShellMode::Ask);
}

// ==================== Tool Execution with Context ====================

TEST_CASE("Tool.Advanced.Execute.WithAbortFlag", "[Tool][Advanced]") {
    AdvancedMockTool tool("abort_test", {{"type", "object"}});
    
    ToolContext ctx;
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(true);
    
    auto result = tool.execute({{"test", "data"}}, ctx);
    
    REQUIRE(result.is_error);
    REQUIRE(result.title == "Aborted");
}

TEST_CASE("Tool.Advanced.Execute.WithoutAbortFlag", "[Tool][Advanced]") {
    AdvancedMockTool tool("no_abort_test", {{"type", "object"}});
    
    ToolContext ctx;
    // No abort flag set
    
    auto result = tool.execute({{"test", "data"}}, ctx);
    
    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.title == "no_abort_test executed");
}
