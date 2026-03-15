#include <turbot/core/tool/builtin/lsp_tool.hpp>
#include <turbot/core/lsp/manager.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/string_utils.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <cstdlib>

namespace turbot::core::tool::builtin {

// ============================================================================
// LSPOperation helpers
// ============================================================================

std::string lsp_operation_to_string(LSPOperation op) {
    switch (op) {
        case LSPOperation::GoToDefinition:    return "goToDefinition";
        case LSPOperation::FindReferences:    return "findReferences";
        case LSPOperation::Hover:             return "hover";
        case LSPOperation::DocumentSymbol:    return "documentSymbol";
        case LSPOperation::WorkspaceSymbol:   return "workspaceSymbol";
        case LSPOperation::GoToImplementation: return "goToImplementation";
    }
    return "unknown";
}

std::optional<LSPOperation> string_to_lsp_operation(const std::string& str) {
    if (str == "goToDefinition")    return LSPOperation::GoToDefinition;
    if (str == "findReferences")    return LSPOperation::FindReferences;
    if (str == "hover")             return LSPOperation::Hover;
    if (str == "documentSymbol")    return LSPOperation::DocumentSymbol;
    if (str == "workspaceSymbol")   return LSPOperation::WorkspaceSymbol;
    if (str == "goToImplementation") return LSPOperation::GoToImplementation;
    return std::nullopt;
}

// ============================================================================
// LSPToolParams
// ============================================================================

LSPToolParams LSPToolParams::from_json(const nlohmann::json& j) {
    LSPToolParams params;
    
    auto op_str = j.at("operation").get<std::string>();
    auto op = string_to_lsp_operation(op_str);
    if (!op) {
        throw std::invalid_argument(fmt::format("Invalid operation: {}", op_str));
    }
    params.operation = *op;
    
    params.file_path = j.at("filePath").get<std::string>();
    params.line = j.at("line").get<int>();
    params.character = j.at("character").get<int>();
    
    return params;
}

nlohmann::json LSPToolParams::to_json() const {
    return {
        {"operation", lsp_operation_to_string(operation)},
        {"filePath", file_path},
        {"line", line},
        {"character", character}
    };
}

// ============================================================================
// LSPTool
// ============================================================================

bool LSPTool::is_enabled() {
    // Check environment variable
    const char* env = std::getenv(ENV_ENABLE_VAR);
    return env != nullptr && (std::string(env) == "1" || std::string(env) == "true");
}

std::string LSPTool::description() const {
    return "Perform LSP operations like go to definition, find references, hover, etc. "
           "EXPERIMENTAL: Requires TURBOT_EXPERIMENTAL_LSP_TOOL=1 environment variable. "
           "Operations: goToDefinition, findReferences, hover, documentSymbol, workspaceSymbol, goToImplementation.";
}

nlohmann::json LSPTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"operation", {
                {"type", "string"},
                {"enum", nlohmann::json::array({
                    "goToDefinition", "findReferences", "hover",
                    "documentSymbol", "workspaceSymbol", "goToImplementation"
                })},
                {"description", "The LSP operation to perform"}
            }},
            {"filePath", {
                {"type", "string"},
                {"description", "The absolute or relative path to the file"}
            }},
            {"line", {
                {"type", "integer"},
                {"minimum", 1},
                {"description", "The line number (1-based, as shown in editors)"}
            }},
            {"character", {
                {"type", "integer"},
                {"minimum", 1},
                {"description", "The character offset (1-based, as shown in editors)"}
            }}
        }},
        {"required", nlohmann::json::array({"operation", "filePath", "line", "character"})}
    };
}

bool LSPTool::validate_input(const nlohmann::json& input) const {
    if (!input.contains("operation") || !input["operation"].is_string()) {
        return false;
    }
    if (!input.contains("filePath") || !input["filePath"].is_string()) {
        return false;
    }
    if (!input.contains("line") || !input["line"].is_number_integer()) {
        return false;
    }
    if (!input.contains("character") || !input["character"].is_number_integer()) {
        return false;
    }
    
    auto op = string_to_lsp_operation(input["operation"].get<std::string>());
    if (!op) return false;
    
    return input["line"].get<int>() >= 1 && input["character"].get<int>() >= 1;
}

