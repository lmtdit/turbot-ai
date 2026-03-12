#include <turbot/core/tool/builtin/question_tool.hpp>
#include <turbot/core/session/session_events.hpp>
#include <turbot/core/event/event_bus.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/crypto_utils.hpp>
#include <fmt/format.h>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

namespace turbot::core::tool::builtin {

// ============================================================================
// QuestionOption serialization
// ============================================================================

nlohmann::json QuestionOption::to_json() const {
    return {{"label", label}, {"description", description}};
}

QuestionOption QuestionOption::from_json(const nlohmann::json& j) {
    QuestionOption opt;
    opt.label       = j.value("label", "");
    opt.description = j.value("description", "");
    return opt;
}

// ============================================================================
// QuestionInfo serialization
// ============================================================================

nlohmann::json QuestionInfo::to_json() const {
    nlohmann::json j;
    j["question"] = question;
    j["header"]   = header;
    j["options"]  = nlohmann::json::array();
    for (const auto& opt : options) {
        j["options"].push_back(opt.to_json());
    }
    if (multiple.has_value()) j["multiple"] = *multiple;
    if (custom.has_value())   j["custom"]   = *custom;
    return j;
}

QuestionInfo QuestionInfo::from_json(const nlohmann::json& j) {
    QuestionInfo info;
    info.question = j.value("question", "");
    info.header   = j.value("header", "");
    if (j.contains("options") && j["options"].is_array()) {
        for (const auto& opt : j["options"]) {
            info.options.push_back(QuestionOption::from_json(opt));
        }
    }
    if (j.contains("multiple") && j["multiple"].is_boolean()) {
        info.multiple = j["multiple"].get<bool>();
    }
    if (j.contains("custom") && j["custom"].is_boolean()) {
        info.custom = j["custom"].get<bool>();
    }
    return info;
}

// ============================================================================
// QuestionRequest serialization
// ============================================================================

nlohmann::json QuestionRequest::to_json() const {
    nlohmann::json j;
    j["id"]         = id;
    j["session_id"] = session_id;
    j["message_id"] = message_id;
    j["call_id"]    = call_id;
    j["questions"]  = nlohmann::json::array();
    for (const auto& q : questions) {
        j["questions"].push_back(q.to_json());
    }
    return j;
}

QuestionRequest QuestionRequest::from_json(const nlohmann::json& j) {
    QuestionRequest req;
    req.id         = j.value("id", "");
    req.session_id = j.value("session_id", "");
    req.message_id = j.value("message_id", "");
    req.call_id    = j.value("call_id", "");
    if (j.contains("questions") && j["questions"].is_array()) {
        for (const auto& q : j["questions"]) {
            req.questions.push_back(QuestionInfo::from_json(q));
        }
    }
    return req;
}

// ============================================================================
// Pending question state (process-wide singleton map)
// ============================================================================

namespace {

struct PendingEntry {
    QuestionRequest                               request;
    std::promise<std::vector<QuestionAnswer>>     promise;
};

std::mutex                                        g_pending_mutex;
std::unordered_map<std::string, PendingEntry>     g_pending;

} // anonymous namespace

// ============================================================================
// Question::ask / reply / reject / list_pending
// ============================================================================

namespace Question {

std::vector<QuestionAnswer> ask(
    const std::string& session_id,
    const std::string& message_id,
    const std::string& call_id,
    const std::vector<QuestionInfo>& questions)
{
    const std::string request_id = turbot::utils::crypto::generate_uuid();

    QuestionRequest req;
    req.id         = request_id;
    req.session_id = session_id;
    req.message_id = message_id;
    req.call_id    = call_id;
    req.questions  = questions;

    // Register in pending map before publishing the event so that a very fast
    // reply() on another thread never misses the entry.
    std::future<std::vector<QuestionAnswer>> fut;
    {
        std::lock_guard<std::mutex> lock(g_pending_mutex);
        auto& entry = g_pending[request_id];
        entry.request = req;
        fut = entry.promise.get_future();
    }

    TURBOT_LOG_INFO("question.ask: session={} id={} count={}", session_id, request_id,
                    questions.size());

    // Publish event — outside the lock (EventBus::publish is independently
    // thread-safe and we must not hold g_pending_mutex while calling handlers
    // that might call reply() / reject()).
    turbot::core::EventBus::instance().publish(
        session::QuestionAskedEvent::kEventName,
        session::QuestionAskedEvent{req}
    );

    // Blocks the tool-executor thread until the user answers or rejects.
    // Throws RejectedError if reject() was called, or std::future_error
    // if the promise was destroyed without fulfilment (should not happen
    // in normal operation).
    return fut.get();
}

bool reply(const std::string& request_id, const std::vector<QuestionAnswer>& answers)
{
    std::unique_lock<std::mutex> lock(g_pending_mutex);
    auto it = g_pending.find(request_id);
    if (it == g_pending.end()) {
        TURBOT_LOG_WARN("question.reply: unknown request_id={}", request_id);
        return false;
    }

    // Extract entry before releasing the lock so the promise outlives the erase.
    PendingEntry entry = std::move(it->second);
    g_pending.erase(it);
    lock.unlock();

    TURBOT_LOG_INFO("question.reply: id={} answers={}", request_id, answers.size());

    // Broadcast the replied event (outside the lock).
    session::QuestionRepliedEvent ev;
    ev.session_id = entry.request.session_id;
    ev.request_id = request_id;
    ev.answers    = answers;
    turbot::core::EventBus::instance().publish(
        session::QuestionRepliedEvent::kEventName, ev);

    entry.promise.set_value(answers);
    return true;
}

bool reject(const std::string& request_id)
{
    std::unique_lock<std::mutex> lock(g_pending_mutex);
    auto it = g_pending.find(request_id);
    if (it == g_pending.end()) {
        TURBOT_LOG_WARN("question.reject: unknown request_id={}", request_id);
        return false;
    }

    PendingEntry entry = std::move(it->second);
    g_pending.erase(it);
    lock.unlock();

    TURBOT_LOG_INFO("question.reject: id={}", request_id);

    // Broadcast the rejected event (outside the lock).
    session::QuestionRejectedEvent ev;
    ev.session_id = entry.request.session_id;
    ev.request_id = request_id;
    turbot::core::EventBus::instance().publish(
        session::QuestionRejectedEvent::kEventName, ev);

    entry.promise.set_exception(
        std::make_exception_ptr(RejectedError{}));
    return true;
}

std::vector<QuestionRequest> list_pending()
{
    std::lock_guard<std::mutex> lock(g_pending_mutex);
    std::vector<QuestionRequest> result;
    result.reserve(g_pending.size());
    for (const auto& [id, entry] : g_pending) {
        result.push_back(entry.request);
    }
    return result;
}

} // namespace Question

// ============================================================================
// QuestionTool
// ============================================================================

std::string QuestionTool::description() const {
    return
        "Use this tool when you need to ask the user questions during execution. "
        "This allows you to:\n"
        "1. Gather user preferences or requirements\n"
        "2. Clarify ambiguous instructions\n"
        "3. Get decisions on implementation choices as you work\n"
        "4. Offer choices to the user about what direction to take.\n\n"
        "Usage notes:\n"
        "- When 'custom' is enabled (default), a \"Type your own answer\" option is "
        "added automatically; don't include \"Other\" or catch-all options\n"
        "- Answers are returned as arrays of labels; set 'multiple: true' to allow "
        "selecting more than one\n"
        "- If you recommend a specific option, make that the first option in the list "
        "and add \"(Recommended)\" at the end of the label";
}

nlohmann::json QuestionTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"questions", {
                {"type", "array"},
                {"description", "Questions to ask"},
                {"items", {
                    {"type", "object"},
                    {"properties", {
                        {"question", {{"type", "string"}, {"description", "Complete question text"}}},
                        {"header",   {{"type", "string"}, {"description", "Very short label (max 30 chars)"}}},
                        {"options",  {
                            {"type", "array"},
                            {"description", "Available choices"},
                            {"items", {
                                {"type", "object"},
                                {"properties", {
                                    {"label",       {{"type", "string"}, {"description", "Display text (1-5 words)"}}},
                                    {"description", {{"type", "string"}, {"description", "Explanation of choice"}}}
                                }},
                                {"required", {"label", "description"}}
                            }}
                        }},
                        {"multiple", {{"type", "boolean"}, {"description", "Allow selecting multiple choices"}}},
                        {"custom",   {{"type", "boolean"}, {"description", "Allow custom text answer (default: true)"}}}
                    }},
                    {"required", {"question", "header", "options"}}
                }}
            }}
        }},
        {"required", {"questions"}}
    };
}

