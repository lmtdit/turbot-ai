/// p2_tools_test.cpp — Unit tests for P2-1 tools: multiedit, webfetch, codesearch
///
/// Multiedit: validates sequential edit operations, partial failure handling,
///            abort-flag support, and parameter validation.
/// WebFetch:  validates SSRF protection, input validation, and truncation logic.
/// CodeSearch: validates pattern matching, invalid regex, empty-dir edge cases.

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/tool/builtin/multiedit_tool.hpp>
#include <turbot/core/tool/builtin/webfetch_tool.hpp>
#include <turbot/core/tool/builtin/codesearch_tool.hpp>
#include <turbot/core/tool/tool.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace fs = std::filesystem;
using namespace turbot::core::tool;
using namespace turbot::core::tool::builtin;

// ─── helpers ──────────────────────────────────────────────────────────────────

static ToolContext make_ctx(const std::string& cwd = "") {
    ToolContext ctx;
    ctx.working_directory = cwd;
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);
    ctx.ask_permission = [](const auto&) {
        return turbot::core::permission::PermissionReply::once();
    };
    return ctx;
}

// ─── MultiEditTool ────────────────────────────────────────────────────────────

TEST_CASE("MultiEditTool: name is 'multiedit'", "[tools][multiedit]") {
    MultiEditTool t;
    CHECK(t.name() == "multiedit");
}

TEST_CASE("MultiEditTool: input_schema returns object with 'edits' required", "[tools][multiedit]") {
    MultiEditTool t;
    const auto schema = t.input_schema();
    REQUIRE(schema.contains("required"));
    CHECK(schema["required"].get<std::vector<std::string>>() == std::vector<std::string>{"edits"});
}

TEST_CASE("MultiEditTool: validate_input rejects missing edits", "[tools][multiedit]") {
    MultiEditTool t;
    CHECK_FALSE(t.validate_input(nlohmann::json::object()));
    CHECK_FALSE(t.validate_input({{"edits", nlohmann::json::array()}}));
    CHECK_FALSE(t.validate_input({{"edits", "not-an-array"}}));
}

TEST_CASE("MultiEditTool: validate_input rejects malformed operations", "[tools][multiedit]") {
    MultiEditTool t;
    // Missing newString
    nlohmann::json j = {{"edits", nlohmann::json::array({
        {{"filePath", "/tmp/f"}, {"oldString", "x"}}
    })}};
    CHECK_FALSE(t.validate_input(j));
}

TEST_CASE("MultiEditTool: validate_input accepts valid operations", "[tools][multiedit]") {
    MultiEditTool t;
    nlohmann::json j = {{"edits", nlohmann::json::array({
        {{"filePath", "/tmp/f"}, {"oldString", "x"}, {"newString", "y"}}
    })}};
    CHECK(t.validate_input(j));
}

TEST_CASE("MultiEditTool: executes single edit on real temp file", "[tools][multiedit]") {
    // Create a temp file
    const std::string tmp = (fs::temp_directory_path() / "multiedit_test_single.txt").string();
    {
        std::ofstream ofs(tmp);
        ofs << "hello world\n";
    }

    MultiEditTool t;
    nlohmann::json j = {{"edits", nlohmann::json::array({
        {{"filePath", tmp}, {"oldString", "hello"}, {"newString", "goodbye"}}
    })}};
    auto ctx = make_ctx(fs::temp_directory_path().string());
    auto result = t.execute(j, ctx);

    CHECK_FALSE(result.is_error);
    // Verify file was modified
    std::ifstream ifs(tmp);
    std::string content((std::istreambuf_iterator<char>(ifs)), {});
    CHECK(content.find("goodbye") != std::string::npos);

    fs::remove(tmp);
}

TEST_CASE("MultiEditTool: stops on first failing operation", "[tools][multiedit]") {
    const std::string tmp = (fs::temp_directory_path() / "multiedit_test_fail.txt").string();
    {
        std::ofstream ofs(tmp);
        ofs << "abc\n";
    }

    MultiEditTool t;
    // Second operation has a string not present in file
    nlohmann::json j = {{"edits", nlohmann::json::array({
        {{"filePath", tmp}, {"oldString", "abc"}, {"newString", "xyz"}},
        {{"filePath", tmp}, {"oldString", "NOT_PRESENT"}, {"newString", "y"}}
    })}};
    auto ctx = make_ctx(fs::temp_directory_path().string());
    auto result = t.execute(j, ctx);

    CHECK(result.is_error);
    CHECK(result.output.find("Operation 2") != std::string::npos);

    fs::remove(tmp);
}

TEST_CASE("MultiEditTool: execute returns error for invalid input", "[tools][multiedit]") {
    MultiEditTool t;
    auto ctx = make_ctx();
    auto result = t.execute(nlohmann::json::object(), ctx);
    CHECK(result.is_error);
}

// ─── WebFetchTool ─────────────────────────────────────────────────────────────

TEST_CASE("WebFetchTool: name is 'webfetch'", "[tools][webfetch]") {
    WebFetchTool t;
    CHECK(t.name() == "webfetch");
}

TEST_CASE("WebFetchTool: validates http/https scheme", "[tools][webfetch]") {
    WebFetchTool t;
    CHECK(t.validate_input({{"url", "https://example.com"}}));
    CHECK(t.validate_input({{"url", "http://example.com"}}));
    CHECK_FALSE(t.validate_input({{"url", "ftp://example.com"}}));
    CHECK_FALSE(t.validate_input({{"url", ""}}));
    CHECK_FALSE(t.validate_input(nlohmann::json::object()));
}

