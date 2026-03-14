#include <turbot/core/lsp/lsp.hpp>

#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace turbot::core::lsp {

// ─── Position ─────────────────────────────────────────────────────────────────

nlohmann::json Position::to_json() const {
    return {{"line", line}, {"character", character}};
}

Position Position::from_json(const nlohmann::json& j) {
    Position p;
    p.line      = j.value("line", 0);
    p.character = j.value("character", 0);
    return p;
}

// ─── Range ────────────────────────────────────────────────────────────────────

nlohmann::json Range::to_json() const {
    return {{"start", start.to_json()}, {"end", end.to_json()}};
}

Range Range::from_json(const nlohmann::json& j) {
    Range r;
    if (j.contains("start")) r.start = Position::from_json(j["start"]);
    if (j.contains("end"))   r.end   = Position::from_json(j["end"]);
    return r;
}

// ─── Location ─────────────────────────────────────────────────────────────────

nlohmann::json Location::to_json() const {
    return {{"uri", uri}, {"range", range.to_json()}};
}

Location Location::from_json(const nlohmann::json& j) {
    Location l;
    l.uri   = j.value("uri", std::string{});
    if (j.contains("range")) l.range = Range::from_json(j["range"]);
    return l;
}

// ─── Symbol ───────────────────────────────────────────────────────────────────

nlohmann::json Symbol::to_json() const {
    nlohmann::json j = {
        {"name", name},
        {"kind", static_cast<int>(kind)},
        {"location", location.to_json()},
    };
    if (container_name) j["containerName"] = *container_name;
    return j;
}

Symbol Symbol::from_json(const nlohmann::json& j) {
    Symbol s;
    s.name     = j.value("name", std::string{});
    s.kind     = static_cast<SymbolKind>(j.value("kind", 0));
    if (j.contains("location")) s.location = Location::from_json(j["location"]);
    if (j.contains("containerName") && !j["containerName"].is_null())
        s.container_name = j["containerName"].get<std::string>();
    return s;
}

// ─── DocumentSymbol ───────────────────────────────────────────────────────────

nlohmann::json DocumentSymbol::to_json() const {
    nlohmann::json j = {
        {"name",           name},
        {"kind",           static_cast<int>(kind)},
        {"range",          range.to_json()},
        {"selectionRange", selection_range.to_json()},
    };
    if (detail) j["detail"] = *detail;
    if (!children.empty()) {
        nlohmann::json kids = nlohmann::json::array();
        for (const auto& c : children) kids.push_back(c.to_json());
        j["children"] = std::move(kids);
    }
    return j;
}

DocumentSymbol DocumentSymbol::from_json(const nlohmann::json& j) {
    DocumentSymbol ds;
    ds.name = j.value("name", std::string{});
    ds.kind = static_cast<SymbolKind>(j.value("kind", 0));
    if (j.contains("detail") && !j["detail"].is_null())
        ds.detail = j["detail"].get<std::string>();
    if (j.contains("range"))          ds.range           = Range::from_json(j["range"]);
    if (j.contains("selectionRange")) ds.selection_range = Range::from_json(j["selectionRange"]);
    if (j.contains("children") && j["children"].is_array()) {
        for (const auto& child : j["children"]) {
            ds.children.push_back(DocumentSymbol::from_json(child));
        }
    }
    return ds;
}

// ─── Diagnostic ───────────────────────────────────────────────────────────────

nlohmann::json Diagnostic::to_json() const {
    nlohmann::json j = {
        {"range",    range.to_json()},
        {"severity", static_cast<int>(severity)},
        {"message",  message},
    };
    if (source) j["source"] = *source;
    if (code)   j["code"]   = *code;
    return j;
}

Diagnostic Diagnostic::from_json(const nlohmann::json& j) {
    Diagnostic d;
    if (j.contains("range")) d.range = Range::from_json(j["range"]);
    d.severity = static_cast<DiagnosticSeverity>(j.value("severity", 1));
    d.message  = j.value("message", std::string{});
    if (j.contains("source") && !j["source"].is_null())
        d.source = j["source"].get<std::string>();
    if (j.contains("code") && !j["code"].is_null()) {
        if (j["code"].is_string())
            d.code = j["code"].get<std::string>();
        else if (j["code"].is_number())
            d.code = std::to_string(j["code"].get<int>());
    }
    return d;
}

