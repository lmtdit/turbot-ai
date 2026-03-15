#pragma once

#include <turbot/core/tool/tool.hpp>
#include <string>
#include <vector>
#include <optional>

namespace turbot::core::tool {

/// Configuration for a custom tool loaded from file
struct TURBOT_CORE_API CustomToolConfig {
    std::string name;                    ///< Tool identifier
    std::string description;             ///< Tool description for AI
    nlohmann::json input_schema;         ///< JSON Schema for parameters
    std::string command;                 ///< Command to execute
    std::vector<std::string> args;       ///< Command arguments (can include ${param} placeholders)
    std::optional<int> timeout_seconds;  ///< Optional timeout
    std::optional<std::string> working_dir; ///< Optional working directory
    bool enabled = true;                 ///< Whether this tool is enabled
    std::string source_path;             ///< Path to the config file (for error messages)

    /// Parse from JSON file
    [[nodiscard]] static std::optional<CustomToolConfig> from_file(const std::string& path);
    
    /// Parse from JSON object
    [[nodiscard]] static std::optional<CustomToolConfig> from_json(const nlohmann::json& j, const std::string& source_path = "");
    
    /// Convert to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Tool that executes an external command
/// Used for custom tools loaded from configuration files
class TURBOT_CORE_API ExternalCommandTool : public Tool {
public:
    /// Create from configuration
    explicit ExternalCommandTool(const CustomToolConfig& config);
    
    /// Get the tool name
    [[nodiscard]] std::string name() const override;
    
    /// Get the tool description
    [[nodiscard]] std::string description() const override;
    
    /// Get the JSON Schema for input parameters
    [[nodiscard]] nlohmann::json input_schema() const override;
    
    /// Execute the tool by running the external command
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;
    
    /// Get the source configuration
    [[nodiscard]] const CustomToolConfig& config() const noexcept { return config_; }

private:
    /// Substitute ${param} placeholders in arguments with actual values
    [[nodiscard]] std::vector<std::string> substitute_args(const nlohmann::json& input) const;
    
    /// Execute command and capture output
    [[nodiscard]] ToolResult run_command(
        const std::vector<std::string>& args,
        const std::string& working_dir,
        int timeout_seconds,
        ToolContext& ctx
    );

    CustomToolConfig config_;
};

} // namespace turbot::core::tool
