#include <turbot/core/tool/builtin/edit_tool.hpp>
#include <turbot/core/permission/permission.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/string_utils.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

namespace turbot::core::tool::builtin {

namespace {

/// Maximum file size for editing (10MB)
constexpr size_t MAX_FILE_SIZE = 10 * 1024 * 1024;

// 使用 turbot::utils 命名空间的字符串工具函数
using turbot::utils::split_lines;
using turbot::utils::join_lines;
using turbot::utils::trim;
using turbot::utils::normalize_line_endings;

} // anonymous namespace

// ============================================================================
// EditToolParams
// ============================================================================

EditToolParams EditToolParams::from_json(const nlohmann::json& j) {
    EditToolParams params;
    params.file_path = j.at("filePath").get<std::string>();
    params.old_string = j.at("oldString").get<std::string>();
    params.new_string = j.at("newString").get<std::string>();
    
    if (j.contains("replaceAll") && !j["replaceAll"].is_null()) {
        params.replace_all = j["replaceAll"].get<bool>();
    }
    
    return params;
}

nlohmann::json EditToolParams::to_json() const {
    nlohmann::json j;
    j["filePath"] = file_path;
    j["oldString"] = old_string;
    j["newString"] = new_string;
    j["replaceAll"] = replace_all;
    return j;
}

// ============================================================================
// EditTool
// ============================================================================

std::string EditTool::description() const {
    return "Perform string replacements in a file. "
           "Supports multiple matching strategies for flexible text replacement. "
           "Can replace single or all occurrences. "
           "Shows a diff of changes after successful edit.";
}

nlohmann::json EditTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"filePath", {
                {"type", "string"},
                {"description", "The absolute path to the file to modify"}
            }},
            {"oldString", {
                {"type", "string"},
                {"description", "The text to replace"}
            }},
            {"newString", {
                {"type", "string"},
                {"description", "The text to replace it with (must be different from oldString)"}
            }},
            {"replaceAll", {
                {"type", "boolean"},
                {"description", "Replace all occurrences of oldString (default false)"},
                {"default", false}
            }}
        }},
        {"required", nlohmann::json::array({"filePath", "oldString", "newString"})}
    };
}

bool EditTool::validate_input(const nlohmann::json& input) const {
    if (!input.contains("filePath") || !input["filePath"].is_string()) {
        return false;
    }
    if (!input.contains("oldString") || !input["oldString"].is_string()) {
        return false;
    }
    if (!input.contains("newString") || !input["newString"].is_string()) {
        return false;
    }
    return input["oldString"] != input["newString"];
}

int EditTool::levenshtein_distance(std::string_view a, std::string_view b) {
    // Add length limit to prevent memory exhaustion
    constexpr size_t MAX_LENGTH = 10000;
    if (a.empty() || b.empty()) {
        return static_cast<int>(std::max(a.length(), b.length()));
    }
    if (a.size() > MAX_LENGTH || b.size() > MAX_LENGTH) {
        // For very long strings, use a simpler approximation
        return static_cast<int>(std::abs(static_cast<long>(a.size()) - static_cast<long>(b.size())));
    }
    
    std::vector<int> matrix((a.length() + 1) * (b.length() + 1));
    auto get = [&](size_t i, size_t j) -> int& {
        return matrix[i * (b.length() + 1) + j];
    };
    
    for (size_t i = 0; i <= a.length(); ++i) {
        get(i, 0) = static_cast<int>(i);
    }
    for (size_t j = 0; j <= b.length(); ++j) {
        get(0, j) = static_cast<int>(j);
    }
    
    for (size_t i = 1; i <= a.length(); ++i) {
        for (size_t j = 1; j <= b.length(); ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            get(i, j) = std::min({
                get(i - 1, j) + 1,
                get(i, j - 1) + 1,
                get(i - 1, j - 1) + cost
            });
        }
    }
    
    return get(a.length(), b.length());
}

std::vector<std::string> EditTool::simple_replacer(const std::string& content, const std::string& find) {
    std::vector<std::string> results;
    if (content.find(find) != std::string::npos) {
        results.push_back(find);
    }
    return results;
}

