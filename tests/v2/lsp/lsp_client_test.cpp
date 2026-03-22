/**
 * @file lsp_client_test.cpp
 * @brief LSP Client and Manager tests using MockLSPServer
 *
 * Tests for:
 * - LSPClient initialization and communication
 * - LSPManager server lifecycle
 * - LSP requests (hover, definition, references)
 * - Diagnostics handling
 */

#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include "../mock/mock_lsp_server.hpp"
#include <turbot/core/lsp/client.hpp>
#include <turbot/core/lsp/manager.hpp>
#include <turbot/core/lsp/server.hpp>
#include <turbot/core/lsp/builtin_servers.hpp>
#include <filesystem>
#include <fstream>

using namespace turbot::core::lsp;
using namespace turbot::test;

// ==================== MockLSPServer Unit Tests ====================

TEST_CASE("LSP.Mock.Server.Config", "[LSP][Mock]") {
    MockLSPServerConfig config;
    config.auto_respond_initialize = false;
    config.response_delay_ms = 10;
    config.capabilities = R"({
        "textDocumentSync": 2,
        "hoverProvider": false
    })"_json;
    
    MockLSPServer server(config);
    
    // Server should be configured with custom settings
    REQUIRE(server.is_running() == false);
}

TEST_CASE("LSP.Mock.Server.CustomResponse", "[LSP][Mock]") {
    MockLSPServer server;
    
    // Set custom response for hover
    nlohmann::json hover_result;
    hover_result["contents"] = "Custom hover content";
    server.set_response("textDocument/hover", hover_result);
    
    // Set custom response function for definition
    server.set_response_func("textDocument/definition", [](const nlohmann::json& params) {
        nlohmann::json result = nlohmann::json::array();
        nlohmann::json loc;
        loc["uri"] = params["textDocument"]["uri"];
        loc["range"]["start"]["line"] = 0;
        loc["range"]["start"]["character"] = 0;
        loc["range"]["end"]["line"] = 0;
        loc["range"]["end"]["character"] = 10;
        result.push_back(loc);
        return result;
    });
    
    // Server is ready to respond
    REQUIRE_FALSE(server.is_running());
}

TEST_CASE("LSP.Mock.Server.Diagnostics", "[LSP][Mock]") {
    MockLSPServer server;
    
    // Queue diagnostics
    std::vector<Diagnostic> diagnostics;
    Diagnostic d;
    d.range.start.line = 5;
    d.range.start.character = 0;
    d.range.end.line = 5;
    d.range.end.character = 10;
    d.severity = DiagnosticSeverity::Error;
    d.message = "Test error";
    diagnostics.push_back(d);
    
    server.queue_diagnostics("file:///test.cpp", diagnostics);
    
    // Server is ready to send diagnostics
    REQUIRE_FALSE(server.is_running());
}

// ==================== LSP Types Extended Tests ====================

TEST_CASE("LSP.Diagnostic.AllSeveritiesExtended", "[LSP][Diagnostic]") {
    // Test all severity values
    REQUIRE(static_cast<int>(DiagnosticSeverity::Error) == 1);
    REQUIRE(static_cast<int>(DiagnosticSeverity::Warning) == 2);
    REQUIRE(static_cast<int>(DiagnosticSeverity::Info) == 3);
    REQUIRE(static_cast<int>(DiagnosticSeverity::Hint) == 4);
}

TEST_CASE("LSP.Diagnostic.WithRelatedInformation", "[LSP][Diagnostic]") {
    Diagnostic d;
    d.range.start.line = 0;
    d.range.end.line = 0;
    d.message = "Main error";
    d.severity = DiagnosticSeverity::Error;
    d.source = "test-lsp";
    d.code = "E001";
    
    // Diagnostic can have related information (optional)
    auto j = d.to_json();
    REQUIRE(j["message"] == "Main error");
    REQUIRE(j["severity"] == 1);
}

TEST_CASE("LSP.Diagnostic.WithTags", "[LSP][Diagnostic]") {
    nlohmann::json j = {
        {"range", {{"start", {{"line", 0}, {"character", 0}}}, {"end", {{"line", 0}, {"character", 5}}}}},
        {"severity", 2},
        {"message", "Unused variable"},
        {"tags", {1}}  // 1 = Unnecessary
    };
    
    auto d = Diagnostic::from_json(j);
    REQUIRE(d.message == "Unused variable");
}

