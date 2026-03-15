#include <turbot/core/tool/builtin/apply_patch_tool.hpp>
#include <turbot/core/permission/permission.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/string_utils.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <random>
#include <regex>

namespace turbot::core::tool::builtin {

namespace {

using turbot::utils::split_lines;
using turbot::utils::join_lines;
using turbot::utils::join;
using turbot::utils::trim;
using turbot::utils::trim_right;

} // anonymous namespace

// ============================================================================
// ApplyPatchToolParams
// ============================================================================

ApplyPatchToolParams ApplyPatchToolParams::from_json(const nlohmann::json& j) {
    ApplyPatchToolParams params;
    params.patch_text = j.at("patchText").get<std::string>();
    return params;
}

nlohmann::json ApplyPatchToolParams::to_json() const {
    nlohmann::json j;
    j["patchText"] = patch_text;
    return j;
}

// ============================================================================
// ApplyPatchTool
// ============================================================================

std::string ApplyPatchTool::description() const {
    return "Apply a unified diff patch to files. "
           "Supports adding new files, updating existing files, deleting files, and moving files. "
           "The patch format uses special markers: *** Begin Patch, *** End Patch, "
           "*** Add File:, *** Delete File:, *** Update File:, *** Move to:, @@ for chunks, "
           "and +/- prefixes for line changes.";
}

nlohmann::json ApplyPatchTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"patchText", {
                {"type", "string"},
                {"description", "The full patch text that describes all changes to be made"}
            }}
        }},
        {"required", nlohmann::json::array({"patchText"})}
    };
}

bool ApplyPatchTool::validate_input(const nlohmann::json& input) const {
    return input.contains("patchText") && input["patchText"].is_string() && !input["patchText"].get<std::string>().empty();
}

