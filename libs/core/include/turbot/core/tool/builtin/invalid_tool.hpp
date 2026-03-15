#pragma once

#include <turbot/core/tool/tool.hpp>
#include <string>

namespace turbot::core::tool::builtin {

/// Parameters for InvalidTool execution
struct InvalidToolParams {
    std::string tool;   ///< The invalid tool name that was attempted
    std::string error;  ///< The error message describing the issue

    /// Parse from JSON
    static InvalidToolParams from_json(const nlohmann::json& j);

    /// Convert to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Tool for handling invalid tool calls (fallback/repair target)
/// 
/// This tool serves as a fallback when a tool call cannot be properly
/// parsed or executed. It's used by the repairToolCall mechanism.
class TURBOT_CORE_API InvalidTool : public Tool {
public:
    InvalidTool() = default;

    [[nodiscard]] std::string name() const override { return "invalid"; }

    [[nodiscard]] std::string description() const override;

    [[nodiscard]] nlohmann::json input_schema() const override;

    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;

    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;
};

} // namespace turbot::core::tool::builtin
