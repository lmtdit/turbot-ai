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
#include <turbot/core/tool/builtin/apply_patch_tool.hpp>
#include <turbot/core/tool/builtin/batch_tool.hpp>
#include <turbot/core/tool/builtin/todo_tool.hpp>
#include <turbot/core/tool/builtin/invalid_tool.hpp>
#include <turbot/core/tool/builtin/question_tool.hpp>
#include <turbot/core/tool/builtin/task_tool.hpp>
#include <turbot/core/tool/builtin/plan_tool.hpp>
#include <turbot/core/tool/builtin/multiedit_tool.hpp>
#include <turbot/core/tool/builtin/webfetch_tool.hpp>
#include <turbot/core/tool/builtin/codesearch_tool.hpp>
#include <turbot/core/tool/builtin/websearch_tool.hpp>
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

TEST_CASE("EditTool.Execute.SimpleEdit", "[Tool][Builtin][Edit]") {
    EditTool tool;
    auto ctx = make_tool_ctx();
    
    // Create a temp file
    std::string path = create_temp_file("Hello World\nThis is a test\nGoodbye World");
    
    nlohmann::json input = {
        {"filePath", path},
        {"oldString", "This is a test"},
        {"newString", "This is modified"}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);
    
    // Verify the edit
    std::ifstream ifs(path);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    REQUIRE(content.find("This is modified") != std::string::npos);
    REQUIRE(content.find("This is a test") == std::string::npos);
    
    // Cleanup
    fs::remove(path);
}

TEST_CASE("EditTool.Execute.ReplaceAll", "[Tool][Builtin][Edit]") {
    EditTool tool;
    auto ctx = make_tool_ctx();
    
    // Create a temp file with multiple occurrences
    std::string path = create_temp_file("foo bar foo baz foo");
    
    nlohmann::json input = {
        {"filePath", path},
        {"oldString", "foo"},
        {"newString", "qux"},
        {"replaceAll", true}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);
    
    // Verify all occurrences were replaced
    std::ifstream ifs(path);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    REQUIRE(content == "qux bar qux baz qux");
    
    // Cleanup
    fs::remove(path);
}

