#pragma once

#include <turbot/core/tool/tool.hpp>
#include <turbot/network/ihttp_client.hpp>
#include <string>
#include <memory>

namespace turbot::core::tool::builtin {

/// Tool for web search using Exa MCP API (aligned with OpenCode websearch)
/// 
/// This tool provides web search capabilities by integrating with the Exa search API.
/// It supports configurable search parameters like numResults, livecrawl mode, and search type.
class TURBOT_CORE_API WebSearchTool : public Tool {
public:
    WebSearchTool() = default;
    
    /// Constructor with HTTP client injection (for testing)
    explicit WebSearchTool(std::shared_ptr<turbot::network::IHttpClient> http_client)
        : http_client_(std::move(http_client)) {}

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
    
    /// Optional HTTP client (for testing)
    std::shared_ptr<turbot::network::IHttpClient> http_client_;
};

} // namespace turbot::core::tool::builtin