std::string pretty_diagnostic(const Diagnostic& d, const std::string& file) {
    static const char* severity_names[] = {"", "ERROR", "WARN", "INFO", "HINT"};
    const int sev = static_cast<int>(d.severity);
    const char* sev_str = (sev >= 1 && sev <= 4) ? severity_names[sev] : "UNKNOWN";

    std::ostringstream oss;
    if (!file.empty()) {
        oss << file << ":" << (d.range.start.line + 1) << ":" << (d.range.start.character + 1) << ": ";
    }
    oss << sev_str
        << " [" << (d.range.start.line + 1) << ":" << (d.range.start.character + 1) << "] "
        << d.message;
    return oss.str();
}

// ─── Hover ────────────────────────────────────────────────────────────────────

nlohmann::json Hover::to_json() const {
    nlohmann::json j = {{"contents", contents}};
    if (range) j["range"] = range->to_json();
    return j;
}

Hover Hover::from_json(const nlohmann::json& j) {
    Hover h;
    // Handle both string and MarkupContent object
    if (j.contains("contents")) {
        if (j["contents"].is_string()) {
            h.contents = j["contents"].get<std::string>();
        } else if (j["contents"].is_object() && j["contents"].contains("value")) {
            h.contents = j["contents"]["value"].get<std::string>();
        } else if (j["contents"].is_array() && !j["contents"].empty()) {
            // MarkedString[]
            const auto& first = j["contents"][0];
            h.contents = first.is_string() ? first.get<std::string>()
                       : first.value("value", std::string{});
        }
    }
    if (j.contains("range") && !j["range"].is_null())
        h.range = Range::from_json(j["range"]);
    return h;
}

// ─── Language ID mapping ──────────────────────────────────────────────────────

std::string language_id_for_extension(const std::string& ext) {
    // Aligned with OpenCode LANGUAGE_EXTENSIONS map (language.ts)
    static const std::unordered_map<std::string, std::string> MAP = {
        {".ts",   "typescript"},
        {".tsx",  "typescriptreact"},
        {".js",   "javascript"},
        {".jsx",  "javascriptreact"},
        {".mjs",  "javascript"},
        {".cjs",  "javascript"},
        {".py",   "python"},
        {".go",   "go"},
        {".rs",   "rust"},
        {".c",    "c"},
        {".h",    "c"},
        {".cpp",  "cpp"},
        {".cc",   "cpp"},
        {".cxx",  "cpp"},
        {".hpp",  "cpp"},
        {".hxx",  "cpp"},
        {".java", "java"},
        {".kt",   "kotlin"},
        {".rb",   "ruby"},
        {".php",  "php"},
        {".cs",   "csharp"},
        {".swift","swift"},
        {".sh",   "shellscript"},
        {".bash", "shellscript"},
        {".zsh",  "shellscript"},
        {".json", "json"},
        {".yaml", "yaml"},
        {".yml",  "yaml"},
        {".toml", "toml"},
        {".md",   "markdown"},
        {".html", "html"},
        {".css",  "css"},
        {".scss", "scss"},
        {".xml",  "xml"},
    };
    auto it = MAP.find(ext);
    return (it != MAP.end()) ? it->second : "plaintext";
}

// ─── URI helpers ──────────────────────────────────────────────────────────────

std::string path_to_uri(const std::string& path) {
    // Simple file:// URI encoding (percent-encode special chars)
    std::string uri = "file://";
    for (const char c : path) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '/' || c == '-' || c == '_' || c == '.' || c == '~') {
            uri += c;
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", static_cast<unsigned char>(c));
            uri += hex;
        }
    }
    return uri;
}

std::string uri_to_path(const std::string& uri) {
    // Strip "file://" prefix and percent-decode
    std::string path;
    const std::string prefix = "file://";
    const size_t start = uri.starts_with(prefix) ? prefix.size() : 0;

    for (size_t i = start; i < uri.size(); ++i) {
        if (uri[i] == '%' && i + 2 < uri.size()) {
            const int hi = uri[i+1];
            const int lo = uri[i+2];
            auto hex_val = [](int c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            const int h = hex_val(hi), l = hex_val(lo);
            if (h >= 0 && l >= 0) {
                path += static_cast<char>((h << 4) | l);
                i += 2;
                continue;
            }
        }
        path += uri[i];
    }
    return path;
}

}  // namespace turbot::core::lsp
