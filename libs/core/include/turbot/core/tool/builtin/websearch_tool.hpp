#pragma once

#include <turbot/core/tool/tool.hpp>
#include <string>

namespace turbot::core::tool::builtin {

/// Tool for web search using Exa MCP API (aligned with OpenCode websearch)
/// 
/// This tool provides web search capabilities by integrating with the Exa search API.
/// It supports configurable search parameters like numResults, livecrawl mode, and search type.
class TURBOT_CORE_API WebSearchTool : public Tool {
public:
    WebSearchTool() = default;

    [[nodiscard]] std::string name() const override { return "websearch"; }
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] nlohmann::json input_schema() const override;
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;
    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;

private:
    /// Build the MCP search request JSON
    [[nodiscard]] nlohmann::json build_mcp_request(const nlohmann::json& params) const;
    
    /// Parse the SSE response from Exa API
    [[nodiscard]] std::string parse_sse_response(const std::string& response) const;
};

} // namespace turbot::core::tool::builtin
