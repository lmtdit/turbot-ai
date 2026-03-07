#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <turbot/core/tool/tool.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/tool/builtin/read_file_tool.hpp>
#include <turbot/core/tool/builtin/write_file_tool.hpp>
#include <turbot/core/tool/builtin/bash_tool.hpp>
#include <turbot/core/permission/permission.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <memory>

using namespace turbot::core::tool;
using namespace turbot::core::permission;
namespace fs = std::filesystem;

// ============================================================================
// Helper: Create a minimal ToolContext with all-allow ruleset
// ============================================================================

static ToolContext make_allow_ctx() {
    ToolContext ctx;
    ctx.session_id  = "test-session";
    ctx.message_id  = "test-message";
    ctx.agent       = "test-agent";
    ctx.ruleset     = {{"read", "*", PermissionAction::Allow},
                       {"write", "*", PermissionAction::Allow},
                       {"execute", "*", PermissionAction::Allow}};
    ctx.abort_flag  = std::make_shared<std::atomic<bool>>(false);
    return ctx;
}

static ToolContext make_deny_ctx() {
    ToolContext ctx;
    ctx.session_id  = "test-session";
    ctx.message_id  = "test-message";
    ctx.agent       = "test-agent";
    ctx.ruleset     = {{"read", "*", PermissionAction::Deny},
                       {"write", "*", PermissionAction::Deny},
                       {"execute", "*", PermissionAction::Deny}};
    ctx.abort_flag  = std::make_shared<std::atomic<bool>>(false);
    return ctx;
}

// ============================================================================
// ToolResult
// ============================================================================

TEST_CASE("ToolResult::success", "[core][tool]") {
    const auto result = ToolResult::success("title", "output");
    REQUIRE(result.title    == "title");
    REQUIRE(result.output   == "output");
    REQUIRE(result.is_error == false);
    REQUIRE(result.metadata.is_object());
}

TEST_CASE("ToolResult::error", "[core][tool]") {
    const auto result = ToolResult::error("error title", "error msg");
    REQUIRE(result.title    == "error title");
    REQUIRE(result.output   == "error msg");
    REQUIRE(result.is_error == true);
}

TEST_CASE("ToolResult::to_json", "[core][tool]") {
    const auto result = ToolResult::success("t", "o", {{"key", "val"}});
    const auto j = result.to_json();
    REQUIRE(j["title"]    == "t");
    REQUIRE(j["output"]   == "o");
    REQUIRE(j["is_error"] == false);
    REQUIRE(j["metadata"]["key"] == "val");
}

TEST_CASE("ToolResult::from_json", "[core][tool]") {
    nlohmann::json j = {
        {"title",    "t2"},
        {"output",   "o2"},
        {"is_error", true},
        {"metadata", nlohmann::json::object()}
    };
    const auto result = ToolResult::from_json(j);
    REQUIRE(result.title    == "t2");
    REQUIRE(result.output   == "o2");
    REQUIRE(result.is_error == true);
}

TEST_CASE("ToolResult round-trip JSON", "[core][tool]") {
    const auto original = ToolResult::error("err", "msg", {{"code", 42}});
    const auto restored = ToolResult::from_json(original.to_json());
    REQUIRE(restored.title          == original.title);
    REQUIRE(restored.output         == original.output);
    REQUIRE(restored.is_error       == original.is_error);
    REQUIRE(restored.metadata["code"] == 42);
}

// ============================================================================
// ToolContext::should_abort
// ============================================================================

TEST_CASE("ToolContext::should_abort false by default", "[core][tool]") {
    ToolContext ctx;
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    REQUIRE_FALSE(ctx.should_abort());
}

TEST_CASE("ToolContext::should_abort true when set", "[core][tool]") {
    ToolContext ctx;
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(true);
    REQUIRE(ctx.should_abort());
}

TEST_CASE("ToolContext::should_abort false when no flag", "[core][tool]") {
    ToolContext ctx;
    ctx.abort_flag = nullptr;
    REQUIRE_FALSE(ctx.should_abort());
}

// ============================================================================
// A concrete test tool for testing base class behaviour
// ============================================================================

class EchoTool : public Tool {
public:
    EchoTool() = default;

    std::string name() const override { return "echo"; }
    std::string description() const override { return "Echoes the input"; }

