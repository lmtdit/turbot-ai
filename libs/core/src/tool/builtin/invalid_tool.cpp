#include <turbot/core/tool/builtin/invalid_tool.hpp>
#include <fmt/format.h>

namespace turbot::core::tool::builtin {

// ============================================================================
// InvalidToolParams
// ============================================================================

InvalidToolParams InvalidToolParams::from_json(const nlohmann::json& j) {
    InvalidToolParams params;
    params.tool = j.contains("tool") ? j["tool"].get<std::string>() : "unknown";
    params.error = j.contains("error") ? j["error"].get<std::string>() : "Unknown error";
    return params;
}

nlohmann::json InvalidToolParams::to_json() const {
    return {
        {"tool", tool},
        {"error", error}
    };
}

// ============================================================================
// InvalidTool
// ============================================================================

std::string InvalidTool::description() const {
    return "Do not use. This is a fallback tool for handling invalid tool calls.";
}

nlohmann::json InvalidTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"tool", {
                {"type", "string"},
                {"description", "The invalid tool name that was attempted"}
            }},
            {"error", {
                {"type", "string"},
                {"description", "The error message describing the issue"}
            }}
        }},
        {"required", nlohmann::json::array({"tool", "error"})}
    };
}

bool InvalidTool::validate_input(const nlohmann::json& input) const {
    // Accept any input - this is a fallback tool
    return true;
}

ToolResult InvalidTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    InvalidToolParams params;
    try {
        params = InvalidToolParams::from_json(input);
    } catch (...) {
        params.tool = "unknown";
        params.error = "Failed to parse parameters";
    }
    
    std::string output = fmt::format(
        "The arguments provided to the tool are invalid: {}",
        params.error
    );
    
    nlohmann::json metadata = {
        {"tool", params.tool},
        {"error", params.error}
    };
    
    return ToolResult::success("Invalid Tool", output, metadata);
}

} // namespace turbot::core::tool::builtin