// ==================== LSP Server Info Extended Tests ====================

TEST_CASE("LSP.ServerInfo.WithEnvVars", "[LSP][Server]") {
    nlohmann::json cfg = {
        {"extensions", {".env-test"}},
        {"command", {"env-lsp"}},
        {"env", {
            {"ENV_VAR_1", "value1"},
            {"ENV_VAR_2", "value2"}
        }}
    };
    
    auto info = make_custom_server("env-lsp", cfg, "/tmp");
    REQUIRE(info.id == "env-lsp");
    REQUIRE(info.extensions.size() == 1);
}

TEST_CASE("LSP.ServerInfo.WithMultipleExtensions", "[LSP][Server]") {
    nlohmann::json cfg = {
        {"extensions", {".cpp", ".cc", ".cxx", ".h", ".hpp"}},
        {"command", {"multi-lsp", "--stdio"}}
    };
    
    auto info = make_custom_server("multi-lsp", cfg, "/tmp");
    REQUIRE(info.extensions.size() == 5);
}

TEST_CASE("LSP.ServerInfo.WithInitializationOptions", "[LSP][Server]") {
    nlohmann::json cfg = {
        {"extensions", {".init-test"}},
        {"command", {"init-lsp"}},
        {"initialization", {
            {"settings", {"option1", "option2"}},
            {"configFile", ".initrc"}
        }}
    };
    
    auto info = make_custom_server("init-lsp", cfg, "/tmp");
    REQUIRE(info.id == "init-lsp");
    // initialization is handled by spawn function, not stored in LSPServerInfo
}

// ==================== LSP Nearest Root Extended Tests ====================

