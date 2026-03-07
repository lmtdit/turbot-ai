#pragma once

#include <turbot/core/tool/tool.hpp>
#include <string>
#include <vector>

namespace turbot::core::tool::builtin {

/// Parameters for GlobTool execution
struct GlobToolParams {
    std::string pattern;            ///< Glob pattern to match files against
    std::optional<std::string> path;///< Directory to search in (optional)

    /// Parse from JSON
    static GlobToolParams from_json(const nlohmann::json& j);

    /// Convert to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// File entry result
struct GlobFileEntry {
    std::string path;       ///< Full file path
    int64_t mtime = 0;      ///< Modification time (Unix timestamp)

    [[nodiscard]] nlohmann::json to_json() const;
};

/// Tool for finding files using glob patterns
class TURBOT_CORE_API GlobTool : public Tool {
public:
    GlobTool() = default;

    [[nodiscard]] std::string name() const override { return "glob"; }

    [[nodiscard]] std::string description() const override;

    [[nodiscard]] nlohmann::json input_schema() const override;

    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;

    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;

private:
    /// Check if a path matches a glob pattern
    [[nodiscard]] static bool matches_glob(std::string_view path, std::string_view pattern);

    /// Scan directory for matching files
    [[nodiscard]] static std::vector<GlobFileEntry> scan_directory(
        const std::string& directory,
        const std::string& pattern,
        size_t limit,
        std::shared_ptr<std::atomic<bool>> abort_flag
    );

    /// Convert glob pattern to regex pattern (basic implementation)
    [[nodiscard]] static std::string glob_to_regex(std::string_view glob);
};

} // namespace turbot::core::tool::builtin
