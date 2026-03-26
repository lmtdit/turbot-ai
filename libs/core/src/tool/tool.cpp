#include <turbot/core/tool/tool.hpp>
#include <fmt/format.h>
#include <stdexcept>

namespace turbot::core::tool {

// ============================================================================
// ShellMode
// ============================================================================

std::string shell_mode_to_string(ShellMode mode) {
    switch (mode) {
        case ShellMode::Normal:  return "normal";
        case ShellMode::Sandbox: return "sandbox";
        case ShellMode::Ask:     return "ask";
    }
    throw std::invalid_argument(fmt::format("Invalid ShellMode value: {}", static_cast<int>(mode)));
}

ShellMode string_to_shell_mode(const std::string& str) {
    if (str == "normal")  return ShellMode::Normal;
    if (str == "sandbox") return ShellMode::Sandbox;
    if (str == "ask")     return ShellMode::Ask;
    // Default to Ask for unknown values
    return ShellMode::Ask;
}

// ============================================================================
// ToolConfig
// ============================================================================

nlohmann::json ToolConfig::to_json() const {
    return {
        {"shell_mode",        shell_mode_to_string(shell_mode)},
        {"default_timeout",   default_timeout},
        {"max_timeout",       max_timeout},
        {"auto_approve_read", auto_approve_read},
        {"auto_approve_edit", auto_approve_edit}
    };
}

ToolConfig ToolConfig::from_json(const nlohmann::json& j) {
    ToolConfig config;
    if (j.contains("shell_mode")) {
        config.shell_mode = string_to_shell_mode(j["shell_mode"].get<std::string>());
    }
    if (j.contains("default_timeout")) {
        config.default_timeout = j["default_timeout"].get<int>();
    }
    if (j.contains("max_timeout")) {
        config.max_timeout = j["max_timeout"].get<int>();
    }
    if (j.contains("auto_approve_read")) {
        config.auto_approve_read = j["auto_approve_read"].get<bool>();
    }
    if (j.contains("auto_approve_edit")) {
        config.auto_approve_edit = j["auto_approve_edit"].get<bool>();
    }
    return config;
}

// ============================================================================
// ToolResult
// ============================================================================

nlohmann::json ToolResult::to_json() const {
    nlohmann::json j = {
        {"title",    title},
        {"output",   output},
        {"metadata", metadata},
        {"is_error", is_error}
    };
    if (!attachments.empty()) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& a : attachments) {
            arr.push_back({{"url", a.url}, {"mime", a.mime}});
        }
        j["attachments"] = std::move(arr);
    }
    return j;
}

ToolResult ToolResult::from_json(const nlohmann::json& j) {
    ToolResult result;
    result.title    = j.at("title").get<std::string>();
    result.output   = j.at("output").get<std::string>();
    result.is_error = j.value("is_error", false);
    if (j.contains("metadata")) {
        result.metadata = j["metadata"];
    }
    if (j.contains("attachments") && j["attachments"].is_array()) {
        for (const auto& a : j["attachments"]) {
            ToolAttachment att;
            att.url  = a.value("url",  std::string{});
            att.mime = a.value("mime", std::string{});
            if (!att.url.empty()) result.attachments.push_back(std::move(att));
        }
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
