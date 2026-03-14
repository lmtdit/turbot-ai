#pragma once

#include <turbot/core/tool/tool.hpp>
#include <string>

namespace turbot::core::tool::builtin {

/// Tool for fetching web content and returning it as Markdown (aligned with OpenCode webfetch)
class TURBOT_CORE_API WebFetchTool : public Tool {
public:
    WebFetchTool() = default;

    [[nodiscard]] std::string name() const override { return "webfetch"; }
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] nlohmann::json input_schema() const override;
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;
    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;
};

} // namespace turbot::core::tool::builtin
