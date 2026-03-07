#pragma once

#include <turbot/core/tool/tool.hpp>
#include <optional>
#include <string>

namespace turbot::core::tool::builtin {

/// Parameters for WriteFileTool execution
struct WriteFileToolParams {
    std::string file_path;      ///< Absolute path to the file to write
    std::string content;        ///< Content to write to the file

    /// Parse from JSON
    static WriteFileToolParams from_json(const nlohmann::json& j);

    /// Convert to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Tool for writing content to files with diff preview
class TURBOT_CORE_API WriteFileTool : public Tool {
public:
    WriteFileTool() = default;

    [[nodiscard]] std::string name() const override { return "write"; }

    [[nodiscard]] std::string description() const override;

    [[nodiscard]] nlohmann::json input_schema() const override;

    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;

    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;

private:
    /// Create a simple diff showing changes
    [[nodiscard]] static std::string create_diff(
        const std::string& file_path,
        const std::string& old_content,
        const std::string& new_content
    );
};

} // namespace turbot::core::tool::builtin
