#pragma once

#include <turbot/core/common/export.hpp>
#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace turbot::core::lsp {

// ─── Basic LSP Types ──────────────────────────────────────────────────────────

struct TURBOT_CORE_API Position {
    int line      = 0;
    int character = 0;

    [[nodiscard]] nlohmann::json to_json() const;
    static Position from_json(const nlohmann::json& j);
};

struct TURBOT_CORE_API Range {
    Position start;
    Position end;

    [[nodiscard]] nlohmann::json to_json() const;
    static Range from_json(const nlohmann::json& j);
};

struct TURBOT_CORE_API Location {
    std::string uri;
    Range range;

    [[nodiscard]] nlohmann::json to_json() const;
    static Location from_json(const nlohmann::json& j);
};

// ─── Symbol types ─────────────────────────────────────────────────────────────

/// SymbolKind values (subset used by Turbot, aligned with OpenCode LSP capabilities)
enum class SymbolKind : int {
    File          = 1,
    Module        = 2,
    Namespace     = 3,
    Package       = 4,
    Class         = 5,
    Method        = 6,
    Property      = 7,
    Field         = 8,
    Constructor   = 9,
    Enum          = 10,
    Interface     = 11,
    Function      = 12,
    Variable      = 13,
    Constant      = 14,
    String        = 15,
    Number        = 16,
    Boolean       = 17,
    Array         = 18,
    Object        = 19,
    Key           = 20,
    Null          = 21,
    EnumMember    = 22,
    Struct        = 23,
    Event         = 24,
    Operator      = 25,
    TypeParameter = 26,
};

struct TURBOT_CORE_API Symbol {
    std::string name;
    SymbolKind  kind;
    Location    location;
    std::optional<std::string> container_name;

    [[nodiscard]] nlohmann::json to_json() const;
    static Symbol from_json(const nlohmann::json& j);
};

struct TURBOT_CORE_API DocumentSymbol {
    std::string name;
    std::optional<std::string> detail;
    SymbolKind  kind;
    Range       range;
    Range       selection_range;
    std::vector<DocumentSymbol> children;

    [[nodiscard]] nlohmann::json to_json() const;
    static DocumentSymbol from_json(const nlohmann::json& j);
};

// ─── Diagnostic ───────────────────────────────────────────────────────────────

/// DiagnosticSeverity (1=Error 2=Warning 3=Info 4=Hint)
enum class DiagnosticSeverity : int {
    Error   = 1,
    Warning = 2,
    Info    = 3,
    Hint    = 4,
};

struct TURBOT_CORE_API Diagnostic {
    Range       range;
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string message;
    std::optional<std::string> source;
    std::optional<std::string> code;

    [[nodiscard]] nlohmann::json to_json() const;
    static Diagnostic from_json(const nlohmann::json& j);
};

/// Format diagnostic for display.
/// Output: "ERROR [line+1:char+1] message" (with optional "file:" prefix)
/// Aligned with OpenCode LSP.Diagnostic.pretty
[[nodiscard]] TURBOT_CORE_API std::string pretty_diagnostic(
    const Diagnostic& d,
    const std::string& file = ""
);

// ─── Hover ───────────────────────────────────────────────────────────────────

struct TURBOT_CORE_API Hover {
    std::string contents;  // MarkupContent.value or plaintext
    std::optional<Range> range;

    [[nodiscard]] nlohmann::json to_json() const;
    static Hover from_json(const nlohmann::json& j);
};

// ─── LanguageId mapping ───────────────────────────────────────────────────────

/// Get LSP languageId for a file extension (e.g., ".cpp" → "cpp")
/// Aligned with OpenCode LANGUAGE_EXTENSIONS map
[[nodiscard]] TURBOT_CORE_API std::string language_id_for_extension(
    const std::string& extension
);

// ─── URI helpers ──────────────────────────────────────────────────────────────

/// Convert absolute file path to file:// URI
[[nodiscard]] TURBOT_CORE_API std::string path_to_uri(const std::string& path);

/// Convert file:// URI to absolute path
[[nodiscard]] TURBOT_CORE_API std::string uri_to_path(const std::string& uri);

}  // namespace turbot::core::lsp
