#pragma once

#include <turbot/core/tool/tool.hpp>
#include <string>
#include <vector>

namespace turbot::core::tool::builtin {

/// Parameters for GrepTool execution
struct GrepToolParams {
    std::string pattern;            ///< Regex pattern to search for
    std::optional<std::string> path;///< Directory to search in (optional)
    std::optional<std::string> include; ///< File pattern to include (e.g., "*.js")

    /// Parse from JSON
    static GrepToolParams from_json(const nlohmann::json& j);

    /// Convert to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Grep match result
struct GrepMatch {
    std::string path;       ///< File path
    int64_t mod_time = 0;   ///< File modification time
    int line_num = 0;       ///< Line number
    std::string line_text;  ///< Line content

    [[nodiscard]] nlohmann::json to_json() const;
};

/// Tool for searching file contents using regex patterns
class TURBOT_CORE_API GrepTool : public Tool {
public:
    GrepTool() = default;

    [[nodiscard]] std::string name() const override { return "grep"; }

    [[nodiscard]] std::string description() const override;

    [[nodiscard]] nlohmann::json input_schema() const override;

    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;

    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;

private:
    /// Search for pattern in files
    [[nodiscard]] static std::vector<GrepMatch> search_files(
        const std::string& directory,
        const std::string& pattern,
        const std::optional<std::string>& include_pattern,
        size_t limit,
        std::shared_ptr<std::atomic<bool>> abort_flag
    );

    /// Check if file matches include pattern
    [[nodiscard]] static bool matches_include(const std::string& filename, const std::optional<std::string>& pattern);
};

} // namespace turbot::core::tool::builtin
