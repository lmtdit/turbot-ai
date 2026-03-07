#pragma once

#include <turbot/core/tool/tool.hpp>
#include <string>

namespace turbot::core::tool::builtin {

/// Tool for executing shell commands
class TURBOT_CORE_API BashTool : public Tool {
public:
    BashTool() = default;

    [[nodiscard]] std::string name() const override { return "bash"; }

    [[nodiscard]] std::string description() const override {
        return "Execute a bash command and return the output. "
               "Use for running scripts, system commands, and other shell operations. "
               "Returns stdout and stderr combined. "
               "Commands run in the current working directory.";
    }

    [[nodiscard]] nlohmann::json input_schema() const override;

    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;

    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;
};

} // namespace turbot::core::tool::builtin
