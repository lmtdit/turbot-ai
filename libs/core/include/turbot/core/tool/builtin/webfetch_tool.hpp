#pragma once

#include <turbot/core/tool/tool.hpp>
#include <turbot/network/ihttp_client.hpp>
#include <string>
#include <memory>

namespace turbot::core::tool::builtin {

/// Tool for fetching web content and returning it as Markdown (aligned with OpenCode webfetch)
class TURBOT_CORE_API WebFetchTool : public Tool {
public:
    WebFetchTool() = default;
    
    /// Constructor with HTTP client injection (for testing)
    explicit WebFetchTool(std::shared_ptr<turbot::network::IHttpClient> http_client)
        : http_client_(std::move(http_client)) {}

    [[nodiscard]] std::string name() const override { return "webfetch"; }
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] nlohmann::json input_schema() const override;
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;
    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;

private:
    /// Optional HTTP client (for testing)
    std::shared_ptr<turbot::network::IHttpClient> http_client_;
};

} // namespace turbot::core::tool::builtin
