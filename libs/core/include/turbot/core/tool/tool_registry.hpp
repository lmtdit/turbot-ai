#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/tool/tool.hpp>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace turbot::core::tool {

/// Registry for all available tools
/// Thread-safe singleton that manages tool registration and lookup
class TURBOT_CORE_API ToolRegistry {
public:
    /// Get the singleton instance
    static ToolRegistry& instance();

    // Non-copyable, non-movable (singleton)
    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;
    ToolRegistry(ToolRegistry&&) = delete;
    ToolRegistry& operator=(ToolRegistry&&) = delete;

    /// Register a tool (replaces any existing tool with the same name)
    void register_tool(std::unique_ptr<Tool> tool);

    /// Get a tool by name
    /// @return shared_ptr to the tool, or nullptr if not found
    [[nodiscard]] ToolPtr get(const std::string& name) const;

    /// Check if a tool is registered
    [[nodiscard]] bool has(const std::string& name) const;

    /// Get all registered tools
    [[nodiscard]] std::vector<ToolPtr> list() const;

    /// Get all tool names
    [[nodiscard]] std::vector<std::string> names() const;

    /// Get tools filtered by a list of allowed names
    /// @param allowed_names Names of tools to include (empty = all tools)
    [[nodiscard]] std::vector<ToolPtr> filter(const std::vector<std::string>& allowed_names) const;

    /// Remove a tool by name
    /// @return true if the tool was found and removed
    bool remove(const std::string& name);

    /// Remove all registered tools
    void clear();

    /// Get the number of registered tools
    [[nodiscard]] size_t size() const;

    /// Convert all tools to JSON tool definitions (for AI provider)
    [[nodiscard]] nlohmann::json to_tool_definitions() const;

    /// Register all built-in tools
    void register_builtin_tools();

    /// Enable or disable the interactive question tool.
    /// Must be called before register_builtin_tools() to take effect.
    /// Default: disabled (question tool requires interactive UI support).
    void enable_question_tool(bool enable = true);

    /// Discover and register custom tools from a project directory.
    /// Scans for .turbot/tools/*.json files and registers them as ExternalCommandTools.
    /// @param project_dir The project root directory to scan
    /// @return Number of tools discovered and registered
    size_t discover_custom_tools(const std::string& project_dir);

    /// Discover and register custom tools from multiple directories.
    /// @param directories List of directories to scan
    /// @return Number of tools discovered and registered
    size_t discover_custom_tools(const std::vector<std::string>& directories);

    /// Get list of custom tool names (loaded via discover_custom_tools)
    [[nodiscard]] std::vector<std::string> custom_tool_names() const;

private:
    ToolRegistry() = default;
    ~ToolRegistry() = default;

    /// Internal list without locking - caller must hold mutex_
    [[nodiscard]] std::vector<ToolPtr> list_locked() const;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, ToolPtr> tools_;
    bool question_tool_enabled_ = false;  ///< Opt-in flag for QuestionTool
    std::vector<std::string> custom_tool_names_;  ///< Names of tools loaded via discover_custom_tools
};

} // namespace turbot::core::tool