std::vector<std::string> EditTool::line_trimmed_replacer(const std::string& content, const std::string& find) {
    std::vector<std::string> results;
    auto original_lines = split_lines(content);
    auto search_lines = split_lines(find);
    
    if (search_lines.empty()) return results;
    
    // Remove trailing empty line if present
    if (search_lines.back().empty()) {
        search_lines.pop_back();
    }
    
    for (size_t i = 0; i + search_lines.size() <= original_lines.size(); ++i) {
        bool matches = true;
        
        for (size_t j = 0; j < search_lines.size(); ++j) {
            if (trim(original_lines[i + j]) != trim(search_lines[j])) {
                matches = false;
                break;
            }
        }
        
        if (matches) {
            // Calculate the match boundaries
            size_t start = 0;
            for (size_t k = 0; k < i; ++k) {
                start += original_lines[k].length() + 1;
            }
            
            size_t end = start;
            for (size_t k = 0; k < search_lines.size(); ++k) {
                end += original_lines[i + k].length();
                if (k < search_lines.size() - 1) {
                    end += 1;
                }
            }
            
            results.push_back(content.substr(start, end - start));
        }
    }
    
    return results;
}

std::vector<std::string> EditTool::block_anchor_replacer(const std::string& content, const std::string& find) {
    std::vector<std::string> results;
    auto original_lines = split_lines(content);
    auto search_lines = split_lines(find);
    
    if (search_lines.size() < 3) return results;
    
    // Remove trailing empty line if present
    if (search_lines.back().empty()) {
        search_lines.pop_back();
    }
    
    std::string first_line_search = trim(search_lines[0]);
    std::string last_line_search = trim(search_lines.back());
    size_t search_block_size = search_lines.size();
    
    // Find candidates where both anchors match
    struct Candidate {
        size_t start_line;
        size_t end_line;
    };
    std::vector<Candidate> candidates;
    
    for (size_t i = 0; i < original_lines.size(); ++i) {
        if (trim(original_lines[i]) != first_line_search) continue;
        
        for (size_t j = i + 2; j < original_lines.size(); ++j) {
            if (trim(original_lines[j]) == last_line_search) {
                candidates.push_back({i, j});
                break;
            }
        }
    }
    
    if (candidates.empty()) return results;
    
    const double single_threshold = 0.0;
    const double multiple_threshold = 0.3;
    
    auto calculate_similarity = [&](size_t start, size_t end) -> double {
        size_t actual_block_size = end - start + 1;
        size_t lines_to_check = std::min(search_block_size - 2, actual_block_size - 2);
        
        if (lines_to_check == 0) return 1.0;
        
        double similarity = 0;
        for (size_t j = 1; j < search_block_size - 1 && j < actual_block_size - 1; ++j) {
            std::string original_line = trim(original_lines[start + j]);
            std::string search_line = trim(search_lines[j]);
            size_t max_len = std::max(original_line.length(), search_line.length());
            if (max_len == 0) continue;
            
            int distance = levenshtein_distance(original_line, search_line);
            similarity += (1.0 - static_cast<double>(distance) / max_len) / lines_to_check;
        }
        return similarity;
    };
    
    auto get_match = [&](size_t start, size_t end) -> std::string {
        size_t match_start = 0;
        for (size_t k = 0; k < start; ++k) {
            match_start += original_lines[k].length() + 1;
        }
        
        size_t match_end = match_start;
        for (size_t k = start; k <= end; ++k) {
            match_end += original_lines[k].length();
            if (k < end) {
                match_end += 1;
            }
        }
        
        return content.substr(match_start, match_end - match_start);
    };
    
    if (candidates.size() == 1) {
        auto& c = candidates[0];
        double similarity = calculate_similarity(c.start_line, c.end_line);
        if (similarity >= single_threshold) {
            results.push_back(get_match(c.start_line, c.end_line));
        }
        return results;
    }
    
    // Multiple candidates - find best match
    double max_similarity = -1;
    size_t best_idx = 0;
    
    for (size_t idx = 0; idx < candidates.size(); ++idx) {
        double similarity = calculate_similarity(candidates[idx].start_line, candidates[idx].end_line);
        if (similarity > max_similarity) {
            max_similarity = similarity;
            best_idx = idx;
        }
    }
    
    if (max_similarity >= multiple_threshold) {
        results.push_back(get_match(candidates[best_idx].start_line, candidates[best_idx].end_line));
    }
    
    return results;
}