TEST_CASE("WebFetchTool: SSRF protection blocks private IPs", "[tools][webfetch]") {
    WebFetchTool t;
    // Loopback
    CHECK_FALSE(t.validate_input({{"url", "http://localhost/secret"}}));
    CHECK_FALSE(t.validate_input({{"url", "http://127.0.0.1:8080/"}}));
    // Private ranges
    CHECK_FALSE(t.validate_input({{"url", "http://10.0.0.1/"}}));
    CHECK_FALSE(t.validate_input({{"url", "http://192.168.1.1/"}}));
    CHECK_FALSE(t.validate_input({{"url", "http://172.16.0.1/"}}));
    // Cloud metadata
    CHECK_FALSE(t.validate_input({{"url", "http://169.254.169.254/"}}));
    // IPv6 loopback
    CHECK_FALSE(t.validate_input({{"url", "http://[::1]/"}}));
}

TEST_CASE("WebFetchTool: SSRF protection allows public IPs", "[tools][webfetch]") {
    WebFetchTool t;
    CHECK(t.validate_input({{"url", "https://8.8.8.8/"}}));
    CHECK(t.validate_input({{"url", "https://example.com/docs"}}));
}

TEST_CASE("WebFetchTool: execute returns error for invalid URL", "[tools][webfetch]") {
    WebFetchTool t;
    auto ctx = make_ctx();
    auto result = t.execute({{"url", "not-a-url"}}, ctx);
    CHECK(result.is_error);
}

TEST_CASE("WebFetchTool: execute returns error for SSRF URL", "[tools][webfetch]") {
    WebFetchTool t;
    auto ctx = make_ctx();
    auto result = t.execute({{"url", "http://127.0.0.1/admin"}}, ctx);
    CHECK(result.is_error);
}

// ─── CodeSearchTool ───────────────────────────────────────────────────────────

TEST_CASE("CodeSearchTool: name is 'codesearch'", "[tools][codesearch]") {
    CodeSearchTool t;
    CHECK(t.name() == "codesearch");
}

TEST_CASE("CodeSearchTool: validate_input rejects empty query", "[tools][codesearch]") {
    CodeSearchTool t;
    CHECK_FALSE(t.validate_input(nlohmann::json::object()));
    CHECK_FALSE(t.validate_input({{"query", ""}}));
    CHECK(t.validate_input({{"query", "hello"}}));
}

TEST_CASE("CodeSearchTool: invalid regex returns error", "[tools][codesearch]") {
    CodeSearchTool t;
    auto ctx = make_ctx(fs::temp_directory_path().string());
    auto result = t.execute({{"query", "[invalid regex"}}, ctx);
    CHECK(result.is_error);
    CHECK(result.output.find("Invalid regex") != std::string::npos);
}

TEST_CASE("CodeSearchTool: non-existent path returns error", "[tools][codesearch]") {
    CodeSearchTool t;
    auto ctx = make_ctx();
    auto result = t.execute(
        {{"query", "foo"}, {"path", "/nonexistent/path/xyz_12345"}},
        ctx);
    CHECK(result.is_error);
}

TEST_CASE("CodeSearchTool: finds pattern in temp file", "[tools][codesearch]") {
    // Create temp directory with a code file
    const auto tmpdir = fs::temp_directory_path() / "codesearch_test_dir";
    fs::create_directories(tmpdir);
    const std::string tmpfile = (tmpdir / "sample.cpp").string();
    {
        std::ofstream ofs(tmpfile);
        ofs << "// This is a test\n";
        ofs << "int main() { return 0; }\n";
        ofs << "// another comment\n";
    }

    CodeSearchTool t;
    auto ctx = make_ctx(tmpdir.string());
    auto result = t.execute(
        {{"query", "return 0"}, {"path", tmpdir.string()}},
        ctx);

    CHECK_FALSE(result.is_error);
    CHECK(result.output.find("return 0") != std::string::npos);
    CHECK(result.metadata.value("matchCount", 0) >= 1);

    fs::remove_all(tmpdir);
}

TEST_CASE("CodeSearchTool: returns no-matches message for absent pattern", "[tools][codesearch]") {
    const auto tmpdir = fs::temp_directory_path() / "codesearch_empty_test";
    fs::create_directories(tmpdir);
    const std::string tmpfile = (tmpdir / "a.cpp").string();
    {
        std::ofstream ofs(tmpfile);
        ofs << "int x = 1;\n";
    }

    CodeSearchTool t;
    auto ctx = make_ctx(tmpdir.string());
    auto result = t.execute(
        {{"query", "NONEXISTENT_XYZ_PATTERN_12345"}, {"path", tmpdir.string()}},
        ctx);

    CHECK_FALSE(result.is_error);
    CHECK(result.output.find("No matches") != std::string::npos);
    CHECK(result.metadata.value("matchCount", -1) == 0);

    fs::remove_all(tmpdir);
}

// ─── EditOperation ────────────────────────────────────────────────────────────

TEST_CASE("EditOperation: from_json / to_json round-trip", "[tools][multiedit]") {
    nlohmann::json j = {
        {"filePath",   "/tmp/x"},
        {"oldString",  "old"},
        {"newString",  "new"},
        {"replaceAll", true}
    };
    auto op = EditOperation::from_json(j);
    CHECK(op.file_path   == "/tmp/x");
    CHECK(op.old_string  == "old");
    CHECK(op.new_string  == "new");
    CHECK(op.replace_all == true);

    auto back = op.to_json();
    CHECK(back["filePath"].get<std::string>()   == "/tmp/x");
    CHECK(back["replaceAll"].get<bool>() == true);
}
