#include <turbot/core/tool/builtin/write_file_tool.hpp>
#include <turbot/core/permission/permission.hpp>
#include <turbot/utils/string_utils.hpp>
#include "fs_tool_common.hpp"
#include <fmt/format.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <algorithm>

namespace turbot::core::tool::builtin {

namespace fs = std::filesystem;

// 使用 turbot::utils 命名空间的字符串工具函数
using turbot::utils::split_lines;

// ============================================================================
// WriteFileToolParams
// ============================================================================

WriteFileToolParams WriteFileToolParams::from_json(const nlohmann::json& j) {
    WriteFileToolParams params;
    params.content = j.at("content").get<std::string>();
    params.file_path = j.at("filePath").get<std::string>();
    return params;
}

nlohmann::json WriteFileToolParams::to_json() const {
    return {
        {"filePath", file_path},
        {"content", content}
    };
}

// ============================================================================
// WriteFileTool
// ============================================================================

std::string WriteFileTool::description() const {
    return "Write content to a file at the given path. "
           "Creates the file if it does not exist, overwrites it if it does. "
           "Creates parent directories as needed. "
           "Shows a diff of changes after writing.";
}

nlohmann::json WriteFileTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"filePath", {
                {"type", "string"},
                {"description", "The absolute path to the file to write (must be absolute, not relative)"}
            }},
            {"content", {
                {"type", "string"},
                {"description", "The content to write to the file"}
            }}
        }},
        {"required", nlohmann::json::array({"filePath", "content"})}
    };
}

bool WriteFileTool::validate_input(const nlohmann::json& input) const {
    return input.contains("filePath") && input["filePath"].is_string() &&
           !input["filePath"].get<std::string>().empty() &&
           input.contains("content") && input["content"].is_string();
}

std::string WriteFileTool::create_diff(
    const std::string& file_path,
    const std::string& old_content,
    const std::string& new_content
) {
    // Delegate to the shared utility to avoid duplicate diff logic
    return turbot::utils::create_diff(file_path, old_content, new_content);
}

ToolResult WriteFileTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    if (!validate_input(input)) {
        return ToolResult::error(
            "Write failed",
            "Invalid input: 'filePath' (non-empty string) and 'content' (string) are required"
        );
    }

    WriteFileToolParams params;
    try {
        params = WriteFileToolParams::from_json(input);
    } catch (const std::exception& e) {
        return ToolResult::error("Write", fmt::format("Invalid parameters: {}", e.what()));
    }
    
    std::string path = params.file_path;
    std::string content = params.content;

    // Resolve relative path
    fs::path file_path(path);
    if (!file_path.is_absolute()) {
        file_path = fs::path(ctx.working_directory) / file_path;
        path = file_path.string();
    }

    // Canonicalize path and enforce workspace boundary to prevent path traversal
    // (e.g. "/workspace/../../etc/passwd" or symlinks escaping the workspace).
    // Only canonicalize when a working directory is set; otherwise the raw path
    // is used as-is (preserves /tmp symlinks and test expectations).
    if (!ctx.working_directory.empty()) {
        std::error_code ec;
        auto canonical_path = fs::weakly_canonical(file_path, ec);
        if (!ec) {
            file_path = canonical_path;
            path = file_path.string();
        }
    }
    if (auto err = check_workspace_boundary(file_path, ctx.working_directory)) {
        return ToolResult::error(path, *err);
    }

    // Check permission
    const auto action = permission::PermissionSystem::evaluate("edit", path, ctx.ruleset);

    if (action == permission::PermissionAction::Deny) {
        return ToolResult::error(path, fmt::format("Permission denied for writing '{}'", path));
    }

    // Read existing content for diff
    // W-5: Skip reading the file if it's too large to avoid OOM before the
    // MAX_DIFF_LINES guard in create_diff can kick in.
    std::string old_content;
    bool file_exists = fs::exists(path);
    
    if (file_exists) {
        std::error_code size_ec;
        auto sz = fs::file_size(path, size_ec);
        constexpr uintmax_t MAX_DIFF_BYTES = 10 * 1024 * 1024;  // 10 MB
        if (!size_ec && sz <= MAX_DIFF_BYTES) {
            std::ifstream ifs(path);
            if (ifs) {
                std::ostringstream oss;
                oss << ifs.rdbuf();
                old_content = oss.str();
            }
        }
    }
    
    // Create diff
    std::string diff = create_diff(path, old_content, content);

    if (action == permission::PermissionAction::Ask) {
        if (!ctx.ask_permission) {
            return ToolResult::error(path, "Permission requires user confirmation but no callback is set");
        }
        permission::PermissionRequest req;
        req.id = fmt::format("write_{:x}", std::hash<std::string>{}(path));
        req.permission = "edit";
        req.patterns = {path};
        req.tool = name();
        req.metadata = {
            {"filepath", path},
            {"diff", diff}
        };

        const auto reply = ctx.ask_permission(req);
        if (reply.type == permission::PermissionReply::Type::Reject) {
            return ToolResult::error(path, fmt::format("User rejected permission for writing '{}'", path));
        }
    }

    // Check abort flag
    if (ctx.should_abort()) {
        return ToolResult::error("Write aborted", "Operation was aborted");
    }

    // Create parent directories and write atomically via temp file
    std::error_code ec;
    if (file_path.has_parent_path()) {
        fs::create_directories(file_path.parent_path(), ec);
        if (ec) {
            return ToolResult::error(
                path,
                fmt::format("Failed to create parent directories for '{}': {}", path, ec.message())
            );
        }
    }

    // Write to a temporary file first, then rename for atomicity
    const fs::path tmp_path = file_path.parent_path() /
        fmt::format("{}.{:x}.turbot_tmp",
            file_path.filename().string(),
            std::hash<std::string>{}(ctx.session_id + ctx.message_id));

    {
        std::ofstream tmp_file(tmp_path, std::ios::out | std::ios::trunc);
        if (!tmp_file.is_open()) {
            return ToolResult::error(
                path,
                fmt::format("Cannot open temp file for writing: '{}'", tmp_path.string())
            );
        }
        tmp_file << content;
        tmp_file.close();
        if (!tmp_file.good()) {
            fs::remove(tmp_path, ec);
            return ToolResult::error(
                path,
                fmt::format("Failed to write content to temp file: '{}'", tmp_path.string())
            );
        }
    }

    // Atomically rename temp file to target
    fs::rename(tmp_path, file_path, ec);
    if (ec) {
        fs::remove(tmp_path, ec);
        return ToolResult::error(
            path,
            fmt::format("Failed to finalize write to '{}': {}", path, ec.message())
        );
    }

    std::string output = "Wrote file successfully.";
    if (!file_exists) {
        output = fmt::format("Created new file: {}", path);
    }

    nlohmann::json metadata = {
        {"filepath", path},
        {"bytes_written", static_cast<int64_t>(content.size())},
        {"exists", file_exists},
        {"diff", diff}
    };

    // Get relative path for title
    std::string title = path;
    std::error_code rel_ec;
    auto relative = fs::relative(path, ctx.working_directory, rel_ec);
    if (!rel_ec && !relative.empty()) {
        title = relative.string();
    }

    return ToolResult::success(title, output, metadata);
}

} // namespace turbot::core::tool::builtin
