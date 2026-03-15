#include <catch2/catch_test_macros.hpp>
#include <turbot/core/tool/builtin/websearch_tool.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/permission/permission.hpp>
#include <memory>
#include <atomic>

using namespace turbot::core::tool;
using namespace turbot::core::tool::builtin;
using namespace turbot::core::permission;

// ============================================================================
// Helper: Create a minimal ToolContext
// ============================================================================

static ToolContext make_test_ctx() {
    ToolContext ctx;
    ctx.session_id = "test-session";
    ctx.message_id = "test-message";
    ctx.agent = "test-agent";
    ctx.ruleset = {{"websearch", "*", PermissionAction::Allow}};
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    return ctx;
}

static ToolContext make_deny_ctx() {
    ToolContext ctx;
    ctx.session_id = "test-session";
    ctx.message_id = "test-message";
    ctx.agent = "test-agent";
    ctx.ruleset = {{"websearch", "*", PermissionAction::Deny}};
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    return ctx;
}

// ============================================================================
// WebSearchTool Tests
// ============================================================================

TEST_CASE("WebSearchTool: name and description", "[core][tool][websearch]") {
    WebSearchTool tool;
    REQUIRE(tool.name() == "websearch");
    REQUIRE_FALSE(tool.description().empty());
    // Description should mention search
    REQUIRE(tool.description().find("search") != std::string::npos);
}

TEST_CASE("WebSearchTool: input_schema", "[core][tool][websearch]") {
    WebSearchTool tool;
    const auto schema = tool.input_schema();
    
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"]["query"]["type"] == "string");
    REQUIRE(schema["required"].is_array());
    
    // Check optional parameters exist
    REQUIRE(schema["properties"].contains("numResults"));
    REQUIRE(schema["properties"].contains("livecrawl"));
    REQUIRE(schema["properties"].contains("type"));
}

TEST_CASE("WebSearchTool: validate_input - valid cases", "[core][tool][websearch]") {
    WebSearchTool tool;
    
    // Minimal valid input
    nlohmann::json input = {{"query", "test search"}};
    REQUIRE(tool.validate_input(input));
    
    // With optional parameters
    input = {
        {"query", "another test"},
        {"numResults", 5},
        {"livecrawl", "fallback"},
        {"type", "auto"}
    };
    REQUIRE(tool.validate_input(input));
    
    // With contextMaxCharacters
    input = {
        {"query", "test"},
        {"contextMaxCharacters", 5000}
    };
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("WebSearchTool: validate_input - invalid cases", "[core][tool][websearch]") {
    WebSearchTool tool;
    
    // Missing query
    nlohmann::json input = nlohmann::json::object();
    REQUIRE_FALSE(tool.validate_input(input));
    
    // Empty query
    input = {{"query", ""}};
    REQUIRE_FALSE(tool.validate_input(input));
    
    // Query too long (over 1000 chars)
    input = {{"query", std::string(1001, 'a')}};
    REQUIRE_FALSE(tool.validate_input(input));
    
    // Wrong type for query
    input = {{"query", 123}};
    REQUIRE_FALSE(tool.validate_input(input));
    
    // Wrong type for numResults
    input = {{"query", "test"}, {"numResults", "five"}};
    REQUIRE_FALSE(tool.validate_input(input));
    
    // Wrong type for livecrawl
    input = {{"query", "test"}, {"livecrawl", 123}};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("WebSearchTool: execute - invalid input returns error", "[core][tool][websearch]") {
    WebSearchTool tool;
    auto ctx = make_test_ctx();
    
    // Missing query
    nlohmann::json input = nlohmann::json::object();
    auto result = tool.execute(input, ctx);
    REQUIRE(result.is_error);
    REQUIRE(result.output.find("Invalid input") != std::string::npos);
}

TEST_CASE("WebSearchTool: execute - empty query returns error", "[core][tool][websearch]") {
    WebSearchTool tool;
    auto ctx = make_test_ctx();
    
    nlohmann::json input = {{"query", ""}};
    auto result = tool.execute(input, ctx);
    REQUIRE(result.is_error);
}

TEST_CASE("WebSearchTool: execute - abort flag respected", "[core][tool][websearch]") {
    WebSearchTool tool;
    auto ctx = make_test_ctx();
    *ctx.abort_flag = true;  // Set abort flag
    
    nlohmann::json input = {{"query", "test query"}};
    auto result = tool.execute(input, ctx);
    REQUIRE(result.is_error);
    REQUIRE(result.output.find("aborted") != std::string::npos);
}

// ============================================================================
// ToolRegistry Integration Tests
// ============================================================================

TEST_CASE("ToolRegistry: websearch tool registered", "[core][tool][websearch]") {
    auto& registry = ToolRegistry::instance();
    registry.register_builtin_tools();
    
    REQUIRE(registry.has("websearch"));
    
    auto tool = registry.get("websearch");
    REQUIRE(tool != nullptr);
    REQUIRE(tool->name() == "websearch");
}

TEST_CASE("ToolRegistry: websearch in tool definitions", "[core][tool][websearch]") {
    auto& registry = ToolRegistry::instance();
    registry.register_builtin_tools();
    
    auto defs = registry.to_tool_definitions();
    
    // Find websearch in definitions
    bool found = false;
    for (const auto& def : defs) {
        if (def.contains("function") && def["function"].contains("name") && 
            def["function"]["name"] == "websearch") {
            found = true;
            REQUIRE(def["function"].contains("description"));
            REQUIRE(def["function"].contains("parameters"));
            break;
        }
    }
    REQUIRE(found);
}

TEST_CASE("ToolRegistry: websearch tool count", "[core][tool][websearch]") {
    auto& registry = ToolRegistry::instance();
    registry.register_builtin_tools();
    
    // After registration, should have at least 12 tools
    // (read, write, edit, multiedit, bash, glob, grep, list, codesearch, webfetch, websearch, task)
    REQUIRE(registry.size() >= 12);
}
