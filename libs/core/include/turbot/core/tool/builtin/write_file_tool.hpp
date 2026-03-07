#pragma once

#include <turbot/core/tool/tool.hpp>
#include <string>

namespace turbot::core::tool::builtin {

/// Tool for writing content to a file
class TURBOT_CORE_API WriteFileTool : public Tool {
public:
    WriteFileTool() = default;

    [[nodiscard]] std::string name() const override { return "write_file"; }

    [[nodiscard]] std::string description() const override {
        return "Write content to a file at the given path. "
               "Creates the file if it does not exist, overwrites it if it does. "
               "Creates parent directories as needed.";
    }

    [[nodiscard]] nlohmann::json input_schema() const override;

    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;

    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;
};

} // namespace turbot::core::tool::builtin
