#include <turbot/core/tool/builtin/read_file_tool.hpp>
#include <turbot/core/permission/permission.hpp>
#include "fs_tool_common.hpp"
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace turbot::core::tool::builtin {

namespace fs = std::filesystem;

namespace {

constexpr int DEFAULT_READ_LIMIT = 2000;
constexpr int MAX_LINE_LENGTH = 2000;
constexpr size_t MAX_BYTES = 50 * 1024;  // 50 KB

// Image extensions for special-cased display (return image metadata instead of raw bytes)
static const std::vector<std::string> image_exts = {
    ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".ico", ".webp"
};

[[nodiscard]] inline bool is_image_extension(const fs::path& path) noexcept {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    for (const auto& e : image_exts) {
        if (ext == e) return true;
    }
    return false;
}

} // anonymous namespace

// ============================================================================
// ReadFileToolParams
// ============================================================================

ReadFileToolParams ReadFileToolParams::from_json(const nlohmann::json& j) {
    ReadFileToolParams params;
    params.file_path = j.at("filePath").get<std::string>();
    
    if (j.contains("offset") && !j["offset"].is_null()) {
        params.offset = j["offset"].get<int>();
    }
    if (j.contains("limit") && !j["limit"].is_null()) {
        params.limit = j["limit"].get<int>();
    }
    
    return params;
}

nlohmann::json ReadFileToolParams::to_json() const {
    nlohmann::json j;
    j["filePath"] = file_path;
    if (offset) j["offset"] = *offset;
    if (limit) j["limit"] = *limit;
    return j;
}

// ============================================================================
// ReadFileTool
// ============================================================================

std::string ReadFileTool::description() const {
    return "Read the contents of a file or list a directory. "
           "Supports pagination with offset and limit parameters. "
           "Returns file contents with line numbers. "
           "Automatically detects binary files and provides helpful suggestions for missing files.";
}

nlohmann::json ReadFileTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"filePath", {
                {"type", "string"},
                {"description", "The absolute path to the file or directory to read"}
            }},
            {"offset", {
                {"type", "integer"},
                {"description", "The line number to start reading from (1-indexed)"}
            }},
            {"limit", {
                {"type", "integer"},
                {"description", "The maximum number of lines to read (defaults to 2000)"}
            }}
        }},
        {"required", nlohmann::json::array({"filePath"})}
    };
}

bool ReadFileTool::validate_input(const nlohmann::json& input) const {
    return input.contains("filePath") && input["filePath"].is_string() &&
           !input["filePath"].get<std::string>().empty();
}

ToolResult ReadFileTool::read_directory(
    const std::string& path,
    int offset,
    int limit,
    ToolContext& ctx
) {
    std::error_code ec;
    std::vector<std::string> entries;
    
    for (const auto& entry : fs::directory_iterator(path, ec)) {
        std::string name = entry.path().filename().string();
        if (entry.is_directory(ec)) {
            name += '/';
        }
        entries.push_back(name);
    }
    
    std::sort(entries.begin(), entries.end());
    
    int start = std::max(0, offset - 1);
    int end = std::min(static_cast<int>(entries.size()), start + limit);
    
    std::vector<std::string> sliced(entries.begin() + start, entries.begin() + end);
    bool truncated = end < static_cast<int>(entries.size());
    
    std::vector<std::string> output_lines;
    output_lines.push_back(fmt::format("<path>{}</path>", path));
    output_lines.push_back("<type>directory</type>");
    output_lines.push_back("<entries>");
    for (const auto& entry : sliced) {
        output_lines.push_back(entry);
    }
    if (truncated) {
        output_lines.push_back(fmt::format(
            "\n(Showing {} of {} entries. Use 'offset' parameter to read beyond entry {})",
            sliced.size(), entries.size(), end + 1
        ));
    } else {
        output_lines.push_back(fmt::format("\n({} entries)", entries.size()));
    }
    output_lines.push_back("</entries>");
    
    return ToolResult::success(
        path,
        fmt::format("{}", fmt::join(output_lines, "\n")),
        {
            {"path", path},
            {"type", "directory"},
            {"count", entries.size()},
            {"truncated", truncated}
        }
    );
}

