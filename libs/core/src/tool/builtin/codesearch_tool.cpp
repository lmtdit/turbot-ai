#include <turbot/core/tool/builtin/codesearch_tool.hpp>
#include <turbot/core/common/logger.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <unordered_set>

namespace turbot::core::tool::builtin {

namespace fs = std::filesystem;

namespace {

/// Maximum number of match results to return
constexpr int MAX_RESULTS = 50;

/// Maximum context lines around each match
constexpr int CONTEXT_LINES = 3;

/// File extensions considered as source code (unordered_set for O(1) lookup)
static const std::unordered_set<std::string> kCodeExtensions = {
    ".c", ".cpp", ".cc", ".cxx", ".h", ".hpp", ".hxx",
    ".py", ".go", ".rs", ".java", ".kt", ".ts", ".tsx",
    ".js", ".jsx", ".rb", ".php", ".swift", ".cs", ".m",
    ".lua", ".sh", ".bash", ".zsh", ".fish",
    ".cmake", ".md", ".txt", ".json", ".yaml", ".yml", ".toml",
    ".xml", ".html", ".css", ".scss",
};

bool is_code_file(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return kCodeExtensions.count(ext) > 0;
}

struct SearchMatch {
    std::string file_path;
    int line_number;   ///< 1-based
    std::string line_content;
    std::vector<std::string> context_before;
    std::vector<std::string> context_after;
};

/// Read lines from a file, returning them as a vector
std::vector<std::string> read_lines(const fs::path& file) {
    std::ifstream ifs(file);
    if (!ifs.is_open()) return {};
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(ifs, line)) {
        lines.push_back(line);
    }
    return lines;
}

/// Search a single file for a pattern, appending matches to results
void search_file(
    const fs::path& file,
    const std::regex& pattern,
    std::vector<SearchMatch>& results,
    int max_results)
{
    if (static_cast<int>(results.size()) >= max_results) return;

    const auto lines = read_lines(file);
    const int total = static_cast<int>(lines.size());

    for (int idx = 0; idx < total; ++idx) {
        if (static_cast<int>(results.size()) >= max_results) break;
        if (!std::regex_search(lines[idx], pattern)) continue;

        SearchMatch m;
        m.file_path    = file.string();
        m.line_number  = idx + 1;
        m.line_content = lines[idx];

        // Context before
        const int before_start = std::max(0, idx - CONTEXT_LINES);
        for (int b = before_start; b < idx; ++b) {
            m.context_before.push_back(lines[b]);
        }
        // Context after
        const int after_end = std::min(total - 1, idx + CONTEXT_LINES);
        for (int a = idx + 1; a <= after_end; ++a) {
            m.context_after.push_back(lines[a]);
        }

        results.push_back(std::move(m));
    }
}

/// Format results as a structured text block
std::string format_results(const std::vector<SearchMatch>& results, const std::string& query) {
    if (results.empty()) {
        return fmt::format("No matches found for: {}", query);
    }

    std::ostringstream out;
    out << fmt::format("Found {} match(es) for: {}\n\n", results.size(), query);

    for (const auto& m : results) {
        out << fmt::format("{}:{}\n", m.file_path, m.line_number);
        // Context before
        int ctx_line = m.line_number - static_cast<int>(m.context_before.size());
        for (const auto& l : m.context_before) {
            out << fmt::format("  {:5d}  {}\n", ctx_line++, l);
        }
        // Match line (highlighted with >>>)
        out << fmt::format(">{:5d}  {}\n", m.line_number, m.line_content);
        ctx_line = m.line_number + 1;
        for (const auto& l : m.context_after) {
            out << fmt::format("  {:5d}  {}\n", ctx_line++, l);
        }
        out << '\n';
    }

    return out.str();
}

} // anonymous namespace

// ============================================================================
// CodeSearchTool
// ============================================================================

std::string CodeSearchTool::description() const {
    return "Search for code patterns across files in the working directory using regular expressions. "
           "Returns matching lines with surrounding context (3 lines before and after). "
           "Searches source code files by default (C/C++, Python, Go, TypeScript, Rust, Java, etc.). "
           "Results are limited to 50 matches. Use 'grep' for plain text search without code focus.";
}

nlohmann::json CodeSearchTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"query", {
                {"type", "string"},
                {"description", "Regular expression pattern to search for in source code"}
            }},
            {"path", {
                {"type", "string"},
                {"description", "Directory or file path to search within (default: working directory)"}
            }},
            {"filePattern", {
                {"type", "string"},
                {"description", "Glob-like file extension filter, e.g. '*.cpp' or '*.py' (default: all code files)"}
            }},
            {"maxResults", {
                {"type", "integer"},
                {"description", fmt::format("Maximum number of results to return (default: {}, max: {})",
                                            MAX_RESULTS, MAX_RESULTS)},
                {"minimum", 1},
                {"maximum", MAX_RESULTS}
            }}
        }},
        {"required", nlohmann::json::array({"query"})}
    };
}