std::vector<std::string> EditTool::whitespace_normalized_replacer(const std::string& content, const std::string& find) {
    std::vector<std::string> results;
    
    auto normalize_whitespace = [](const std::string& text) -> std::string {
        std::string result;
        bool in_whitespace = false;
        for (char c : text) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (!in_whitespace) {
                    result += ' ';
                    in_whitespace = true;
                }
            } else {
                result += c;
                in_whitespace = false;
            }
        }
        // Trim
        while (!result.empty() && std::isspace(static_cast<unsigned char>(result.back()))) {
            result.pop_back();
        }
        size_t start = result.find_first_not_of(" \t");
        if (start != std::string::npos) {
            result = result.substr(start);
        }
        return result;
    };
    
    std::string normalized_find = normalize_whitespace(find);
    
    // Check single line matches
    auto lines = split_lines(content);
    for (const auto& line : lines) {
        if (normalize_whitespace(line) == normalized_find) {
            results.push_back(line);
        }
    }
    
    // Check multi-line matches
    auto find_lines = split_lines(find);
    if (find_lines.size() > 1) {
        for (size_t i = 0; i + find_lines.size() <= lines.size(); ++i) {
            std::string block;
            for (size_t j = 0; j < find_lines.size(); ++j) {
                if (j > 0) block += '\n';
                block += lines[i + j];
            }
            if (normalize_whitespace(block) == normalized_find) {
                results.push_back(block);
            }
        }
    }
    
    return results;
}

std::vector<std::string> EditTool::indentation_flexible_replacer(const std::string& content, const std::string& find) {
    std::vector<std::string> results;
    
    auto remove_indentation = [](const std::string& text) -> std::string {
        auto lines = split_lines(text);
        std::vector<std::string> non_empty_lines;
        for (const auto& line : lines) {
            if (!trim(line).empty()) {
                non_empty_lines.push_back(line);
            }
        }
        
        if (non_empty_lines.empty()) return text;
        
        size_t min_indent = SIZE_MAX;
        for (const auto& line : non_empty_lines) {
            size_t indent = 0;
            for (char c : line) {
                if (c == ' ' || c == '\t') {
                    ++indent;
                } else {
                    break;
                }
            }
            min_indent = std::min(min_indent, indent);
        }
        
        if (min_indent == 0 || min_indent == SIZE_MAX) return text;
        
        for (auto& line : lines) {
            if (!trim(line).empty() && line.length() >= min_indent) {
                line = line.substr(min_indent);
            }
        }
        
        return join_lines(lines);
    };
    
    std::string normalized_find = remove_indentation(find);
    auto content_lines = split_lines(content);
    auto find_lines = split_lines(find);
    
    for (size_t i = 0; i + find_lines.size() <= content_lines.size(); ++i) {
        std::string block;
        for (size_t j = 0; j < find_lines.size(); ++j) {
            if (j > 0) block += '\n';
            block += content_lines[i + j];
        }
        if (remove_indentation(block) == normalized_find) {
            results.push_back(block);
        }
    }
    
    return results;
}

std::vector<std::string> EditTool::multi_occurrence_replacer(const std::string& content, const std::string& find) {
    std::vector<std::string> results;
    size_t start = 0;
    
    while (true) {
        size_t pos = content.find(find, start);
        if (pos == std::string::npos) break;
        results.push_back(find);
        start = pos + find.length();
    }
    
    return results;
}

std::string EditTool::create_diff(
    const std::string& file_path,
    const std::string& old_content,
    const std::string& new_content
) {
    // Delegate to the shared utility to avoid duplicate diff logic
    return turbot::utils::create_diff(file_path, old_content, new_content);
}