ToolResult LSPTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    // Check if experimental feature is enabled
    if (!is_enabled()) {
        return ToolResult::error("lsp", 
            "LSP tool is experimental and disabled by default. "
            "Set TURBOT_EXPERIMENTAL_LSP_TOOL=1 to enable.");
    }
    
    if (!validate_input(input)) {
        return ToolResult::error("lsp", 
            "Invalid input. Required: operation (string), filePath (string), line (int >= 1), character (int >= 1).");
    }
    
    LSPToolParams params;
    try {
        params = LSPToolParams::from_json(input);
    } catch (const std::exception& e) {
        return ToolResult::error("lsp", fmt::format("Invalid parameters: {}", e.what()));
    }
    
    // Resolve file path
    std::filesystem::path file_path = params.file_path;
    if (!file_path.is_absolute()) {
        file_path = std::filesystem::path(ctx.working_directory) / file_path;
    }
    std::string file_path_str = file_path.string();
    
    // Check if file exists
    std::error_code ec;
    if (!std::filesystem::exists(file_path, ec)) {
        return ToolResult::error("lsp", fmt::format("File not found: {}", file_path_str));
    }
    
    // Get LSP manager
    auto& lsp_manager = lsp::LSPManager::instance();
    
    // Check if LSP is available for this file
    if (!lsp_manager.has_clients(file_path_str)) {
        return ToolResult::error("lsp", "No LSP server available for this file type.");
    }
    
    // Touch the file to ensure LSP knows about it
    lsp_manager.touch_file(file_path_str, false);
    
    // Convert to 0-based position
    lsp::Position pos;
    pos.line = params.line - 1;
    pos.character = params.character - 1;
    
    // Execute the operation
    std::string title = fmt::format("{} {}:{}:{}", 
        lsp_operation_to_string(params.operation),
        file_path.filename().string(),
        params.line,
        params.character);
    
    nlohmann::json result_json;
    
    try {
        switch (params.operation) {
            case LSPOperation::GoToDefinition: {
                auto future = lsp_manager.definition(file_path_str, pos);
                auto locations = future.get();
                nlohmann::json arr = nlohmann::json::array();
                for (const auto& loc : locations) {
                    arr.push_back({
                        {"uri", loc.uri},
                        {"range", {
                            {"start", {{"line", loc.range.start.line + 1}, {"character", loc.range.start.character + 1}}},
                            {"end", {{"line", loc.range.end.line + 1}, {"character", loc.range.end.character + 1}}}
                        }}
                    });
                }
                result_json = arr;
                break;
            }
            
            case LSPOperation::FindReferences: {
                auto future = lsp_manager.references(file_path_str, pos);
                auto locations = future.get();
                nlohmann::json arr = nlohmann::json::array();
                for (const auto& loc : locations) {
                    arr.push_back({
                        {"uri", loc.uri},
                        {"range", {
                            {"start", {{"line", loc.range.start.line + 1}, {"character", loc.range.start.character + 1}}},
                            {"end", {{"line", loc.range.end.line + 1}, {"character", loc.range.end.character + 1}}}
                        }}
                    });
                }
                result_json = arr;
                break;
            }
            
            case LSPOperation::Hover: {
                auto future = lsp_manager.hover(file_path_str, pos);
                auto hover = future.get();
                if (hover) {
                    result_json = {
                        {"contents", hover->contents},
                        {"range", hover->range.has_value() ? nlohmann::json{
                            {"start", {{"line", hover->range->start.line + 1}, {"character", hover->range->start.character + 1}}},
                            {"end", {{"line", hover->range->end.line + 1}, {"character", hover->range->end.character + 1}}}
                        } : nlohmann::json(nullptr)}
                    };
                } else {
                    result_json = nullptr;
                }
                break;
            }
            
            case LSPOperation::DocumentSymbol: {
                auto future = lsp_manager.document_symbol(file_path_str);
                auto symbols = future.get();
                nlohmann::json arr = nlohmann::json::array();
                for (const auto& sym : symbols) {
                    arr.push_back(sym);
                }
                result_json = arr;
                break;
            }
            
            case LSPOperation::WorkspaceSymbol: {
                auto future = lsp_manager.workspace_symbol("");
                auto symbols = future.get();
                nlohmann::json arr = nlohmann::json::array();
                for (const auto& sym : symbols) {
                    arr.push_back({
                        {"name", sym.name},
                        {"kind", static_cast<int>(sym.kind)},
                        {"uri", sym.location.uri},
                        {"range", {
                            {"start", {{"line", sym.location.range.start.line + 1}, {"character", sym.location.range.start.character + 1}}},
                            {"end", {{"line", sym.location.range.end.line + 1}, {"character", sym.location.range.end.character + 1}}}
                        }}
                    });
                }
                result_json = arr;
                break;
            }
            
            case LSPOperation::GoToImplementation: {
                auto future = lsp_manager.implementation(file_path_str, pos);
                auto locations = future.get();
                nlohmann::json arr = nlohmann::json::array();
                for (const auto& loc : locations) {
                    arr.push_back({
                        {"uri", loc.uri},
                        {"range", {
                            {"start", {{"line", loc.range.start.line + 1}, {"character", loc.range.start.character + 1}}},
                            {"end", {{"line", loc.range.end.line + 1}, {"character", loc.range.end.character + 1}}}
                        }}
                    });
                }
                result_json = arr;
                break;
            }
        }
    } catch (const std::exception& e) {
        return ToolResult::error("lsp", fmt::format("LSP operation failed: {}", e.what()));
    }
    
    // Format output
    std::string output;
    if (result_json.is_null() || (result_json.is_array() && result_json.empty())) {
        output = fmt::format("No results found for {}", lsp_operation_to_string(params.operation));
    } else {
        output = result_json.dump(2);
    }
    
    nlohmann::json metadata = {
        {"operation", lsp_operation_to_string(params.operation)},
        {"filePath", file_path_str},
        {"line", params.line},
        {"character", params.character},
        {"result", result_json}
    };
    
    return ToolResult::success(title, output, metadata);
}

} // namespace turbot::core::tool::builtin
