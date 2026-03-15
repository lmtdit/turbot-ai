#pragma once

#include <turbot/core/tool/tool.hpp>
#include <string>
#include <vector>
#include <optional>

namespace turbot::core::tool::builtin {

/// A single tool call within a batch
struct BatchToolCall {
    std::string tool;               ///< Tool name to execute
    nlohmann::json parameters;      ///< Parameters for the tool
};

/// Parameters for BatchTool execution
struct BatchToolParams {
    std::vector<BatchToolCall> tool_calls;  ///< Array of tool calls to execute

    /// Parse from JSON
    static BatchToolParams from_json(const nlohmann::json& j);

    /// Convert to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Result of a single tool call in a batch
struct BatchCallResult {
    std::string tool;           ///< Tool name
    bool success = false;       ///< Whether the call succeeded
    std::optional<ToolResult> result;  ///< Result if successful
    std::string error;          ///< Error message if failed
};

/// Tool for executing multiple tool calls in parallel
class TURBOT_CORE_API BatchTool : public Tool {
public:
    BatchTool() = default;

    [[nodiscard]] std::string name() const override { return "batch"; }

    [[nodiscard]] std::string description() const override;

    [[nodiscard]] nlohmann::json input_schema() const override;

    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;

    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;

    /// Get the set of disallowed tool names (cannot be called within batch)
    [[nodiscard]] static const std::vector<std::string>& disallowed_tools();

private:
    /// Maximum number of tool calls in a single batch
    static constexpr size_t MAX_BATCH_SIZE = 25;

    /// Execute a single tool call
    [[nodiscard]] static BatchCallResult execute_single_call(
        const BatchToolCall& call,
        ToolContext& ctx
    );
};

} // namespace turbot::core::tool::builtin
