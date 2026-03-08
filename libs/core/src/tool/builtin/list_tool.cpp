#include <turbot/core/tool/builtin/list_tool.hpp>
#include <turbot/core/permission/permission.hpp>
#include "fs_tool_common.hpp"
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <filesystem>

namespace turbot::core::tool::builtin {

namespace {

constexpr size_t DEFAULT_LIMIT = 100;

} // anonymous namespace

// ============================================================================
// ListToolParams
// ============================================================================

ListToolParams ListToolParams::from_json(const nlohmann::json& j) {
    ListToolParams params;
    
    if (j.contains("path") && !j["path"].is_null()) {
        params.path = j["path"].get<std::string>();
    }
    
    if (j.contains("ignore") && !j["ignore"].is_null() && j["ignore"].is_array()) {
        for (const auto& item : j["ignore"]) {
            if (item.is_string()) {
                params.ignore.push_back(item.get<std::string>());
            }
        }
    }
    
    return params;
}

nlohmann::json ListToolParams::to_json() const {
    nlohmann::json j;
    if (path) j["path"] = *path;
    j["ignore"] = ignore;
    return j;
}

// ============================================================================
// ListTool
// ============================================================================

std::string ListTool::description() const {
    return "List directory contents. "
           "Returns a tree-style view of files and directories. "
           "Automatically ignores common directories like node_modules, .git, etc.";
}

nlohmann::json ListTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {
                {"type", "string"},
                {"description", "The absolute path to the directory to list (must be absolute, not relative)"}
            }},
            {"ignore", {
                {"type", "array"},
                {"items", {{"type", "string"}}},
                {"description", "List of glob patterns to ignore"}
            }}
        }},
        {"required", nlohmann::json::array()}
    };
}

bool ListTool::validate_input(const nlohmann::json& input) const {
    // Path is optional
    if (input.contains("path") && !input["path"].is_null() && !input["path"].is_string()) {
        return false;
    }
    if (input.contains("ignore") && !input["ignore"].is_null() && !input["ignore"].is_array()) {
        return false;
    }
    return true;
}

std::vector<std::string> ListTool::default_ignore_patterns() {
    // Delegate to the shared implementation in fs_tool_common.hpp so that
    // all three filesystem tools (glob, grep, list) stay in sync.
    const auto& patterns = turbot::core::tool::builtin::default_ignore_patterns();
    return std::vector<std::string>(patterns.begin(), patterns.end());
}