    nlohmann::json input_schema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"message", {{"type", "string"}}}
            }},
            {"required", nlohmann::json::array({"message"})}
        };
    }

    ToolResult execute(const nlohmann::json& input, ToolContext& /*ctx*/) override {
        return ToolResult::success("Echo", input.value("message", ""));
    }
};

TEST_CASE("Tool::validate_input checks required fields", "[core][tool]") {
    EchoTool tool;
    nlohmann::json valid   = {{"message", "hello"}};
    nlohmann::json invalid = nlohmann::json::object();

    REQUIRE(tool.validate_input(valid));
    REQUIRE_FALSE(tool.validate_input(invalid));
}

TEST_CASE("Tool::format_validation_error", "[core][tool]") {
    EchoTool tool;
    const std::string msg = tool.format_validation_error("missing field");
    REQUIRE_THAT(msg, Catch::Matchers::ContainsSubstring("echo"));
    REQUIRE_THAT(msg, Catch::Matchers::ContainsSubstring("missing field"));
}

TEST_CASE("Tool::to_tool_definition", "[core][tool]") {
    EchoTool tool;
    const auto def = tool.to_tool_definition();
    REQUIRE(def["type"]                    == "function");
    REQUIRE(def["function"]["name"]        == "echo");
    REQUIRE(def["function"]["description"] == "Echoes the input");
    REQUIRE(def["function"]["parameters"].contains("required"));
}

// ============================================================================
// ToolRegistry
// ============================================================================

// Use a local registry instead of the singleton to avoid test pollution
// We test singleton via public API but clear state between tests

TEST_CASE("ToolRegistry basic register/get", "[core][tool][registry]") {
    ToolRegistry& reg = ToolRegistry::instance();
    reg.clear();  // Clean state

    reg.register_tool(std::make_unique<EchoTool>());
    REQUIRE(reg.has("echo"));
    REQUIRE(reg.size() == 1);

    const auto tool = reg.get("echo");
    REQUIRE(tool != nullptr);
    REQUIRE(tool->name() == "echo");

    reg.clear();
}

TEST_CASE("ToolRegistry get non-existent returns nullptr", "[core][tool][registry]") {
    ToolRegistry& reg = ToolRegistry::instance();
    reg.clear();

    REQUIRE(reg.get("nonexistent") == nullptr);
    REQUIRE_FALSE(reg.has("nonexistent"));

    reg.clear();
}

TEST_CASE("ToolRegistry register overwrites same name", "[core][tool][registry]") {
    ToolRegistry& reg = ToolRegistry::instance();
    reg.clear();

    reg.register_tool(std::make_unique<EchoTool>());
    reg.register_tool(std::make_unique<EchoTool>());  // Register again

    REQUIRE(reg.size() == 1);  // Still just one

    reg.clear();
}

TEST_CASE("ToolRegistry register null throws", "[core][tool][registry]") {
    ToolRegistry& reg = ToolRegistry::instance();
    reg.clear();

    REQUIRE_THROWS_AS(reg.register_tool(nullptr), std::invalid_argument);

    reg.clear();
}

TEST_CASE("ToolRegistry list", "[core][tool][registry]") {
    ToolRegistry& reg = ToolRegistry::instance();
    reg.clear();

    reg.register_tool(std::make_unique<EchoTool>());
    reg.register_tool(std::make_unique<builtin::ReadFileTool>());

    const auto tools = reg.list();
    REQUIRE(tools.size() == 2);

    reg.clear();
}

TEST_CASE("ToolRegistry names", "[core][tool][registry]") {
    ToolRegistry& reg = ToolRegistry::instance();
    reg.clear();

    reg.register_tool(std::make_unique<EchoTool>());
    reg.register_tool(std::make_unique<builtin::WriteFileTool>());

    const auto ns = reg.names();
    REQUIRE(ns.size() == 2);
    bool has_echo  = std::find(ns.begin(), ns.end(), "echo")       != ns.end();
    bool has_write = std::find(ns.begin(), ns.end(), "write_file") != ns.end();
    REQUIRE(has_echo);
    REQUIRE(has_write);

    reg.clear();
}

TEST_CASE("ToolRegistry filter empty returns all", "[core][tool][registry]") {
    ToolRegistry& reg = ToolRegistry::instance();
    reg.clear();

    reg.register_tool(std::make_unique<EchoTool>());
    reg.register_tool(std::make_unique<builtin::ReadFileTool>());

    const auto all = reg.filter({});
    REQUIRE(all.size() == 2);

    reg.clear();
}

