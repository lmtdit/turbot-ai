#include <turbot/core/lsp/tools.hpp>
#include <turbot/core/lsp/manager.hpp>
#include <turbot/core/lsp/lsp.hpp>
#include <turbot/core/tool/tool.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/common/logger.hpp>

#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace turbot::core::lsp {

using namespace turbot::core::tool;

// ─── SymbolKind filter (aligned with OpenCode LSP.kinds) ─────────────────────

static const std::vector<int> kFilteredKinds = {
    5,   // Class
    6,   // Method
    10,  // Enum
    11,  // Interface
    12,  // Function
    13,  // Variable
    14,  // Constant
    23,  // Struct
};

static bool is_filtered_kind(int kind) {
    for (const int k : kFilteredKinds) {
        if (k == kind) return true;
    }
    return false;
}

static std::string kind_name(int kind) {
    switch (kind) {
        case 1:  return "File";
        case 2:  return "Module";
        case 3:  return "Namespace";
        case 4:  return "Package";
        case 5:  return "Class";
        case 6:  return "Method";
        case 7:  return "Property";
        case 8:  return "Field";
        case 9:  return "Constructor";
        case 10: return "Enum";
        case 11: return "Interface";
        case 12: return "Function";
        case 13: return "Variable";
        case 14: return "Constant";
        case 23: return "Struct";
        default: return "Symbol";
    }
}

// ─── lsp_diagnostics ─────────────────────────────────────────────────────────

class LspDiagnosticsTool final : public Tool {
public:
    [[nodiscard]] std::string name() const override { return "lsp_diagnostics"; }
    [[nodiscard]] std::string description() const override {
        return "Get LSP diagnostics (errors, warnings) for a source file. "
               "Triggers the LSP server to analyze the file and returns all diagnostics.";
    }
    [[nodiscard]] nlohmann::json input_schema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"file", {{"type", "string"}, {"description", "Absolute path to the source file"}}},
            }},
            {"required", nlohmann::json::array({"file"})},
            {"additionalProperties", false},
        };
    }
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& /*ctx*/) override {
        const std::string file = input.value("file", std::string{});
        if (file.empty()) {
            return ToolResult::error(name(), "Missing required parameter: file");
        }
        try {
            // touch_file with wait_for_diagnostics=true
            LSPManager::instance().touch_file(file, true);
            const auto diags = LSPManager::instance().diagnostics();
            auto it = diags.find(file);
            if (it == diags.end() || it->second.empty()) {
                return ToolResult::success(name(), "No diagnostics found.");
            }
            std::ostringstream oss;
            for (const auto& d : it->second) {
                oss << pretty_diagnostic(d, file) << "\n";
            }
            return ToolResult::success(name(), oss.str());
        } catch (const std::exception& ex) {
            return ToolResult::error(name(), std::string("LSP diagnostics failed: ") + ex.what());
        }
    }
};

// ─── lsp_hover ────────────────────────────────────────────────────────────────

class LspHoverTool final : public Tool {
public:
    [[nodiscard]] std::string name() const override { return "lsp_hover"; }
    [[nodiscard]] std::string description() const override {
        return "Get hover information (type, documentation) for a symbol at a specific position in a source file.";
    }
    [[nodiscard]] nlohmann::json input_schema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"file",      {{"type", "string"}, {"description", "Absolute path to the source file"}}},
                {"line",      {{"type", "integer"}, {"description", "Line number (0-indexed)"}}},
                {"character", {{"type", "integer"}, {"description", "Character offset (0-indexed)"}}},
            }},
            {"required", nlohmann::json::array({"file", "line", "character"})},
            {"additionalProperties", false},
        };
    }
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& /*ctx*/) override {
        const std::string file = input.value("file", std::string{});
        const int line      = input.value("line", 0);
        const int character = input.value("character", 0);
        if (file.empty()) return ToolResult::error(name(), "Missing required parameter: file");
        try {
            LSPManager::instance().touch_file(file);
            Position pos{line, character};
            const auto result = LSPManager::instance().hover(file, pos).get();
            if (!result || result->contents.empty()) {
                return ToolResult::success(name(), "No hover information available.");
            }
            return ToolResult::success(name(), result->contents);
        } catch (const std::exception& ex) {
            return ToolResult::error(name(), std::string("LSP hover failed: ") + ex.what());
        }
    }
};

// ─── lsp_definition ───────────────────────────────────────────────────────────

