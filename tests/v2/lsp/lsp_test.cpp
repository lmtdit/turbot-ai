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
    nlohmann::json j = nlohmann::json::object();
    
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

TEST_CASE("LanguageId.MoreExtensions", "[LSP][LanguageId]") {
    REQUIRE(language_id_for_extension(".tsx") == "typescriptreact");
    REQUIRE(language_id_for_extension(".jsx") == "javascriptreact");
    REQUIRE(language_id_for_extension(".java") == "java");
    REQUIRE(language_id_for_extension(".kt") == "kotlin");
    REQUIRE(language_id_for_extension(".rb") == "ruby");
    REQUIRE(language_id_for_extension(".php") == "php");
    REQUIRE(language_id_for_extension(".cs") == "csharp");
    REQUIRE(language_id_for_extension(".swift") == "swift");
    REQUIRE(language_id_for_extension(".sh") == "shellscript");
    REQUIRE(language_id_for_extension(".bash") == "shellscript");
    REQUIRE(language_id_for_extension(".zsh") == "shellscript");
    REQUIRE(language_id_for_extension(".json") == "json");
    REQUIRE(language_id_for_extension(".yaml") == "yaml");
    REQUIRE(language_id_for_extension(".yml") == "yaml");
    REQUIRE(language_id_for_extension(".toml") == "toml");
    REQUIRE(language_id_for_extension(".md") == "markdown");
    REQUIRE(language_id_for_extension(".html") == "html");
    REQUIRE(language_id_for_extension(".css") == "css");
    REQUIRE(language_id_for_extension(".scss") == "scss");
    REQUIRE(language_id_for_extension(".xml") == "xml");
}

TEST_CASE("LanguageId.UnknownExtension", "[LSP][LanguageId]") {
    REQUIRE(language_id_for_extension(".xyz") == "plaintext");
    REQUIRE(language_id_for_extension(".unknown") == "plaintext");
}

TEST_CASE("LanguageId.CppVariants", "[LSP][LanguageId]") {
    REQUIRE(language_id_for_extension(".cc") == "cpp");
    REQUIRE(language_id_for_extension(".cxx") == "cpp");
    REQUIRE(language_id_for_extension(".hxx") == "cpp");
}

TEST_CASE("LanguageId.JsVariants", "[LSP][LanguageId]") {
    REQUIRE(language_id_for_extension(".mjs") == "javascript");
    REQUIRE(language_id_for_extension(".cjs") == "javascript");
}

// ==================== Hover Advanced Tests ====================

TEST_CASE("Hover.FromJson.MarkupContent", "[LSP][Hover]") {
    nlohmann::json j = {
        {"contents", {{"kind", "markdown"}, {"value", "**bold** text"}}}
    };
    
    auto h = Hover::from_json(j);
    REQUIRE(h.contents == "**bold** text");
}

TEST_CASE("Hover.FromJson.MarkedStringArray", "[LSP][Hover]") {
    nlohmann::json j = {
        {"contents", {"First line", "Second line"}}
    };
    
    auto h = Hover::from_json(j);
    REQUIRE(h.contents == "First line");
}

TEST_CASE("Hover.FromJson.MarkedStringObjectArray", "[LSP][Hover]") {
    nlohmann::json j = {
        {"contents", {
            {{"language", "cpp"}, {"value", "int x = 42;"}}
        }}
    };
    
    auto h = Hover::from_json(j);
    REQUIRE(h.contents == "int x = 42;");
}

TEST_CASE("Hover.WithRange", "[LSP][Hover]") {
    nlohmann::json j = {
        {"contents", "test content"},
        {"range", {{"start", {{"line", 5}, {"character", 0}}}, {"end", {{"line", 5}, {"character", 10}}}}}
    };
    
    auto h = Hover::from_json(j);
    REQUIRE(h.contents == "test content");
    REQUIRE(h.range.has_value());
    REQUIRE(h.range->start.line == 5);
}

// ==================== URI Encoding Tests ====================

