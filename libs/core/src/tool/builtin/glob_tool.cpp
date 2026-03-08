#include <turbot/core/tool/builtin/glob_tool.hpp>
#include <turbot/core/permission/permission.hpp>
#include "fs_tool_common.hpp"
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <filesystem>
#include <regex>

namespace turbot::core::tool::builtin {

namespace {

constexpr size_t DEFAULT_LIMIT = 100;

} // anonymous namespace

// ============================================================================
// GlobToolParams
// ============================================================================

GlobToolParams GlobToolParams::from_json(const nlohmann::json& j) {
    GlobToolParams params;
    params.pattern = j.at("pattern").get<std::string>();
    
    if (j.contains("path") && !j["path"].is_null()) {
        params.path = j["path"].get<std::string>();
    }
    
    return params;
}

nlohmann::json GlobToolParams::to_json() const {
    nlohmann::json j;
    j["pattern"] = pattern;
    if (path) {
        j["path"] = *path;
    }
    return j;
}

// ============================================================================
// GlobFileEntry
// ============================================================================

nlohmann::json GlobFileEntry::to_json() const {
    return {
        {"path", path},
        {"mtime", mtime}
    };
}

// ============================================================================
// GlobTool
// ============================================================================

std::string GlobTool::description() const {
    return "Find files matching a glob pattern. "
           "Returns a list of file paths sorted by modification time. "
           "Supports common glob patterns like **/*.js, src/**/*.ts, etc.";
}

nlohmann::json GlobTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"pattern", {
                {"type", "string"},
                {"description", "The glob pattern to match files against (e.g., **/*.js, src/**/*.ts)"}
            }},
            {"path", {
                {"type", "string"},
                {"description", "The directory to search in. Defaults to the current working directory."}
            }}
        }},
        {"required", nlohmann::json::array({"pattern"})}
    };
}

bool GlobTool::validate_input(const nlohmann::json& input) const {
    return input.contains("pattern") && input["pattern"].is_string() &&
           !input["pattern"].get<std::string>().empty();
}

std::string GlobTool::glob_to_regex(std::string_view glob) {
    std::string regex;
    regex.reserve(glob.size() * 2);
    
    size_t i = 0;
    while (i < glob.size()) {
        char c = glob[i];
        
        // Handle **
        if (c == '*' && i + 1 < glob.size() && glob[i + 1] == '*') {
            if (i + 2 < glob.size() && glob[i + 2] == '/') {
                // **/ matches any directory
                regex += "(?:.*/)?";
                i += 3;
            } else {
                // ** matches anything
                regex += ".*";
                i += 2;
            }
        }
        // Handle *
        else if (c == '*') {
            regex += "[^/]*";
            ++i;
        }
        // Handle ?
        else if (c == '?') {
            regex += "[^/]";
            ++i;
        }
        // Handle character class [a-z]
        else if (c == '[') {
            size_t end = glob.find(']', i);
            if (end != std::string_view::npos) {
                regex += '[';
                for (size_t j = i + 1; j < end; ++j) {
                    char ch = glob[j];
                    if (ch == '\\' || ch == ']' || ch == '^') {
                        regex += '\\';
                    }
                    regex += ch;
                }
                regex += ']';
                i = end + 1;
            } else {
                regex += "\\[";
                ++i;
            }
        }
        // Handle {a,b,c} alternatives
        else if (c == '{') {
            size_t end = glob.find('}', i);
            if (end != std::string_view::npos) {
                regex += "(?:";
                for (size_t j = i + 1; j < end; ++j) {
                    char ch = glob[j];
                    if (ch == ',') {
                        regex += '|';
                    } else if (ch == '\\' || ch == '.' || ch == '+' || 
                               ch == '^' || ch == '$' || ch == '|' ||
                               ch == '(' || ch == ')' || ch == '[' || ch == ']') {
                        regex += '\\';
                        regex += ch;
                    } else {
                        regex += ch;
                    }
                }
                regex += ')';
                i = end + 1;
            } else {
                regex += "\\{";
                ++i;
            }
        }
        // Escape regex special characters
        else if (c == '.' || c == '+' || c == '^' || c == '$' ||
                 c == '|' || c == '(' || c == ')' || c == '\\' ||
                 c == ']' || c == '}') {
            regex += '\\';
            regex += c;
            ++i;
        }
        else {
            regex += c;
            ++i;
        }
    }
    
    return regex;
}

bool GlobTool::matches_glob(std::string_view path, std::string_view pattern) {
    std::string regex_str = glob_to_regex(pattern);
    regex_str = "^" + regex_str + "$";
    
    try {
        std::regex re(regex_str, std::regex::ECMAScript | std::regex::icase);
        return std::regex_match(path.begin(), path.end(), re);
    } catch (const std::regex_error&) {
        // If regex fails, try simple string matching
        return path.find(pattern) != std::string_view::npos;
    }
}

