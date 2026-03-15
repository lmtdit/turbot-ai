#pragma once

#include <turbot/core/tool/tool.hpp>
#include <string>
#include <vector>
#include <optional>
#include <variant>

namespace turbot::core::tool::builtin {

/// Represents a chunk of changes in an update operation
struct UpdateFileChunk {
    std::vector<std::string> old_lines;     ///< Lines to be replaced
    std::vector<std::string> new_lines;     ///< Replacement lines
    std::optional<std::string> change_context;  ///< Context line for seeking
    bool is_end_of_file = false;            ///< Whether this is at end of file
};

/// Represents an "add" hunk - creating a new file
struct AddHunk {
    std::string path;       ///< File path to create
    std::string contents;   ///< Content to write
};

/// Represents a "delete" hunk - removing a file
struct DeleteHunk {
    std::string path;       ///< File path to delete
};

/// Represents an "update" hunk - modifying an existing file
struct UpdateHunk {
    std::string path;                       ///< File path to modify
    std::optional<std::string> move_path;   ///< Optional new path (for move)
    std::vector<UpdateFileChunk> chunks;    ///< Chunks of changes
};

/// A hunk in a patch - can be add, delete, or update
using Hunk = std::variant<AddHunk, DeleteHunk, UpdateHunk>;

/// Result of parsing a patch
struct ParseResult {
    std::vector<Hunk> hunks;  ///< Parsed hunks
    std::string error;        ///< Error message if parsing failed
};

/// Result of a file change operation
struct FileChangeResult {
    std::string file_path;        ///< Absolute file path
    std::string relative_path;    ///< Relative path from workspace
    std::string type;             ///< "add", "update", "delete", or "move"
    std::string diff;             ///< Unified diff
    std::string old_content;      ///< Content before change
    std::string new_content;      ///< Content after change
    int additions = 0;            ///< Number of added lines
    int deletions = 0;            ///< Number of deleted lines
    std::optional<std::string> move_path;  ///< Target path for move
};

/// Parameters for ApplyPatchTool execution
struct ApplyPatchToolParams {
    std::string patch_text;  ///< The full patch text

    /// Parse from JSON
    static ApplyPatchToolParams from_json(const nlohmann::json& j);

    /// Convert to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Tool for applying unified diff patches to files
class TURBOT_CORE_API ApplyPatchTool : public Tool {
public:
    ApplyPatchTool() = default;

    [[nodiscard]] std::string name() const override { return "apply_patch"; }

    [[nodiscard]] std::string description() const override;

    [[nodiscard]] nlohmann::json input_schema() const override;

    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;

    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;

    // Patch parsing functions - public for testing
    [[nodiscard]] static ParseResult parse_patch(const std::string& patch_text);

    // Derive new content from chunks
    [[nodiscard]] static std::pair<std::string, std::string> derive_new_contents_from_chunks(
        const std::string& file_path,
        const std::vector<UpdateFileChunk>& chunks
    );

private:
    /// Maximum file size for patching (10MB)
    static constexpr size_t MAX_FILE_SIZE = 10 * 1024 * 1024;

    /// Strip heredoc wrapper if present
    [[nodiscard]] static std::string strip_heredoc(const std::string& input);

    /// Parse patch header line
    [[nodiscard]] static std::optional<std::tuple<std::string, std::optional<std::string>, size_t, std::string>>
        parse_patch_header(const std::vector<std::string>& lines, size_t start_idx);

    /// Parse update file chunks
    [[nodiscard]] static std::pair<std::vector<UpdateFileChunk>, size_t>
        parse_update_chunks(const std::vector<std::string>& lines, size_t start_idx);

    /// Parse add file content
    [[nodiscard]] static std::pair<std::string, size_t>
        parse_add_content(const std::vector<std::string>& lines, size_t start_idx);

    /// Seek a pattern in lines
    [[nodiscard]] static int seek_sequence(
        const std::vector<std::string>& lines,
        const std::vector<std::string>& pattern,
        size_t start_index,
        bool eof = false
    );

    /// Normalize Unicode punctuation to ASCII
    [[nodiscard]] static std::string normalize_unicode(const std::string& str);

    /// Compute replacements from chunks
    [[nodiscard]] static std::vector<std::tuple<size_t, size_t, std::vector<std::string>>>
        compute_replacements(
            const std::vector<std::string>& original_lines,
            const std::string& file_path,
            const std::vector<UpdateFileChunk>& chunks
        );

    /// Apply replacements to lines
    [[nodiscard]] static std::vector<std::string> apply_replacements(
        const std::vector<std::string>& lines,
        const std::vector<std::tuple<size_t, size_t, std::vector<std::string>>>& replacements
    );

    /// Create a unified diff
    [[nodiscard]] static std::string create_unified_diff(
        const std::string& file_path,
        const std::string& old_content,
        const std::string& new_content
    );

    /// Count additions and deletions in a diff
    [[nodiscard]] static std::pair<int, int> count_diff_changes(
        const std::string& old_content,
        const std::string& new_content
    );
};

} // namespace turbot::core::tool::builtin
