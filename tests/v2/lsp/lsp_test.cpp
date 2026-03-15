#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/lsp/lsp.hpp>

using namespace turbot::core::lsp;
using namespace turbot::test;

// ==================== Position 测试 ====================

TEST_CASE("LSP.Position.Defaults", "[LSP]") {
    Position pos;
    REQUIRE(pos.line == 0);
    REQUIRE(pos.character == 0);
}

TEST_CASE("LSP.Position.JsonSerialization", "[LSP]") {
    Position pos{5, 10};
    
    nlohmann::json j = pos.to_json();
    REQUIRE(j["line"] == 5);
    REQUIRE(j["character"] == 10);
    
    auto restored = Position::from_json(j);
    REQUIRE(restored.line == 5);
    REQUIRE(restored.character == 10);
}

// ==================== Range 测试 ====================

TEST_CASE("LSP.Range.Defaults", "[LSP]") {
    Range range;
    REQUIRE(range.start.line == 0);
    REQUIRE(range.end.line == 0);
}

TEST_CASE("LSP.Range.JsonSerialization", "[LSP]") {
    Range range{{1, 0}, {5, 20}};
    
    nlohmann::json j = range.to_json();
    REQUIRE(j["start"]["line"] == 1);
    REQUIRE(j["end"]["line"] == 5);
    
    auto restored = Range::from_json(j);
    REQUIRE(restored.start.line == 1);
    REQUIRE(restored.end.character == 20);
}

// ==================== Location 测试 ====================

TEST_CASE("LSP.Location.Defaults", "[LSP]") {
    Location loc;
    REQUIRE(loc.uri.empty());
}

TEST_CASE("LSP.Location.JsonSerialization", "[LSP]") {
    Location loc;
    loc.uri = "file:///test.cpp";
    loc.range = {{0, 0}, {10, 5}};
    
    nlohmann::json j = loc.to_json();
    REQUIRE(j["uri"] == "file:///test.cpp");
    REQUIRE(j.contains("range"));
    
    auto restored = Location::from_json(j);
    REQUIRE(restored.uri == "file:///test.cpp");
    REQUIRE(restored.range.start.line == 0);
}

// ==================== SymbolKind 测试 ====================

TEST_CASE("LSP.SymbolKind.Values", "[LSP]") {
    REQUIRE(static_cast<int>(SymbolKind::File) == 1);
    REQUIRE(static_cast<int>(SymbolKind::Class) == 5);
    REQUIRE(static_cast<int>(SymbolKind::Function) == 12);
    REQUIRE(static_cast<int>(SymbolKind::Interface) == 11);
    REQUIRE(static_cast<int>(SymbolKind::Struct) == 23);
}

// ==================== Symbol 测试 ====================

TEST_CASE("LSP.Symbol.Defaults", "[LSP]") {
    Symbol sym;
    REQUIRE(sym.name.empty());
    REQUIRE_FALSE(sym.container_name.has_value());
}

TEST_CASE("LSP.Symbol.JsonSerialization", "[LSP]") {
    Symbol sym;
    sym.name = "myFunction";
    sym.kind = SymbolKind::Function;
    sym.location.uri = "file:///src/utils.cpp";
    sym.location.range = {{10, 0}, {20, 5}};
    sym.container_name = "Utils";
    
    nlohmann::json j = sym.to_json();
    REQUIRE(j["name"] == "myFunction");
    REQUIRE(j["kind"] == 12);
    REQUIRE(j["containerName"] == "Utils");
    
    auto restored = Symbol::from_json(j);
    REQUIRE(restored.name == "myFunction");
    REQUIRE(restored.kind == SymbolKind::Function);
    REQUIRE(restored.container_name == "Utils");
}

// ==================== DocumentSymbol 测试 ====================

TEST_CASE("LSP.DocumentSymbol.Defaults", "[LSP]") {
    DocumentSymbol sym;
    REQUIRE(sym.name.empty());
    REQUIRE(sym.children.empty());
    REQUIRE_FALSE(sym.detail.has_value());
}

TEST_CASE("LSP.DocumentSymbol.WithChildren", "[LSP]") {
    DocumentSymbol parent;
    parent.name = "MyClass";
    parent.kind = SymbolKind::Class;
    parent.range = {{0, 0}, {100, 0}};
    
    DocumentSymbol child;
    child.name = "myMethod";
    child.kind = SymbolKind::Method;
    child.range = {{10, 4}, {50, 5}};
    
    parent.children.push_back(child);
    
    REQUIRE(parent.children.size() == 1);
    REQUIRE(parent.children[0].name == "myMethod");
}

// ==================== DiagnosticSeverity 测试 ====================