TEST_CASE("ToolRegistry filter by names", "[core][tool][registry]") {
    ToolRegistry& reg = ToolRegistry::instance();
    reg.clear();

    reg.register_tool(std::make_unique<EchoTool>());
    reg.register_tool(std::make_unique<builtin::ReadFileTool>());
    reg.register_tool(std::make_unique<builtin::WriteFileTool>());

    const auto filtered = reg.filter({"echo", "read_file"});
    REQUIRE(filtered.size() == 2);

    reg.clear();
}

TEST_CASE("ToolRegistry filter non-existent name silently omits", "[core][tool][registry]") {
    ToolRegistry& reg = ToolRegistry::instance();
    reg.clear();

    reg.register_tool(std::make_unique<EchoTool>());

    const auto filtered = reg.filter({"echo", "nonexistent"});
    REQUIRE(filtered.size() == 1);

    reg.clear();
}

TEST_CASE("ToolRegistry remove", "[core][tool][registry]") {
    ToolRegistry& reg = ToolRegistry::instance();
    reg.clear();

    reg.register_tool(std::make_unique<EchoTool>());
    REQUIRE(reg.has("echo"));

    REQUIRE(reg.remove("echo"));
    REQUIRE_FALSE(reg.has("echo"));
    REQUIRE(reg.size() == 0);

    // Remove non-existent returns false
    REQUIRE_FALSE(reg.remove("echo"));

    reg.clear();
}

TEST_CASE("ToolRegistry to_tool_definitions", "[core][tool][registry]") {
    ToolRegistry& reg = ToolRegistry::instance();
    reg.clear();

    reg.register_tool(std::make_unique<EchoTool>());

    const auto defs = reg.to_tool_definitions();
    REQUIRE(defs.is_array());
    REQUIRE(defs.size() == 1);
    REQUIRE(defs[0]["type"] == "function");

    reg.clear();
}

TEST_CASE("ToolRegistry register_builtin_tools", "[core][tool][registry]") {
    ToolRegistry& reg = ToolRegistry::instance();
    reg.clear();

    reg.register_builtin_tools();

    REQUIRE(reg.has("read_file"));
    REQUIRE(reg.has("write_file"));
    REQUIRE(reg.has("bash"));
    REQUIRE(reg.size() == 3);

    reg.clear();
}

// ============================================================================
// ReadFileTool
// ============================================================================

TEST_CASE("ReadFileTool name/description/schema", "[core][tool][read_file]") {
    builtin::ReadFileTool tool;
    REQUIRE(tool.name()        == "read_file");
    REQUIRE(!tool.description().empty());
    const auto schema = tool.input_schema();
    REQUIRE(schema.contains("required"));
}

TEST_CASE("ReadFileTool validate_input", "[core][tool][read_file]") {
    builtin::ReadFileTool tool;
    REQUIRE(tool.validate_input({{"path", "/tmp/file.txt"}}));
    REQUIRE_FALSE(tool.validate_input(nlohmann::json::object()));
    REQUIRE_FALSE(tool.validate_input({{"path", ""}}));
    REQUIRE_FALSE(tool.validate_input({{"path", 123}}));
}

TEST_CASE("ReadFileTool execute success", "[core][tool][read_file]") {
    // Create a temp file
    const std::string tmp_path = "/tmp/turbot_test_read.txt";
    {
        std::ofstream f(tmp_path);
        f << "hello world";
    }

    builtin::ReadFileTool tool;
    auto ctx = make_allow_ctx();

    const auto result = tool.execute({{"path", tmp_path}}, ctx);
    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.output == "hello world");
    REQUIRE_THAT(result.title, Catch::Matchers::ContainsSubstring(tmp_path));
    REQUIRE(result.metadata["path"] == tmp_path);

    fs::remove(tmp_path);
}

TEST_CASE("ReadFileTool execute file not found", "[core][tool][read_file]") {
    builtin::ReadFileTool tool;
    auto ctx = make_allow_ctx();

    const auto result = tool.execute({{"path", "/tmp/nonexistent_turbot_test.txt"}}, ctx);
    REQUIRE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("not found"));
}