TEST_CASE("PathToUri.SpecialChars", "[LSP][URI]") {
    std::string path = "/home/user/my file.cpp";
    std::string uri = path_to_uri(path);
    REQUIRE(uri.find("file://") == 0);
    REQUIRE(uri.find("%20") != std::string::npos);  // Space is encoded
}

TEST_CASE("PathToUri.UnreservedChars", "[LSP][URI]") {
    std::string path = "/home/user/test-file_1.0.cpp";
    std::string uri = path_to_uri(path);
    REQUIRE(uri.find("file://") == 0);
    // Unreserved chars should not be encoded
    REQUIRE(uri.find("-") != std::string::npos);
    REQUIRE(uri.find("_") != std::string::npos);
    REQUIRE(uri.find(".") != std::string::npos);
}

TEST_CASE("UriToPath.EncodedChars", "[LSP][URI]") {
    std::string uri = "file:///home/user/my%20file.cpp";
    std::string path = uri_to_path(uri);
    REQUIRE(path.find("my file.cpp") != std::string::npos);
}

TEST_CASE("UriToPath.NoPrefix", "[LSP][URI]") {
    std::string uri = "/home/user/test.cpp";
    std::string path = uri_to_path(uri);
    REQUIRE(path.find("test.cpp") != std::string::npos);
}

// ==================== Diagnostic Advanced Tests ====================

TEST_CASE("Diagnostic.AllSeverities", "[LSP][Diagnostic]") {
    Diagnostic d;
    d.message = "test";
    
    d.severity = DiagnosticSeverity::Error;
    REQUIRE(pretty_diagnostic(d).find("ERROR") != std::string::npos);
    
    d.severity = DiagnosticSeverity::Warning;
    REQUIRE(pretty_diagnostic(d).find("WARN") != std::string::npos);
    
    d.severity = DiagnosticSeverity::Info;
    REQUIRE(pretty_diagnostic(d).find("INFO") != std::string::npos);
    
    d.severity = DiagnosticSeverity::Hint;
    REQUIRE(pretty_diagnostic(d).find("HINT") != std::string::npos);
}

TEST_CASE("Diagnostic.NoFile", "[LSP][Diagnostic]") {
    Diagnostic d;
    d.range.start.line = 5;
    d.range.start.character = 10;
    d.severity = DiagnosticSeverity::Error;
    d.message = "Error without file";
    
    std::string result = pretty_diagnostic(d);
    REQUIRE(result.find("ERROR") != std::string::npos);
    REQUIRE(result.find("Error without file") != std::string::npos);
}

TEST_CASE("Diagnostic.CodeAsString", "[LSP][Diagnostic]") {
    nlohmann::json j = {
        {"range", {{"start", {{"line", 0}}}, {"end", {{"line", 0}}}}},
        {"severity", 1},
        {"message", "Error"},
        {"code", "E001"}
    };
    
    auto d = Diagnostic::from_json(j);
    REQUIRE(d.code == "E001");
}

TEST_CASE("Diagnostic.CodeAsNumber", "[LSP][Diagnostic]") {
    nlohmann::json j = {
        {"range", {{"start", {{"line", 0}}}, {"end", {{"line", 0}}}}},
        {"severity", 1},
        {"message", "Error"},
        {"code", 123}
    };
    
    auto d = Diagnostic::from_json(j);
    REQUIRE(d.code == "123");
}

// ==================== DocumentSymbol Children Tests ====================

TEST_CASE("DocumentSymbol.WithChildren", "[LSP][DocumentSymbol]") {
    DocumentSymbol parent;
    parent.name = "MyClass";
    parent.kind = SymbolKind::Class;
    
    DocumentSymbol child;
    child.name = "myMethod";
    child.kind = SymbolKind::Method;
    
    parent.children.push_back(child);
    
    nlohmann::json j = parent.to_json();
    REQUIRE(j["children"].is_array());
    REQUIRE(j["children"].size() == 1);
    REQUIRE(j["children"][0]["name"] == "myMethod");
}

