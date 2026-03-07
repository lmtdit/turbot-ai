#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/permission/permission.hpp>
#include <nlohmann/json.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace turbot::core::tool {

/// Result of a tool execution
struct TURBOT_CORE_API ToolResult {
    std::string title;         ///< Short title of the result
    std::string output;        ///< Text output from the tool
    nlohmann::json metadata;   ///< Optional structured metadata
    bool is_error = false;     ///< Whether this is an error result

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static ToolResult from_json(const nlohmann::json& j);

    /// Create a success result
    [[nodiscard]] static ToolResult success(
        const std::string& title,
        const std::string& output,
        const nlohmann::json& metadata = nlohmann::json::object()
    );

    /// Create an error result
    [[nodiscard]] static ToolResult error(
        const std::string& title,
        const std::string& message,
        const nlohmann::json& metadata = nlohmann::json::object()
    );
};

/// Context passed to tool execution
struct TURBOT_CORE_API ToolContext {
    std::string session_id;    ///< Current session ID
    std::string message_id;    ///< Current message ID
    std::string agent;         ///< Agent name making the request
    std::optional<std::string> call_id;  ///< Tool call ID from LLM

    /// Callback to report metadata during execution
    std::function<void(const nlohmann::json&)> on_metadata;

    /// Callback to request permission from user
    /// Returns the permission reply
    std::function<permission::PermissionReply(const permission::PermissionRequest&)> ask_permission;

    /// Current ruleset for automatic permission evaluation
    permission::Ruleset ruleset;

    /// Abort flag - tool should check this and stop if set to true
    std::shared_ptr<std::atomic<bool>> abort_flag;

    /// Check if execution should be aborted
    [[nodiscard]] bool should_abort() const noexcept {
        return abort_flag && abort_flag->load(std::memory_order_relaxed);
    }
};

/// Abstract base class for all tools
class TURBOT_CORE_API Tool {
public:
    virtual ~Tool() = default;

    // Non-copyable
    Tool(const Tool&) = delete;
    Tool& operator=(const Tool&) = delete;
    Tool(Tool&&) = default;
    Tool& operator=(Tool&&) = default;

    /// Get the tool name (used as identifier)
    [[nodiscard]] virtual std::string name() const = 0;

    /// Get the tool description (shown to the AI model)
    [[nodiscard]] virtual std::string description() const = 0;

    /// Get the JSON Schema for the input parameters
    [[nodiscard]] virtual nlohmann::json input_schema() const = 0;

    /// Execute the tool with given input
    /// @param input JSON input matching the input_schema
    /// @param ctx Execution context
    /// @return Tool execution result
    [[nodiscard]] virtual ToolResult execute(const nlohmann::json& input, ToolContext& ctx) = 0;

    /// Validate input against the schema (basic validation)
    /// @param input JSON input to validate
    /// @return true if input is valid
    [[nodiscard]] virtual bool validate_input(const nlohmann::json& input) const;

    /// Format a validation error message
    [[nodiscard]] virtual std::string format_validation_error(const std::string& error) const;

    /// Convert the tool to a provider::ToolDefinition JSON format
    [[nodiscard]] nlohmann::json to_tool_definition() const;

protected:
    Tool() = default;
};

/// Smart pointer for Tool
using ToolPtr = std::shared_ptr<Tool>;

} // namespace turbot::core::tool
