#include <turbot/core/tool/builtin/multiedit_tool.hpp>
#include <turbot/core/tool/builtin/edit_tool.hpp>
#include <turbot/core/common/logger.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace turbot::core::tool::builtin {

namespace fs = std::filesystem;

// ============================================================================
// EditOperation
// ============================================================================

EditOperation EditOperation::from_json(const nlohmann::json& j) {
    EditOperation op;
    op.file_path   = j.at("filePath").get<std::string>();
    op.old_string  = j.at("oldString").get<std::string>();
    op.new_string  = j.at("newString").get<std::string>();
    if (j.contains("replaceAll") && !j["replaceAll"].is_null()) {
        op.replace_all = j["replaceAll"].get<bool>();
    }
    return op;
}

nlohmann::json EditOperation::to_json() const {
    return {
        {"filePath",   file_path},
        {"oldString",  old_string},
        {"newString",  new_string},
        {"replaceAll", replace_all},
    };
}

// ============================================================================
// MultiEditTool
// ============================================================================

std::string MultiEditTool::description() const {
    return "Perform multiple string replacements across one or more files in a single call. "
           "Accepts an array of edit operations; each operation specifies filePath, oldString, "
           "and newString. Operations are applied sequentially in the order provided. "
           "If any operation fails, processing stops and an error is returned. "
           "Use this instead of calling 'edit' repeatedly for the same or different files.";
}

nlohmann::json MultiEditTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"edits", {
                {"type", "array"},
                {"description", "Array of edit operations to apply sequentially"},
                {"items", {
                    {"type", "object"},
                    {"properties", {
                        {"filePath", {
                            {"type", "string"},
                            {"description", "Absolute path to the file to modify"}
                        }},
                        {"oldString", {
                            {"type", "string"},
                            {"description", "The exact text to replace"}
                        }},
                        {"newString", {
                            {"type", "string"},
                            {"description", "The replacement text"}
                        }},
                        {"replaceAll", {
                            {"type", "boolean"},
                            {"description", "Replace all occurrences (default false)"}
                        }}
                    }},
                    {"required", nlohmann::json::array({"filePath", "oldString", "newString"})}
                }},
                {"minItems", 1}
            }}
        }},
        {"required", nlohmann::json::array({"edits"})}
    };
}

bool MultiEditTool::validate_input(const nlohmann::json& input) const {
    if (!input.contains("edits") || !input["edits"].is_array()) {
        return false;
    }
    for (const auto& op : input["edits"]) {
        if (!op.contains("filePath")  || !op["filePath"].is_string()  ||
            !op.contains("oldString") || !op["oldString"].is_string() ||
            !op.contains("newString") || !op["newString"].is_string()) {
            return false;
        }
    }
    return !input["edits"].empty();
}

ToolResult MultiEditTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    if (!validate_input(input)) {
        return ToolResult::error(
            "MultiEdit failed",
            "Invalid input: 'edits' must be a non-empty array of {filePath, oldString, newString}"
        );
    }

    // Parse all operations upfront
    std::vector<EditOperation> ops;
    try {
        for (const auto& j : input["edits"]) {
            ops.push_back(EditOperation::from_json(j));
        }
    } catch (const std::exception& e) {
        return ToolResult::error(
            "MultiEdit failed",
            fmt::format("Failed to parse edit operations: {}", e.what())
        );
    }

    // Delegate each operation to the underlying EditTool logic
    EditTool edit_tool;
    std::ostringstream summary;
    int success_count = 0;

    for (std::size_t i = 0; i < ops.size(); ++i) {
        if (ctx.should_abort()) {
            return ToolResult::error(
                "MultiEdit aborted",
                fmt::format("Aborted after {} of {} operations", success_count, ops.size())
            );
        }

        const auto& op = ops[i];
        nlohmann::json edit_input = {
            {"filePath",   op.file_path},
            {"oldString",  op.old_string},
            {"newString",  op.new_string},
            {"replaceAll", op.replace_all},
        };

        ToolResult result = edit_tool.execute(edit_input, ctx);
        if (result.is_error) {
            return ToolResult::error(
                "MultiEdit failed",
                fmt::format("Operation {}/{} failed on '{}': {}",
                            i + 1, ops.size(), op.file_path, result.output)
            );
        }

        ++success_count;
        summary << fmt::format("[{}/{}] {} — applied\n", i + 1, ops.size(), op.file_path);
        TURBOT_LOG_DEBUG("multiedit: operation {}/{} applied to '{}'",
                         i + 1, ops.size(), op.file_path);
    }

    return ToolResult::success(
        fmt::format("MultiEdit: {} operation(s) applied", success_count),
        summary.str()
    );
}

} // namespace turbot::core::tool::builtin