class LspDefinitionTool final : public Tool {
public:
    [[nodiscard]] std::string name() const override { return "lsp_definition"; }
    [[nodiscard]] std::string description() const override {
        return "Jump to the definition of a symbol at a specific position in a source file. "
               "Returns file path, line, and column of the definition.";
    }
    [[nodiscard]] nlohmann::json input_schema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"file",      {{"type", "string"}}},
                {"line",      {{"type", "integer"}}},
                {"character", {{"type", "integer"}}},
            }},
            {"required", nlohmann::json::array({"file", "line", "character"})},
            {"additionalProperties", false},
        };
    }
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& /*ctx*/) override {
        const std::string file = input.value("file", std::string{});
        const int line      = input.value("line", 0);
        const int character = input.value("character", 0);
        if (file.empty()) return ToolResult::error(name(), "Missing required parameter: file");
        try {
            LSPManager::instance().touch_file(file);
            Position pos{line, character};
            const auto locs = LSPManager::instance().definition(file, pos).get();
            if (locs.empty()) return ToolResult::success(name(), "No definition found.");
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& loc : locs) {
                const std::string path = uri_to_path(loc.uri);
                arr.push_back({
                    {"file", path},
                    {"line", loc.range.start.line + 1},
                    {"character", loc.range.start.character + 1},
                });
            }
            return ToolResult::success(name(), arr.dump(2));
        } catch (const std::exception& ex) {
            return ToolResult::error(name(), std::string("LSP definition failed: ") + ex.what());
        }
    }
};

// ─── lsp_references ───────────────────────────────────────────────────────────

class LspReferencesTool final : public Tool {
public:
    [[nodiscard]] std::string name() const override { return "lsp_references"; }
    [[nodiscard]] std::string description() const override {
        return "Find all references to a symbol at a specific position in a source file. "
               "Includes the declaration. Returns list of file paths with line and column.";
    }
    [[nodiscard]] nlohmann::json input_schema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"file",      {{"type", "string"}}},
                {"line",      {{"type", "integer"}}},
                {"character", {{"type", "integer"}}},
            }},
            {"required", nlohmann::json::array({"file", "line", "character"})},
            {"additionalProperties", false},
        };
    }
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& /*ctx*/) override {
        const std::string file = input.value("file", std::string{});
        const int line      = input.value("line", 0);
        const int character = input.value("character", 0);
        if (file.empty()) return ToolResult::error(name(), "Missing required parameter: file");
        try {
            LSPManager::instance().touch_file(file);
            Position pos{line, character};
            const auto locs = LSPManager::instance().references(file, pos).get();
            if (locs.empty()) return ToolResult::success(name(), "No references found.");
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& loc : locs) {
                const std::string path = uri_to_path(loc.uri);
                arr.push_back({
                    {"file", path},
                    {"line", loc.range.start.line + 1},
                    {"character", loc.range.start.character + 1},
                });
            }
            return ToolResult::success(name(), arr.dump(2));
        } catch (const std::exception& ex) {
            return ToolResult::error(name(), std::string("LSP references failed: ") + ex.what());
        }
    }
};

// ─── lsp_workspace_symbol ─────────────────────────────────────────────────────

class LspWorkspaceSymbolTool final : public Tool {
public:
    [[nodiscard]] std::string name() const override { return "lsp_workspace_symbol"; }
    [[nodiscard]] std::string description() const override {
        return "Search for symbols (classes, functions, methods, interfaces, etc.) across the workspace. "
               "Returns up to 10 results filtered to meaningful symbol types. "
               "Symbols include: Class, Method, Enum, Interface, Function, Variable, Constant, Struct.";
    }
    [[nodiscard]] nlohmann::json input_schema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"query", {{"type", "string"}, {"description", "Search query for symbol names"}}},
            }},
            {"required", nlohmann::json::array({"query"})},
            {"additionalProperties", false},
        };
    }
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& /*ctx*/) override {
        const std::string query = input.value("query", std::string{});
        if (query.empty()) return ToolResult::error(name(), "Missing required parameter: query");
        try {
            auto symbols = LSPManager::instance().workspace_symbol(query).get();

            // Filter by kind (aligned with OpenCode LSP.kinds)
            std::vector<Symbol> filtered;
            for (const auto& s : symbols) {
                if (is_filtered_kind(static_cast<int>(s.kind))) {
                    filtered.push_back(s);
                }
            }
            // Take top 10
            if (filtered.size() > 10) filtered.resize(10);

            if (filtered.empty()) return ToolResult::success(name(), "No symbols found.");

            nlohmann::json arr = nlohmann::json::array();
            for (const auto& s : filtered) {
                const std::string path = uri_to_path(s.location.uri);
                arr.push_back({
                    {"name",      s.name},
                    {"kind",      kind_name(static_cast<int>(s.kind))},
                    {"file",      path},
                    {"line",      s.location.range.start.line + 1},
                    {"character", s.location.range.start.character + 1},
                });
            }
            return ToolResult::success(name(), arr.dump(2));
        } catch (const std::exception& ex) {
            return ToolResult::error(name(), std::string("LSP workspace_symbol failed: ") + ex.what());
        }
    }
};

// ─── register_lsp_tools ───────────────────────────────────────────────────────

void register_lsp_tools() {
    auto& registry = ToolRegistry::instance();
    registry.register_tool(std::make_unique<LspDiagnosticsTool>());
    registry.register_tool(std::make_unique<LspHoverTool>());
    registry.register_tool(std::make_unique<LspDefinitionTool>());
    registry.register_tool(std::make_unique<LspReferencesTool>());
    registry.register_tool(std::make_unique<LspWorkspaceSymbolTool>());
    TURBOT_LOG_INFO("LSP: registered 5 tools (diagnostics/hover/definition/references/workspace_symbol)");
}

}  // namespace turbot::core::lsp
