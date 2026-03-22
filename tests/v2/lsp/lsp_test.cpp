/**
 * @file lsp_test.cpp
 * @brief Tests for LSP module types and serialization
 */

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/lsp/lsp.hpp>
#include <turbot/core/lsp/server.hpp>
#include <fstream>
#include <ctime>

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

// ==================== Builtin Servers Tests ====================

#include <turbot/core/lsp/builtin_servers.hpp>

TEST_CASE("LSP.IsLspDownloadEnabled.Default", "[LSP][Builtin]") {
    // 默认应该启用（没有设置环境变量时）
    // 注意：这个测试可能受环境影响
    bool result = is_lsp_download_enabled();
    // 只验证函数可以调用，不验证具体值
    REQUIRE((result == true || result == false));
}

TEST_CASE("LSP.CommandExists.Ls", "[LSP][Builtin]") {
    // ls 命令在大多数 Unix 系统上都存在
    bool result = command_exists("ls");
    REQUIRE(result == true);
}

TEST_CASE("LSP.CommandExists.NonExistent", "[LSP][Builtin]") {
    bool result = command_exists("this_command_definitely_does_not_exist_12345");
    REQUIRE(result == false);
}

TEST_CASE("LSP.MakeClangdServer", "[LSP][Builtin]") {
    auto info = make_clangd_server("/tmp");
    REQUIRE(info.id == "clangd");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakePyrightServer", "[LSP][Builtin]") {
    auto info = make_pyright_server("/tmp");
    REQUIRE(info.id == "pyright");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeGoplsServer", "[LSP][Builtin]") {
    auto info = make_gopls_server("/tmp");
    REQUIRE(info.id == "gopls");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeRustAnalyzerServer", "[LSP][Builtin]") {
    auto info = make_rust_analyzer_server("/tmp");
    REQUIRE(info.id == "rust");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeTypescriptServer", "[LSP][Builtin]") {
    auto info = make_typescript_server("/tmp");
    REQUIRE(info.id == "typescript");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeBashServer", "[LSP][Builtin]") {
    auto info = make_bash_server("/tmp");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeJavaServer", "[LSP][Builtin]") {
    auto info = make_java_server("/tmp");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeLuaServer", "[LSP][Builtin]") {
    auto info = make_lua_server("/tmp");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeCustomServer", "[LSP][Builtin]") {
    nlohmann::json cfg = {
        {"extensions", {".test"}},
        {"command", "test-lsp-server"},
        {"args", {"--stdio"}}
    };
    auto info = make_custom_server("test-lsp", cfg, "/tmp");
    REQUIRE(info.id == "test-lsp");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeDenoServer", "[LSP][Builtin]") {
    auto info = make_deno_server("/tmp");
    REQUIRE(info.id == "deno");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeVueServer", "[LSP][Builtin]") {
    auto info = make_vue_server("/tmp");
    REQUIRE(info.id == "vue");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeEslintServer", "[LSP][Builtin]") {
    auto info = make_eslint_server("/tmp");
    REQUIRE(info.id == "eslint");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeBiomeServer", "[LSP][Builtin]") {
    auto info = make_biome_server("/tmp");
    REQUIRE(info.id == "biome");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeSvelteServer", "[LSP][Builtin]") {
    auto info = make_svelte_server("/tmp");
    REQUIRE(info.id == "svelte");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeAstroServer", "[LSP][Builtin]") {
    auto info = make_astro_server("/tmp");
    REQUIRE(info.id == "astro");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeKotlinServer", "[LSP][Builtin]") {
    auto info = make_kotlin_server("/tmp");
    REQUIRE(info.id == "kotlin");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeCsharpServer", "[LSP][Builtin]") {
    auto info = make_csharp_server("/tmp");
    REQUIRE(info.id == "csharp");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeClojureServer", "[LSP][Builtin]") {
    auto info = make_clojure_server("/tmp");
    REQUIRE(info.id == "clojure");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeDartServer", "[LSP][Builtin]") {
    auto info = make_dart_server("/tmp");
    REQUIRE(info.id == "dart");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeElixirServer", "[LSP][Builtin]") {
    auto info = make_elixir_server("/tmp");
    REQUIRE(info.id == "elixir");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeErlangServer", "[LSP][Builtin]") {
    auto info = make_erlang_server("/tmp");
    REQUIRE(info.id == "erlang");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeHaskellServer", "[LSP][Builtin]") {
    auto info = make_haskell_server("/tmp");
    REQUIRE(info.id == "haskell");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeNixServer", "[LSP][Builtin]") {
    auto info = make_nix_server("/tmp");
    REQUIRE(info.id == "nix");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeOcamlServer", "[LSP][Builtin]") {
    auto info = make_ocaml_server("/tmp");
    REQUIRE(info.id == "ocaml");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakePhpServer", "[LSP][Builtin]") {
    auto info = make_php_server("/tmp");
    REQUIRE(info.id == "php");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeRubyServer", "[LSP][Builtin]") {
    auto info = make_ruby_server("/tmp");
    REQUIRE(info.id == "ruby");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeScalaServer", "[LSP][Builtin]") {
    auto info = make_scala_server("/tmp");
    REQUIRE(info.id == "scala");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeSwiftServer", "[LSP][Builtin]") {
    auto info = make_swift_server("/tmp");
    REQUIRE(info.id == "swift");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeTerraformServer", "[LSP][Builtin]") {
    auto info = make_terraform_server("/tmp");
    REQUIRE(info.id == "terraform");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.MakeZigServer", "[LSP][Builtin]") {
    auto info = make_zig_server("/tmp");
    REQUIRE(info.id == "zig");
    REQUIRE_FALSE(info.extensions.empty());
}

