#include <turbot/core/tool/tool.hpp>
#include <fmt/format.h>
#include <stdexcept>

namespace turbot::core::tool {

// ============================================================================
// ToolResult
// ============================================================================

nlohmann::json ToolResult::to_json() const {
    return {
        {"title",    title},
        {"output",   output},
        {"metadata", metadata},
        {"is_error", is_error}
    };
}

ToolResult ToolResult::from_json(const nlohmann::json& j) {
    ToolResult result;
    result.title    = j.at("title").get<std::string>();
    result.output   = j.at("output").get<std::string>();
    result.is_error = j.value("is_error", false);
    if (j.contains("metadata")) {
        result.metadata = j["metadata"];
    }
    return result;
}

ToolResult ToolResult::success(
    const std::string& title,
    const std::string& output,
    const nlohmann::json& metadata
) {
    ToolResult r;
    r.title    = title;
    r.output   = output;
    r.metadata = metadata;
    r.is_error = false;
    return r;
}

ToolResult ToolResult::error(
    const std::string& title,
    const std::string& message,
    const nlohmann::json& metadata
) {
    ToolResult r;
    r.title    = title;
    r.output   = message;
    r.metadata = metadata;
    r.is_error = true;
    return r;
}

// ============================================================================
// Tool
// ============================================================================

bool Tool::validate_input(const nlohmann::json& input) const {
    // Default: just check that required fields from schema exist
    const auto schema = input_schema();
    if (!schema.contains("required")) {
        return true;
    }
    for (const auto& req : schema["required"]) {
        const std::string field = req.get<std::string>();
        if (!input.contains(field)) {
            return false;
        }
    }
    return true;
}

std::string Tool::format_validation_error(const std::string& error) const {
    return fmt::format("Tool '{}' validation error: {}", name(), error);
}

nlohmann::json Tool::to_tool_definition() const {
    return {
        {"type",     "function"},
        {"function", {
            {"name",        name()},
            {"description", description()},
            {"parameters",  input_schema()}
        }}
    };
}

} // namespace turbot::core::tool
