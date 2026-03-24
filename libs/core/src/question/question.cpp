#include <turbot/core/question/question.hpp>
#include <turbot/core/tool/builtin/question_tool.hpp>

/// Question module implementation — mirrors OpenCode question/index.ts
///
/// This module provides the Question service as a standalone component
/// (not coupled to the Tool subsystem), matching OpenCode's architecture
/// where question/index.ts is independent of tool/question.ts.
///
/// Implementation strategy: delegate to tool::builtin::Question to share
/// a single pending-request map, avoiding split-brain state.

namespace turbot::core::question {
namespace Question {

// ─── Type adapters ────────────────────────────────────────────────────────────

static tool::builtin::QuestionInfo to_tool_info(const QuestionInfo& qi) {
    tool::builtin::QuestionInfo ti;
    ti.question = qi.question;
    ti.header   = qi.header;
    ti.multiple = qi.multiple;
    ti.custom   = qi.custom;
    for (const auto& o : qi.options) {
        ti.options.push_back({o.label, o.description});
    }
    return ti;
}

// ─── Question::ask ────────────────────────────────────────────────────────────

std::vector<QuestionAnswer> ask(
    const std::string& session_id,
    const std::string& message_id,
    const std::string& call_id,
    const std::vector<QuestionInfo>& questions)
{
    // Convert to tool namespace types and delegate to the shared implementation
    std::vector<tool::builtin::QuestionInfo> tool_questions;
    tool_questions.reserve(questions.size());
    for (const auto& q : questions) {
        tool_questions.push_back(to_tool_info(q));
    }

    // tool::builtin::Question::ask() manages the shared g_pending state
    return tool::builtin::Question::ask(session_id, message_id, call_id, tool_questions);
}

// ─── Question::reply ──────────────────────────────────────────────────────────

bool reply(const std::string& request_id, const std::vector<QuestionAnswer>& answers) {
    return tool::builtin::Question::reply(request_id, answers);
}

// ─── Question::reject ─────────────────────────────────────────────────────────

bool reject(const std::string& request_id) {
    return tool::builtin::Question::reject(request_id);
}

// ─── Question::list_pending ───────────────────────────────────────────────────

std::vector<QuestionRequest> list_pending() {
    // Convert from tool namespace types to question namespace types
    const auto tool_pending = tool::builtin::Question::list_pending();
    std::vector<QuestionRequest> result;
    result.reserve(tool_pending.size());
    for (const auto& tp : tool_pending) {
        QuestionRequest qr;
        qr.id         = tp.id;
        qr.session_id = tp.session_id;
        qr.message_id = tp.message_id;
        qr.call_id    = tp.call_id;
        for (const auto& tq : tp.questions) {
            QuestionInfo qi;
            qi.question  = tq.question;
            qi.header    = tq.header;
            qi.multiple  = tq.multiple;
            qi.custom    = tq.custom;
            for (const auto& to : tq.options) {
                qi.options.push_back({to.label, to.description});
            }
            qr.questions.push_back(std::move(qi));
        }
        result.push_back(std::move(qr));
    }
    return result;
}

} // namespace Question
} // namespace turbot::core::question