ToolResult ReadFileTool::read_file(
    const std::string& path,
    int offset,
    int limit,
    ToolContext& ctx
) {
    std::error_code ec;
    
    // Check if binary — delegate to the unified implementation in fs_tool_common.hpp
    // to ensure consistent behaviour with grep_tool (C-1: eliminate local fork).
    if (is_binary_file(fs::path(path))) {
        if (is_image_extension(fs::path(path))) {
            return ToolResult::success(
                path,
                fmt::format("Image file: {}", path),
                {{"type", "image"}, {"path", path}}
            );
        }
        return ToolResult::error(
            path,
            fmt::format("Cannot read binary file: {}", path)
        );
    }
    
    // Read file with line numbers
    std::ifstream file(path);
    if (!file) {
        return ToolResult::error(path, fmt::format("Cannot open file: {}", path));
    }
    
    std::vector<std::string> lines;
    std::string line;
    int line_count = 0;
    size_t total_bytes = 0;
    bool truncated_by_bytes = false;
    bool has_more_lines = false;
    
    int start = std::max(1, offset);
    
    while (std::getline(file, line)) {
        line_count++;
        
        if (line_count < start) continue;
        
        if (static_cast<int>(lines.size()) >= limit) {
            has_more_lines = true;
            continue;
        }
        
        // Truncate long lines
        if (line.length() > MAX_LINE_LENGTH) {
            line = line.substr(0, MAX_LINE_LENGTH) + fmt::format("... (line truncated to {} chars)", MAX_LINE_LENGTH);
        }
        
        size_t line_size = line.size() + (lines.empty() ? 0 : 1);
        if (total_bytes + line_size > MAX_BYTES) {
            truncated_by_bytes = true;
            has_more_lines = true;
            break;
        }
        
        lines.push_back(line);
        total_bytes += line_size;
    }
    
    // Build output with line numbers
    std::vector<std::string> output_lines;
    output_lines.push_back(fmt::format("<path>{}</path>", path));
    output_lines.push_back("<type>file</type>");
    output_lines.push_back("<content>");
    
    for (size_t i = 0; i < lines.size(); ++i) {
        output_lines.push_back(fmt::format("{}: {}", start + static_cast<int>(i), lines[i]));
    }
    
    int last_read_line = start + static_cast<int>(lines.size()) - 1;
    int next_offset = last_read_line + 1;
    
    if (truncated_by_bytes) {
        output_lines.push_back(fmt::format(
            "\n\n(Output capped at 50 KB. Showing lines {}-{}. Use offset={} to continue.)",
            start, last_read_line, next_offset
        ));
    } else if (has_more_lines) {
        output_lines.push_back(fmt::format(
            "\n\n(Showing lines {}-{} of {}. Use offset={} to continue.)",
            start, last_read_line, line_count, next_offset
        ));
    } else {
        output_lines.push_back(fmt::format("\n\n(End of file - total {} lines)", line_count));
    }
    output_lines.push_back("</content>");
    
    return ToolResult::success(
        path,
        fmt::format("{}", fmt::join(output_lines, "\n")),
        {
            {"path", path},
            {"type", "file"},
            {"lines", line_count},
            {"truncated", has_more_lines || truncated_by_bytes}
        }
    );
}

ToolResult ReadFileTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    if (!validate_input(input)) {
        return ToolResult::error("Read failed", "Invalid input: 'filePath' is required and must be a non-empty string");
    }

    ReadFileToolParams params;
    try {
        params = ReadFileToolParams::from_json(input);
    } catch (const std::exception& e) {
        return ToolResult::error("Read", fmt::format("Invalid parameters: {}", e.what()));
    }
    
    // Validate offset
    if (params.offset.has_value() && params.offset.value() < 1) {
        return ToolResult::error("Read", "offset must be greater than or equal to 1");
    }

    std::string path = params.file_path;
    
    // Resolve relative path
    fs::path file_path(path);
    if (!file_path.is_absolute()) {
        file_path = fs::path(ctx.working_directory) / file_path;
        path = file_path.string();
    }

    // Check workspace boundary (prevent path traversal attacks)
    if (auto err = check_workspace_boundary(file_path, ctx.working_directory)) {
        return ToolResult::error(path, *err);
    }

    // Check permission
    const auto action = permission::PermissionSystem::evaluate("read", path, ctx.ruleset);

    if (action == permission::PermissionAction::Deny) {
        return ToolResult::error(path, fmt::format("Permission denied for reading '{}'", path));
    }

    if (action == permission::PermissionAction::Ask) {
        if (!ctx.ask_permission) {
            return ToolResult::error(path, "Permission requires user confirmation but no callback is set");
        }
        permission::PermissionRequest req;
        req.id = fmt::format("read_{:x}", std::hash<std::string>{}(path));
        req.permission = "read";
        req.patterns = {path};
        req.tool = name();

        const auto reply = ctx.ask_permission(req);
        if (reply.type == permission::PermissionReply::Type::Reject) {
            return ToolResult::error(path, fmt::format("User rejected permission for reading '{}'", path));
        }
    }

    // Check abort flag
    if (ctx.should_abort()) {
        return ToolResult::error("Read aborted", "Operation was aborted");
    }

    // Check if path exists
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        // Try to provide suggestions
        fs::path parent = fs::path(path).parent_path();
        std::string basename = fs::path(path).filename().string();
        
        std::vector<std::string> suggestions;
        if (fs::exists(parent, ec)) {
            for (const auto& entry : fs::directory_iterator(parent, ec)) {
                std::string name = entry.path().filename().string();
                // Simple similarity check
                if (name.find(basename) != std::string::npos || basename.find(name) != std::string::npos) {
                    suggestions.push_back(entry.path().string());
                }
            }
        }
        
        if (!suggestions.empty()) {
            return ToolResult::error(
                path,
                fmt::format("File not found: {}\n\nDid you mean one of these?\n{}", path, fmt::join(suggestions, "\n"))
            );
        }
        
        return ToolResult::error(path, fmt::format("File not found: {}", path));
    }

    // Get limit and offset
    int limit = params.limit.value_or(DEFAULT_READ_LIMIT);
    int offset = params.offset.value_or(1);

    // Check if directory
    if (fs::is_directory(path, ec)) {
        return read_directory(path, offset, limit, ctx);
    }

    // Check if regular file
    if (!fs::is_regular_file(path, ec)) {
        return ToolResult::error(path, fmt::format("Path is not a regular file or directory: {}", path));
    }

    return read_file(path, offset, limit, ctx);
}

} // namespace turbot::core::tool::builtin
