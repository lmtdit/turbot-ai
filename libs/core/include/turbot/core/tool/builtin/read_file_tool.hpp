#pragma once

#include <turbot/core/tool/tool.hpp>
#include <optional>
#include <string>

namespace turbot::core::tool::builtin {

/// Parameters for ReadFileTool execution
struct ReadFileToolParams {
    std::string file_path;              ///< Absolute path to the file or directory to read
    std::optional<int> offset;           ///< Line number to start reading from (1-indexed)
    std::optional<int> limit;            ///< Maximum number of lines to read

    /// Parse from JSON
    static ReadFileToolParams from_json(const nlohmann::json& j);

    /// Convert to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Tool for reading file contents with pagination support
class TURBOT_CORE_API ReadFileTool : public Tool {
public:
    ReadFileTool() = default;

    [[nodiscard]] std::string name() const override { return "read"; }

    [[nodiscard]] std::string description() const override;

    [[nodiscard]] nlohmann::json input_schema() const override;

    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;

    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;

private:
    /// Check if file is binary
    [[nodiscard]] static bool is_binary_file(const std::string& path, size_t file_size);

    /// Read directory contents
    [[nodiscard]] static ToolResult read_directory(
        const std::string& path,
        int offset,
        int limit,
        ToolContext& ctx
    );

    /// Read file contents with pagination
    [[nodiscard]] static ToolResult read_file(
        const std::string& path,
        int offset,
        int limit,
        ToolContext& ctx
    );
};

} // namespace turbot::core::tool::builtin
