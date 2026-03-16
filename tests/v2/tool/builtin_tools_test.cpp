/**
 * @file builtin_tools_test.cpp
 * @brief Tests for builtin tools: BashTool, EditTool, ApplyPatchTool, etc.
 *
 * Tests for:
 * - BashTool: command execution, sandbox mode, timeout
 * - EditTool: file editing, string replacement
 * - ApplyPatchTool: patch application
 * - WriteFileTool: file writing
 * - ReadFileTool: file reading
 */

#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/tool/tool.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/tool/builtin/bash_tool.hpp>
#include <turbot/core/tool/builtin/edit_tool.hpp>
#include <turbot/core/tool/builtin/read_file_tool.hpp>
#include <turbot/core/tool/builtin/write_file_tool.hpp>
#include <turbot/core/tool/builtin/list_tool.hpp>
#include <turbot/core/tool/builtin/glob_tool.hpp>
#include <turbot/core/tool/builtin/grep_tool.hpp>
#include <turbot/core/permission/permission.hpp>
#include <filesystem>
#include <fstream>

using namespace turbot::core::tool;
using namespace turbot::core::tool::builtin;
using namespace turbot::core::permission;
using namespace turbot::test;
namespace fs = std::filesystem;

// ==================== Helper Functions ====================

static ToolContext make_tool_ctx() {
    ToolContext ctx;
    ctx.session_id = "test-session";
    ctx.message_id = "test-message";
    ctx.agent = "test-agent";
    ctx.ruleset = {
        {"execute", "*", PermissionAction::Allow},
        {"read", "*", PermissionAction::Allow},
        {"write", "*", PermissionAction::Allow},
        {"edit", "*", PermissionAction::Allow},
        {"list", "*", PermissionAction::Allow},
        {"glob", "*", PermissionAction::Allow},
        {"grep", "*", PermissionAction::Allow}
    };
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    return ctx;
}

static std::string create_temp_file(const std::string& content) {
    static int counter = 0;
    std::string path = "/tmp/turbot_test_" + std::to_string(++counter) + ".txt";
    std::ofstream ofs(path);
    ofs << content;
    ofs.close();
    return path;
}

// ==================== BashTool Tests ====================

TEST_CASE("BashTool.Name", "[Tool][Builtin][Bash]") {
    BashTool tool;
    REQUIRE(tool.name() == "bash");
}

TEST_CASE("BashTool.Description", "[Tool][Builtin][Bash]") {
    BashTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("BashTool.InputSchema", "[Tool][Builtin][Bash]") {
    BashTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("command"));
}

