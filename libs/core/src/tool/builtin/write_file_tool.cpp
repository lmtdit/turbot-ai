#include <turbot/core/tool/builtin/write_file_tool.hpp>
#include <turbot/core/permission/permission.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <fstream>
#include <functional>

namespace turbot::core::tool::builtin {

namespace fs = std::filesystem;

nlohmann::json WriteFileTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {
                {"type", "string"},
                {"description", "The absolute or relative path to the file to write"}
            }},
            {"content", {
                {"type", "string"},
                {"description", "The content to write to the file"}
            }}
        }},
        {"required", nlohmann::json::array({"path", "content"})}
    };
}

bool WriteFileTool::validate_input(const nlohmann::json& input) const {
    return input.contains("path") && input["path"].is_string() &&
           !input["path"].get<std::string>().empty() &&
           input.contains("content") && input["content"].is_string();
}

ToolResult WriteFileTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    if (!validate_input(input)) {
        return ToolResult::error(
            "Write file failed",
            "Invalid input: 'path' (non-empty string) and 'content' (string) are required"
        );
    }

    const std::string path    = input["path"].get<std::string>();
    const std::string content = input["content"].get<std::string>();

    // Check permission via ruleset first
    const auto action = permission::PermissionSystem::evaluate("write", path, ctx.ruleset);

    if (action == permission::PermissionAction::Deny) {
        return ToolResult::error(
            fmt::format("Write file: {}", path),
            fmt::format("Permission denied for writing '{}'", path)
        );
    }

    if (action == permission::PermissionAction::Ask) {
        if (!ctx.ask_permission) {
            return ToolResult::error(
                fmt::format("Write file: {}", path),
                "Permission requires user confirmation but no ask_permission callback is set"
            );
        }
        permission::PermissionRequest req;
        req.id         = fmt::format("write_file_{:x}", std::hash<std::string>{}(path));
        req.permission = "write";
        req.patterns   = {path};
        req.tool       = name();

        const auto reply = ctx.ask_permission(req);
        if (reply.type == permission::PermissionReply::Type::Reject) {
            return ToolResult::error(
                fmt::format("Write file: {}", path),
                fmt::format("User rejected permission for writing '{}'", path)
            );
        }
    }

    // Check abort flag
    if (ctx.should_abort()) {
        return ToolResult::error("Write file aborted", "Operation was aborted");
    }

    // Create parent directories and write atomically via temp file
    std::error_code ec;
    const fs::path file_path(path);
    if (file_path.has_parent_path()) {
        fs::create_directories(file_path.parent_path(), ec);
        if (ec) {
            return ToolResult::error(
                fmt::format("Write file: {}", path),
                fmt::format("Failed to create parent directories for '{}': {}", path, ec.message())
            );
        }
    }

    // Write to a temporary file first, then rename for atomicity
    // Use session+message ID for unique tmp filename to prevent concurrent write races
    const fs::path tmp_path = file_path.parent_path() /
        fmt::format("{}.{:x}.turbot_tmp",
            file_path.filename().string(),
            std::hash<std::string>{}(ctx.session_id + ctx.message_id));

    {
        std::ofstream tmp_file(tmp_path, std::ios::out | std::ios::trunc);
        if (!tmp_file.is_open()) {
            return ToolResult::error(
                fmt::format("Write file: {}", path),
                fmt::format("Cannot open temp file for writing: '{}'", tmp_path.string())
            );
        }
        tmp_file << content;
        tmp_file.close();
        if (!tmp_file.good()) {
            fs::remove(tmp_path, ec);
            return ToolResult::error(
                fmt::format("Write file: {}", path),
                fmt::format("Failed to write content to temp file: '{}'", tmp_path.string())
            );
        }
    }

    // Atomically rename temp file to target
    fs::rename(tmp_path, file_path, ec);
    if (ec) {
        fs::remove(tmp_path, ec);
        return ToolResult::error(
            fmt::format("Write file: {}", path),
            fmt::format("Failed to finalize write to '{}': {}", path, ec.message())
        );
    }

    nlohmann::json metadata = {
        {"path",          path},
        {"bytes_written", static_cast<int64_t>(content.size())}
    };

    return ToolResult::success(
        fmt::format("Write file: {}", path),
        fmt::format("Successfully wrote {} bytes to '{}'", content.size(), path),
        metadata
    );
}

} // namespace turbot::core::tool::builtin
