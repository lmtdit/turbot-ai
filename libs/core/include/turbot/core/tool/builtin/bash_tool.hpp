#pragma once

#include <turbot/core/tool/tool.hpp>
#include <string>

namespace turbot::core::tool::builtin {

/// Parameters for BashTool execution
struct BashToolParams {
    std::string command;
    std::string description;    ///< Human-readable description (5-10 words)
    std::optional<int> timeout; ///< Timeout in milliseconds
    std::optional<std::string> workdir; ///< Working directory

    /// Parse from JSON
    static BashToolParams from_json(const nlohmann::json& j);
    
    /// Convert to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Tool for executing shell commands with sandbox support
class TURBOT_CORE_API BashTool : public Tool {
public:
    BashTool() = default;

    [[nodiscard]] std::string name() const override { return "bash"; }

    [[nodiscard]] std::string description() const override;

    [[nodiscard]] nlohmann::json input_schema() const override;

    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;

    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;

private:
    /// Execute command in normal mode
    [[nodiscard]] ToolResult execute_normal(
        const BashToolParams& params,
        ToolContext& ctx,
        int timeout_ms
    );

    /// Execute command in sandbox mode
    [[nodiscard]] ToolResult execute_sandbox(
        const BashToolParams& params,
        ToolContext& ctx,
        int timeout_ms
    );

    /// Ask user for shell mode and save to config
    [[nodiscard]] ShellMode ask_shell_mode(ToolContext& ctx);
};

} // namespace turbot::core::tool::builtin
