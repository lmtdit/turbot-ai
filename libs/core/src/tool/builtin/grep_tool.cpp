#include <turbot/core/tool/builtin/grep_tool.hpp>
#include <turbot/core/permission/permission.hpp>
#include "fs_tool_common.hpp"
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace turbot::core::tool::builtin {

namespace {

constexpr size_t DEFAULT_LIMIT = 100;
constexpr size_t MAX_LINE_LENGTH = 2000;

/// Check if file is binary by reading first few bytes
bool is_binary_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return true;
    
    char buffer[8192];
    file.read(buffer, sizeof(buffer));
    std::streamsize bytes_read = file.gcount();
    
    for (std::streamsize i = 0; i < bytes_read; ++i) {
        if (buffer[i] == '\0') {
            return true;
        }
    }
    
    return false;
}

/// Get file extension from path
std::string get_extension(const std::string& filename) {
    size_t pos = filename.rfind('.');
    if (pos != std::string::npos && pos > 0) {
        return filename.substr(pos);
    }
    return "";
}

} // anonymous namespace

// ============================================================================
// GrepToolParams
// ============================================================================

GrepToolParams GrepToolParams::from_json(const nlohmann::json& j) {
    GrepToolParams params;
    params.pattern = j.at("pattern").get<std::string>();
    
    if (j.contains("path") && !j["path"].is_null()) {
        params.path = j["path"].get<std::string>();
    }
    if (j.contains("include") && !j["include"].is_null()) {
        params.include = j["include"].get<std::string>();
    }
    
    return params;
}

nlohmann::json GrepToolParams::to_json() const {
    nlohmann::json j;
    j["pattern"] = pattern;
    if (path) j["path"] = *path;
    if (include) j["include"] = *include;
    return j;
}

// ============================================================================
// GrepMatch
// ============================================================================

nlohmann::json GrepMatch::to_json() const {
    return {
        {"path", path},
        {"lineNum", line_num},
        {"lineText", line_text}
    };
}

// ============================================================================
// GrepTool
// ============================================================================

std::string GrepTool::description() const {
    return "Search for a regex pattern in file contents. "
           "Returns matching lines with file paths and line numbers. "
           "Results are sorted by file modification time.";
}

nlohmann::json GrepTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"pattern", {
                {"type", "string"},
                {"description", "The regex pattern to search for in file contents"}
            }},
            {"path", {
                {"type", "string"},
                {"description", "The directory to search in. Defaults to the current working directory."}
            }},
            {"include", {
                {"type", "string"},
                {"description", "File pattern to include in the search (e.g., \"*.js\", \"*.{ts,tsx}\")"}
            }}
        }},
        {"required", nlohmann::json::array({"pattern"})}
    };
}

bool GrepTool::validate_input(const nlohmann::json& input) const {
    return input.contains("pattern") && input["pattern"].is_string() &&
           !input["pattern"].get<std::string>().empty();
}

bool GrepTool::matches_include(const std::string& filename, const std::optional<std::string>& pattern) {
    if (!pattern) return true;
    
    // Simple glob matching for include pattern
    const std::string& pat = *pattern;
    
    // Handle *.{ext1,ext2} patterns
    if (pat.find('{') != std::string::npos && pat.find('}') != std::string::npos) {
        // Extract extensions from {ext1,ext2}
        size_t start = pat.find('{');
        size_t end = pat.find('}');
        if (start < end) {
            std::string extensions = pat.substr(start + 1, end - start - 1);
            std::string prefix = pat.substr(0, start);
            
            // Check if filename matches prefix
            if (prefix == "*." || prefix == "*") {
                // Get file extension
                std::string ext = get_extension(filename);
                if (ext.empty()) return false;
                
                // Check each extension
                std::istringstream ss(extensions);
                std::string ext_pattern;
                while (std::getline(ss, ext_pattern, ',')) {
                    if (ext == "." + ext_pattern || ext == ext_pattern) {
                        return true;
                    }
                }
                return false;
            }
        }
    }
    
    // Handle *.ext patterns
    if (pat.substr(0, 2) == "*.") {
        std::string ext = pat.substr(1); // Get .ext
        std::string file_ext = get_extension(filename);
        return file_ext == ext;
    }
    
    // Handle simple patterns
    if (pat == "*") return true;
    
    // Default: check if filename contains pattern (after removing *)
    std::string simplified = pat;
    size_t star_pos;
    while ((star_pos = simplified.find('*')) != std::string::npos) {
        simplified.erase(star_pos, 1);
    }
    return filename.find(simplified) != std::string::npos;
}