bool ListTool::should_ignore(const std::string& name, const std::vector<std::string>& ignore_patterns) {
    for (const auto& pattern : ignore_patterns) {
        // Simple pattern matching: exact match or glob-style match
        if (name == pattern) {
            return true;
        }
        
        // Handle patterns like "*.log" or ".*"
        if (pattern.size() >= 2 && pattern[0] == '*' && pattern[1] == '.') {
            std::string ext = pattern.substr(1); // Get .ext
            if (name.size() >= ext.size() && name.substr(name.size() - ext.size()) == ext) {
                return true;
            }
        }
        
        // Handle patterns ending with /
        if (!pattern.empty() && pattern.back() == '/' && name == pattern.substr(0, pattern.size() - 1)) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> ListTool::list_directory(
    const std::string& directory,
    const std::vector<std::string>& ignore_patterns,
    size_t limit,
    std::shared_ptr<std::atomic<bool>> abort_flag
) {
    std::vector<std::string> results;
    std::filesystem::path dir_path(directory);
    
    std::error_code ec;
    if (!std::filesystem::exists(dir_path, ec) || !std::filesystem::is_directory(dir_path, ec)) {
        return results;
    }
    
    auto all_ignore = default_ignore_patterns();
    for (const auto& p : ignore_patterns) {
        all_ignore.push_back(p);
    }
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
            dir_path,
            std::filesystem::directory_options::skip_permission_denied,
            ec
        )) {
            if (abort_flag && abort_flag->load(std::memory_order_relaxed)) {
                break;
            }
            
            // Get relative path
            auto relative = std::filesystem::relative(entry.path(), dir_path, ec);
            if (ec) continue;
            
            std::string relative_str = relative.string();
            
            // Check ignore patterns
            bool skip = false;
            for (const auto& part : relative) {
                if (should_ignore(part.string(), all_ignore)) {
                    skip = true;
                    break;
                }
            }
            if (skip) continue;
            
            results.push_back(relative_str);
            
            if (results.size() >= limit) {
                break;
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore filesystem errors
    }
    
    // Sort alphabetically
    std::sort(results.begin(), results.end());
    
    return results;
}

ToolResult ListTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    if (!validate_input(input)) {
        return ToolResult::error(
            "List failed",
            "Invalid input: 'path' must be a string if provided, 'ignore' must be an array if provided"
        );
    }
    
    ListToolParams params;
    try {
        params = ListToolParams::from_json(input);
    } catch (const std::exception& e) {
        return ToolResult::error("List", fmt::format("Invalid parameters: {}", e.what()));
    }
    
    // Determine directory path
    std::string dir_path = params.path.value_or(ctx.working_directory);
    if (dir_path.empty()) {
        std::error_code ec;
        dir_path = std::filesystem::current_path(ec).string();
        if (ec) {
            dir_path = ".";
        }
    }
    
    // Resolve to absolute path
    std::filesystem::path abs_path(dir_path);
    if (!abs_path.is_absolute()) {
        abs_path = std::filesystem::path(ctx.working_directory) / abs_path;
    }
    dir_path = abs_path.string();
    
    // Check workspace boundary (prevent path traversal attacks)
    if (auto err = check_workspace_boundary(abs_path, ctx.working_directory)) {
        return ToolResult::error("List", *err);
    }

    // Evaluate permission rules (respects Deny rules in ruleset)
    const auto perm_action = permission::PermissionSystem::evaluate(
        "list", dir_path, ctx.ruleset);
    if (perm_action == permission::PermissionAction::Deny) {
        return ToolResult::error("List",
            fmt::format("Permission denied for listing '{}'", dir_path));
    }

    // Request permission
    if (perm_action == permission::PermissionAction::Ask) {
        if (!ctx.ask_permission) {
            return ToolResult::error("List",
                "Permission requires user confirmation but no callback is set");
        }
        permission::PermissionRequest req;
        req.id = fmt::format("list_{}", dir_path);
        req.permission = "list";
        req.patterns = {dir_path};
        req.tool = name();
        req.metadata = {
            {"path", dir_path}
        };
        
        auto reply = ctx.ask_permission(req);
        if (reply.type == permission::PermissionReply::Type::Reject) {
            return ToolResult::error("List", "User rejected permission to list directory");
        }
    }
    
    // Check if directory exists
    std::error_code ec;
    if (!std::filesystem::exists(dir_path, ec)) {
        return ToolResult::error("List", fmt::format("Directory not found: {}", dir_path));
    }
    
    if (!std::filesystem::is_directory(dir_path, ec)) {
        return ToolResult::error("List", fmt::format("Path is not a directory: {}", dir_path));
    }
    
    // List directory
    auto files = list_directory(dir_path, params.ignore, DEFAULT_LIMIT, ctx.abort_flag);
    
    // Build tree output
    std::vector<std::string> output_lines;
    output_lines.push_back(fmt::format("{}/", dir_path));
    
    if (files.empty()) {
        output_lines.push_back("  (empty directory)");
    } else {
        // Build directory tree structure
        std::map<std::string, std::vector<std::string>> dirs;
        std::map<std::string, std::vector<std::string>> files_by_dir;
        
        for (const auto& file : files) {
            std::filesystem::path p(file);
            std::string parent = p.parent_path().string();
            if (parent.empty() || parent == ".") {
                parent = ".";
            }
            files_by_dir[parent].push_back(p.filename().string());
        }
        
        // Render tree
        std::function<void(const std::string&, int)> render_dir;
        render_dir = [&](const std::string& dir_path, int depth) {
            std::string indent(depth * 2, ' ');
            
            if (depth > 0) {
                std::string dir_name = std::filesystem::path(dir_path).filename().string();
                output_lines.push_back(fmt::format("{}{}/", indent, dir_name));
            }
            
            // Render subdirectories first
            std::vector<std::string> subdirs;
            for (const auto& [d, _] : files_by_dir) {
                if (d != dir_path && d.find(dir_path) == 0) {
                    std::string rest = d.substr(dir_path.size());
                    // Require a '/' boundary: "src-test" must NOT match as child of "src"
                    if (rest.empty() || rest[0] != '/') continue;
                    rest = rest.substr(1);  // strip the leading '/'
                    if (rest.find('/') == std::string::npos) {
                        subdirs.push_back(d);
                    }
                }
            }
            std::sort(subdirs.begin(), subdirs.end());
            
            for (const auto& subdir : subdirs) {
                render_dir(subdir, depth + 1);
            }
            
            // Render files
            std::string child_indent((depth + 1) * 2, ' ');
            auto it = files_by_dir.find(dir_path);
            if (it != files_by_dir.end()) {
                auto file_names = it->second;
                std::sort(file_names.begin(), file_names.end());
                for (const auto& file : file_names) {
                    output_lines.push_back(fmt::format("{}{}", child_indent, file));
                }
            }
        };
        
        render_dir(".", 0);
        
        if (files.size() >= DEFAULT_LIMIT) {
            output_lines.push_back("");
            output_lines.push_back(fmt::format(
                "(Results truncated: showing first {} entries. Consider using a more specific path.)",
                DEFAULT_LIMIT
            ));
        }
    }
    
    // Get relative path for title
    std::string title = dir_path;
    std::error_code rel_ec;
    auto relative = std::filesystem::relative(dir_path, ctx.working_directory, rel_ec);
    if (!rel_ec && !relative.empty()) {
        title = relative.string();
    }
    
    nlohmann::json metadata = {
        {"path", dir_path},
        {"count", files.size()},
        {"truncated", files.size() >= DEFAULT_LIMIT}
    };
    
    return ToolResult::success(title, fmt::format("{}", fmt::join(output_lines, "\n")), metadata);
}

} // namespace turbot::core::tool::builtin