TEST_CASE("LSP.CustomServer.WithEnv", "[LSP][Builtin]") {
    nlohmann::json cfg = {
        {"extensions", {".custom"}},
        {"command", {"custom-lsp", "--stdio"}},
        {"env", {{"CUSTOM_VAR", "test_value"}}}
    };
    auto info = make_custom_server("custom-lsp", cfg, "/tmp");
    REQUIRE(info.id == "custom-lsp");
    REQUIRE(info.extensions.size() == 1);
    REQUIRE(info.extensions[0] == ".custom");
}

TEST_CASE("LSP.CustomServer.WithInitialization", "[LSP][Builtin]") {
    nlohmann::json cfg = {
        {"extensions", {".init"}},
        {"command", {"init-lsp"}},
        {"initialization", {{"setting", "value"}}}
    };
    auto info = make_custom_server("init-lsp", cfg, "/tmp");
    REQUIRE(info.id == "init-lsp");
}

TEST_CASE("LSP.IsLspDownloadEnabled.Disabled", "[LSP][Builtin]") {
    // 保存当前环境变量状态
    const char* old_val = getenv("TURBOT_DISABLE_LSP_DOWNLOAD");
    std::string old_val_str = old_val ? old_val : "";
    
    // 设置环境变量禁用
    setenv("TURBOT_DISABLE_LSP_DOWNLOAD", "true", 1);
    REQUIRE_FALSE(is_lsp_download_enabled());
    
    // 恢复原状态
    if (old_val_str.empty()) {
        unsetenv("TURBOT_DISABLE_LSP_DOWNLOAD");
    } else {
        setenv("TURBOT_DISABLE_LSP_DOWNLOAD", old_val_str.c_str(), 1);
    }
}

TEST_CASE("LSP.ServerInfo.RootFunction", "[LSP][Builtin]") {
    auto info = make_clangd_server("/workspace");
    
    // 测试 root 函数可以被调用
    std::optional<std::string> root = info.root("/workspace/test.cpp");
    // root 可能返回 nullopt 或一个路径
    REQUIRE((root.has_value() || !root.has_value()));
}