std::vector<GrepMatch> GrepTool::search_files(
    const std::string& directory,
    const std::string& pattern,
    const std::optional<std::string>& include_pattern,
    size_t limit,
    std::shared_ptr<std::atomic<bool>> abort_flag
) {
    std::vector<GrepMatch> results;
    std::filesystem::path dir_path(directory);
    
    std::error_code ec;
    if (!std::filesystem::exists(dir_path, ec) || !std::filesystem::is_directory(dir_path, ec)) {
        return results;
    }
    
    // Compile regex pattern
    // Use case-sensitive matching (std::regex::ECMAScript default) — icase was
    // removed because it can allow security-relevant pattern bypasses on
    // case-sensitive file systems and increases backtracking risk.
    std::regex re;
    try {
        re = std::regex(pattern, std::regex::ECMAScript);
    } catch (const std::regex_error& e) {
        // Return empty results for invalid regex
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
            
            // Check include pattern
            std::string filename = entry.path().filename().string();
            if (!matches_include(filename, include_pattern)) {
                continue;
            }
            
            // Skip binary files
            if (is_binary_file(entry.path())) {
                continue;
            }
            
            // Get file modification time
            int64_t mod_time = 0;
            auto ftime = std::filesystem::last_write_time(entry.path(), ec);
            if (!ec) {
                mod_time = file_time_to_unix_sec(ftime);
            }
            
            // Search in file
            std::ifstream file(entry.path());
            if (!file) continue;
            
            std::string line;
            int line_num = 0;
            
            while (std::getline(file, line)) {
                ++line_num;
                
                if (std::regex_search(line, re)) {
                    GrepMatch match;
                    match.path = entry.path().string();
                    match.mod_time = mod_time;
                    match.line_num = line_num;
                    
                    // Truncate long lines
                    if (line.length() > MAX_LINE_LENGTH) {
                        match.line_text = line.substr(0, MAX_LINE_LENGTH) + "...";
                    } else {
                        match.line_text = line;
                    }
                    
                    results.push_back(std::move(match));
                    
                    if (results.size() >= limit) {
                        return results;
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Ignore filesystem errors
    }
    
    // Sort by modification time (newest first)
    std::sort(results.begin(), results.end(), [](const GrepMatch& a, const GrepMatch& b) {
        return a.mod_time > b.mod_time;
    });
    
    return results;
}

ToolResult GrepTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    if (!validate_input(input)) {
        return ToolResult::error(
            "Grep failed",
            "Invalid input: 'pattern' is required and must be a non-empty string"
        );
    }
    
    GrepToolParams params;
    try {
        params = GrepToolParams::from_json(input);
    } catch (const std::exception& e) {
        return ToolResult::error("Grep", fmt::format("Invalid parameters: {}", e.what()));
    }
    
    // Validate regex pattern
    try {
        std::regex test(params.pattern, std::regex::ECMAScript);
    } catch (const std::regex_error& e) {
        return ToolResult::error("Grep", fmt::format("Invalid regex pattern: {}", e.what()));
    }

    // Basic ReDoS mitigation: reject patterns with highly-nested quantifiers that
    // are the hallmark of catastrophic-backtracking patterns (e.g. (a+)+ or (a*)*)
    {
        static const std::regex redos_pattern(R"((\([^)]*[+*][^)]*\)[+*]|\{[0-9,]+\}[+*]|\([^)]*\)\{[0-9]{3,}\}))");
        if (std::regex_search(params.pattern, redos_pattern)) {
            return ToolResult::error("Grep",
                "Pattern rejected: nested quantifiers detected (potential ReDoS risk). "
                "Simplify the pattern.");
        }
        // Also reject patterns that are unreasonably long
        if (params.pattern.size() > 512) {
            return ToolResult::error("Grep",
                "Pattern too long (max 512 characters).");
        }
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
        req.id = fmt::format("grep_{}", params.pattern);
        req.permission = "grep";
        req.patterns = {params.pattern};
        req.tool = name();
        req.metadata = {
            {"pattern", params.pattern},
            {"path", search_dir},
            {"include", params.include.value_or("")}
        };
        
        auto reply = ctx.ask_permission(req);
        if (reply.type == permission::PermissionReply::Type::Reject) {
            return ToolResult::error("Grep", "User rejected permission to search files");
        }
    }
    
    // Search files
    auto matches = search_files(search_dir, params.pattern, params.include, DEFAULT_LIMIT, ctx.abort_flag);
    
    // Build output
    std::vector<std::string> output_lines;
    
    if (matches.empty()) {
        output_lines.push_back("No files found");
    } else {
        size_t total_matches = matches.size();
        bool truncated = total_matches >= DEFAULT_LIMIT;
        
        output_lines.push_back(fmt::format("Found {} matches{}", 
            total_matches, truncated ? fmt::format(" (showing first {})", DEFAULT_LIMIT) : ""));
        
        std::string current_file;
        for (const auto& match : matches) {
            if (current_file != match.path) {
                if (!current_file.empty()) {
                    output_lines.push_back("");
                }
                current_file = match.path;
                output_lines.push_back(fmt::format("{}:", match.path));
            }
            output_lines.push_back(fmt::format("  Line {}: {}", match.line_num, match.line_text));
        }
        
        if (truncated) {
            output_lines.push_back("");
            output_lines.push_back(fmt::format(
                "(Results truncated: showing {} matches. Consider using a more specific path or pattern.)",
                DEFAULT_LIMIT
            ));
        }
    }
    
    nlohmann::json metadata = {
        {"pattern", params.pattern},
        {"path", search_dir},
        {"matches", matches.size()},
        {"truncated", matches.size() >= DEFAULT_LIMIT}
    };
    
    return ToolResult::success(params.pattern, fmt::format("{}", fmt::join(output_lines, "\n")), metadata);
}

} // namespace turbot::core::tool::builtin