TEST_CASE("LSP.DiagnosticSeverity.Values", "[LSP]") {
    REQUIRE(static_cast<int>(DiagnosticSeverity::Error) == 1);
    REQUIRE(static_cast<int>(DiagnosticSeverity::Warning) == 2);
    REQUIRE(static_cast<int>(DiagnosticSeverity::Info) == 3);
    REQUIRE(static_cast<int>(DiagnosticSeverity::Hint) == 4);
}

// ==================== Diagnostic 测试 ====================

TEST_CASE("LSP.Diagnostic.Defaults", "[LSP]") {
    Diagnostic diag;
    REQUIRE(diag.message.empty());
    REQUIRE(diag.severity == DiagnosticSeverity::Error);
    REQUIRE_FALSE(diag.source.has_value());
    REQUIRE_FALSE(diag.code.has_value());
}

TEST_CASE("LSP.Diagnostic.JsonSerialization", "[LSP]") {
    Diagnostic diag;
    diag.range = {{5, 0}, {5, 20}};
    diag.severity = DiagnosticSeverity::Warning;
    diag.message = "Unused variable";
    diag.source = "clang-tidy";
    diag.code = "unused-variable";
    
    nlohmann::json j = diag.to_json();
    REQUIRE(j["severity"] == 2);
    REQUIRE(j["message"] == "Unused variable");
    REQUIRE(j["source"] == "clang-tidy");
    
    auto restored = Diagnostic::from_json(j);
    REQUIRE(restored.severity == DiagnosticSeverity::Warning);
    REQUIRE(restored.message == "Unused variable");
}

// ==================== pretty_diagnostic 测试 ====================

TEST_CASE("LSP.PrettyDiagnostic.Basic", "[LSP]") {
    Diagnostic diag;
    diag.range = {{4, 9}, {4, 15}};  // line 4, char 9 (0-indexed)
    diag.severity = DiagnosticSeverity::Error;
    diag.message = "undefined variable";
    
    std::string pretty = pretty_diagnostic(diag);
    // Should show line+1, char+1 (1-indexed)
    REQUIRE(pretty.find("ERROR") != std::string::npos);
    REQUIRE(pretty.find("undefined variable") != std::string::npos);
}

TEST_CASE("LSP.PrettyDiagnostic.WithFile", "[LSP]") {
    Diagnostic diag;
    diag.range = {{9, 0}, {9, 10}};
    diag.severity = DiagnosticSeverity::Warning;
    diag.message = "Test warning";
    
    std::string pretty = pretty_diagnostic(diag, "test.cpp");
    REQUIRE(pretty.find("test.cpp") != std::string::npos);
    REQUIRE(pretty.find("WARN") != std::string::npos);  // Implementation uses "WARN" not "WARNING"
}

// ==================== Hover 测试 ====================

TEST_CASE("LSP.Hover.Defaults", "[LSP]") {
    Hover hover;
    REQUIRE(hover.contents.empty());
    REQUIRE_FALSE(hover.range.has_value());
}

TEST_CASE("LSP.Hover.JsonSerialization", "[LSP]") {
    Hover hover;
    hover.contents = "```cpp\nvoid myFunction()\n```";
    hover.range = {{10, 5}, {10, 15}};
    
    nlohmann::json j = hover.to_json();
    REQUIRE(j["contents"] == "```cpp\nvoid myFunction()\n```");
    
    auto restored = Hover::from_json(j);
    REQUIRE(restored.contents == hover.contents);
    REQUIRE(restored.range.has_value());
}

// ==================== language_id_for_extension 测试 ====================

TEST_CASE("LSP.LanguageId.CommonExtensions", "[LSP]") {
    REQUIRE(language_id_for_extension(".cpp") == "cpp");
    REQUIRE(language_id_for_extension(".hpp") == "cpp");
    REQUIRE(language_id_for_extension(".c") == "c");
    REQUIRE(language_id_for_extension(".h") == "c");
    REQUIRE(language_id_for_extension(".py") == "python");
    REQUIRE(language_id_for_extension(".ts") == "typescript");
    REQUIRE(language_id_for_extension(".js") == "javascript");
    REQUIRE(language_id_for_extension(".rs") == "rust");
    REQUIRE(language_id_for_extension(".go") == "go");
}

// ==================== URI helpers 测试 ====================

TEST_CASE("LSP.PathToUri", "[LSP]") {
    std::string uri = path_to_uri("/home/user/project/file.cpp");
    REQUIRE(uri.find("file://") == 0);
    REQUIRE(uri.find("file.cpp") != std::string::npos);
}

TEST_CASE("LSP.UriToPath", "[LSP]") {
    std::string path = uri_to_path("file:///home/user/project/file.cpp");
    REQUIRE(path == "/home/user/project/file.cpp");
}

TEST_CASE("LSP.UriRoundTrip", "[LSP]") {
    std::string original = "/home/user/test.cpp";
    std::string uri = path_to_uri(original);
    std::string restored = uri_to_path(uri);
    REQUIRE(restored == original);
}
