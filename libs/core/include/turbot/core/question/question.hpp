#pragma once

#include <turbot/core/common/export.hpp>
#include <optional>
#include <string>
#include <vector>

/// Question module — mirrors OpenCode question/index.ts
///
/// Provides a service for the LLM agent to ask the user questions during
/// a session and receive answers asynchronously.
///
/// Usage:
///   turbot::core::question::Question::ask(session_id, message_id, call_id, questions)
///   turbot::core::question::Question::reply(request_id, answers)
///   turbot::core::question::Question::reject(request_id)
///   turbot::core::question::Question::list_pending()

namespace turbot::core::question {

// ─── Domain types ─────────────────────────────────────────────────────────────

/// A single option that the user can choose (mirrors Question.Option)
struct TURBOT_CORE_API QuestionOption {
    std::string label;        ///< Display text (1-5 words, concise)
    std::string description;  ///< Explanation of what this option means
};

/// One question presented to the user (mirrors Question.Info)
struct TURBOT_CORE_API QuestionInfo {
    std::string question;                  ///< Complete question text
    std::string header;                    ///< Very short label (max 30 chars)
    std::vector<QuestionOption> options;   ///< Available choices
    std::optional<bool> multiple;          ///< Allow selecting multiple choices
    std::optional<bool> custom;            ///< Allow typing custom answer (default true)
};

/// Answer to a single question — array of selected labels (mirrors Question.Answer)
using QuestionAnswer = std::vector<std::string>;

/// Complete question request broadcast via EventBus (mirrors Question.Request)
struct TURBOT_CORE_API QuestionRequest {
    std::string id;                        ///< Unique request ID
    std::string session_id;               ///< Session that triggered the question
    std::string message_id;               ///< Message ID for UI context
    std::string call_id;                  ///< Tool call ID for UI context
    std::vector<QuestionInfo> questions;  ///< Questions to ask
};

// ─── Question service ─────────────────────────────────────────────────────────

namespace Question {

/// Thrown when the user dismisses (rejects) a question request.
class TURBOT_CORE_API RejectedError : public std::runtime_error {
public:
    explicit RejectedError()
        : std::runtime_error("The user dismissed this question") {}
};

/// Send questions to the user and block until answered or rejected.
///
/// Publishes a QuestionAskedEvent via EventBus, then blocks the calling thread
/// until reply() or reject() is called with the matching request_id.
///
/// @param session_id  Session that is asking.
/// @param message_id  Message ID for UI context.
/// @param call_id     Tool call ID for UI context.
/// @param questions   Questions to present.
/// @return            Answers in the same order as questions.
/// @throws RejectedError  If the user rejects the question.
[[nodiscard]] TURBOT_CORE_API
std::vector<QuestionAnswer> ask(
    const std::string& session_id,
    const std::string& message_id,
    const std::string& call_id,
    const std::vector<QuestionInfo>& questions
);

/// Deliver the user's answers for a pending question request.
///
/// Thread-safe: may be called from any thread (e.g. a UI callback).
///
/// @param request_id  ID returned in QuestionRequest (from the event).
/// @param answers     Answers in the same order as the original questions.
/// @return true if the request was found and resolved, false otherwise.
TURBOT_CORE_API
bool reply(const std::string& request_id, const std::vector<QuestionAnswer>& answers);

/// Reject (dismiss) a pending question request.
///
/// Thread-safe: may be called from any thread.
///
/// @param request_id  ID of the pending request.
/// @return true if the request was found and rejected, false otherwise.
TURBOT_CORE_API
bool reject(const std::string& request_id);

/// List all pending question requests (e.g. for UI enumeration).
[[nodiscard]] TURBOT_CORE_API
std::vector<QuestionRequest> list_pending();

} // namespace Question

} // namespace turbot::core::question
