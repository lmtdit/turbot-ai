/**
 * @file lsp_test.cpp
 * @brief Tests for LSP module types and serialization
 */

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/lsp/lsp.hpp>

using namespace turbot::core::lsp;

// ==================== Position Tests ====================

TEST_CASE("Position.ToJson", "[LSP][Position]") {
    Position p;
    p.line = 10;
    p.character = 5;
    
    auto j = p.to_json();
    REQUIRE(j["line"] == 10);
    REQUIRE(j["character"] == 5);
}

TEST_CASE("Position.FromJson", "[LSP][Position]") {
    nlohmann::json j = {{"line", 20}, {"character", 15}};
    
    auto p = Position::from_json(j);
    REQUIRE(p.line == 20);
    REQUIRE(p.character == 15);
}

TEST_CASE("Position.FromJson.Defaults", "[LSP][Position]") {
    nlohmann::json j = {};
    
    auto p = Position::from_json(j);
    REQUIRE(p.line == 0);
    REQUIRE(p.character == 0);
}

// ==================== Range Tests ====================

TEST_CASE("Range.ToJson", "[LSP][Range]") {
    Range r;
    r.start.line = 1;
    r.start.character = 2;
    r.end.line = 3;
    r.end.character = 4;
    
    auto j = r.to_json();
    REQUIRE(j["start"]["line"] == 1);
    REQUIRE(j["start"]["character"] == 2);
    REQUIRE(j["end"]["line"] == 3);
    REQUIRE(j["end"]["character"] == 4);
}

TEST_CASE("Range.FromJson", "[LSP][Range]") {
    nlohmann::json j = {
        {"start", {{"line", 5}, {"character", 6}}},
        {"end", {{"line", 7}, {"character", 8}}}
    };
    
    auto r = Range::from_json(j);
    REQUIRE(r.start.line == 5);
    REQUIRE(r.start.character == 6);
    REQUIRE(r.end.line == 7);
    REQUIRE(r.end.character == 8);
}

// ==================== Location Tests ====================

TEST_CASE("Location.ToJson", "[LSP][Location]") {
    Location l;
    l.uri = "file:///test.cpp";
    l.range.start.line = 1;
    l.range.start.character = 0;
    l.range.end.line = 1;
    l.range.end.character = 10;
    
    auto j = l.to_json();
    REQUIRE(j["uri"] == "file:///test.cpp");
    REQUIRE(j["range"]["start"]["line"] == 1);
}

TEST_CASE("Location.FromJson", "[LSP][Location]") {
    nlohmann::json j = {
        {"uri", "file:///example.cpp"},
        {"range", {{"start", {{"line", 0}, {"character", 0}}}, {"end", {{"line", 0}, {"character", 5}}}}}
    };
    
    auto l = Location::from_json(j);
    REQUIRE(l.uri == "file:///example.cpp");
    REQUIRE(l.range.start.line == 0);
}

// ==================== Symbol Tests ====================

TEST_CASE("Symbol.ToJson", "[LSP][Symbol]") {
    Symbol s;
    s.name = "myFunction";
    s.kind = SymbolKind::Function;
    s.location.uri = "file:///test.cpp";
    s.container_name = "MyClass";
    
    auto j = s.to_json();
    REQUIRE(j["name"] == "myFunction");
    REQUIRE(j["kind"] == static_cast<int>(SymbolKind::Function));
    REQUIRE(j["containerName"] == "MyClass");
}

TEST_CASE("Symbol.FromJson", "[LSP][Symbol]") {
    nlohmann::json j = {
        {"name", "MyClass"},
        {"kind", 5},
        {"location", {{"uri", "file:///test.h"}, {"range", {{"start", {{"line", 0}}}, {"end", {{"line", 0}}}}}}},
        {"containerName", "MyNamespace"}
    };
    
    auto s = Symbol::from_json(j);
    REQUIRE(s.name == "MyClass");
    REQUIRE(s.kind == SymbolKind::Class);
    REQUIRE(s.container_name == "MyNamespace");
}

// ==================== DocumentSymbol Tests ====================

TEST_CASE("DocumentSymbol.ToJson", "[LSP][DocumentSymbol]") {
    DocumentSymbol ds;
    ds.name = "main";
    ds.kind = SymbolKind::Function;
    ds.range.start.line = 0;
    ds.range.end.line = 10;
    ds.selection_range.start.line = 0;
    ds.selection_range.end.line = 0;
    ds.detail = "int main()";
    
    auto j = ds.to_json();
    REQUIRE(j["name"] == "main");
    REQUIRE(j["kind"] == static_cast<int>(SymbolKind::Function));
    REQUIRE(j["detail"] == "int main()");
}