std::string ApplyPatchTool::strip_heredoc(const std::string& input) {
    // Match heredoc patterns like: cat <<'EOF'\n...\nEOF or <<EOF\n...\nEOF
    static const std::regex heredoc_pattern(R"(^(?:cat\s+)?<<['"]?(\w+)['"]?\s*\n([\s\S]*?)\n\1\s*$)");
    std::smatch match;
    if (std::regex_match(input, match, heredoc_pattern)) {
        return match[2].str();
    }
    return input;
}

std::string ApplyPatchTool::normalize_unicode(const std::string& str) {
    std::string result = str;
    // Normalize various Unicode punctuation to ASCII equivalents
    static const std::vector<std::pair<std::string, std::string>> replacements = {
        {"\u2018", "'"}, {"\u2019", "'"}, {"\u201A", "'"}, {"\u201B", "'"},  // single quotes
        {"\u201C", "\""}, {"\u201D", "\""}, {"\u201E", "\""}, {"\u201F", "\""},  // double quotes
        {"\u2010", "-"}, {"\u2011", "-"}, {"\u2012", "-"}, {"\u2013", "-"}, 
        {"\u2014", "-"}, {"\u2015", "-"},  // dashes
        {"\u2026", "..."},  // ellipsis
        {"\u00A0", " "}  // non-breaking space
    };
    
    for (const auto& [from, to] : replacements) {
        size_t pos = 0;
        while ((pos = result.find(from, pos)) != std::string::npos) {
            result.replace(pos, from.length(), to);
            pos += to.length();
        }
    }
    return result;
}

auto ApplyPatchTool::parse_patch_header(const std::vector<std::string>& lines, size_t start_idx)
    -> std::optional<std::tuple<std::string, std::optional<std::string>, size_t, std::string>> {
    
    if (start_idx >= lines.size()) {
        return std::nullopt;
    }
    
    const std::string& line = lines[start_idx];
    
    if (line.rfind("*** Add File:", 0) == 0) {
        std::string file_path = trim(line.substr(13));
        if (file_path.empty()) return std::nullopt;
        return std::make_tuple(file_path, std::nullopt, start_idx + 1, std::string("add"));
    }
    
    if (line.rfind("*** Delete File:", 0) == 0) {
        std::string file_path = trim(line.substr(16));
        if (file_path.empty()) return std::nullopt;
        return std::make_tuple(file_path, std::nullopt, start_idx + 1, std::string("delete"));
    }
    
    if (line.rfind("*** Update File:", 0) == 0) {
        std::string file_path = trim(line.substr(16));
        if (file_path.empty()) return std::nullopt;
        
        std::optional<std::string> move_path;
        size_t next_idx = start_idx + 1;
        
        // Check for move directive
        if (next_idx < lines.size() && lines[next_idx].rfind("*** Move to:", 0) == 0) {
            move_path = trim(lines[next_idx].substr(12));
            next_idx++;
        }
        
        return std::make_tuple(file_path, move_path, next_idx, std::string("update"));
    }
    
    return std::nullopt;
}

auto ApplyPatchTool::parse_update_chunks(const std::vector<std::string>& lines, size_t start_idx)
    -> std::pair<std::vector<UpdateFileChunk>, size_t> {
    
    std::vector<UpdateFileChunk> chunks;
    size_t i = start_idx;
    
    while (i < lines.size() && lines[i].rfind("***", 0) != 0) {
        if (lines[i].rfind("@@", 0) == 0) {
            // Parse context line
            std::string context_line = lines[i].substr(2);
            context_line = trim(context_line);
            i++;
            
            std::vector<std::string> old_lines;
            std::vector<std::string> new_lines;
            bool is_end_of_file = false;
            
            // Parse change lines
            while (i < lines.size() && lines[i].rfind("@@", 0) != 0 && lines[i].rfind("***", 0) != 0) {
                const std::string& change_line = lines[i];
                
                if (change_line == "*** End of File") {
                    is_end_of_file = true;
                    i++;
                    break;
                }
                
                if (!change_line.empty()) {
                    if (change_line[0] == ' ') {
                        // Keep line - appears in both old and new
                        std::string content = change_line.substr(1);
                        old_lines.push_back(content);
                        new_lines.push_back(content);
                    } else if (change_line[0] == '-') {
                        // Remove line - only in old
                        old_lines.push_back(change_line.substr(1));
                    } else if (change_line[0] == '+') {
                        // Add line - only in new
                        new_lines.push_back(change_line.substr(1));
                    }
                }
                i++;
            }
            
            UpdateFileChunk chunk;
            chunk.old_lines = std::move(old_lines);
            chunk.new_lines = std::move(new_lines);
            if (!context_line.empty()) {
                chunk.change_context = context_line;
            }
            chunk.is_end_of_file = is_end_of_file;
            chunks.push_back(std::move(chunk));
        } else {
            i++;
        }
    }
    
    return {chunks, i};
}

auto ApplyPatchTool::parse_add_content(const std::vector<std::string>& lines, size_t start_idx)
    -> std::pair<std::string, size_t> {
    
    std::string content;
    size_t i = start_idx;
    
    while (i < lines.size() && lines[i].rfind("***", 0) != 0) {
        if (lines[i].rfind("+", 0) == 0) {
            content += lines[i].substr(1) + "\n";
        }
        i++;
    }
    
    // Remove trailing newline
    if (!content.empty() && content.back() == '\n') {
        content.pop_back();
    }
    
    return {content, i};
}

int ApplyPatchTool::seek_sequence(
    const std::vector<std::string>& lines,
    const std::vector<std::string>& pattern,
    size_t start_index,
    bool eof
) {
    if (pattern.empty()) return -1;
    
    // Helper function for trying matches with different comparators
    auto try_match = [&](const std::function<bool(const std::string&, const std::string&)>& compare) -> int {
        // If EOF anchor, try matching from end of file first
        if (eof) {
            int from_end = static_cast<int>(lines.size()) - static_cast<int>(pattern.size());
            if (from_end >= static_cast<int>(start_index)) {
                bool matches = true;
                for (size_t j = 0; j < pattern.size(); j++) {
                    if (!compare(lines[from_end + j], pattern[j])) {
                        matches = false;
                        break;
                    }
                }
                if (matches) return from_end;
            }
        }
        
        // Forward search from startIndex
        for (size_t i = start_index; i + pattern.size() <= lines.size(); i++) {
            bool matches = true;
            for (size_t j = 0; j < pattern.size(); j++) {
                if (!compare(lines[i + j], pattern[j])) {
                    matches = false;
                    break;
                }
            }
            if (matches) return static_cast<int>(i);
        }
        
        return -1;
    };
    
    // Pass 1: exact match
    int result = try_match([](const std::string& a, const std::string& b) { return a == b; });
    if (result != -1) return result;
    
    // Pass 2: rstrip (trim trailing whitespace)
    result = try_match([](const std::string& a, const std::string& b) { 
        return trim_right(a) == trim_right(b); 
    });
    if (result != -1) return result;
    
    // Pass 3: trim (both ends)
    result = try_match([](const std::string& a, const std::string& b) { 
        return trim(a) == trim(b); 
    });
    if (result != -1) return result;
    
    // Pass 4: normalized (Unicode punctuation to ASCII)
    result = try_match([](const std::string& a, const std::string& b) { 
        return normalize_unicode(trim(a)) == normalize_unicode(trim(b)); 
    });
    
    return result;
}

auto ApplyPatchTool::compute_replacements(
    const std::vector<std::string>& original_lines,
    const std::string& file_path,
    const std::vector<UpdateFileChunk>& chunks
) -> std::vector<std::tuple<size_t, size_t, std::vector<std::string>>> {
    
    std::vector<std::tuple<size_t, size_t, std::vector<std::string>>> replacements;
    size_t line_index = 0;
    
    for (const auto& chunk : chunks) {
        // Handle context-based seeking
        if (chunk.change_context.has_value()) {
            int context_idx = seek_sequence(original_lines, {chunk.change_context.value()}, line_index);
            if (context_idx == -1) {
                throw std::runtime_error(fmt::format("Failed to find context '{}' in {}", 
                    chunk.change_context.value(), file_path));
            }
            line_index = static_cast<size_t>(context_idx) + 1;
        }
        
        // Handle pure addition (no old lines)
        if (chunk.old_lines.empty()) {
            size_t insertion_idx = original_lines.size();
            replacements.push_back(std::make_tuple(insertion_idx, size_t(0), chunk.new_lines));
            continue;
        }
        
        // Try to match old lines in the file
        std::vector<std::string> pattern = chunk.old_lines;
        std::vector<std::string> new_slice = chunk.new_lines;
        int found = seek_sequence(original_lines, pattern, line_index, chunk.is_end_of_file);
        
        // Retry without trailing empty line if not found
        if (found == -1 && !pattern.empty() && pattern.back().empty()) {
            pattern.pop_back();
            if (!new_slice.empty() && new_slice.back().empty()) {
                new_slice.pop_back();
            }
            found = seek_sequence(original_lines, pattern, line_index, chunk.is_end_of_file);
        }
        
        if (found != -1) {
            replacements.push_back(std::make_tuple(
                static_cast<size_t>(found), 
                pattern.size(), 
                new_slice
            ));
            line_index = static_cast<size_t>(found) + pattern.size();
        } else {
            std::string old_lines_str;
            for (const auto& line : chunk.old_lines) {
                old_lines_str += line + "\n";
            }
            throw std::runtime_error(fmt::format("Failed to find expected lines in {}:\n{}", 
                file_path, old_lines_str));
        }
    }
    
    // Sort replacements by index to apply in order
    std::sort(replacements.begin(), replacements.end(), 
        [](const auto& a, const auto& b) { return std::get<0>(a) < std::get<0>(b); });
    
    return replacements;
}

auto ApplyPatchTool::apply_replacements(
    const std::vector<std::string>& lines,
    const std::vector<std::tuple<size_t, size_t, std::vector<std::string>>>& replacements
) -> std::vector<std::string> {
    
    std::vector<std::string> result = lines;
    
    // Apply replacements in reverse order to avoid index shifting
    for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
        const auto& [start_idx, old_len, new_segment] = *it;
        
        // Remove old lines
        if (start_idx < result.size()) {
            result.erase(result.begin() + start_idx, 
                        result.begin() + std::min(start_idx + old_len, result.size()));
        }
        
        // Insert new lines
        for (size_t j = 0; j < new_segment.size(); j++) {
            result.insert(result.begin() + start_idx + j, new_segment[j]);
        }
    }
    
    return result;
}

