#pragma once

#include <turbot/core/tool/tool.hpp>
#include <string>
#include <vector>

namespace turbot::core::tool::builtin {

/// A single edit operation within a multiedit request
struct EditOperation {
    std::string file_path;   ///< Absolute path to the file
    std::string old_string;  ///< Text to replace
    std::string new_string;  ///< Replacement text
    bool replace_all = false; ///< Replace all occurrences

    static EditOperation from_json(const nlohmann::json& j);
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Tool for performing multiple file edits in a single call (aligned with OpenCode multiedit)
class TURBOT_CORE_API MultiEditTool : public Tool {
public:
    MultiEditTool() = default;

    [[nodiscard]] std::string name() const override { return "multiedit"; }
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] nlohmann::json input_schema() const override;
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;
    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;
};

} // namespace turbot::core::tool::builtin
