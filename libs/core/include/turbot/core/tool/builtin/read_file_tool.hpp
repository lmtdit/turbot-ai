#pragma once

#include <turbot/core/tool/tool.hpp>
#include <string>

namespace turbot::core::tool::builtin {

/// Tool for reading file contents
class TURBOT_CORE_API ReadFileTool : public Tool {
public:
    ReadFileTool() = default;

    [[nodiscard]] std::string name() const override { return "read_file"; }

    [[nodiscard]] std::string description() const override {
        return "Read the contents of a file at the given path. "
               "Returns the file contents as text.";
    }

    [[nodiscard]] nlohmann::json input_schema() const override;

    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;

    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;
};

} // namespace turbot::core::tool::builtin
