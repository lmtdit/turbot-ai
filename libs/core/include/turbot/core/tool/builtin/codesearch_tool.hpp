#pragma once

#include <turbot/core/tool/tool.hpp>
#include <string>

namespace turbot::core::tool::builtin {

/// Tool for semantic code search using grep-based pattern matching
/// (aligned with OpenCode codesearch — lightweight implementation)
class TURBOT_CORE_API CodeSearchTool : public Tool {
public:
    CodeSearchTool() = default;

    [[nodiscard]] std::string name() const override { return "codesearch"; }
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] nlohmann::json input_schema() const override;
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;
    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;
};

} // namespace turbot::core::tool::builtin
