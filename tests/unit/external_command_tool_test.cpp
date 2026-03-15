#include <turbot/core/tool/external_command_tool.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

namespace turbot::core::tool::test {

// Helper to create a temporary tool config file
class TempToolConfig {
public:
    TempToolConfig(const std::string& name, const nlohmann::json& config)
        : path_(std::filesystem::temp_directory_path() / (name + ".json")) {
        std::ofstream file(path_);
        file << config.dump(2);
    }
    
    ~TempToolConfig() {
        std::filesystem::remove(path_);
    }
    
    const std::string& path() const { return path_; }
    
private:
    std::string path_;
};

TEST_CASE("CustomToolConfig::from_json - valid config", "[tool][external_command]") {
    nlohmann::json j = {
        {"name", "test_tool"},
        {"description", "A test tool"},
        {"command", "echo"},
        {"args", nlohmann::json::array({"${message}"})},
        {"input_schema", {
            {"type", "object"},
            {"properties", {{"message", {{"type", "string"}}}}},
            {"required", nlohmann::json::array({"message"})}
        }},
        {"timeout_seconds", 30},
        {"enabled", true}
    };
    
    auto config = CustomToolConfig::from_json(j);
    REQUIRE(config.has_value());
    CHECK(config->name == "test_tool");
    CHECK(config->description == "A test tool");
    CHECK(config->command == "echo");
    CHECK(config->args.size() == 1);
    CHECK(config->args[0] == "${message}");
    CHECK(config->timeout_seconds == 30);
    CHECK(config->enabled == true);
}

TEST_CASE("CustomToolConfig::from_json - minimal config", "[tool][external_command]") {
    nlohmann::json j = {
        {"name", "minimal_tool"},
        {"command", "ls"}
    };
    
    auto config = CustomToolConfig::from_json(j);
    REQUIRE(config.has_value());
    CHECK(config->name == "minimal_tool");
    CHECK(config->command == "ls");
    CHECK(config->description == "Custom tool: minimal_tool");
    CHECK(config->enabled == true);  // Default
    CHECK_FALSE(config->timeout_seconds.has_value());
}

TEST_CASE("CustomToolConfig::from_json - missing required fields", "[tool][external_command]") {
    SECTION("missing name") {
        nlohmann::json j = {{"command", "ls"}};
        CHECK_FALSE(CustomToolConfig::from_json(j).has_value());
    }
    
    SECTION("missing command") {
        nlohmann::json j = {{"name", "test"}};
        CHECK_FALSE(CustomToolConfig::from_json(j).has_value());
    }
}

TEST_CASE("ExternalCommandTool - basic execution", "[tool][external_command]") {
    CustomToolConfig config;
    config.name = "echo_test";
    config.description = "Echo test tool";
    config.command = "echo";
    config.args = {"${message}"};
    config.input_schema = {
        {"type", "object"},
        {"properties", {{"message", {{"type", "string"}}}}}
    };
    
    ExternalCommandTool tool(config);
    
    CHECK(tool.name() == "echo_test");
    CHECK(tool.description() == "Echo test tool");
    CHECK(tool.input_schema()["type"] == "object");
}

TEST_CASE("ExternalCommandTool::substitute_args", "[tool][external_command]") {
    CustomToolConfig config;
    config.name = "test";
    config.command = "test";
    config.args = {"${foo}", "${bar.nested}", "literal"};
    
    ExternalCommandTool tool(config);
    
    nlohmann::json input = {
        {"foo", "hello"},
        {"bar", {{"nested", "world"}}}
    };
    
    // Access private method via friend class or public interface
    // For now, test through execute
    ToolContext ctx;
    ctx.working_directory = "/tmp";
    
    // This is an integration test - would need better mocking for unit test
}

TEST_CASE("ToolRegistry::discover_custom_tools - empty directory", "[tool][registry]") {
    ToolRegistry::instance().clear();
    
    // Create a temp directory with no tools
    std::string temp_dir = std::filesystem::temp_directory_path() / "turbot_test_empty";
    std::filesystem::create_directories(temp_dir + "/.turbot/tools");
    
    size_t count = ToolRegistry::instance().discover_custom_tools(temp_dir);
    CHECK(count == 0);
    
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("ToolRegistry::discover_custom_tools - with valid tool", "[tool][registry]") {
    ToolRegistry::instance().clear();
    
    // Create a temp directory with a tool
    std::string temp_dir = std::filesystem::temp_directory_path() / "turbot_test_tools";
    std::string tools_dir = temp_dir + "/.turbot/tools";
    std::filesystem::create_directories(tools_dir);
    
    // Write a valid tool config
    nlohmann::json tool_config = {
        {"name", "test_echo"},
        {"command", "echo"},
        {"args", nlohmann::json::array({"hello"})},
        {"enabled", true}
    };
    
    std::ofstream file(tools_dir + "/test_echo.json");
    file << tool_config.dump(2);
    file.close();
    
    size_t count = ToolRegistry::instance().discover_custom_tools(temp_dir);
    CHECK(count == 1);
    CHECK(ToolRegistry::instance().has("test_echo"));
    
    std::filesystem::remove_all(temp_dir);
    ToolRegistry::instance().clear();
}

TEST_CASE("ToolRegistry::discover_custom_tools - disabled tool", "[tool][registry]") {
    ToolRegistry::instance().clear();
    
    std::string temp_dir = std::filesystem::temp_directory_path() / "turbot_test_disabled";
    std::string tools_dir = temp_dir + "/.turbot/tools";
    std::filesystem::create_directories(tools_dir);
    
    nlohmann::json tool_config = {
        {"name", "disabled_tool"},
        {"command", "echo"},
        {"enabled", false}
    };
    
    std::ofstream file(tools_dir + "/disabled.json");
    file << tool_config.dump(2);
    file.close();
    
    size_t count = ToolRegistry::instance().discover_custom_tools(temp_dir);
    CHECK(count == 0);  // Disabled tools should not be registered
    CHECK_FALSE(ToolRegistry::instance().has("disabled_tool"));
    
    std::filesystem::remove_all(temp_dir);
    ToolRegistry::instance().clear();
}

TEST_CASE("CustomToolConfig::to_json roundtrip", "[tool][external_command]") {
    CustomToolConfig original;
    original.name = "roundtrip_test";
    original.description = "Test roundtrip";
    original.command = "test";
    original.args = {"arg1", "arg2"};
    original.timeout_seconds = 60;
    original.working_dir = "/tmp";
    original.enabled = false;
    original.input_schema = {{"type", "object"}};
    
    nlohmann::json j = original.to_json();
    auto restored = CustomToolConfig::from_json(j);
    
    REQUIRE(restored.has_value());
    CHECK(restored->name == original.name);
    CHECK(restored->description == original.description);
    CHECK(restored->command == original.command);
    CHECK(restored->args == original.args);
    CHECK(restored->timeout_seconds == original.timeout_seconds);
    CHECK(restored->working_dir == original.working_dir);
    CHECK(restored->enabled == original.enabled);
}

} // namespace turbot::core::tool::test