TEST_CASE("ReadFileTool execute permission denied", "[core][tool][read_file]") {
    builtin::ReadFileTool tool;
    auto ctx = make_deny_ctx();

    const auto result = tool.execute({{"path", "/tmp/any_file.txt"}}, ctx);
    REQUIRE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("Permission denied"));
}

TEST_CASE("ReadFileTool execute invalid input", "[core][tool][read_file]") {
    builtin::ReadFileTool tool;
    auto ctx = make_allow_ctx();

    const auto result = tool.execute(nlohmann::json::object(), ctx);
    REQUIRE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("Invalid input"));
}

TEST_CASE("ReadFileTool execute ask permission granted", "[core][tool][read_file]") {
    const std::string tmp_path = "/tmp/turbot_test_ask_read.txt";
    {
        std::ofstream f(tmp_path);
        f << "ask permission content";
    }

    builtin::ReadFileTool tool;
    ToolContext ctx;
    ctx.session_id = "s";
    ctx.message_id = "m";
    ctx.agent      = "a";
    ctx.ruleset    = {{"read", "*", PermissionAction::Ask}};
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);

    bool asked = false;
    ctx.ask_permission = [&](const PermissionRequest& req) -> PermissionReply {
        asked = true;
        REQUIRE(req.permission == "read");
        return PermissionReply::once();
    };

    const auto result = tool.execute({{"path", tmp_path}}, ctx);
    REQUIRE(asked);
    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.output == "ask permission content");

    fs::remove(tmp_path);
}

TEST_CASE("ReadFileTool ask permission rejected", "[core][tool][read_file]") {
    builtin::ReadFileTool tool;
    ToolContext ctx;
    ctx.session_id = "s";
    ctx.message_id = "m";
    ctx.agent      = "a";
    ctx.ruleset    = {{"read", "*", PermissionAction::Ask}};
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    ctx.ask_permission = [](const PermissionRequest&) -> PermissionReply {
        return PermissionReply::reject();
    };

    const auto result = tool.execute({{"path", "/tmp/any.txt"}}, ctx);
    REQUIRE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("rejected"));
}

TEST_CASE("ReadFileTool ask no callback returns error", "[core][tool][read_file]") {
    builtin::ReadFileTool tool;
    ToolContext ctx;
    ctx.session_id = "s";
    ctx.message_id = "m";
    ctx.agent      = "a";
    ctx.ruleset    = {{"read", "*", PermissionAction::Ask}};
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    // No ask_permission callback set

    const auto result = tool.execute({{"path", "/tmp/any.txt"}}, ctx);
    REQUIRE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("no ask_permission callback"));
}

TEST_CASE("ReadFileTool execute aborted", "[core][tool][read_file]") {
    builtin::ReadFileTool tool;
    ToolContext ctx;
    ctx.session_id = "s";
    ctx.message_id = "m";
    ctx.agent      = "a";
    ctx.ruleset    = {{"read", "*", PermissionAction::Allow}};
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(true);  // Already aborted

    const auto result = tool.execute({{"path", "/tmp/any.txt"}}, ctx);
    REQUIRE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("aborted"));
}

// ============================================================================
// WriteFileTool
// ============================================================================

TEST_CASE("WriteFileTool name/description/schema", "[core][tool][write_file]") {
    builtin::WriteFileTool tool;
    REQUIRE(tool.name()        == "write_file");
    REQUIRE(!tool.description().empty());
    const auto schema = tool.input_schema();
    REQUIRE(schema.contains("required"));
}

TEST_CASE("WriteFileTool validate_input", "[core][tool][write_file]") {
    builtin::WriteFileTool tool;
    REQUIRE(tool.validate_input({{"path", "/tmp/f.txt"}, {"content", "hello"}}));
    REQUIRE(tool.validate_input({{"path", "/tmp/f.txt"}, {"content", ""}}));  // Empty content is valid
    REQUIRE_FALSE(tool.validate_input({{"path", "/tmp/f.txt"}}));  // Missing content
    REQUIRE_FALSE(tool.validate_input({{"content", "hello"}}));    // Missing path
    REQUIRE_FALSE(tool.validate_input({{"path", ""}, {"content", "x"}}));  // Empty path
    REQUIRE_FALSE(tool.validate_input(nlohmann::json::object()));
}