std::vector<GlobFileEntry> GlobTool::scan_directory(
    const std::string& directory,
    const std::string& pattern,
    size_t limit,
    std::shared_ptr<std::atomic<bool>> abort_flag
) {
    std::vector<GlobFileEntry> results;
    std::filesystem::path dir_path(directory);
    
    std::error_code ec;
    if (!std::filesystem::exists(dir_path, ec) || !std::filesystem::is_directory(dir_path, ec)) {
        return results;
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
            
            if (!entry.is_regular_file(ec)) {
                continue;
            }
            
            // Skip ignored directories
            if (should_ignore(entry.path())) {
                continue;
            }
            
            // Get relative path
            std::filesystem::path relative = std::filesystem::relative(entry.path(), dir_path, ec);
            if (ec) continue;
            
            std::string relative_str = relative.string();
            
            // Check pattern match
            if (matches_glob(relative_str, pattern) || matches_glob(entry.path().string(), pattern)) {
                GlobFileEntry file_entry;
                file_entry.path = entry.path().string();
                
                auto ftime = std::filesystem::last_write_time(entry.path(), ec);
                if (!ec) {
                    // Convert file_time_type to time_t
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
                    );
                    file_entry.mtime = std::chrono::duration_cast<std::chrono::seconds>(
                        sctp.time_since_epoch()
                    ).count();
                }
                
                results.push_back(std::move(file_entry));
                
                if (results.size() >= limit) {
                    break;
                }
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore filesystem errors
    }
    
    // Sort by modification time (newest first)
    std::sort(results.begin(), results.end(), [](const GlobFileEntry& a, const GlobFileEntry& b) {
        return a.mtime > b.mtime;
    });
    
    return results;
}

ToolResult GlobTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    if (!validate_input(input)) {
        return ToolResult::error(
            "Glob failed",
            "Invalid input: 'pattern' is required and must be a non-empty string"
        );
    }
    
    GlobToolParams params;
    try {
        params = GlobToolParams::from_json(input);
    } catch (const std::exception& e) {
        return ToolResult::error("Glob", fmt::format("Invalid parameters: {}", e.what()));
    }
    
    // Determine search directory
    std::string search_dir = params.path.value_or(ctx.working_directory);
    if (search_dir.empty()) {
        std::error_code ec;
        search_dir = std::filesystem::current_path(ec).string();
        if (ec) {
            search_dir = ".";
        }
    }
    
    // Resolve to absolute path
    std::filesystem::path search_path(search_dir);
    if (!search_path.is_absolute()) {
        search_path = std::filesystem::path(ctx.working_directory) / search_path;
    }
    search_dir = search_path.string();
    
    // Request permission
    if (ctx.ask_permission) {
        permission::PermissionRequest req;
        req.id = fmt::format("glob_{}", params.pattern);
        req.permission = "glob";
        req.patterns = {params.pattern};
        req.tool = name();
        req.metadata = {
            {"pattern", params.pattern},
            {"path", search_dir}
        };
        
        auto reply = ctx.ask_permission(req);
        if (reply.type == permission::PermissionReply::Type::Reject) {
            return ToolResult::error("Glob", "User rejected permission to search files");
        }
    }
    
    // Scan for files
    auto files = scan_directory(search_dir, params.pattern, DEFAULT_LIMIT, ctx.abort_flag);
    
    // Build output
    std::vector<std::string> output_lines;
    
    if (files.empty()) {
        output_lines.push_back("No files found");
    } else {
        for (const auto& file : files) {
            output_lines.push_back(file.path);
        }
        
        if (files.size() >= DEFAULT_LIMIT) {
            output_lines.push_back("");
            output_lines.push_back(fmt::format(
                "(Results are truncated: showing first {} results. Consider using a more specific path or pattern.)",
                DEFAULT_LIMIT
            ));
        }
    }
    
    nlohmann::json metadata = {
        {"pattern", params.pattern},
        {"path", search_dir},
        {"count", files.size()},
        {"truncated", files.size() >= DEFAULT_LIMIT}
    };
    
    std::string title = search_dir;
    std::error_code ec;
    auto relative = std::filesystem::relative(search_dir, ctx.working_directory, ec);
    if (!ec && !relative.empty()) {
        title = relative.string();
    }
    
    return ToolResult::success(title, fmt::format("{}", fmt::join(output_lines, "\n")), metadata);
}

} // namespace turbot::core::tool::builtin