bool CodeSearchTool::validate_input(const nlohmann::json& input) const {
    return input.contains("query") && input["query"].is_string() &&
           !input["query"].get<std::string>().empty();
}

ToolResult CodeSearchTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    if (!validate_input(input)) {
        return ToolResult::error(
            "CodeSearch failed",
            "Invalid input: 'query' (non-empty string) is required"
        );
    }

    const std::string query = input["query"].get<std::string>();

    // Compile regex
    std::regex pattern;
    try {
        pattern = std::regex(query, std::regex::ECMAScript | std::regex::icase);
    } catch (const std::regex_error& e) {
        return ToolResult::error(
            "CodeSearch failed",
            fmt::format("Invalid regex '{}': {}", query, e.what())
        );
    }

    // Determine search root
    fs::path root;
    if (input.contains("path") && input["path"].is_string()) {
        root = fs::weakly_canonical(input["path"].get<std::string>());
    } else if (!ctx.working_directory.empty()) {
        root = fs::weakly_canonical(ctx.working_directory);
    } else {
        root = fs::current_path();
    }

    // Path-traversal guard: user-supplied path must be under working_directory
    if (input.contains("path") && input["path"].is_string() &&
        !ctx.working_directory.empty()) {
        const fs::path wd = fs::weakly_canonical(ctx.working_directory);
        // Check that root starts with wd (i.e., is wd or a subdirectory)
        const auto root_str = root.string();
        const auto wd_str   = wd.string();
        const bool is_under = (root_str == wd_str) ||
            (root_str.size() > wd_str.size() &&
             root_str.compare(0, wd_str.size(), wd_str) == 0 &&
             root_str[wd_str.size()] == '/');
        if (!is_under) {
            return ToolResult::error(
                "CodeSearch failed",
                fmt::format("Search path '{}' is outside the working directory",
                            root.string())
            );
        }
    }

    std::error_code ec;
    if (!fs::exists(root, ec) || ec) {
        return ToolResult::error(
            "CodeSearch failed",
            fmt::format("Search path does not exist: {}", root.string())
        );
    }

    // Optional file extension filter
    std::string ext_filter;
    if (input.contains("filePattern") && input["filePattern"].is_string()) {
        ext_filter = input["filePattern"].get<std::string>();
        // Strip leading '*' if user wrote '*.cpp'
        const auto star_pos = ext_filter.find('*');
        if (star_pos != std::string::npos) {
            ext_filter = ext_filter.substr(star_pos + 1);
        }
    }

    const int max_results = input.contains("maxResults") && input["maxResults"].is_number_integer()
        ? std::min(input["maxResults"].get<int>(), MAX_RESULTS)
        : MAX_RESULTS;

    TURBOT_LOG_DEBUG("codesearch: query='{}' root='{}'", query, root.string());

    std::vector<SearchMatch> results;

    // Walk directory recursively
    try {
        for (auto it = fs::recursive_directory_iterator(
                 root, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); ++it) {
            if (ec) { ec.clear(); continue; }
            if (ctx.should_abort()) break;
            if (static_cast<int>(results.size()) >= max_results) break;

            const auto& entry = *it;
            if (!entry.is_regular_file(ec) || ec) { ec.clear(); continue; }

            const fs::path& fp = entry.path();

            // Skip hidden directories (e.g. .git)
            if (fp.filename().string().rfind('.', 0) == 0) continue;
            const auto parent = fp.parent_path();
            bool skip = false;
            for (const auto& comp : parent) {
                if (comp.string().rfind('.', 0) == 0) { skip = true; break; }
            }
            if (skip) continue;

            // Extension filter
            if (!ext_filter.empty()) {
                std::string fext = fp.extension().string();
                std::transform(fext.begin(), fext.end(), fext.begin(), ::tolower);
                std::string lower_filter = ext_filter;
                std::transform(lower_filter.begin(), lower_filter.end(), lower_filter.begin(), ::tolower);
                if (fext != lower_filter) continue;
            } else if (!is_code_file(fp)) {
                continue;
            }

            search_file(fp, pattern, results, max_results);
        }
    } catch (const std::exception& e) {
        if (results.empty()) {
            return ToolResult::error(
                "CodeSearch failed",
                fmt::format("Error scanning '{}': {}", root.string(), e.what())
            );
        }
        // Partial results — still return them with a warning
    }

    const std::string output = format_results(results, query);
    return ToolResult::success(
        fmt::format("CodeSearch: {} match(es)", results.size()),
        output,
        {{"query", query}, {"matchCount", static_cast<int>(results.size())}}
    );
}

} // namespace turbot::core::tool::builtin