TEST_CASE("WriteFileTool execute success", "[core][tool][write_file]") {
    const std::string tmp_path = "/tmp/turbot_test_write.txt";

    builtin::WriteFileTool tool;
    auto ctx = make_allow_ctx();

    const auto result = tool.execute({{"path", tmp_path}, {"content", "written content"}}, ctx);
    REQUIRE_FALSE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("Successfully wrote"));
    REQUIRE(result.metadata["path"] == tmp_path);

    // Verify file was written
    std::ifstream f(tmp_path);
    std::string actual((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    REQUIRE(actual == "written content");

    fs::remove(tmp_path);
}

TEST_CASE("WriteFileTool creates parent directories", "[core][tool][write_file]") {
    const std::string tmp_path = "/tmp/turbot_test_dir/subdir/test.txt";

    builtin::WriteFileTool tool;
    auto ctx = make_allow_ctx();

    const auto result = tool.execute({{"path", tmp_path}, {"content", "nested"}}, ctx);
    REQUIRE_FALSE(result.is_error);

    REQUIRE(fs::exists(tmp_path));

    // Cleanup
    fs::remove_all("/tmp/turbot_test_dir");
}

TEST_CASE("WriteFileTool execute permission denied", "[core][tool][write_file]") {
    builtin::WriteFileTool tool;
    auto ctx = make_deny_ctx();

    const auto result = tool.execute({{"path", "/tmp/x.txt"}, {"content", "y"}}, ctx);
    REQUIRE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("Permission denied"));
}

TEST_CASE("WriteFileTool execute invalid input", "[core][tool][write_file]") {
    builtin::WriteFileTool tool;
    auto ctx = make_allow_ctx();

    const auto result = tool.execute(nlohmann::json::object(), ctx);
    REQUIRE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("Invalid input"));
}

TEST_CASE("WriteFileTool ask permission rejected", "[core][tool][write_file]") {
    builtin::WriteFileTool tool;
    ToolContext ctx;
    ctx.session_id = "s";
    ctx.message_id = "m";
    ctx.agent      = "a";
    ctx.ruleset    = {{"write", "*", PermissionAction::Ask}};
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    ctx.ask_permission = [](const PermissionRequest&) -> PermissionReply {
        return PermissionReply::reject();
    };

    const auto result = tool.execute({{"path", "/tmp/x.txt"}, {"content", "y"}}, ctx);
    REQUIRE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("rejected"));
}

TEST_CASE("WriteFileTool ask no callback returns error", "[core][tool][write_file]") {
    builtin::WriteFileTool tool;
    ToolContext ctx;
    ctx.session_id = "s";
    ctx.message_id = "m";
    ctx.agent      = "a";
    ctx.ruleset    = {{"write", "*", PermissionAction::Ask}};
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    // No ask_permission callback

    const auto result = tool.execute({{"path", "/tmp/x.txt"}, {"content", "y"}}, ctx);
    REQUIRE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("no ask_permission callback"));
}

TEST_CASE("WriteFileTool execute aborted", "[core][tool][write_file]") {
    builtin::WriteFileTool tool;
    ToolContext ctx;
    ctx.session_id = "s";
    ctx.message_id = "m";
    ctx.agent      = "a";
    ctx.ruleset    = {{"write", "*", PermissionAction::Allow}};
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(true);

    const auto result = tool.execute({{"path", "/tmp/x.txt"}, {"content", "y"}}, ctx);
    REQUIRE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("aborted"));
}

// ============================================================================
// BashTool
// ============================================================================

TEST_CASE("BashTool name/description/schema", "[core][tool][bash]") {
    builtin::BashTool tool;
    REQUIRE(tool.name()        == "bash");
    REQUIRE(!tool.description().empty());
    const auto schema = tool.input_schema();
    REQUIRE(schema.contains("required"));
}

TEST_CASE("BashTool validate_input", "[core][tool][bash]") {
    builtin::BashTool tool;
    REQUIRE(tool.validate_input({{"command", "echo hello"}}));
    REQUIRE_FALSE(tool.validate_input(nlohmann::json::object()));
    REQUIRE_FALSE(tool.validate_input({{"command", ""}}));
    REQUIRE_FALSE(tool.validate_input({{"command", 42}}));
}

TEST_CASE("BashTool execute simple command", "[core][tool][bash]") {
    builtin::BashTool tool;
    auto ctx = make_allow_ctx();

    const auto result = tool.execute({{"command", "echo hello"}}, ctx);
    REQUIRE_FALSE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("hello"));
    REQUIRE(result.metadata["exit_code"] == 0);
}

