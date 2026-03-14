#pragma once

#include <turbot/core/common/export.hpp>

namespace turbot::core::lsp {

/// Register all LSP tools into ToolRegistry.
/// Call after LSPManager::initialize().
///
/// Registers:
///   - lsp_diagnostics:      Get file diagnostics (formatted)
///   - lsp_hover:            Hover info at position
///   - lsp_definition:       Jump to definition
///   - lsp_references:       Find all references
///   - lsp_workspace_symbol: Workspace symbol search (filtered + top 10)
TURBOT_CORE_API void register_lsp_tools();

}  // namespace turbot::core::lsp