std::string ApplyPatchTool::create_unified_diff(
    const std::string& file_path,
    const std::string& old_content,
    const std::string& new_content
) {
    // Delegate to the shared utility
    return turbot::utils::create_diff(file_path, old_content, new_content);
}

std::pair<int, int> ApplyPatchTool::count_diff_changes(
    const std::string& old_content,
    const std::string& new_content
) {
    auto old_lines = split_lines(old_content);
    auto new_lines = split_lines(new_content);
    
    // Simple LCS-based counting
    int additions = 0;
    int deletions = 0;
    
    // Remove trailing empty elements for consistent counting
    if (!old_lines.empty() && old_lines.back().empty()) old_lines.pop_back();
    if (!new_lines.empty() && new_lines.back().empty()) new_lines.pop_back();
    
    // Simple diff: count added and removed lines
    size_t max_len = std::max(old_lines.size(), new_lines.size());
    for (size_t i = 0; i < max_len; i++) {
        if (i >= old_lines.size()) {
            additions++;
        } else if (i >= new_lines.size()) {
            deletions++;
        } else if (old_lines[i] != new_lines[i]) {
            additions++;
            deletions++;
        }
    }
    
    return {additions, deletions};
}

ParseResult ApplyPatchTool::parse_patch(const std::string& patch_text) {
    ParseResult result;
    
    std::string cleaned = strip_heredoc(patch_text);
    
    // Normalize line endings
    cleaned = turbot::utils::normalize_line_endings(cleaned);
    trim(cleaned);
    
    auto lines = split_lines(cleaned);
    std::vector<Hunk> hunks;
    
    // Look for Begin/End patch markers
    const std::string begin_marker = "*** Begin Patch";
    const std::string end_marker = "*** End Patch";
    
    size_t begin_idx = lines.size();
    size_t end_idx = lines.size();
    
    for (size_t i = 0; i < lines.size(); i++) {
        if (trim(lines[i]) == begin_marker) begin_idx = i;
        if (trim(lines[i]) == end_marker) end_idx = i;
    }
    
    if (begin_idx == lines.size() || end_idx == lines.size() || begin_idx >= end_idx) {
        result.error = "Invalid patch format: missing Begin/End markers";
        return result;
    }
    
    // Parse content between markers
    size_t i = begin_idx + 1;
    
    while (i < end_idx) {
        auto header = parse_patch_header(lines, i);
        if (!header) {
            i++;
            continue;
        }
        
        auto [file_path, move_path, next_idx, type] = *header;
        
        if (type == "add") {
            auto [content, next] = parse_add_content(lines, next_idx);
            hunks.push_back(AddHunk{file_path, content});
            i = next;
        } else if (type == "delete") {
            hunks.push_back(DeleteHunk{file_path});
            i = next_idx;
        } else if (type == "update") {
            auto [chunks, next] = parse_update_chunks(lines, next_idx);
            hunks.push_back(UpdateHunk{file_path, move_path, std::move(chunks)});
            i = next;
        } else {
            i++;
        }
    }
    
    result.hunks = std::move(hunks);
    return result;
}

