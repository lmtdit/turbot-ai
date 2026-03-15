#pragma once

#include <turbot/core/tool/tool.hpp>
#include <string>
#include <optional>

namespace turbot::core::tool::builtin {

/// LSP operation types
enum class LSPOperation {
    GoToDefinition,     ///< Jump to definition
    FindReferences,     ///< Find all references
    Hover,              ///< Get hover information
    DocumentSymbol,     ///< Get document symbols
    WorkspaceSymbol,    ///< Search workspace symbols
    GoToImplementation, ///< Jump to implementation
};

/// Convert LSPOperation to string
[[nodiscard]] std::string lsp_operation_to_string(LSPOperation op);

/// Convert string to LSPOperation
[[nodiscard]] std::optional<LSPOperation> string_to_lsp_operation(const std::string& str);

/// Parameters for LSPTool execution
struct LSPToolParams {
    LSPOperation operation;     ///< The LSP operation to perform
    std::string file_path;      ///< The absolute or relative path to the file
    int line;                   ///< The line number (1-based, as shown in editors)
    int character;              ///< The character offset (1-based, as shown in editors)

    /// Parse from JSON
    static LSPToolParams from_json(const nlohmann::json& j);

    /// Convert to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Tool for performing LSP operations (experimental)
///
/// This tool provides direct access to LSP server functionality.
/// It requires the TURBOT_EXPERIMENTAL_LSP_TOOL environment variable to be set.
class TURBOT_CORE_API LSPTool : public Tool {
public:
    LSPTool() = default;

    [[nodiscard]] std::string name() const override { return "lsp"; }

    [[nodiscard]] std::string description() const override;

    [[nodiscard]] nlohmann::json input_schema() const override;

    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;

    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;

    /// Check if LSP tool is enabled (via environment variable)
    [[nodiscard]] static bool is_enabled();

private:
    /// Environment variable name to enable this tool
    static constexpr const char* ENV_ENABLE_VAR = "TURBOT_EXPERIMENTAL_LSP_TOOL";
};

} // namespace turbot::core::tool::builtin