ToolResult QuestionTool::execute(const nlohmann::json& input, ToolContext& ctx)
{
    // Parse questions array
    if (!input.contains("questions") || !input["questions"].is_array()) {
        return ToolResult::error("QuestionTool", "Missing or invalid 'questions' array");
    }

    std::vector<QuestionInfo> questions;
    try {
        for (const auto& q : input["questions"]) {
            questions.push_back(QuestionInfo::from_json(q));
        }
    } catch (const std::exception& e) {
        return ToolResult::error("QuestionTool",
            fmt::format("Failed to parse questions: {}", e.what()));
    }

    if (questions.empty()) {
        return ToolResult::error("QuestionTool", "Questions array must not be empty");
    }

    const std::string call_id = ctx.call_id.value_or("");

    // Blocking call — throws RejectedError if user dismisses
    std::vector<QuestionAnswer> answers =
        Question::ask(ctx.session_id, ctx.message_id, call_id, questions);

    // Format result — mirrors opencode QuestionTool output
    auto format_answer = [](const QuestionAnswer& ans) -> std::string {
        if (ans.empty()) return "Unanswered";
        std::string out;
        for (size_t i = 0; i < ans.size(); ++i) {
            if (i > 0) out += ", ";
            out += ans[i];
        }
        return out;
    };

    std::string formatted;
    for (size_t i = 0; i < questions.size(); ++i) {
        if (i > 0) formatted += ", ";
        formatted += fmt::format("\"{}\"=\"{}\"",
            questions[i].question,
            format_answer(i < answers.size() ? answers[i] : QuestionAnswer{}));
    }

    const std::string title = fmt::format("Asked {} question{}",
        questions.size(), questions.size() == 1 ? "" : "s");

    const std::string output = fmt::format(
        "User has answered your questions: {}. "
        "You can now continue with the user's answers in mind.",
        formatted);

    // Store answers as metadata for consumers (mirrors opencode metadata.answers)
    nlohmann::json metadata;
    metadata["answers"] = nlohmann::json::array();
    for (const auto& ans : answers) {
        metadata["answers"].push_back(ans);
    }

    return ToolResult::success(title, output, metadata);
}

} // namespace turbot::core::tool::builtin