ReplacementResult EditTool::replace(
    const std::string& content,
    const std::string& old_string,
    const std::string& new_string,
    bool replace_all
) {
    ReplacementResult result;
    
    if (old_string == new_string) {
        result.error = "No changes to apply: oldString and newString are identical.";
        return result;
    }
    
    // Try each replacer in order
    std::vector<std::function<std::vector<std::string>(const std::string&, const std::string&)>> replacers = {
        simple_replacer,
        line_trimmed_replacer,
        block_anchor_replacer,
        whitespace_normalized_replacer,
        indentation_flexible_replacer,
        multi_occurrence_replacer
    };
    
    for (const auto& replacer : replacers) {
        auto searches = replacer(content, old_string);
        
        for (const auto& search : searches) {
            size_t index = content.find(search);
            if (index == std::string::npos) continue;
            
            result.found = true;
            
            if (replace_all) {
                result.content = content;
                size_t pos = 0;
                while ((pos = result.content.find(search, pos)) != std::string::npos) {
                    result.content.replace(pos, search.length(), new_string);
                    pos += new_string.length();
                }
                return result;
            }
            
            size_t last_index = content.rfind(search);
            if (index != last_index) {
                result.multiple_matches = true;
                continue;
            }
            
            result.content = content.substr(0, index) + new_string + content.substr(index + search.length());
            return result;
        }
    }
    
    if (!result.found) {
        result.error = "Could not find oldString in the file. It must match exactly, including whitespace, indentation, and line endings.";
    } else if (result.multiple_matches) {
        result.error = "Found multiple matches for oldString. Provide more surrounding context to make the match unique.";
    }
    
    return result;
}