std::pair<std::string, std::string> ApplyPatchTool::derive_new_contents_from_chunks(
    const std::string& file_path,
    const std::vector<UpdateFileChunk>& chunks
) {
    // Read original file content
    std::ifstream ifs(file_path);
    if (!ifs) {
        throw std::runtime_error(fmt::format("Failed to read file: {}", file_path));
    }
    
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string original_content = buffer.str();
    ifs.close();
    
    auto original_lines = split_lines(original_content);
    
    // Drop trailing empty element for consistent line counting
    if (!original_lines.empty() && original_lines.back().empty()) {
        original_lines.pop_back();
    }
    
    auto replacements = compute_replacements(original_lines, file_path, chunks);
    auto new_lines = apply_replacements(original_lines, replacements);
    
    // Ensure trailing newline
    if (new_lines.empty() || !new_lines.back().empty()) {
        new_lines.push_back("");
    }
    
    std::string new_content = join_lines(new_lines);
    
    // Generate unified diff
    std::string unified_diff = create_unified_diff(file_path, original_content, new_content);
    
    return {new_content, unified_diff};
}

ToolResult ApplyPatchTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    if (!validate_input(input)) {
        return ToolResult::error("apply_patch", "Invalid input: 'patchText' is required and must be a non-empty string.");
    }
    
    ApplyPatchToolParams params;
    try {
        params = ApplyPatchToolParams::from_json(input);
    } catch (const std::exception& e) {
        return ToolResult::error("apply_patch", fmt::format("Invalid parameters: {}", e.what()));
    }
    
    // Parse the patch
    auto parse_result = parse_patch(params.patch_text);
    if (!parse_result.error.empty()) {
        return ToolResult::error("apply_patch verification failed", parse_result.error);
    }
    
    if (parse_result.hunks.empty()) {
        std::string normalized = params.patch_text;
        // Normalize line endings
        for (char& c : normalized) {
            if (c == '\r') c = '\n';
        }
        trim(normalized);
        if (normalized == "*** Begin Patch\n*** End Patch") {
            return ToolResult::error("apply_patch", "patch rejected: empty patch");
        }
        return ToolResult::error("apply_patch verification failed", "no hunks found");
    }
    
    // Validate file paths and prepare changes
    std::vector<FileChangeResult> file_changes;
    std::string total_diff;
    
    for (const auto& hunk : parse_result.hunks) {
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            
            if constexpr (std::is_same_v<T, AddHunk>) {
                const auto& add = arg;
                std::filesystem::path file_path = std::filesystem::path(ctx.working_directory) / add.path;
                std::string file_path_str = file_path.string();
                
                std::string new_content = add.contents;
                if (!new_content.empty() && new_content.back() != '\n') {
                    new_content += '\n';
                }
                
                std::string diff = create_unified_diff(file_path_str, "", new_content);
                auto [additions, deletions] = count_diff_changes("", new_content);
                
                FileChangeResult change;
                change.file_path = file_path_str;
                change.relative_path = add.path;
                change.type = "add";
                change.diff = diff;
                change.old_content = "";
                change.new_content = new_content;
                change.additions = additions;
                change.deletions = deletions;
                file_changes.push_back(std::move(change));
                
                total_diff += diff + "\n";
                
            } else if constexpr (std::is_same_v<T, DeleteHunk>) {
                const auto& del = arg;
                std::filesystem::path file_path = std::filesystem::path(ctx.working_directory) / del.path;
                std::string file_path_str = file_path.string();
                
                // Read file content before deletion
                std::ifstream ifs(file_path);
                if (!ifs) {
                    throw std::runtime_error(fmt::format("Failed to read file for deletion: {}", file_path_str));
                }
                std::stringstream buffer;
                buffer << ifs.rdbuf();
                std::string old_content = buffer.str();
                ifs.close();
                
                std::string diff = create_unified_diff(file_path_str, old_content, "");
                int deletions = static_cast<int>(std::count(old_content.begin(), old_content.end(), '\n'));
                if (!old_content.empty() && old_content.back() != '\n') deletions++;
                
                FileChangeResult change;
                change.file_path = file_path_str;
                change.relative_path = del.path;
                change.type = "delete";
                change.diff = diff;
                change.old_content = old_content;
                change.new_content = "";
                change.additions = 0;
                change.deletions = deletions;
                file_changes.push_back(std::move(change));
                
                total_diff += diff + "\n";
                
            } else if constexpr (std::is_same_v<T, UpdateHunk>) {
                const auto& upd = arg;
                std::filesystem::path file_path = std::filesystem::path(ctx.working_directory) / upd.path;
                std::string file_path_str = file_path.string();
                
                // Check if file exists
                std::error_code ec;
                if (!std::filesystem::exists(file_path, ec)) {
                    throw std::runtime_error(fmt::format("File not found for update: {}", file_path_str));
                }
                
                // Derive new content from chunks
                auto [new_content, unified_diff] = derive_new_contents_from_chunks(file_path_str, upd.chunks);
                
                // Read old content for diff
                std::ifstream ifs(file_path);
                std::stringstream buffer;
                buffer << ifs.rdbuf();
                std::string old_content = buffer.str();
                ifs.close();
                
                std::string diff = create_unified_diff(file_path_str, old_content, new_content);
                auto [additions, deletions] = count_diff_changes(old_content, new_content);
                
                std::optional<std::string> move_path;
                if (upd.move_path) {
                    move_path = (std::filesystem::path(ctx.working_directory) / *upd.move_path).string();
                }
                
                FileChangeResult change;
                change.file_path = file_path_str;
                change.relative_path = upd.path;
                change.type = upd.move_path ? "move" : "update";
                change.diff = diff;
                change.old_content = old_content;
                change.new_content = new_content;
                change.additions = additions;
                change.deletions = deletions;
                change.move_path = move_path;
                file_changes.push_back(std::move(change));
                
                total_diff += diff + "\n";
            }
        }, hunk);
    }
    
    // Build per-file metadata for permission request
    std::vector<std::string> relative_paths;
    nlohmann::json files_json = nlohmann::json::array();
    
    for (const auto& change : file_changes) {
        relative_paths.push_back(change.relative_path);
        
        nlohmann::json file_json = {
            {"filePath", change.file_path},
            {"relativePath", change.relative_path},
            {"type", change.type},
            {"diff", change.diff},
            {"before", change.old_content},
            {"after", change.new_content},
            {"additions", change.additions},
            {"deletions", change.deletions}
        };
        if (change.move_path) {
            file_json["movePath"] = *change.move_path;
        }
        files_json.push_back(file_json);
    }
    
    // Request permission
    if (ctx.ask_permission) {
        permission::PermissionRequest req;
        req.id = "apply_patch";
        req.permission = "edit";
        req.patterns = relative_paths;
        req.tool = name();
        req.metadata = {
            {"filepath", join(relative_paths, ", ")},
            {"diff", total_diff},
            {"files", files_json}
        };
        
        auto reply = ctx.ask_permission(req);
        if (reply.type == permission::PermissionReply::Type::Reject) {
            return ToolResult::error("apply_patch", "User rejected permission to apply patch");
        }
    }
    
    // Apply the changes
    for (const auto& change : file_changes) {
        std::error_code ec;
        
        if (change.type == "add") {
            // Create parent directories
            std::filesystem::path p(change.file_path);
            std::filesystem::create_directories(p.parent_path(), ec);
            
            // Write file atomically
            std::filesystem::path temp_path = p;
            temp_path += ".tmp." + std::to_string(std::random_device{}());
            
            std::ofstream ofs(temp_path);
            if (!ofs) {
                throw std::runtime_error(fmt::format("Failed to create temp file: {}", temp_path.string()));
            }
            ofs << change.new_content;
            ofs.close();
            
            std::filesystem::rename(temp_path, p, ec);
            if (ec) {
                std::filesystem::remove(temp_path, ec);
                throw std::runtime_error(fmt::format("Failed to rename temp file: {}", ec.message()));
            }
            
        } else if (change.type == "delete") {
            std::filesystem::remove(change.file_path, ec);
            if (ec) {
                throw std::runtime_error(fmt::format("Failed to delete file: {}", change.file_path));
            }
            
        } else if (change.type == "update") {
            // Write file atomically
            std::filesystem::path p(change.file_path);
            std::filesystem::path temp_path = p;
            temp_path += ".tmp." + std::to_string(std::random_device{}());
            
            std::ofstream ofs(temp_path);
            if (!ofs) {
                throw std::runtime_error(fmt::format("Failed to create temp file: {}", temp_path.string()));
            }
            ofs << change.new_content;
            ofs.close();
            
            std::filesystem::rename(temp_path, p, ec);
            if (ec) {
                std::filesystem::remove(temp_path, ec);
                throw std::runtime_error(fmt::format("Failed to rename temp file: {}", ec.message()));
            }
            
        } else if (change.type == "move" && change.move_path) {
            // Create parent directories for target
            std::filesystem::path target(*change.move_path);
            std::filesystem::create_directories(target.parent_path(), ec);
            
            // Write to new location
            std::filesystem::path temp_path = target;
            temp_path += ".tmp." + std::to_string(std::random_device{}());
            
            std::ofstream ofs(temp_path);
            if (!ofs) {
                throw std::runtime_error(fmt::format("Failed to create temp file: {}", temp_path.string()));
            }
            ofs << change.new_content;
            ofs.close();
            
            std::filesystem::rename(temp_path, target, ec);
            if (ec) {
                std::filesystem::remove(temp_path, ec);
                throw std::runtime_error(fmt::format("Failed to rename temp file: {}", ec.message()));
            }
            
            // Delete original
            std::filesystem::remove(change.file_path, ec);
        }
    }
    
    // Generate output summary
    std::vector<std::string> summary_lines;
    for (const auto& change : file_changes) {
        if (change.type == "add") {
            summary_lines.push_back(fmt::format("A {}", change.relative_path));
        } else if (change.type == "delete") {
            summary_lines.push_back(fmt::format("D {}", change.relative_path));
        } else if (change.type == "move" && change.move_path) {
            summary_lines.push_back(fmt::format("M {} -> {}", change.relative_path, *change.move_path));
        } else {
            summary_lines.push_back(fmt::format("M {}", change.relative_path));
        }
    }
    
    std::string output = fmt::format("Success. Updated the following files:\n{}", 
        join(summary_lines, "\n"));
    
    nlohmann::json metadata = {
        {"diff", total_diff},
        {"files", files_json}
    };
    
    return ToolResult::success("apply_patch", output, metadata);
}

} // namespace turbot::core::tool::builtin