TEST_CASE("EditTool.Execute.FileNotFound", "[Tool][Builtin][Edit]") {
    EditTool tool;
    auto ctx = make_tool_ctx();
    
    nlohmann::json input = {
        {"filePath", "/nonexistent/path/file.txt"},
        {"oldString", "old"},
        {"newString", "new"}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE(result.is_error);
}

TEST_CASE("EditTool.Execute.OldStringNotFound", "[Tool][Builtin][Edit]") {
    EditTool tool;
    auto ctx = make_tool_ctx();
    
    std::string path = create_temp_file("Hello World");
    
    nlohmann::json input = {
        {"filePath", path},
        {"oldString", "NonExistentString"},
        {"newString", "new"}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE(result.is_error);
    
    // Cleanup
    fs::remove(path);
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
    std::string dir = "/Users/jg/Codes/turbot/turbot-ai/test_output/turbot_list_test_" + std::to_string(std::time(nullptr));
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

TEST_CASE("GlobTool.Execute.WithPattern", "[Tool][Builtin][Glob]") {
    GlobTool tool;
    auto ctx = make_tool_ctx();
    
    // Create temp directory with files
    std::string dir = "/Users/jg/Codes/turbot/turbot-ai/test_output/turbot_glob_test_" + std::to_string(std::time(nullptr));
    fs::create_directory(dir);
    std::ofstream(dir + "/test1.txt") << "content1";
    std::ofstream(dir + "/test2.txt") << "content2";
    std::ofstream(dir + "/test3.cpp") << "content3";
    
    // Set working directory to the test directory
    ctx.working_directory = dir;
    
    nlohmann::json input = {
        {"pattern", "*.txt"},
        {"path", dir}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);
    // Check that output contains .txt files (either relative or absolute path)
    bool found_txt = result.output.find(".txt") != std::string::npos;
    REQUIRE(found_txt);
    // Check that .cpp file is not included
    bool found_cpp = result.output.find("test3.cpp") != std::string::npos;
    REQUIRE_FALSE(found_cpp);
    
    // Cleanup
    fs::remove_all(dir);
}

TEST_CASE("GlobTool.Execute.RecursivePattern", "[Tool][Builtin][Glob]") {
    GlobTool tool;
    auto ctx = make_tool_ctx();
    
    // Create nested directory structure
    std::string dir = "/Users/jg/Codes/turbot/turbot-ai/test_output/turbot_glob_recursive_" + std::to_string(std::time(nullptr));
    fs::create_directory(dir);
    fs::create_directory(dir + "/subdir");
    std::ofstream(dir + "/file.txt") << "content";
    std::ofstream(dir + "/subdir/nested.txt") << "nested content";
    
    // Set working directory to the test directory
    ctx.working_directory = dir;
    
    nlohmann::json input = {
        {"pattern", "**/*.txt"},
        {"path", dir}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);
    // Check that output contains .txt files
    bool found_txt = result.output.find(".txt") != std::string::npos;
    REQUIRE(found_txt);
    
    // Cleanup
    fs::remove_all(dir);
}

TEST_CASE("GlobToolParams.FromJson", "[Tool][Builtin][Glob]") {
    nlohmann::json j = {
        {"pattern", "*.cpp"},
        {"path", "/src"}
    };
    auto params = GlobToolParams::from_json(j);
    REQUIRE(params.pattern == "*.cpp");
    REQUIRE(params.path.value() == "/src");
}

TEST_CASE("GlobToolParams.ToJson", "[Tool][Builtin][Glob]") {
    GlobToolParams params;
    params.pattern = "*.h";
    params.path = "/include";
    
    auto j = params.to_json();
    REQUIRE(j["pattern"] == "*.h");
    REQUIRE(j["path"] == "/include");
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

TEST_CASE("GrepTool.Execute.WithPattern", "[Tool][Builtin][Grep]") {
    GrepTool tool;
    auto ctx = make_tool_ctx();
    
    // Create temp directory with files
    std::string dir = "/Users/jg/Codes/turbot/turbot-ai/test_output/turbot_grep_test_" + std::to_string(std::time(nullptr));
    fs::create_directory(dir);
    std::ofstream(dir + "/file1.txt") << "hello world\nfoo bar\nhello again";
    std::ofstream(dir + "/file2.txt") << "no match here";
    
    // Set working directory to the test directory
    ctx.working_directory = dir;
    
    nlohmann::json input = {
        {"pattern", "hello"},
        {"path", dir}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);
    // Check that output contains matches
    bool found_hello = result.output.find("hello") != std::string::npos;
    REQUIRE(found_hello);
    
    // Cleanup
    fs::remove_all(dir);
}

TEST_CASE("GrepTool.Execute.WithInclude", "[Tool][Builtin][Grep]") {
    GrepTool tool;
    auto ctx = make_tool_ctx();
    
    // Create temp directory with files
    std::string dir = "/tmp/turbot_grep_include_" + std::to_string(std::time(nullptr));
    fs::create_directory(dir);
    std::ofstream(dir + "/test.cpp") << "int main() { return 0; }";
    std::ofstream(dir + "/test.txt") << "int main() { return 0; }";
    
    // Set working directory to the test directory
    ctx.working_directory = dir;
    
    nlohmann::json input = {
        {"pattern", "main"},
        {"path", dir},
        {"include", "*.cpp"}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);
    // Check that output contains matches
    bool found_main = result.output.find("main") != std::string::npos;
    REQUIRE(found_main);
    
    // Cleanup
    fs::remove_all(dir);
}

TEST_CASE("GrepTool.Execute.InvalidRegex", "[Tool][Builtin][Grep]") {
    GrepTool tool;
    auto ctx = make_tool_ctx();
    ctx.working_directory = "/tmp";
    
    nlohmann::json input = {
        {"pattern", "[invalid(regex"}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE(result.is_error);
}

TEST_CASE("GrepToolParams.FromJson", "[Tool][Builtin][Grep]") {
    nlohmann::json j = {
        {"pattern", "TODO"},
        {"path", "/src"},
        {"include", "*.cpp"}
    };
    auto params = GrepToolParams::from_json(j);
    REQUIRE(params.pattern == "TODO");
    REQUIRE(params.path.value() == "/src");
    REQUIRE(params.include.value() == "*.cpp");
}

TEST_CASE("GrepToolParams.ToJson", "[Tool][Builtin][Grep]") {
    GrepToolParams params;
    params.pattern = "FIXME";
    params.path = "/lib";
    params.include = "*.h";
    
    auto j = params.to_json();
    REQUIRE(j["pattern"] == "FIXME");
    REQUIRE(j["path"] == "/lib");
    REQUIRE(j["include"] == "*.h");
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

// ==================== ApplyPatchTool Tests ====================

TEST_CASE("ApplyPatchTool.Name", "[Tool][Builtin][ApplyPatch]") {
    ApplyPatchTool tool;
    REQUIRE(tool.name() == "apply_patch");
}

TEST_CASE("ApplyPatchTool.Description", "[Tool][Builtin][ApplyPatch]") {
    ApplyPatchTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("ApplyPatchTool.InputSchema", "[Tool][Builtin][ApplyPatch]") {
    ApplyPatchTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("patchText"));
}

TEST_CASE("ApplyPatchTool.ValidateInput.Valid", "[Tool][Builtin][ApplyPatch]") {
    ApplyPatchTool tool;
    nlohmann::json input = {
        {"patchText", "*** Begin Patch\n*** End Patch"}
    };
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("ApplyPatchTool.ValidateInput.MissingPatchText", "[Tool][Builtin][ApplyPatch]") {
    ApplyPatchTool tool;
    nlohmann::json input = {};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("ApplyPatchTool.ValidateInput.EmptyPatchText", "[Tool][Builtin][ApplyPatch]") {
    ApplyPatchTool tool;
    nlohmann::json input = {
        {"patchText", ""}
    };
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("ApplyPatchToolParams.FromJson", "[Tool][Builtin][ApplyPatch]") {
    nlohmann::json j = {
        {"patchText", "sample patch content"}
    };
    auto params = ApplyPatchToolParams::from_json(j);
    REQUIRE(params.patch_text == "sample patch content");
}

TEST_CASE("ApplyPatchToolParams.ToJson", "[Tool][Builtin][ApplyPatch]") {
    ApplyPatchToolParams params;
    params.patch_text = "test patch";
    
    auto j = params.to_json();
    REQUIRE(j["patchText"] == "test patch");
}

TEST_CASE("ApplyPatchTool.Execute.AddFile", "[Tool][Builtin][ApplyPatch]") {
    ApplyPatchTool tool;
    auto ctx = make_tool_ctx();
    
    // Create temp directory and get canonical path (resolve symlinks like /tmp -> /private/tmp)
    std::string dir = "/tmp/turbot_patch_test_" + std::to_string(std::time(nullptr));
    fs::create_directory(dir);
    ctx.working_directory = fs::canonical(dir).string();
    
    std::string patch = R"(*** Begin Patch
*** Add File: new_file.txt
+Hello World
+This is a new file
*** End Patch)";
    
    nlohmann::json input = {{"patchText", patch}};
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);
    REQUIRE(fs::exists(fs::canonical(dir) / "new_file.txt"));
    
    // Cleanup
    fs::remove_all(dir);
}

TEST_CASE("ApplyPatchTool.Execute.UpdateFile", "[Tool][Builtin][ApplyPatch]") {
    ApplyPatchTool tool;
    auto ctx = make_tool_ctx();
    
    // Create temp directory with existing file
    std::string dir = "/tmp/turbot_patch_update_" + std::to_string(std::time(nullptr));
    fs::create_directory(dir);
    std::ofstream(dir + "/test.txt") << "Hello World\n";
    ctx.working_directory = fs::canonical(dir).string();
    
    std::string patch = R"(*** Begin Patch
*** Update File: test.txt
@@
 Hello World
+New Line Added
*** End Patch)";
    
    nlohmann::json input = {{"patchText", patch}};
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);
    
    // Cleanup
    fs::remove_all(dir);
}

TEST_CASE("ApplyPatchTool.Execute.DeleteFile", "[Tool][Builtin][ApplyPatch]") {
    ApplyPatchTool tool;
    auto ctx = make_tool_ctx();
    
    // Create temp directory with file to delete
    std::string dir = "/tmp/turbot_patch_delete_" + std::to_string(std::time(nullptr));
    fs::create_directory(dir);
    std::ofstream(dir + "/to_delete.txt") << "Content to delete\n";
    ctx.working_directory = fs::canonical(dir).string();
    
    std::string patch = R"(*** Begin Patch
*** Delete File: to_delete.txt
*** End Patch)";
    
    nlohmann::json input = {{"patchText", patch}};
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);
    REQUIRE_FALSE(fs::exists(dir + "/to_delete.txt"));
    
    // Cleanup
    fs::remove_all(dir);
}

TEST_CASE("ApplyPatchTool.Execute.InvalidPatch", "[Tool][Builtin][ApplyPatch]") {
    ApplyPatchTool tool;
    auto ctx = make_tool_ctx();
    ctx.working_directory = "/tmp";
    
    // Missing End Patch marker
    std::string patch = "*** Begin Patch\n*** Add File: test.txt\n+content";
    
    nlohmann::json input = {{"patchText", patch}};
    auto result = tool.execute(input, ctx);
    REQUIRE(result.is_error);
}

TEST_CASE("ApplyPatchTool.Execute.EmptyPatch", "[Tool][Builtin][ApplyPatch]") {
    ApplyPatchTool tool;
    auto ctx = make_tool_ctx();
    ctx.working_directory = "/tmp";
    
    std::string patch = "*** Begin Patch\n*** End Patch";
    
    nlohmann::json input = {{"patchText", patch}};
    auto result = tool.execute(input, ctx);
    REQUIRE(result.is_error);
}

// ==================== BatchTool Tests ====================

TEST_CASE("BatchTool.Name", "[Tool][Builtin][Batch]") {
    BatchTool tool;
    REQUIRE(tool.name() == "batch");
}

TEST_CASE("BatchTool.Description", "[Tool][Builtin][Batch]") {
    BatchTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("BatchTool.InputSchema", "[Tool][Builtin][Batch]") {
    BatchTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("tool_calls"));
}

TEST_CASE("BatchTool.ValidateInput.Valid", "[Tool][Builtin][Batch]") {
    BatchTool tool;
    nlohmann::json input = {
        {"tool_calls", nlohmann::json::array({
            {{"tool", "read"}, {"parameters", {{"file_path", "/tmp/test.txt"}}}}
        })}
    };
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("BatchTool.ValidateInput.MissingToolCalls", "[Tool][Builtin][Batch]") {
    BatchTool tool;
    nlohmann::json input = {};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("BatchTool.ValidateInput.EmptyToolCalls", "[Tool][Builtin][Batch]") {
    BatchTool tool;
    nlohmann::json input = {{"tool_calls", nlohmann::json::array()}};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("BatchTool.ValidateInput.MissingTool", "[Tool][Builtin][Batch]") {
    BatchTool tool;
    nlohmann::json input = {
        {"tool_calls", nlohmann::json::array({
            {{"parameters", {{"file_path", "/tmp/test.txt"}}}}
        })}
    };
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("BatchToolParams.FromJson", "[Tool][Builtin][Batch]") {
    nlohmann::json j = {
        {"tool_calls", nlohmann::json::array({
            {{"tool", "read"}, {"parameters", {{"file_path", "/tmp/test.txt"}}}},
            {{"tool", "list"}, {"parameters", {{"path", "/tmp"}}}}
        })}
    };
    auto params = BatchToolParams::from_json(j);
    REQUIRE(params.tool_calls.size() == 2);
    REQUIRE(params.tool_calls[0].tool == "read");
    REQUIRE(params.tool_calls[1].tool == "list");
}

TEST_CASE("BatchToolParams.ToJson", "[Tool][Builtin][Batch]") {
    BatchToolParams params;
    BatchToolCall call1;
    call1.tool = "read";
    call1.parameters = {{"file_path", "/tmp/test.txt"}};
    params.tool_calls.push_back(call1);
    
    auto j = params.to_json();
    REQUIRE(j["tool_calls"].size() == 1);
    REQUIRE(j["tool_calls"][0]["tool"] == "read");
}

TEST_CASE("BatchTool.Execute.InvalidInput", "[Tool][Builtin][Batch]") {
    BatchTool tool;
    auto ctx = make_tool_ctx();
    
    nlohmann::json input = {{"tool_calls", nlohmann::json::array()}};
    auto result = tool.execute(input, ctx);
    REQUIRE(result.is_error);
}

TEST_CASE("BatchTool.Execute.ToolNotFound", "[Tool][Builtin][Batch]") {
    BatchTool tool;
    auto ctx = make_tool_ctx();
    
    nlohmann::json input = {
        {"tool_calls", nlohmann::json::array({
            {{"tool", "nonexistent_tool"}, {"parameters", nlohmann::json::object()}}
        })}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);  // Batch returns success even if individual tools fail
}

// ==================== TodoTool Tests ====================

TEST_CASE("TodoStatus.ToString", "[Tool][Builtin][Todo]") {
    REQUIRE(todo_status_to_string(TodoStatus::Pending) == "pending");
    REQUIRE(todo_status_to_string(TodoStatus::InProgress) == "in_progress");
    REQUIRE(todo_status_to_string(TodoStatus::Completed) == "completed");
    REQUIRE(todo_status_to_string(TodoStatus::Cancelled) == "cancelled");
}

TEST_CASE("TodoStatus.FromString", "[Tool][Builtin][Todo]") {
    REQUIRE(string_to_todo_status("pending") == TodoStatus::Pending);
    REQUIRE(string_to_todo_status("in_progress") == TodoStatus::InProgress);
    REQUIRE(string_to_todo_status("completed") == TodoStatus::Completed);
    REQUIRE(string_to_todo_status("cancelled") == TodoStatus::Cancelled);
    REQUIRE(string_to_todo_status("unknown") == TodoStatus::Pending);
}

TEST_CASE("TodoItem.ToJson", "[Tool][Builtin][Todo]") {
    TodoItem item;
    item.id = "test-1";
    item.content = "Test task";
    item.status = TodoStatus::InProgress;
    item.priority = "high";
    item.position = 0;
    
    auto j = item.to_json();
    REQUIRE(j["id"] == "test-1");
    REQUIRE(j["content"] == "Test task");
    REQUIRE(j["status"] == "in_progress");
    REQUIRE(j["priority"] == "high");
}

TEST_CASE("TodoItem.FromJson", "[Tool][Builtin][Todo]") {
    nlohmann::json j = {
        {"id", "test-2"},
        {"content", "Another task"},
        {"status", "completed"},
        {"priority", "low"},
        {"position", 1}
    };
    
    auto item = TodoItem::from_json(j);
    REQUIRE(item.id == "test-2");
    REQUIRE(item.content == "Another task");
    REQUIRE(item.status == TodoStatus::Completed);
    REQUIRE(item.priority == "low");
    REQUIRE(item.position == 1);
}

TEST_CASE("TodoItem.Equality", "[Tool][Builtin][Todo]") {
    TodoItem item1;
    item1.id = "test";
    item1.content = "Task";
    item1.status = TodoStatus::Pending;
    item1.priority = "medium";
    item1.position = 0;
    
    TodoItem item2 = item1;
    REQUIRE(item1 == item2);
    
    item2.status = TodoStatus::Completed;
    REQUIRE_FALSE(item1 == item2);
}

TEST_CASE("TodoReadTool.Name", "[Tool][Builtin][Todo]") {
    TodoReadTool tool;
    REQUIRE(tool.name() == "todoread");
}

TEST_CASE("TodoReadTool.Description", "[Tool][Builtin][Todo]") {
    TodoReadTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("TodoReadTool.InputSchema", "[Tool][Builtin][Todo]") {
    TodoReadTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
}

TEST_CASE("TodoReadTool.Execute.Empty", "[Tool][Builtin][Todo]") {
    TodoReadTool tool;
    auto ctx = make_tool_ctx();
    
    TodoManager::instance().clear_todos(ctx.session_id);
    
    auto result = tool.execute(nlohmann::json::object(), ctx);
    REQUIRE_FALSE(result.is_error);
}

TEST_CASE("TodoWriteTool.Name", "[Tool][Builtin][Todo]") {
    TodoWriteTool tool;
    REQUIRE(tool.name() == "todowrite");
}

TEST_CASE("TodoWriteTool.Description", "[Tool][Builtin][Todo]") {
    TodoWriteTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("TodoWriteTool.InputSchema", "[Tool][Builtin][Todo]") {
    TodoWriteTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("todos"));
}

TEST_CASE("TodoWriteTool.ValidateInput.Valid", "[Tool][Builtin][Todo]") {
    TodoWriteTool tool;
    nlohmann::json input = {
        {"todos", nlohmann::json::array({
            {{"content", "Task 1"}, {"status", "pending"}, {"priority", "high"}}
        })}
    };
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("TodoWriteTool.ValidateInput.MissingTodos", "[Tool][Builtin][Todo]") {
    TodoWriteTool tool;
    nlohmann::json input = {};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("TodoWriteTool.ValidateInput.MissingContent", "[Tool][Builtin][Todo]") {
    TodoWriteTool tool;
    nlohmann::json input = {
        {"todos", nlohmann::json::array({
            {{"status", "pending"}}
        })}
    };
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("TodoWriteTool.ValidateInput.InvalidStatus", "[Tool][Builtin][Todo]") {
    TodoWriteTool tool;
    nlohmann::json input = {
        {"todos", nlohmann::json::array({
            {{"content", "Task"}, {"status", "invalid_status"}}
        })}
    };
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("TodoWriteTool.ValidateInput.InvalidPriority", "[Tool][Builtin][Todo]") {
    TodoWriteTool tool;
    nlohmann::json input = {
        {"todos", nlohmann::json::array({
            {{"content", "Task"}, {"priority", "invalid_priority"}}
        })}
    };
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("TodoWriteTool.Execute.Valid", "[Tool][Builtin][Todo]") {
    TodoWriteTool tool;
    auto ctx = make_tool_ctx();
    
    nlohmann::json input = {
        {"todos", nlohmann::json::array({
            {{"content", "Task 1"}, {"status", "pending"}, {"priority", "high"}},
            {{"content", "Task 2"}, {"status", "in_progress"}, {"priority", "medium"}}
        })}
    };
    
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);
    
    // Verify todos were saved
    auto todos = TodoManager::instance().get_todos(ctx.session_id);
    REQUIRE(todos.size() == 2);
    
    // Cleanup
    TodoManager::instance().clear_todos(ctx.session_id);
}

TEST_CASE("TodoManager.SetAndGet", "[Tool][Builtin][Todo]") {
    std::string session_id = "test-session-mgr";
    
    std::vector<TodoItem> todos;
    TodoItem item;
    item.id = "mgr-1";
    item.content = "Manager task";
    item.status = TodoStatus::Pending;
    item.priority = "medium";
    item.position = 0;
    todos.push_back(item);
    
    TodoManager::instance().set_todos(session_id, todos);
    
    auto retrieved = TodoManager::instance().get_todos(session_id);
    REQUIRE(retrieved.size() == 1);
    REQUIRE(retrieved[0].content == "Manager task");
    
    TodoManager::instance().clear_todos(session_id);
    REQUIRE(TodoManager::instance().get_todos(session_id).empty());
}

TEST_CASE("TodoManager.ClearAll", "[Tool][Builtin][Todo]") {
    TodoManager::instance().set_todos("session1", {TodoItem{}});
    TodoManager::instance().set_todos("session2", {TodoItem{}});
    
    TodoManager::instance().clear_all();
    
    REQUIRE(TodoManager::instance().get_todos("session1").empty());
    REQUIRE(TodoManager::instance().get_todos("session2").empty());
}

// ==================== InvalidTool Tests ====================

TEST_CASE("InvalidTool.Name", "[Tool][Builtin][Invalid]") {
    InvalidTool tool;
    REQUIRE(tool.name() == "invalid");
}

TEST_CASE("InvalidTool.Description", "[Tool][Builtin][Invalid]") {
    InvalidTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("InvalidTool.InputSchema", "[Tool][Builtin][Invalid]") {
    InvalidTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("tool"));
    REQUIRE(schema["properties"].contains("error"));
}

TEST_CASE("InvalidTool.ValidateInput.AlwaysTrue", "[Tool][Builtin][Invalid]") {
    InvalidTool tool;
    REQUIRE(tool.validate_input(nlohmann::json::object()));
    REQUIRE(tool.validate_input({{"any", "input"}}));
}

TEST_CASE("InvalidToolParams.FromJson", "[Tool][Builtin][Invalid]") {
    nlohmann::json j = {
        {"tool", "bad_tool"},
        {"error", "Tool not found"}
    };
    auto params = InvalidToolParams::from_json(j);
    REQUIRE(params.tool == "bad_tool");
    REQUIRE(params.error == "Tool not found");
}

TEST_CASE("InvalidToolParams.FromJson.Defaults", "[Tool][Builtin][Invalid]") {
    nlohmann::json j = {};
    auto params = InvalidToolParams::from_json(j);
    REQUIRE(params.tool == "unknown");
    REQUIRE(params.error == "Unknown error");
}

TEST_CASE("InvalidToolParams.ToJson", "[Tool][Builtin][Invalid]") {
    InvalidToolParams params;
    params.tool = "test_tool";
    params.error = "test error";
    
    auto j = params.to_json();
    REQUIRE(j["tool"] == "test_tool");
    REQUIRE(j["error"] == "test error");
}

TEST_CASE("InvalidTool.Execute", "[Tool][Builtin][Invalid]") {
    InvalidTool tool;
    auto ctx = make_tool_ctx();
    
    nlohmann::json input = {
        {"tool", "nonexistent"},
        {"error", "No such tool"}
    };
    
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);  // InvalidTool always returns success
    REQUIRE(result.output.find("invalid") != std::string::npos);
}

// ==================== QuestionTool Tests ====================

TEST_CASE("QuestionOption.ToJson", "[Tool][Builtin][Question]") {
    QuestionOption opt;
    opt.label = "Yes";
    opt.description = "Confirm the action";
    
    auto j = opt.to_json();
    REQUIRE(j["label"] == "Yes");
    REQUIRE(j["description"] == "Confirm the action");
}

TEST_CASE("QuestionOption.FromJson", "[Tool][Builtin][Question]") {
    nlohmann::json j = {
        {"label", "No"},
        {"description", "Cancel the action"}
    };
    
    auto opt = QuestionOption::from_json(j);
    REQUIRE(opt.label == "No");
    REQUIRE(opt.description == "Cancel the action");
}

TEST_CASE("QuestionInfo.ToJson", "[Tool][Builtin][Question]") {
    QuestionInfo info;
    info.question = "Do you want to continue?";
    info.header = "Confirm";
    info.options = {QuestionOption{"Yes", "Continue"}, QuestionOption{"No", "Cancel"}};
    info.multiple = false;
    info.custom = true;
    
    auto j = info.to_json();
    REQUIRE(j["question"] == "Do you want to continue?");
    REQUIRE(j["header"] == "Confirm");
    REQUIRE(j["options"].size() == 2);
    REQUIRE(j["multiple"] == false);
    REQUIRE(j["custom"] == true);
}

TEST_CASE("QuestionInfo.FromJson", "[Tool][Builtin][Question]") {
    nlohmann::json j = {
        {"question", "Select options"},
        {"header", "Options"},
        {"options", nlohmann::json::array({
            {{"label", "A"}, {"description", "Option A"}},
            {{"label", "B"}, {"description", "Option B"}}
        })},
        {"multiple", true}
    };
    
    auto info = QuestionInfo::from_json(j);
    REQUIRE(info.question == "Select options");
    REQUIRE(info.header == "Options");
    REQUIRE(info.options.size() == 2);
    REQUIRE(info.multiple == true);
}

TEST_CASE("QuestionRequest.ToJson", "[Tool][Builtin][Question]") {
    QuestionRequest req;
    req.id = "req-123";
    req.session_id = "session-456";
    req.message_id = "msg-789";
    req.call_id = "call-012";
    req.questions = {QuestionInfo{"Question?", "Header", {}, std::nullopt, std::nullopt}};
    
    auto j = req.to_json();
    REQUIRE(j["id"] == "req-123");
    REQUIRE(j["session_id"] == "session-456");
    REQUIRE(j["message_id"] == "msg-789");
    REQUIRE(j["call_id"] == "call-012");
    REQUIRE(j["questions"].size() == 1);
}

TEST_CASE("QuestionRequest.FromJson", "[Tool][Builtin][Question]") {
    nlohmann::json j = {
        {"id", "req-abc"},
        {"session_id", "session-def"},
        {"message_id", "msg-ghi"},
        {"call_id", "call-jkl"},
        {"questions", nlohmann::json::array()}
    };
    
    auto req = QuestionRequest::from_json(j);
    REQUIRE(req.id == "req-abc");
    REQUIRE(req.session_id == "session-def");
    REQUIRE(req.message_id == "msg-ghi");
    REQUIRE(req.call_id == "call-jkl");
}

TEST_CASE("QuestionTool.Name", "[Tool][Builtin][Question]") {
    QuestionTool tool;
    REQUIRE(tool.name() == "question");
}

TEST_CASE("QuestionTool.Description", "[Tool][Builtin][Question]") {
    QuestionTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("QuestionTool.InputSchema", "[Tool][Builtin][Question]") {
    QuestionTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("questions"));
}

TEST_CASE("QuestionTool.ValidateInput.Valid", "[Tool][Builtin][Question]") {
    QuestionTool tool;
    nlohmann::json input = {
        {"questions", nlohmann::json::array({
            {{"question", "Test?"}, {"header", "Test"}, {"options", nlohmann::json::array()}}
        })}
    };
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("QuestionTool.ValidateInput.MissingQuestions", "[Tool][Builtin][Question]") {
    QuestionTool tool;
    nlohmann::json input = {};
    REQUIRE_FALSE(tool.validate_input(input));
}

// ==================== TaskTool Tests ====================

TEST_CASE("TaskToolParams.FromJson", "[Tool][Builtin][Task]") {
    nlohmann::json j = {
        {"prompt", "Test prompt"},
        {"description", "Test description"},
        {"subagent_type", "build"},
        {"task_id", "task-123"}
    };
    
    auto params = TaskToolParams::from_json(j);
    REQUIRE(params.prompt == "Test prompt");
    REQUIRE(params.description == "Test description");
    REQUIRE(params.subagent_type == "build");
    REQUIRE(params.task_id == "task-123");
}

TEST_CASE("TaskToolParams.ToJson", "[Tool][Builtin][Task]") {
    TaskToolParams params;
    params.prompt = "Test prompt";
    params.description = "Test description";
    params.subagent_type = "explore";
    params.task_id = "task-456";
    
    auto j = params.to_json();
    REQUIRE(j["prompt"] == "Test prompt");
    REQUIRE(j["description"] == "Test description");
    REQUIRE(j["subagent_type"] == "explore");
    REQUIRE(j["task_id"] == "task-456");
}

TEST_CASE("TaskTool.Name", "[Tool][Builtin][Task]") {
    TaskTool tool;
    REQUIRE(tool.name() == "task");
}

TEST_CASE("TaskTool.Description", "[Tool][Builtin][Task]") {
    TaskTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("TaskTool.InputSchema", "[Tool][Builtin][Task]") {
    TaskTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("prompt"));
    REQUIRE(schema["properties"].contains("description"));
    REQUIRE(schema["properties"].contains("subagent_type"));
}

// ==================== PlanTool Tests ====================

TEST_CASE("PlanMode.ToString", "[Tool][Builtin][Plan]") {
    REQUIRE(plan_mode_to_string(PlanMode::Planning) == "planning");
    REQUIRE(plan_mode_to_string(PlanMode::Normal) == "normal");
}

TEST_CASE("PlanMode.FromString", "[Tool][Builtin][Plan]") {
    REQUIRE(string_to_plan_mode("planning") == PlanMode::Planning);
    REQUIRE(string_to_plan_mode("normal") == PlanMode::Normal);
    REQUIRE(string_to_plan_mode("unknown") == PlanMode::Normal);
}

TEST_CASE("PlanManager.GetSetMode", "[Tool][Builtin][Plan]") {
    std::string session_id = "test-plan-session";
    
    REQUIRE(PlanManager::instance().get_mode(session_id) == PlanMode::Normal);
    
    PlanManager::instance().set_mode(session_id, PlanMode::Planning);
    REQUIRE(PlanManager::instance().get_mode(session_id) == PlanMode::Planning);
    
    PlanManager::instance().clear_session(session_id);
    REQUIRE(PlanManager::instance().get_mode(session_id) == PlanMode::Normal);
}

TEST_CASE("PlanManager.GetSetPlan", "[Tool][Builtin][Plan]") {
    std::string session_id = "test-plan-content";
    
    REQUIRE(PlanManager::instance().get_plan(session_id).empty());
    
    PlanManager::instance().set_plan(session_id, "# Test Plan\n\nContent here");
    REQUIRE(PlanManager::instance().get_plan(session_id) == "# Test Plan\n\nContent here");
    
    PlanManager::instance().clear_session(session_id);
    REQUIRE(PlanManager::instance().get_plan(session_id).empty());
}

TEST_CASE("PlanManager.ClearAll", "[Tool][Builtin][Plan]") {
    PlanManager::instance().set_mode("session1", PlanMode::Planning);
    PlanManager::instance().set_plan("session1", "Plan 1");
    PlanManager::instance().set_mode("session2", PlanMode::Planning);
    
    PlanManager::instance().clear_all();
    
    REQUIRE(PlanManager::instance().get_mode("session1") == PlanMode::Normal);
    REQUIRE(PlanManager::instance().get_plan("session1").empty());
    REQUIRE(PlanManager::instance().get_mode("session2") == PlanMode::Normal);
}

TEST_CASE("PlanEnterTool.Name", "[Tool][Builtin][Plan]") {
    PlanEnterTool tool;
    REQUIRE(tool.name() == "plan_enter");
}

TEST_CASE("PlanEnterTool.Description", "[Tool][Builtin][Plan]") {
    PlanEnterTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("PlanEnterTool.InputSchema", "[Tool][Builtin][Plan]") {
    PlanEnterTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
}

TEST_CASE("PlanEnterTool.Execute", "[Tool][Builtin][Plan]") {
    PlanEnterTool tool;
    auto ctx = make_tool_ctx();
    ctx.working_directory = "/tmp";
    
    auto result = tool.execute(nlohmann::json::object(), ctx);
    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.metadata["mode"] == "planning");
    
    // Cleanup
    PlanManager::instance().clear_session(ctx.session_id);
}

TEST_CASE("PlanExitTool.Name", "[Tool][Builtin][Plan]") {
    PlanExitTool tool;
    REQUIRE(tool.name() == "plan_exit");
}

TEST_CASE("PlanExitTool.Description", "[Tool][Builtin][Plan]") {
    PlanExitTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("PlanExitTool.InputSchema", "[Tool][Builtin][Plan]") {
    PlanExitTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("plan"));
}

// ==================== MultiEditTool Tests ====================

TEST_CASE("EditOperation.FromJson", "[Tool][Builtin][MultiEdit]") {
    nlohmann::json j = {
        {"filePath", "/tmp/test.txt"},
        {"oldString", "old"},
        {"newString", "new"},
        {"replaceAll", true}
    };
    
    auto op = EditOperation::from_json(j);
    REQUIRE(op.file_path == "/tmp/test.txt");
    REQUIRE(op.old_string == "old");
    REQUIRE(op.new_string == "new");
    REQUIRE(op.replace_all == true);
}

TEST_CASE("EditOperation.ToJson", "[Tool][Builtin][MultiEdit]") {
    EditOperation op;
    op.file_path = "/path/to/file.txt";
    op.old_string = "foo";
    op.new_string = "bar";
    op.replace_all = false;
    
    auto j = op.to_json();
    REQUIRE(j["filePath"] == "/path/to/file.txt");
    REQUIRE(j["oldString"] == "foo");
    REQUIRE(j["newString"] == "bar");
    REQUIRE(j["replaceAll"] == false);
}

TEST_CASE("MultiEditTool.Name", "[Tool][Builtin][MultiEdit]") {
    MultiEditTool tool;
    REQUIRE(tool.name() == "multiedit");
}

TEST_CASE("MultiEditTool.Description", "[Tool][Builtin][MultiEdit]") {
    MultiEditTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("MultiEditTool.InputSchema", "[Tool][Builtin][MultiEdit]") {
    MultiEditTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("edits"));
}

TEST_CASE("MultiEditTool.ValidateInput.Valid", "[Tool][Builtin][MultiEdit]") {
    MultiEditTool tool;
    nlohmann::json input = {
        {"edits", nlohmann::json::array({
            {{"filePath", "/tmp/a.txt"}, {"oldString", "a"}, {"newString", "b"}}
        })}
    };
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("MultiEditTool.ValidateInput.MissingEdits", "[Tool][Builtin][MultiEdit]") {
    MultiEditTool tool;
    nlohmann::json input = {};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("MultiEditTool.ValidateInput.EmptyEdits", "[Tool][Builtin][MultiEdit]") {
    MultiEditTool tool;
    nlohmann::json input = {{"edits", nlohmann::json::array()}};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("MultiEditTool.ValidateInput.MissingFilePath", "[Tool][Builtin][MultiEdit]") {
    MultiEditTool tool;
    nlohmann::json input = {
        {"edits", nlohmann::json::array({
            {{"oldString", "a"}, {"newString", "b"}}
        })}
    };
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("MultiEditTool.Execute.InvalidInput", "[Tool][Builtin][MultiEdit]") {
    MultiEditTool tool;
    auto ctx = make_tool_ctx();
    
    nlohmann::json input = {{"edits", nlohmann::json::array()}};
    auto result = tool.execute(input, ctx);
    REQUIRE(result.is_error);
}

// ==================== WebFetchTool Tests ====================

TEST_CASE("WebFetchTool.Name", "[Tool][Builtin][WebFetch]") {
    WebFetchTool tool;
    REQUIRE(tool.name() == "webfetch");
}

TEST_CASE("WebFetchTool.Description", "[Tool][Builtin][WebFetch]") {
    WebFetchTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("WebFetchTool.InputSchema", "[Tool][Builtin][WebFetch]") {
    WebFetchTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("url"));
}

TEST_CASE("WebFetchTool.ValidateInput.Valid", "[Tool][Builtin][WebFetch]") {
    WebFetchTool tool;
    nlohmann::json input = {{"url", "https://example.com"}};
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("WebFetchTool.ValidateInput.MissingUrl", "[Tool][Builtin][WebFetch]") {
    WebFetchTool tool;
    nlohmann::json input = {};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("WebFetchTool.ValidateInput.EmptyUrl", "[Tool][Builtin][WebFetch]") {
    WebFetchTool tool;
    nlohmann::json input = {{"url", ""}};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("WebFetchTool.ValidateInput.PrivateUrl", "[Tool][Builtin][WebFetch]") {
    WebFetchTool tool;
    nlohmann::json input = {{"url", "http://localhost/test"}};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("WebFetchTool.ValidateInput.InternalIp", "[Tool][Builtin][WebFetch]") {
    WebFetchTool tool;
    nlohmann::json input = {{"url", "http://192.168.1.1/test"}};
    REQUIRE_FALSE(tool.validate_input(input));
}

// ==================== CodeSearchTool Tests ====================

TEST_CASE("CodeSearchTool.Name", "[Tool][Builtin][CodeSearch]") {
    CodeSearchTool tool;
    REQUIRE(tool.name() == "codesearch");
}

TEST_CASE("CodeSearchTool.Description", "[Tool][Builtin][CodeSearch]") {
    CodeSearchTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("CodeSearchTool.InputSchema", "[Tool][Builtin][CodeSearch]") {
    CodeSearchTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("query"));
    REQUIRE(schema["required"].is_array());
}

TEST_CASE("CodeSearchTool.ValidateInput.Valid", "[Tool][Builtin][CodeSearch]") {
    CodeSearchTool tool;
    nlohmann::json input = {{"query", "function\\s+\\w+"}};
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("CodeSearchTool.ValidateInput.MissingQuery", "[Tool][Builtin][CodeSearch]") {
    CodeSearchTool tool;
    nlohmann::json input = {};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("CodeSearchTool.ValidateInput.EmptyQuery", "[Tool][Builtin][CodeSearch]") {
    CodeSearchTool tool;
    nlohmann::json input = {{"query", ""}};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("CodeSearchTool.ValidateInput.NonStringQuery", "[Tool][Builtin][CodeSearch]") {
    CodeSearchTool tool;
    nlohmann::json input = {{"query", 123}};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("CodeSearchTool.Execute.InvalidRegex", "[Tool][Builtin][CodeSearch]") {
    CodeSearchTool tool;
    auto ctx = make_tool_ctx();
    ctx.working_directory = "/tmp";
    
    nlohmann::json input = {{"query", "[invalid(regex"}};
    auto result = tool.execute(input, ctx);
    REQUIRE(result.is_error);
}

TEST_CASE("CodeSearchTool.Execute.PathNotFound", "[Tool][Builtin][CodeSearch]") {
    CodeSearchTool tool;
    auto ctx = make_tool_ctx();
    ctx.working_directory = "/tmp";
    
    nlohmann::json input = {
        {"query", "test"},
        {"path", "/nonexistent/path/that/does/not/exist"}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE(result.is_error);
}

TEST_CASE("CodeSearchTool.Execute.WithFilePattern", "[Tool][Builtin][CodeSearch]") {
    CodeSearchTool tool;
    auto ctx = make_tool_ctx();
    
    // Create temp directory with test file
    std::string dir = "/tmp/turbot_codesearch_test_" + std::to_string(std::time(nullptr));
    fs::create_directories(dir);
    ctx.working_directory = fs::canonical(dir).string();
    
    // Create test files
    std::ofstream(dir + "/test.cpp") << "void myFunction() {}\nint main() { return 0; }\n";
    std::ofstream(dir + "/test.py") << "def my_function():\n    pass\n";
    
    nlohmann::json input = {
        {"query", "function"},
        {"filePattern", "*.cpp"}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);
    
    // Cleanup
    fs::remove_all(dir);
}

TEST_CASE("CodeSearchTool.Execute.WithMaxResults", "[Tool][Builtin][CodeSearch]") {
    CodeSearchTool tool;
    auto ctx = make_tool_ctx();
    
    std::string dir = "/tmp/turbot_codesearch_max_" + std::to_string(std::time(nullptr));
    fs::create_directories(dir);
    ctx.working_directory = fs::canonical(dir).string();
    
    // Create test file with multiple matches
    std::ofstream(dir + "/test.cpp") << "int a = 1;\nint b = 2;\nint c = 3;\nint d = 4;\nint e = 5;\n";
    
    nlohmann::json input = {
        {"query", "int"},
        {"maxResults", 2}
    };
    auto result = tool.execute(input, ctx);
    REQUIRE_FALSE(result.is_error);
    
    // Cleanup
    fs::remove_all(dir);
}

// ==================== WebSearchTool Tests ====================

TEST_CASE("WebSearchTool.Name", "[Tool][Builtin][WebSearch]") {
    WebSearchTool tool;
    REQUIRE(tool.name() == "websearch");
}

TEST_CASE("WebSearchTool.Description", "[Tool][Builtin][WebSearch]") {
    WebSearchTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("WebSearchTool.InputSchema", "[Tool][Builtin][WebSearch]") {
    WebSearchTool tool;
    auto schema = tool.input_schema();
    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("query"));
    REQUIRE(schema["required"].is_array());
}

TEST_CASE("WebSearchTool.ValidateInput.Valid", "[Tool][Builtin][WebSearch]") {
    WebSearchTool tool;
    nlohmann::json input = {{"query", "test search"}};
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("WebSearchTool.ValidateInput.WithOptions", "[Tool][Builtin][WebSearch]") {
    WebSearchTool tool;
    nlohmann::json input = {
        {"query", "test search"},
        {"numResults", 5},
        {"livecrawl", "fallback"},
        {"type", "auto"},
        {"contextMaxCharacters", 5000}
    };
    REQUIRE(tool.validate_input(input));
}

TEST_CASE("WebSearchTool.ValidateInput.MissingQuery", "[Tool][Builtin][WebSearch]") {
    WebSearchTool tool;
    nlohmann::json input = {};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("WebSearchTool.ValidateInput.EmptyQuery", "[Tool][Builtin][WebSearch]") {
    WebSearchTool tool;
    nlohmann::json input = {{"query", ""}};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("WebSearchTool.ValidateInput.QueryTooLong", "[Tool][Builtin][WebSearch]") {
    WebSearchTool tool;
    std::string long_query(1001, 'a');
    nlohmann::json input = {{"query", long_query}};
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("WebSearchTool.ValidateInput.InvalidNumResults", "[Tool][Builtin][WebSearch]") {
    WebSearchTool tool;
    nlohmann::json input = {
        {"query", "test"},
        {"numResults", "not a number"}
    };
    REQUIRE_FALSE(tool.validate_input(input));
}

TEST_CASE("WebSearchTool.ValidateInput.InvalidType", "[Tool][Builtin][WebSearch]") {
    WebSearchTool tool;
    nlohmann::json input = {
        {"query", "test"},
        {"type", 123}
    };
    REQUIRE_FALSE(tool.validate_input(input));
}