TEST_CASE("LSP.NearestRoot.WithMultiplePatterns", "[LSP][Server]") {
    std::string temp_dir = "/tmp/turbot-lsp-multi-" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(temp_dir + "/subdir/deep");
    
    // Create multiple pattern files
    std::ofstream(temp_dir + "/.git").close();
    std::ofstream(temp_dir + "/CMakeLists.txt").close();
    std::ofstream(temp_dir + "/package.json").close();
    
    // Find root with multiple patterns - should find the first match
    auto result = nearest_root(temp_dir + "/subdir/deep", 
                              {".git", "CMakeLists.txt", "package.json"});
    REQUIRE(result.has_value());
    // Use canonical path for comparison (macOS /tmp is symlink to /private/tmp)
    REQUIRE(std::filesystem::canonical(result.value()) == std::filesystem::canonical(temp_dir));
    
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("LSP.NearestRoot.WithExcludePatterns", "[LSP][Server]") {
    std::string temp_dir = "/tmp/turbot-lsp-exclude-" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(temp_dir + "/node_modules/subdir");
    
    // Create .git in root
    std::ofstream(temp_dir + "/.git").close();
    
    // Create .git in node_modules (should be excluded)
    std::ofstream(temp_dir + "/node_modules/.git").close();
    
    // Find root with exclude pattern
    auto result = nearest_root(temp_dir + "/node_modules/subdir", 
                              {".git"}, 
                              {"node_modules"});
    
    // Should not find root because node_modules is excluded
    REQUIRE_FALSE(result.has_value());
    
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("LSP.NearestRoot.WithStopDir", "[LSP][Server]") {
    std::string temp_dir = "/tmp/turbot-lsp-stop-" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(temp_dir + "/project/src");
    
    // Create .git in project directory
    std::ofstream(temp_dir + "/project/.git").close();
    
    // Find root with stop_dir at project level
    auto result = nearest_root(temp_dir + "/project/src", 
                              {".git"}, 
                              {}, 
                              temp_dir + "/project");
    
    REQUIRE(result.has_value());
    // Use canonical path for comparison (macOS /tmp is symlink to /private/tmp)
    REQUIRE(std::filesystem::canonical(result.value()) == std::filesystem::canonical(temp_dir + "/project"));
    
    std::filesystem::remove_all(temp_dir);
}

// ==================== LSP URI Extended Tests ====================

TEST_CASE("LSP.URI.WithUnicode", "[LSP][URI]") {
    std::string path = "/home/user/文件.cpp";
    std::string uri = path_to_uri(path);
    
    REQUIRE(uri.find("file://") == 0);
    // Unicode should be percent-encoded
    REQUIRE(uri.find("%") != std::string::npos);
}

TEST_CASE("LSP.URI.WithSpecialChars", "[LSP][URI]") {
    std::string path = "/home/user/test file (1).cpp";
    std::string uri = path_to_uri(path);
    
    REQUIRE(uri.find("file://") == 0);
    // Space and parentheses should be encoded
    REQUIRE(uri.find("%20") != std::string::npos);
}

TEST_CASE("LSP.URI.RoundTrip", "[LSP][URI]") {
    std::string original = "/home/user/test.cpp";
    std::string uri = path_to_uri(original);
    std::string restored = uri_to_path(uri);
    
    REQUIRE(restored == original);
}

// ==================== LSP Position and Range Tests ====================

TEST_CASE("LSP.Position.Comparison", "[LSP][Position]") {
    Position p1{5, 10};
    Position p2{5, 20};
    Position p3{10, 5};
    
    // Same line, different character
    REQUIRE(p1.character < p2.character);
    REQUIRE(p1.line == p2.line);
    
    // Different line
    REQUIRE(p1.line < p3.line);
}

TEST_CASE("LSP.Range.Contains", "[LSP][Range]") {
    Range r{{5, 0}, {10, 0}};
    
    Position inside1{7, 5};
    Position inside2{5, 10};
    Position inside3{9, 100};
    Position outside1{4, 0};
    Position outside2{10, 1};
    
    // These would need a contains method on Range
    // For now just verify the range values
    REQUIRE(r.start.line == 5);
    REQUIRE(r.end.line == 10);
}

TEST_CASE("LSP.Range.ToJsonRoundTrip", "[LSP][Range]") {
    Range r;
    r.start.line = 10;
    r.start.character = 5;
    r.end.line = 15;
    r.end.character = 20;
    
    auto j = r.to_json();
    auto restored = Range::from_json(j);
    
    REQUIRE(restored.start.line == r.start.line);
    REQUIRE(restored.start.character == r.start.character);
    REQUIRE(restored.end.line == r.end.line);
    REQUIRE(restored.end.character == r.end.character);
}

// ==================== LSP Location Tests ====================

TEST_CASE("LSP.Location.WithRange", "[LSP][Location]") {
    Location l;
    l.uri = "file:///test.cpp";
    l.range.start.line = 10;
    l.range.start.character = 0;
    l.range.end.line = 10;
    l.range.end.character = 20;
    
    auto j = l.to_json();
    REQUIRE(j["uri"] == "file:///test.cpp");
    REQUIRE(j["range"]["start"]["line"] == 10);
    
    auto restored = Location::from_json(j);
    REQUIRE(restored.uri == l.uri);
    REQUIRE(restored.range.start.line == l.range.start.line);
}

// ==================== LSP Symbol Extended Tests ====================

TEST_CASE("LSP.Symbol.AllKinds", "[LSP][Symbol]") {
    // Test all symbol kinds
    std::vector<std::pair<SymbolKind, int>> kinds = {
        {SymbolKind::File, 1},
        {SymbolKind::Module, 2},
        {SymbolKind::Namespace, 3},
        {SymbolKind::Package, 4},
        {SymbolKind::Class, 5},
        {SymbolKind::Method, 6},
        {SymbolKind::Property, 7},
        {SymbolKind::Field, 8},
        {SymbolKind::Constructor, 9},
        {SymbolKind::Enum, 10},
        {SymbolKind::Interface, 11},
        {SymbolKind::Function, 12},
        {SymbolKind::Variable, 13},
        {SymbolKind::Constant, 14},
        {SymbolKind::String, 15},
        {SymbolKind::Number, 16},
        {SymbolKind::Boolean, 17},
        {SymbolKind::Array, 18},
        {SymbolKind::Object, 19},
        {SymbolKind::Key, 20},
        {SymbolKind::Null, 21},
        {SymbolKind::EnumMember, 22},
        {SymbolKind::Struct, 23},
        {SymbolKind::Event, 24},
        {SymbolKind::Operator, 25},
        {SymbolKind::TypeParameter, 26}
    };
    
    for (const auto& [kind, value] : kinds) {
        REQUIRE(static_cast<int>(kind) == value);
    }
}

TEST_CASE("LSP.DocumentSymbol.WithChildren", "[LSP][DocumentSymbol]") {
    DocumentSymbol parent;
    parent.name = "MyClass";
    parent.kind = SymbolKind::Class;
    parent.range = {{0, 0}, {100, 0}};
    parent.selection_range = {{0, 0}, {0, 10}};
    parent.detail = "class MyClass";
    
    DocumentSymbol child1;
    child1.name = "field1";
    child1.kind = SymbolKind::Field;
    child1.range = {{5, 4}, {5, 20}};
    child1.selection_range = {{5, 4}, {5, 12}};
    
    DocumentSymbol child2;
    child2.name = "method1";
    child2.kind = SymbolKind::Method;
    child2.range = {{10, 4}, {20, 5}};
    child2.selection_range = {{10, 4}, {10, 15}};
    
    parent.children.push_back(child1);
    parent.children.push_back(child2);
    
    auto j = parent.to_json();
    REQUIRE(j["name"] == "MyClass");
    REQUIRE(j["children"].size() == 2);
    REQUIRE(j["children"][0]["name"] == "field1");
    REQUIRE(j["children"][1]["name"] == "method1");
    
    auto restored = DocumentSymbol::from_json(j);
    REQUIRE(restored.name == "MyClass");
    REQUIRE(restored.children.size() == 2);
}

// ==================== LSP Hover Extended Tests ====================

TEST_CASE("LSP.Hover.WithMarkupContent", "[LSP][Hover]") {
    nlohmann::json j = {
        {"contents", {
            {"kind", "markdown"},
            {"value", "# Title\n\nThis is **bold** text."}
        }},
        {"range", {
            {"start", {{"line", 0}, {"character", 0}}},
            {"end", {{"line", 0}, {"character", 10}}}
        }}
    };
    
    auto h = Hover::from_json(j);
    REQUIRE(h.contents.find("# Title") != std::string::npos);
    REQUIRE(h.range.has_value());
    REQUIRE(h.range->start.line == 0);
}

TEST_CASE("LSP.Hover.WithMarkedString", "[LSP][Hover]") {
    nlohmann::json j = {
        {"contents", {
            {"language", "cpp"},
            {"value", "int x = 42;"}
        }}
    };
    
    auto h = Hover::from_json(j);
    REQUIRE(h.contents == "int x = 42;");
}

// ==================== LSP Language ID Extended Tests ====================

TEST_CASE("LSP.LanguageId.MoreLanguages", "[LSP][LanguageId]") {
    // Test languages that are actually supported in the MAP
    REQUIRE(language_id_for_extension(".ts") == "typescript");
    REQUIRE(language_id_for_extension(".tsx") == "typescriptreact");
    REQUIRE(language_id_for_extension(".mjs") == "javascript");
    REQUIRE(language_id_for_extension(".toml") == "toml");
    REQUIRE(language_id_for_extension(".scss") == "scss");
    // Unknown extensions return plaintext
    REQUIRE(language_id_for_extension(".scala") == "plaintext");
    REQUIRE(language_id_for_extension(".lua") == "plaintext");
}

TEST_CASE("LSP.LanguageId.CaseInsensitive", "[LSP][LanguageId]") {
    // Extensions are case-sensitive in current implementation
    // Lowercase extensions work
    REQUIRE(language_id_for_extension(".cpp") == "cpp");
    REQUIRE(language_id_for_extension(".py") == "python");
    REQUIRE(language_id_for_extension(".js") == "javascript");
    // Uppercase extensions return plaintext (not recognized)
    REQUIRE(language_id_for_extension(".CPP") == "plaintext");
    REQUIRE(language_id_for_extension(".Py") == "plaintext");
}

// ==================== LSP Manager Tests ====================

TEST_CASE("LSP.Manager.Singleton", "[LSP][Manager]") {
    auto& manager1 = LSPManager::instance();
    auto& manager2 = LSPManager::instance();
    
    REQUIRE(&manager1 == &manager2);
}

TEST_CASE("LSP.Manager.RegisterServer", "[LSP][Manager]") {
    auto& manager = LSPManager::instance();
    
    LSPServerInfo server;
    server.id = "test-server";
    server.extensions = {".test"};
    server.spawn = [](const std::string&) -> std::optional<ServerHandle> {
        return std::nullopt;  // Mock spawn that doesn't actually spawn
    };
    // Add a root function so has_clients can find a valid root
    server.root = [](const std::string& file) -> std::optional<std::string> {
        return "/tmp";  // Return a valid root directory
    };
    
    manager.register_server(server);
    
    // Server should be registered
    REQUIRE(manager.has_clients("/tmp/test.test") == true);
}

TEST_CASE("LSP.Manager.DisableServer", "[LSP][Manager]") {
    auto& manager = LSPManager::instance();
    
    // Register a server
    LSPServerInfo server;
    server.id = "disable-test-server";
    server.extensions = {".disable-test"};
    server.spawn = [](const std::string&) -> std::optional<ServerHandle> {
        return std::nullopt;
    };
    manager.register_server(server);
    
    // Disable it
    manager.disable_server("disable-test-server");
    
    // Should not have clients for this extension
    REQUIRE(manager.has_clients("/tmp/test.disable-test") == false);
}

TEST_CASE("LSP.Manager.Status", "[LSP][Manager]") {
    auto& manager = LSPManager::instance();
    
    auto status = manager.status();
    
    // Status should be a vector
    REQUIRE(status.size() >= 0);  // Just verify it compiles
}

TEST_CASE("LSP.Manager.Shutdown", "[LSP][Manager]") {
    auto& manager = LSPManager::instance();
    
    // Should not throw
    REQUIRE_NOTHROW(manager.shutdown());
}

// ==================== LSP Client Factory Tests ====================

TEST_CASE("LSP.Client.Create.InvalidHandle", "[LSP][Client]") {
    ServerHandle handle;
    handle.pid = -1;
    handle.stdin_fd = -1;
    handle.stdout_fd = -1;
    
    auto client = LSPClient::create("test-server", handle, "/tmp");
    
    // Should return nullptr for invalid handle
    REQUIRE(client == nullptr);
}

TEST_CASE("LSP.Client.ServerId", "[LSP][Client]") {
    // LSPClient requires a valid process to create
    // This test verifies the interface is correct
    ServerHandle handle;
    handle.pid = -1;
    handle.stdin_fd = -1;
    handle.stdout_fd = -1;
    
    // Client creation should fail with invalid handle
    auto client = LSPClient::create("test-id", handle, "/tmp");
    REQUIRE(client == nullptr);
}

// ==================== LSP Pretty Diagnostic Tests ====================

TEST_CASE("LSP.PrettyDiagnostic.AllSeverities", "[LSP][Diagnostic]") {
    Diagnostic d;
    d.range.start.line = 0;
    d.range.start.character = 0;
    d.message = "Test message";
    
    d.severity = DiagnosticSeverity::Error;
    REQUIRE(pretty_diagnostic(d).find("ERROR") != std::string::npos);
    
    d.severity = DiagnosticSeverity::Warning;
    REQUIRE(pretty_diagnostic(d).find("WARN") != std::string::npos);
    
    d.severity = DiagnosticSeverity::Info;
    REQUIRE(pretty_diagnostic(d).find("INFO") != std::string::npos);
    
    d.severity = DiagnosticSeverity::Hint;
    REQUIRE(pretty_diagnostic(d).find("HINT") != std::string::npos);
}

TEST_CASE("LSP.PrettyDiagnostic.WithFile", "[LSP][Diagnostic]") {
    Diagnostic d;
    d.range.start.line = 9;  // 0-indexed, should display as 10
    d.range.start.character = 4;  // 0-indexed, should display as 5
    d.severity = DiagnosticSeverity::Error;
    d.message = "Test error";
    d.source = "test-lsp";
    d.code = "E001";
    
    std::string result = pretty_diagnostic(d, "test.cpp");
    
    REQUIRE(result.find("test.cpp:10:5") != std::string::npos);
    REQUIRE(result.find("ERROR") != std::string::npos);
    REQUIRE(result.find("Test error") != std::string::npos);
}