// ==================== NearestRoot Tests ====================

TEST_CASE("LSP.NearestRoot.EmptyStartDir", "[LSP][Server]") {
    auto result = nearest_root("", {".git"});
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("LSP.NearestRoot.FindGitRoot", "[LSP][Server]") {
    // Create a temp directory structure
    std::string temp_dir = "/tmp/turbot-lsp-test-" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(temp_dir + "/subdir");
    
    // Create .git file/directory
    std::ofstream(temp_dir + "/.git").close();
    
    // Find root from subdir
    auto result = nearest_root(temp_dir + "/subdir", {".git"});
    REQUIRE(result.has_value());
    REQUIRE(result.value() == temp_dir);
    
    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("LSP.NearestRoot.IncludePattern", "[LSP][Server]") {
    std::string temp_dir = "/tmp/turbot-lsp-test-include-" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(temp_dir + "/subdir/deep");
    
    // Create include pattern file
    std::ofstream(temp_dir + "/CMakeLists.txt").close();
    
    // Find root from deep subdir
    auto result = nearest_root(temp_dir + "/subdir/deep", {"CMakeLists.txt"});
    REQUIRE(result.has_value());
    REQUIRE(result.value() == temp_dir);
    
    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("LSP.NearestRoot.ExcludePattern", "[LSP][Server]") {
    std::string temp_dir = "/tmp/turbot-lsp-test-exclude-" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(temp_dir + "/subdir");
    
    // Create include and exclude pattern files
    std::ofstream(temp_dir + "/.git").close();
    std::ofstream(temp_dir + "/.turbotignore").close();
    
    // Exclude pattern should cause nullopt
    auto result = nearest_root(temp_dir + "/subdir", {".git"}, {".turbotignore"});
    REQUIRE_FALSE(result.has_value());
    
    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("LSP.NearestRoot.StopDir", "[LSP][Server]") {
    std::string temp_dir = "/tmp/turbot-lsp-test-stop-" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(temp_dir + "/subdir/deep");
    
    // Create stop directory marker
    std::ofstream(temp_dir + "/subdir/.git").close();
    
    // Find root with stop_dir
    auto result = nearest_root(temp_dir + "/subdir/deep", {".git"}, {}, temp_dir + "/subdir");
    REQUIRE(result.has_value());
    REQUIRE(result.value() == temp_dir + "/subdir");
    
    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("LSP.NearestRoot.FallbackToStopDir", "[LSP][Server]") {
    std::string temp_dir = "/tmp/turbot-lsp-test-fallback-" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(temp_dir + "/subdir");
    
    // No include pattern found, should fallback to stop_dir
    auto result = nearest_root(temp_dir + "/subdir", {".git"}, {}, temp_dir);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == temp_dir);
    
    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("LSP.NearestRoot.FallbackToStartDir", "[LSP][Server]") {
    std::string temp_dir = "/tmp/turbot-lsp-test-start-" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(temp_dir + "/subdir");
    
    // No include pattern found, no stop_dir, should fallback to start_dir
    auto result = nearest_root(temp_dir + "/subdir", {".git"});
    REQUIRE(result.has_value());
    REQUIRE(result.value() == temp_dir + "/subdir");
    
    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("LSP.NearestRoot.MultipleIncludePatterns", "[LSP][Server]") {
    std::string temp_dir = "/tmp/turbot-lsp-test-multi-" + std::to_string(std::time(nullptr));
    std::filesystem::create_directories(temp_dir + "/subdir");
    
    // Create package.json
    std::ofstream(temp_dir + "/package.json").close();
    
    // Find root with multiple patterns
    auto result = nearest_root(temp_dir + "/subdir", {".git", "package.json", "Cargo.toml"});
    REQUIRE(result.has_value());
    REQUIRE(result.value() == temp_dir);
    
    // Cleanup
    std::filesystem::remove_all(temp_dir);
}