TEST_CASE("BashTool.ValidateInput.Valid", "[Tool][Builtin][Bash]") {
    BashTool tool;
    nlohmann::json input = {
        {"command", "echo hello"}
    };
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("BashTool.ValidateInput.MissingCommand", "[Tool][Builtin][Bash]") {
    BashTool tool;
    nlohmann::json input = {};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("BashTool.Execute.SimpleCommand", "[Tool][Builtin][Bash]") {
    BashTool tool;
    auto ctx = make_tool_ctx();
    nlohmann::json input = {
        {"command", "echo hello"}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.output.find("hello") != std::string::npos);
}

TEST_CASE("BashTool.Execute.WithDescription", "[Tool][Builtin][Bash]") {
    BashTool tool;
    auto ctx = make_tool_ctx();
    nlohmann::json input = {
        {"command", "echo test"},
        {"description", "Test echo command"}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE(result.title == "Test echo command");
}

TEST_CASE("BashTool.Execute.FailingCommand", "[Tool][Builtin][Bash]") {
    BashTool tool;
    auto ctx = make_tool_ctx();
    nlohmann::json input = {
        {"command", "exit 1"}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE(result.is_error);
}

TEST_CASE("BashTool.Execute.WithTimeout", "[Tool][Builtin][Bash]") {
    BashTool tool;
    auto ctx = make_tool_ctx();
    nlohmann::json input = {
        {"command", "echo timed"},
        {"timeout", 5000}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);
}

TEST_CASE("BashToolParams.FromJson", "[Tool][Builtin][Bash]") {
    nlohmann::json j = {
        {"command", "ls -la"},
        {"description", "List files"},
        {"timeout", 10000},
        {"workdir", "/tmp"}
    };
    auto params = BashToolParams::from_json(j);
    REQUIRE(params.command == "ls -la");
    REQUIRE(params.description == "List files");
    REQUIRE(params.timeout.value() == 10000);
    REQUIRE(params.workdir.value() == "/tmp");
}

TEST_CASE("BashToolParams.ToJson", "[Tool][Builtin][Bash]") {
    BashToolParams params;
    params.command = "pwd";
    params.description = "Print working directory";
    params.timeout = 5000;
    params.workdir = "/home";
    
    auto j = params.to_json();
    REQUIRE(j["command"] == "pwd");
    REQUIRE(j["description"] == "Print working directory");
    REQUIRE(j["timeout"] == 5000);
    REQUIRE(j["workdir"] == "/home");
}

// ==================== EditTool Tests ====================

TEST_CASE("EditTool.Name", "[Tool][Builtin][Edit]") {
    EditTool tool;
    REQUIRE(tool.name() == "edit");
}

TEST_CASE("EditTool.Description", "[Tool][Builtin][Edit]") {
    EditTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("EditTool.InputSchema", "[Tool][Builtin][Edit]") {
    EditTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("filePath"));
    REQUIRE(schema["properties"].contains("oldString"));
    REQUIRE(schema["properties"].contains("newString"));
}

TEST_CASE("EditTool.ValidateInput.Valid", "[Tool][Builtin][Edit]") {
    EditTool tool;
    nlohmann::json input = {
        {"filePath", "/tmp/test.txt"},
        {"oldString", "hello"},
        {"newString", "world"}
    };
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("EditTool.ValidateInput.MissingFilePath", "[Tool][Builtin][Edit]") {
    EditTool tool;
    nlohmann::json input = {
        {"oldString", "hello"},
        {"newString", "world"}
    };
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("EditTool.ValidateInput.MissingOldString", "[Tool][Builtin][Edit]") {
    EditTool tool;
    nlohmann::json input = {
        {"filePath", "/tmp/test.txt"},
        {"newString", "world"}
    };
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("EditTool.ValidateInput.SameStrings", "[Tool][Builtin][Edit]") {
    EditTool tool;
    nlohmann::json input = {
        {"filePath", "/tmp/test.txt"},
        {"oldString", "same"},
        {"newString", "same"}
    };
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("EditToolParams.FromJson", "[Tool][Builtin][Edit]") {
    nlohmann::json j = {
        {"filePath", "/tmp/test.txt"},
        {"oldString", "old"},
        {"newString", "new"},
        {"replaceAll", true}
    };
    auto params = EditToolParams::from_json(j);
    REQUIRE(params.file_path == "/tmp/test.txt");
    REQUIRE(params.old_string == "old");
    REQUIRE(params.new_string == "new");
    REQUIRE(params.replace_all == true);
}

TEST_CASE("EditToolParams.ToJson", "[Tool][Builtin][Edit]") {
    EditToolParams params;
    params.file_path = "/tmp/test.txt";
    params.old_string = "old";
    params.new_string = "new";
    params.replace_all = true;
    
    auto j = params.to_json();
    REQUIRE(j["filePath"] == "/tmp/test.txt");
    REQUIRE(j["oldString"] == "old");
    REQUIRE(j["newString"] == "new");
    REQUIRE(j["replaceAll"] == true);
}

// ==================== ReadFileTool Tests ====================

TEST_CASE("ReadFileTool.Name", "[Tool][Builtin][Read]") {
    ReadFileTool tool;
    REQUIRE(tool.name() == "read");
}

TEST_CASE("ReadFileTool.Description", "[Tool][Builtin][Read]") {
    ReadFileTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("ReadFileTool.InputSchema", "[Tool][Builtin][Read]") {
    ReadFileTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("filePath"));
}

TEST_CASE("ReadFileTool.ValidateInput.Valid", "[Tool][Builtin][Read]") {
    ReadFileTool tool;
    nlohmann::json input = {
        {"filePath", "/tmp/test.txt"}
    };
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("ReadFileTool.ValidateInput.MissingPath", "[Tool][Builtin][Read]") {
    ReadFileTool tool;
    nlohmann::json input = {};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("ReadFileTool.Execute.ReadFile", "[Tool][Builtin][Read]") {
    ReadFileTool tool;
    auto ctx = make_tool_ctx();
    
    // Create a temp file
    std::string path = create_temp_file("Hello, World!");
    
    nlohmann::json input = {
        {"filePath", path}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.output.find("Hello, World!") != std::string::npos);
    
    // Cleanup
    fs::remove(path);
}

TEST_CASE("ReadFileTool.Execute.NonExistentFile", "[Tool][Builtin][Read]") {
    ReadFileTool tool;
    auto ctx = make_tool_ctx();
    nlohmann::json input = {
        {"filePath", "/nonexistent/path/file.txt"}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE(result.is_error);
}

// ==================== WriteFileTool Tests ====================

TEST_CASE("WriteFileTool.Name", "[Tool][Builtin][Write]") {
    WriteFileTool tool;
    REQUIRE(tool.name() == "write");
}

TEST_CASE("WriteFileTool.Description", "[Tool][Builtin][Write]") {
    WriteFileTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("WriteFileTool.InputSchema", "[Tool][Builtin][Write]") {
    WriteFileTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("filePath"));
    REQUIRE(schema["properties"].contains("content"));
}

TEST_CASE("WriteFileTool.ValidateInput.Valid", "[Tool][Builtin][Write]") {
    WriteFileTool tool;
    nlohmann::json input = {
        {"filePath", "/tmp/test.txt"},
        {"content", "Hello"}
    };
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("WriteFileTool.ValidateInput.MissingPath", "[Tool][Builtin][Write]") {
    WriteFileTool tool;
    nlohmann::json input = {
        {"content", "Hello"}
    };
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("WriteFileTool.ValidateInput.MissingContent", "[Tool][Builtin][Write]") {
    WriteFileTool tool;
    nlohmann::json input = {
        {"filePath", "/tmp/test.txt"}
    };
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("WriteFileTool.Execute.WriteFile", "[Tool][Builtin][Write]") {
    WriteFileTool tool;
    auto ctx = make_tool_ctx();
    
    std::string path = "/tmp/turbot_write_test_" + std::to_string(std::time(nullptr)) + ".txt";
    
    nlohmann::json input = {
        {"filePath", path},
        {"content", "Test content"}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);
    
    // Verify file was created
    REQUIRE(fs::exists(path));
    
    // Verify content
    std::ifstream ifs(path);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    REQUIRE(content == "Test content");
    
    // Cleanup
    fs::remove(path);
}

// ==================== ListTool Tests ====================

TEST_CASE("ListTool.Name", "[Tool][Builtin][List]") {
    ListTool tool;
    REQUIRE(tool.name() == "list");
}

TEST_CASE("ListTool.Description", "[Tool][Builtin][List]") {
    ListTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("ListTool.InputSchema", "[Tool][Builtin][List]") {
    ListTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("path"));
}

TEST_CASE("ListTool.ValidateInput.Valid", "[Tool][Builtin][List]") {
    ListTool tool;
    nlohmann::json input = {
        {"path", "/tmp"}
    };
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("ListTool.Execute.ListDirectory", "[Tool][Builtin][List]") {
    ListTool tool;
    auto ctx = make_tool_ctx();
    
    // Create temp directory with files
    std::string dir = "/tmp/turbot_list_test_" + std::to_string(std::time(nullptr));
    fs::create_directory(dir);
    std::ofstream(dir + "/file1.txt") << "content1";
    std::ofstream(dir + "/file2.txt") << "content2";
    
    nlohmann::json input = {
        {"path", dir}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.output.find("file1.txt") != std::string::npos);
    REQUIRE(result.output.find("file2.txt") != std::string::npos);
    
    // Cleanup
    fs::remove_all(dir);
}

// ==================== GlobTool Tests ====================

TEST_CASE("GlobTool.Name", "[Tool][Builtin][Glob]") {
    GlobTool tool;
    REQUIRE(tool.name() == "glob");
}

TEST_CASE("GlobTool.Description", "[Tool][Builtin][Glob]") {
    GlobTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("GlobTool.InputSchema", "[Tool][Builtin][Glob]") {
    GlobTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("pattern"));
}

TEST_CASE("GlobTool.ValidateInput.Valid", "[Tool][Builtin][Glob]") {
    GlobTool tool;
    nlohmann::json input = {
        {"pattern", "*.txt"}
    };
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("GlobTool.ValidateInput.MissingPattern", "[Tool][Builtin][Glob]") {
    GlobTool tool;
    nlohmann::json input = {};
    REQUIRE_FALSE(tool.validate_input(input));
}

// ==================== GrepTool Tests ====================

TEST_CASE("GrepTool.Name", "[Tool][Builtin][Grep]") {
    GrepTool tool;
    REQUIRE(tool.name() == "grep");
}

TEST_CASE("GrepTool.Description", "[Tool][Builtin][Grep]") {
    GrepTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("GrepTool.InputSchema", "[Tool][Builtin][Grep]") {
    GrepTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("pattern"));
}

TEST_CASE("GrepTool.ValidateInput.Valid", "[Tool][Builtin][Grep]") {
    GrepTool tool;
    nlohmann::json input = {
        {"pattern", "test"}
    };
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("GrepTool.ValidateInput.MissingPattern", "[Tool][Builtin][Grep]") {
    GrepTool tool;
    nlohmann::json input = {
        {"path", "/tmp"}
    };
    REQUIRE_FALSE(tool.validate_input(input));
}

// ==================== Tool Registry Integration ====================

TEST_CASE("Tool.Registry.BuiltinToolsRegistration", "[Tool][Builtin]") {
    auto& registry = ToolRegistry::instance();
    
    // Register builtin tools
    registry.register_tool(std::make_unique<BashTool>());
    registry.register_tool(std::make_unique<EditTool>());
    registry.register_tool(std::make_unique<ReadFileTool>());
    registry.register_tool(std::make_unique<WriteFileTool>());
    registry.register_tool(std::make_unique<ListTool>());
    registry.register_tool(std::make_unique<GlobTool>());
    registry.register_tool(std::make_unique<GrepTool>());
    
    REQUIRE(registry.has("bash"));
    REQUIRE(registry.has("edit"));
    REQUIRE(registry.has("read"));
    REQUIRE(registry.has("write"));
    REQUIRE(registry.has("list"));
    REQUIRE(registry.has("glob"));
    REQUIRE(registry.has("grep"));
}

TEST_CASE("Tool.Registry.GetBuiltinTool", "[Tool][Builtin]") {
    auto& registry = ToolRegistry::instance();
    
    // Register builtin tools first
    registry.register_tool(std::make_unique<BashTool>());
    registry.register_tool(std::make_unique<EditTool>());
    registry.register_tool(std::make_unique<ReadFileTool>());
    
    auto bash_tool = registry.get("bash");
    REQUIRE(bash_tool != nullptr);
    REQUIRE(bash_tool->name() == "bash");
    
    auto edit_tool = registry.get("edit");
    REQUIRE(edit_tool != nullptr);
    REQUIRE(edit_tool->name() == "edit");
    
    auto read_tool = registry.get("read");
    REQUIRE(read_tool != nullptr);
    REQUIRE(read_tool->name() == "read");
}