ToolResult EditTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    if (!validate_input(input)) {
        return ToolResult::error(
            "Edit failed",
            "Invalid input: 'filePath', 'oldString', and 'newString' are required. 'oldString' and 'newString' must be different."
        );
    }
    
    EditToolParams params;
    try {
        params = EditToolParams::from_json(input);
    } catch (const std::exception& e) {
        return ToolResult::error("Edit", fmt::format("Invalid parameters: {}", e.what()));
    }
    
    // Resolve file path
    std::filesystem::path file_path = params.file_path;
    if (!file_path.is_absolute()) {
        file_path = std::filesystem::path(ctx.working_directory) / file_path;
    }
    
    std::string file_path_str = file_path.string();
    
    // Check if file exists
    std::error_code ec;
    if (!std::filesystem::exists(file_path, ec)) {
        // If old_string is empty, create new file
        if (params.old_string.empty()) {
            // Validate path is not a symlink target outside workspace
            // (basic protection against path traversal)
            
            // Request permission
            if (ctx.ask_permission) {
                permission::PermissionRequest req;
                req.id = fmt::format("edit_{}", file_path_str);
                req.permission = "edit";
                req.patterns = {file_path_str};
                req.tool = name();
                
                auto reply = ctx.ask_permission(req);
                if (reply.type == permission::PermissionReply::Type::Reject) {
                    return ToolResult::error("Edit", "User rejected permission to create file");
                }
            }
            
            // Validate new_string size
            if (params.new_string.size() > MAX_FILE_SIZE) {
                return ToolResult::error("Edit", 
                    fmt::format("Content too large: {} bytes (max: {} bytes)", 
                                params.new_string.size(), MAX_FILE_SIZE));
            }
            
            // Create parent directories if needed
            std::filesystem::create_directories(file_path.parent_path(), ec);
            
            // Write new file atomically using temp file + rename
            std::filesystem::path temp_path = file_path;
            temp_path += ".tmp." + std::to_string(std::random_device{}());
            
            std::ofstream ofs(temp_path);
            if (!ofs) {
                return ToolResult::error("Edit", fmt::format("Failed to create temp file: {}", temp_path.string()));
            }
            ofs << params.new_string;
            ofs.flush();
            if (!ofs) {
                std::error_code remove_ec;
                std::filesystem::remove(temp_path, remove_ec);
                return ToolResult::error("Edit", fmt::format("Failed to write to file: {}", file_path_str));
            }
            ofs.close();
            
            // Rename temp file to target (atomic on most filesystems)
            std::error_code rename_ec;
            std::filesystem::rename(temp_path, file_path, rename_ec);
            if (rename_ec) {
                std::error_code remove_ec;
                std::filesystem::remove(temp_path, remove_ec);
                return ToolResult::error("Edit", fmt::format("Failed to rename temp file: {}", rename_ec.message()));
            }
            
            nlohmann::json metadata = {
                {"filepath", file_path_str},
                {"created", true}
            };
            
            return ToolResult::success(
                file_path.filename().string(),
                fmt::format("Created new file: {}", file_path_str),
                metadata
            );
        }
        
        return ToolResult::error("Edit", fmt::format("File not found: {}", file_path_str));
    }
    
    // Check if it's a symbolic link
    if (std::filesystem::is_symlink(file_path, ec)) {
        // Resolve the symlink target
        auto resolved = std::filesystem::canonical(file_path, ec);
        if (ec) {
            return ToolResult::error("Edit", fmt::format("Failed to resolve symlink: {}", file_path_str));
        }
        TURBOT_LOG_INFO("Editing symlink {} -> {}", file_path_str, resolved.string());
        file_path = resolved;
        file_path_str = file_path.string();
    }
    
    // Check if it's a directory
    if (std::filesystem::is_directory(file_path, ec)) {
        return ToolResult::error("Edit", fmt::format("Path is a directory, not a file: {}", file_path_str));
    }
    
    // Check file size before reading
    auto file_size = std::filesystem::file_size(file_path, ec);
    if (ec || file_size == static_cast<std::uintmax_t>(-1)) {
        return ToolResult::error("Edit", fmt::format("Failed to get file size: {}", file_path_str));
    }
    if (file_size > MAX_FILE_SIZE) {
        return ToolResult::error("Edit", 
            fmt::format("File too large: {} bytes (max: {} bytes)", file_size, MAX_FILE_SIZE));
    }
    
    // Read file content
    std::ifstream ifs(file_path);
    if (!ifs) {
        return ToolResult::error("Edit", fmt::format("Failed to open file: {}", file_path_str));
    }
    
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string content = buffer.str();
    ifs.close();
    
    // Perform replacement
    auto replace_result = replace(content, params.old_string, params.new_string, params.replace_all);
    
    if (!replace_result.error.empty()) {
        return ToolResult::error("Edit", replace_result.error);
    }
    
    // Create diff for permission request
    std::string diff = create_diff(file_path_str, content, replace_result.content);
    
    // Request permission
    if (ctx.ask_permission) {
        permission::PermissionRequest req;
        req.id = fmt::format("edit_{}", file_path_str);
        req.permission = "edit";
        req.patterns = {file_path_str};
        req.tool = name();
        req.metadata = {
            {"filepath", file_path_str},
            {"diff", diff}
        };
        
        auto reply = ctx.ask_permission(req);
        if (reply.type == permission::PermissionReply::Type::Reject) {
            return ToolResult::error("Edit", "User rejected permission to edit file");
        }
    }
    
    // Write modified content atomically
    std::filesystem::path temp_path = file_path;
    temp_path += ".tmp." + std::to_string(std::random_device{}());
    
    std::ofstream ofs(temp_path);
    if (!ofs) {
        return ToolResult::error("Edit", fmt::format("Failed to create temp file: {}", temp_path.string()));
    }
    ofs << replace_result.content;
    ofs.flush();
    if (!ofs) {
        std::error_code remove_ec;
        std::filesystem::remove(temp_path, remove_ec);
        return ToolResult::error("Edit", fmt::format("Failed to write to file: {}", file_path_str));
    }
    ofs.close();
    
    // Rename temp file to target (atomic on most filesystems)
    std::error_code rename_ec;
    std::filesystem::rename(temp_path, file_path, rename_ec);
    if (rename_ec) {
        std::error_code remove_ec;
        std::filesystem::remove(temp_path, remove_ec);
        return ToolResult::error("Edit", fmt::format("Failed to rename temp file: {}", rename_ec.message()));
    }
    
    nlohmann::json metadata = {
        {"filepath", file_path_str},
        {"diff", diff},
        {"replaceAll", params.replace_all}
    };
    
    return ToolResult::success(
        file_path.filename().string(),
        fmt::format("Edit applied successfully to: {}", file_path_str),
        metadata
    );
}

} // namespace turbot::core::tool::builtin