TEST_CASE("BashTool execute returns stdout", "[core][tool][bash]") {
    builtin::BashTool tool;
    auto ctx = make_allow_ctx();

    const auto result = tool.execute({{"command", "echo test_output_123"}}, ctx);
    REQUIRE_FALSE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("test_output_123"));
}

TEST_CASE("BashTool execute non-zero exit code", "[core][tool][bash]") {
    builtin::BashTool tool;
    auto ctx = make_allow_ctx();

    const auto result = tool.execute({{"command", "exit 1"}}, ctx);
    REQUIRE(result.is_error);
    REQUIRE(result.metadata["exit_code"] != 0);
}

TEST_CASE("BashTool execute command with special chars", "[core][tool][bash]") {
    builtin::BashTool tool;
    auto ctx = make_allow_ctx();

    const auto result = tool.execute({{"command", "echo 'hello world'"}}, ctx);
    REQUIRE_FALSE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("hello world"));
}

TEST_CASE("BashTool execute permission denied", "[core][tool][bash]") {
    builtin::BashTool tool;
    auto ctx = make_deny_ctx();

    const auto result = tool.execute({{"command", "echo test"}}, ctx);
    REQUIRE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("Permission denied"));
}

TEST_CASE("BashTool execute invalid input", "[core][tool][bash]") {
    builtin::BashTool tool;
    auto ctx = make_allow_ctx();

    const auto result = tool.execute(nlohmann::json::object(), ctx);
    REQUIRE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("Invalid input"));
}

TEST_CASE("BashTool execute aborted", "[core][tool][bash]") {
    builtin::BashTool tool;
    ToolContext ctx;
    ctx.session_id = "s";
    ctx.message_id = "m";
    ctx.agent      = "a";
    ctx.ruleset    = {{"execute", "*", PermissionAction::Allow}};
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(true);

    const auto result = tool.execute({{"command", "echo test"}}, ctx);
    REQUIRE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("aborted"));
}

TEST_CASE("BashTool ask permission rejected", "[core][tool][bash]") {
    builtin::BashTool tool;
    ToolContext ctx;
    ctx.session_id = "s";
    ctx.message_id = "m";
    ctx.agent      = "a";
    ctx.ruleset    = {{"execute", "*", PermissionAction::Ask}};
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    ctx.ask_permission = [](const PermissionRequest&) -> PermissionReply {
        return PermissionReply::reject();
    };

    const auto result = tool.execute({{"command", "echo test"}}, ctx);
    REQUIRE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("rejected"));
}

TEST_CASE("BashTool ask no callback returns error", "[core][tool][bash]") {
    builtin::BashTool tool;
    ToolContext ctx;
    ctx.session_id = "s";
    ctx.message_id = "m";
    ctx.agent      = "a";
    ctx.ruleset    = {{"execute", "*", PermissionAction::Ask}};
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    // No ask_permission callback

    const auto result = tool.execute({{"command", "echo test"}}, ctx);
    REQUIRE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("no ask_permission callback"));
}

TEST_CASE("BashTool ask permission granted", "[core][tool][bash]") {
    builtin::BashTool tool;
    ToolContext ctx;
    ctx.session_id = "s";
    ctx.message_id = "m";
    ctx.agent      = "a";
    ctx.ruleset    = {{"execute", "*", PermissionAction::Ask}};
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);

    bool asked = false;
    ctx.ask_permission = [&](const PermissionRequest& req) -> PermissionReply {
        asked = true;
        REQUIRE(req.permission == "execute");
        return PermissionReply::once();
    };

    const auto result = tool.execute({{"command", "echo granted"}}, ctx);
    REQUIRE(asked);
    REQUIRE_FALSE(result.is_error);
    REQUIRE_THAT(result.output, Catch::Matchers::ContainsSubstring("granted"));
}

TEST_CASE("BashTool metadata contains command and exit_code", "[core][tool][bash]") {
    builtin::BashTool tool;
    auto ctx = make_allow_ctx();

    const auto result = tool.execute({{"command", "echo meta_test"}}, ctx);
    REQUIRE(result.metadata.contains("command"));
    REQUIRE(result.metadata.contains("exit_code"));
    REQUIRE(result.metadata.contains("timeout"));
    REQUIRE(result.metadata["command"] == "echo meta_test");
}
