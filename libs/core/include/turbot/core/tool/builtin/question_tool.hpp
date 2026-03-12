#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/tool/tool.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace turbot::core::tool::builtin {

// ============================================================================
// Question domain types (mirrors opencode Question namespace)
// ============================================================================

/// A single option that the user can choose
struct TURBOT_CORE_API QuestionOption {
    std::string label;        ///< Display text (1-5 words, concise)
    std::string description;  ///< Explanation of what this option means

    [[nodiscard]] nlohmann::json to_json() const;
    static QuestionOption from_json(const nlohmann::json& j);
};

/// One question presented to the user
struct TURBOT_CORE_API QuestionInfo {
    std::string question;                  ///< Complete question text
    std::string header;                    ///< Very short label (max 30 chars)
    std::vector<QuestionOption> options;   ///< Available choices
    std::optional<bool> multiple;          ///< Allow selecting multiple choices
    std::optional<bool> custom;            ///< Allow typing custom answer (default true)

    [[nodiscard]] nlohmann::json to_json() const;
    static QuestionInfo from_json(const nlohmann::json& j);
};

/// Answer to a single question — array of selected labels
using QuestionAnswer = std::vector<std::string>;

/// Complete question request broadcast via EventBus
struct TURBOT_CORE_API QuestionRequest {
    std::string id;                ///< Unique request ID (UUID)
    std::string session_id;        ///< Session that triggered the question
    std::string message_id;        ///< Message ID for context
    std::string call_id;           ///< Tool call ID for context
    std::vector<QuestionInfo> questions;   ///< Questions to ask

    [[nodiscard]] nlohmann::json to_json() const;
    static QuestionRequest from_json(const nlohmann::json& j);
};

// ============================================================================
// Question manager — ask / reply / reject
// ============================================================================

namespace Question {

/// Thrown when the user dismisses (rejects) a question request.
/// Caught by SessionLoop::execute_tool() to set blocked_ = true.
class TURBOT_CORE_API RejectedError : public std::runtime_error {
public:
    explicit RejectedError()
        : std::runtime_error("The user dismissed this question") {}
};

/**
 * @brief Send one or more questions to the user and block until answered.
 *
 * Publishes a QuestionAskedEvent via EventBus, then blocks the calling thread
 * on a std::promise until reply() or reject() is called with the matching
 * request_id.
 *
 * @param session_id  Session that is asking.
 * @param message_id  Message ID (for UI context).
 * @param call_id     Tool call ID (for UI context).
 * @param questions   Questions to present.
 * @return            Answers in the same order as questions.
 * @throws RejectedError  If the user rejects the question.
 */
[[nodiscard]] TURBOT_CORE_API
std::vector<QuestionAnswer> ask(
    const std::string& session_id,
    const std::string& message_id,
    const std::string& call_id,
    const std::vector<QuestionInfo>& questions
);

/**
 * @brief Deliver the user's answers for a pending question request.
 *
 * Thread-safe: may be called from any thread (e.g. a UI callback).
 *
 * @param request_id  ID returned in QuestionRequest (from the event).
 * @param answers     Answers in the same order as the original questions.
 * @return true if the request was found and resolved, false otherwise.
 */
TURBOT_CORE_API
bool reply(const std::string& request_id, const std::vector<QuestionAnswer>& answers);

/**
 * @brief Reject (dismiss) a pending question request.
 *
 * Thread-safe: may be called from any thread.
 *
 * @param request_id  ID of the pending request.
 * @return true if the request was found and rejected, false otherwise.
 */
TURBOT_CORE_API
bool reject(const std::string& request_id);

/**
 * @brief List all pending question requests (e.g. for UI enumeration).
 */
[[nodiscard]] TURBOT_CORE_API
std::vector<QuestionRequest> list_pending();

} // namespace Question

// ============================================================================
// QuestionTool
// ============================================================================

/**
 * @brief Tool that allows the LLM agent to ask the user questions.
 *
 * Mirrors opencode QuestionTool. The tool is optional: it must be explicitly
 * enabled via QuestionTool::set_enabled(true) before registering it in the
 * ToolRegistry (e.g. for CLI/app clients that support interactive questions).
 *
 * Input schema:
 * {
 *   "questions": [
 *     {
 *       "question": "<full question text>",
 *       "header": "<short label>",
 *       "options": [{"label": "...", "description": "..."}],
 *       "multiple": false,
 *       "custom": true
 *     }
 *   ]
 * }
 */
class TURBOT_CORE_API QuestionTool : public Tool {
public:
    QuestionTool() = default;

    [[nodiscard]] std::string name() const override { return "question"; }
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] nlohmann::json input_schema() const override;
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;
};

} // namespace turbot::core::tool::builtin