TEST_CASE("DocumentSymbol.FromJson", "[LSP][DocumentSymbol]") {
    nlohmann::json j = {
        {"name", "testFunc"},
        {"kind", 12},
        {"range", {{"start", {{"line", 5}}}, {"end", {{"line", 10}}}}},
        {"selectionRange", {{"start", {{"line", 5}}}, {"end", {{"line", 5}}}}},
        {"detail", "void testFunc()"},
        {"children", nlohmann::json::array()}
    };
    
    auto ds = DocumentSymbol::from_json(j);
    REQUIRE(ds.name == "testFunc");
    REQUIRE(ds.kind == SymbolKind::Function);
    REQUIRE(ds.detail == "void testFunc()");
}

// ==================== Diagnostic Tests ====================

TEST_CASE("Diagnostic.ToJson", "[LSP][Diagnostic]") {
    Diagnostic d;
    d.range.start.line = 5;
    d.range.start.character = 10;
    d.range.end.line = 5;
    d.range.end.character = 15;
    d.severity = DiagnosticSeverity::Error;
    d.message = "Undefined variable";
    d.source = "clang";
    d.code = "undeclared";
    
    auto j = d.to_json();
    REQUIRE(j["severity"] == static_cast<int>(DiagnosticSeverity::Error));
    REQUIRE(j["message"] == "Undefined variable");
    REQUIRE(j["source"] == "clang");
    REQUIRE(j["code"] == "undeclared");
}

TEST_CASE("Diagnostic.FromJson", "[LSP][Diagnostic]") {
    nlohmann::json j = {
        {"range", {{"start", {{"line", 1}, {"character", 0}}}, {"end", {{"line", 1}, {"character", 5}}}}},
        {"severity", 2},
        {"message", "Warning message"},
        {"source", "gcc"},
        {"code", 123}
    };
    
    auto d = Diagnostic::from_json(j);
    REQUIRE(d.severity == DiagnosticSeverity::Warning);
    REQUIRE(d.message == "Warning message");
    REQUIRE(d.source == "gcc");
    REQUIRE(d.code == "123");
}

TEST_CASE("PrettyDiagnostic.Format", "[LSP][Diagnostic]") {
    Diagnostic d;
    d.range.start.line = 9;  // 0-indexed, should display as 10
    d.range.start.character = 4;  // 0-indexed, should display as 5
    d.severity = DiagnosticSeverity::Error;
    d.message = "Test error";
    
    std::string result = pretty_diagnostic(d, "test.cpp");
    REQUIRE(result.find("test.cpp:10:5") != std::string::npos);
    REQUIRE(result.find("ERROR") != std::string::npos);
    REQUIRE(result.find("Test error") != std::string::npos);
}

// ==================== Hover Tests ====================

TEST_CASE("Hover.ToJson", "[LSP][Hover]") {
    Hover h;
    h.contents = "int x = 42";
    h.range = Range{Position{0, 0}, Position{0, 5}};
    
    auto j = h.to_json();
    REQUIRE(j["contents"] == "int x = 42");
}

TEST_CASE("Hover.FromJson", "[LSP][Hover]") {
    nlohmann::json j = {
        {"contents", "**bold** text"}
    };
    
    auto h = Hover::from_json(j);
    REQUIRE(h.contents == "**bold** text");
}

// ==================== URI Helpers Tests ====================

TEST_CASE("PathToUri", "[LSP][URI]") {
    std::string path = "/home/user/test.cpp";
    std::string uri = path_to_uri(path);
    REQUIRE(uri.find("file://") == 0);
    REQUIRE(uri.find("test.cpp") != std::string::npos);
}

TEST_CASE("UriToPath", "[LSP][URI]") {
    std::string uri = "file:///home/user/test.cpp";
    std::string path = uri_to_path(uri);
    REQUIRE(path.find("test.cpp") != std::string::npos);
}

// ==================== LanguageId Tests ====================

TEST_CASE("LanguageId.ForExtension", "[LSP][LanguageId]") {
    REQUIRE(language_id_for_extension(".cpp") == "cpp");
    REQUIRE(language_id_for_extension(".c") == "c");
    REQUIRE(language_id_for_extension(".h") == "c");  // .h maps to c by default
    REQUIRE(language_id_for_extension(".hpp") == "cpp");
    REQUIRE(language_id_for_extension(".py") == "python");
    REQUIRE(language_id_for_extension(".js") == "javascript");
    REQUIRE(language_id_for_extension(".ts") == "typescript");
    REQUIRE(language_id_for_extension(".go") == "go");
    REQUIRE(language_id_for_extension(".rs") == "rust");
}
