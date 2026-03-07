#pragma once

#include <turbot/core/tool/tool.hpp>
#include <functional>
#include <string>
#include <vector>

namespace turbot::core::tool::builtin {

/// Parameters for EditTool execution
struct EditToolParams {
    std::string file_path;          ///< Absolute path to the file to modify
    std::string old_string;         ///< Text to replace
    std::string new_string;         ///< Replacement text (must be different from old_string)
    bool replace_all = false;       ///< Replace all occurrences (default false)

    /// Parse from JSON
    static EditToolParams from_json(const nlohmann::json& j);

    /// Convert to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Result of a replacement operation
struct ReplacementResult {
    std::string content;            ///< The modified content
    bool found = false;             ///< Whether old_string was found
    bool multiple_matches = false;  ///< Whether multiple matches were found
    std::string error;              ///< Error message if any
};

/// Tool for editing files with intelligent string replacement
class TURBOT_CORE_API EditTool : public Tool {
public:
    EditTool() = default;

    [[nodiscard]] std::string name() const override { return "edit"; }

    [[nodiscard]] std::string description() const override;

    [[nodiscard]] nlohmann::json input_schema() const override;

    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;

    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;

private:
    /// Replace text in content using various matching strategies
    [[nodiscard]] static ReplacementResult replace(
        const std::string& content,
        const std::string& old_string,
        const std::string& new_string,
        bool replace_all
    );

    /// Simple exact match replacer - yields possible search strings to find in content
    static std::vector<std::string> simple_replacer(const std::string& content, const std::string& find);

    /// Line-trimmed replacer - matches lines with trimmed comparison
    static std::vector<std::string> line_trimmed_replacer(const std::string& content, const std::string& find);

    /// Block anchor replacer - matches based on first and last lines
    static std::vector<std::string> block_anchor_replacer(const std::string& content, const std::string& find);

    /// Whitespace normalized replacer
    static std::vector<std::string> whitespace_normalized_replacer(const std::string& content, const std::string& find);

    /// Indentation flexible replacer
    static std::vector<std::string> indentation_flexible_replacer(const std::string& content, const std::string& find);

    /// Multi-occurrence replacer - yields all exact matches
    static std::vector<std::string> multi_occurrence_replacer(const std::string& content, const std::string& find);

    /// Calculate Levenshtein distance between two strings
    [[nodiscard]] static int levenshtein_distance(std::string_view a, std::string_view b);

    /// Create a diff output showing changes
    [[nodiscard]] static std::string create_diff(
        const std::string& file_path,
        const std::string& old_content,
        const std::string& new_content
    );
};

} // namespace turbot::core::tool::builtin