TEST_CASE("DocumentSymbol.FromJson.WithChildren", "[LSP][DocumentSymbol]") {
    nlohmann::json j = {
        {"name", "ParentClass"},
        {"kind", 5},
        {"range", {{"start", {{"line", 0}}}, {"end", {{"line", 10}}}}},
        {"selectionRange", {{"start", {{"line", 0}}}, {"end", {{"line", 0}}}}},
        {"children", {
            {{"name", "childMethod"}, {"kind", 6}, {"range", {{"start", {{"line", 2}}}, {"end", {{"line", 5}}}}}, {"selectionRange", {{"start", {{"line", 2}}}, {"end", {{"line", 2}}}}}}
        }}
    };
    
    auto ds = DocumentSymbol::from_json(j);
    REQUIRE(ds.name == "ParentClass");
    REQUIRE(ds.children.size() == 1);
    REQUIRE(ds.children[0].name == "childMethod");
}

// ==================== Symbol Advanced Tests ====================

TEST_CASE("Symbol.WithoutContainerName", "[LSP][Symbol]") {
    nlohmann::json j = {
        {"name", "globalFunc"},
        {"kind", 12},
        {"location", {{"uri", "file:///test.cpp"}, {"range", {{"start", {{"line", 0}}}, {"end", {{"line", 0}}}}}}}
    };
    
    auto s = Symbol::from_json(j);
    REQUIRE(s.name == "globalFunc");
    REQUIRE_FALSE(s.container_name.has_value());
}

TEST_CASE("Symbol.NullContainerName", "[LSP][Symbol]") {
    nlohmann::json j = {
        {"name", "myFunc"},
        {"kind", 12},
        {"location", {{"uri", "file:///test.cpp"}, {"range", {{"start", {{"line", 0}}}, {"end", {{"line", 0}}}}}}},
        {"containerName", nullptr}
    };
    
    auto s = Symbol::from_json(j);
    REQUIRE_FALSE(s.container_name.has_value());
}

// ==================== SymbolKind Tests ====================

TEST_CASE("SymbolKind.AllKinds", "[LSP][SymbolKind]") {
    REQUIRE(static_cast<int>(SymbolKind::File) == 1);
    REQUIRE(static_cast<int>(SymbolKind::Module) == 2);
    REQUIRE(static_cast<int>(SymbolKind::Namespace) == 3);
    REQUIRE(static_cast<int>(SymbolKind::Package) == 4);
    REQUIRE(static_cast<int>(SymbolKind::Class) == 5);
    REQUIRE(static_cast<int>(SymbolKind::Method) == 6);
    REQUIRE(static_cast<int>(SymbolKind::Property) == 7);
    REQUIRE(static_cast<int>(SymbolKind::Field) == 8);
    REQUIRE(static_cast<int>(SymbolKind::Constructor) == 9);
    REQUIRE(static_cast<int>(SymbolKind::Enum) == 10);
    REQUIRE(static_cast<int>(SymbolKind::Interface) == 11);
    REQUIRE(static_cast<int>(SymbolKind::Function) == 12);
    REQUIRE(static_cast<int>(SymbolKind::Variable) == 13);
    REQUIRE(static_cast<int>(SymbolKind::Constant) == 14);
    REQUIRE(static_cast<int>(SymbolKind::Struct) == 23);
}

// ==================== Range Edge Cases ====================

TEST_CASE("Range.FromJson.MissingFields", "[LSP][Range]") {
    nlohmann::json j = nlohmann::json::object();  // Empty object
    
    auto r = Range::from_json(j);
    REQUIRE(r.start.line == 0);
    REQUIRE(r.start.character == 0);
    REQUIRE(r.end.line == 0);
    REQUIRE(r.end.character == 0);
}

// ==================== Location Edge Cases ====================

TEST_CASE("Location.FromJson.MissingRange", "[LSP][Location]") {
    nlohmann::json j = {{"uri", "file:///test.cpp"}};
    
    auto l = Location::from_json(j);
    REQUIRE(l.uri == "file:///test.cpp");
    REQUIRE(l.range.start.line == 0);
}
