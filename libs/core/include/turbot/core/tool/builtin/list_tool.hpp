#pragma once

#include <turbot/core/tool/tool.hpp>
#include <string>
#include <vector>

namespace turbot::core::tool::builtin {

/// Parameters for ListTool execution
struct ListToolParams {
    std::optional<std::string> path;    ///< Directory to list (optional)
    std::vector<std::string> ignore;    ///< Patterns to ignore

    /// Parse from JSON
    static ListToolParams from_json(const nlohmann::json& j);

    /// Convert to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Tool for listing directory contents
class TURBOT_CORE_API ListTool : public Tool {
public:
    ListTool() = default;

    [[nodiscard]] std::string name() const override { return "list"; }

    [[nodiscard]] std::string description() const override;

    [[nodiscard]] nlohmann::json input_schema() const override;

    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;

    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;

private:
    /// Get default ignore patterns
    [[nodiscard]] static std::vector<std::string> default_ignore_patterns();
    
    /// Check if path should be ignored
    [[nodiscard]] static bool should_ignore(
        const std::string& name, 
        const std::vector<std::string>& ignore_patterns
    );
    
    /// List directory contents recursively
    [[nodiscard]] static std::vector<std::string> list_directory(
        const std::string& directory,
        const std::vector<std::string>& ignore_patterns,
        size_t limit,
        std::shared_ptr<std::atomic<bool>> abort_flag
    );
};

} // namespace turbot::core::tool::builtin
