#include <turbot/core/tool/builtin/read_file_tool.hpp>
#include <turbot/core/permission/permission.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace turbot::core::tool::builtin {

namespace fs = std::filesystem;

nlohmann::json ReadFileTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {
                {"type", "string"},
                {"description", "The absolute or relative path to the file to read"}
            }}
        }},
        {"required", nlohmann::json::array({"path"})}
    };
}

bool ReadFileTool::validate_input(const nlohmann::json& input) const {
    return input.contains("path") && input["path"].is_string() &&
           !input["path"].get<std::string>().empty();
}

ToolResult ReadFileTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    if (!validate_input(input)) {
        return ToolResult::error("Read file failed", "Invalid input: 'path' is required and must be a non-empty string");
    }

    const std::string path = input["path"].get<std::string>();

    // Check permission via ruleset first
    const auto action = permission::PermissionSystem::evaluate("read", path, ctx.ruleset);

    if (action == permission::PermissionAction::Deny) {
        return ToolResult::error(
            fmt::format("Read file: {}", path),
            fmt::format("Permission denied for reading '{}'", path)
        );
    }

    if (action == permission::PermissionAction::Ask) {
        if (!ctx.ask_permission) {
            return ToolResult::error(
                fmt::format("Read file: {}", path),
                "Permission requires user confirmation but no ask_permission callback is set"
            );
        }
        permission::PermissionRequest req;
        req.id         = fmt::format("read_file_{:x}", std::hash<std::string>{}(path));
        req.permission = "read";
        req.patterns   = {path};
        req.tool       = name();

        const auto reply = ctx.ask_permission(req);
        if (reply.type == permission::PermissionReply::Type::Reject) {
            return ToolResult::error(
                fmt::format("Read file: {}", path),
                fmt::format("User rejected permission for reading '{}'", path)
            );
        }
    }

    // Check abort flag
    if (ctx.should_abort()) {
        return ToolResult::error("Read file aborted", "Operation was aborted");
    }

    // Read the file
    std::error_code ec;
    const bool file_exists = fs::exists(path, ec);
    if (ec) {
        return ToolResult::error(
            fmt::format("Read file: {}", path),
            fmt::format("Cannot access '{}': {}", path, ec.message())
        );
    }
    if (!file_exists) {
        return ToolResult::error(
            fmt::format("Read file: {}", path),
            fmt::format("File not found: '{}'", path)
        );
    }

    const bool is_regular = fs::is_regular_file(path, ec);
    if (ec) {
        return ToolResult::error(
            fmt::format("Read file: {}", path),
            fmt::format("Cannot stat '{}': {}", path, ec.message())
        );
    }
    if (!is_regular) {
        return ToolResult::error(
            fmt::format("Read file: {}", path),
            fmt::format("'{}' is not a regular file", path)
        );
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return ToolResult::error(
            fmt::format("Read file: {}", path),
            fmt::format("Cannot open file: '{}'", path)
        );
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    if (file.fail() && !file.eof()) {
        return ToolResult::error(
            fmt::format("Read file: {}", path),
            fmt::format("I/O error while reading '{}'", path)
        );
    }
    const std::string content = oss.str();

    const auto file_size = fs::file_size(path, ec);
    nlohmann::json metadata = {
        {"path", path},
        {"size", ec ? 0 : static_cast<int64_t>(file_size)}
    };

    return ToolResult::success(
        fmt::format("Read file: {}", path),
        content,
        metadata
    );
}

} // namespace turbot::core::tool::builtin
