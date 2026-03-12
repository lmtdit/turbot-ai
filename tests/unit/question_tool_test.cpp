#include <catch2/catch_test_macros.hpp>
#include <turbot/core/tool/builtin/question_tool.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/session/session_events.hpp>
#include <turbot/core/event/event_bus.hpp>
#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace turbot::core::tool;
using namespace turbot::core::tool::builtin;
using namespace turbot::core;

// ============================================================================
// QuestionOption serialization
// ============================================================================

TEST_CASE("QuestionOption::to_json / from_json round-trip", "[core][tool][question]") {
    QuestionOption opt;
    opt.label       = "Option A";
    opt.description = "This is option A";

    auto j = opt.to_json();
    REQUIRE(j["label"]       == "Option A");
    REQUIRE(j["description"] == "This is option A");

    auto restored = QuestionOption::from_json(j);
    REQUIRE(restored.label       == opt.label);
    REQUIRE(restored.description == opt.description);
}

TEST_CASE("QuestionOption::from_json with missing fields uses defaults", "[core][tool][question]") {
    auto opt = QuestionOption::from_json(nlohmann::json::object());
    REQUIRE(opt.label.empty());
    REQUIRE(opt.description.empty());
}

// ============================================================================
// QuestionInfo serialization
// ============================================================================

TEST_CASE("QuestionInfo::to_json / from_json round-trip", "[core][tool][question]") {
    QuestionInfo info;
    info.question = "Which approach do you prefer?";
    info.header   = "Approach";
    info.options  = {{"Incremental", "Step by step"}, {"Big Bang", "All at once"}};
    info.multiple = false;
    info.custom   = true;

    auto j = info.to_json();
    REQUIRE(j["question"]   == "Which approach do you prefer?");
    REQUIRE(j["header"]     == "Approach");
    REQUIRE(j["options"].size() == 2);
    REQUIRE(j["multiple"]   == false);
    REQUIRE(j["custom"]     == true);

    auto restored = QuestionInfo::from_json(j);
    REQUIRE(restored.question == info.question);
    REQUIRE(restored.header   == info.header);
    REQUIRE(restored.options.size() == 2);
    REQUIRE(restored.options[0].label == "Incremental");
    REQUIRE(restored.options[1].label == "Big Bang");
    REQUIRE(restored.multiple.has_value());
    REQUIRE(*restored.multiple == false);
    REQUIRE(restored.custom.has_value());
    REQUIRE(*restored.custom == true);
}

TEST_CASE("QuestionInfo without optional fields", "[core][tool][question]") {
    QuestionInfo info;
    info.question = "Question?";
    info.header   = "Q";

    auto j = info.to_json();
    REQUIRE_FALSE(j.contains("multiple"));
    REQUIRE_FALSE(j.contains("custom"));

    auto restored = QuestionInfo::from_json(j);
    REQUIRE_FALSE(restored.multiple.has_value());
    REQUIRE_FALSE(restored.custom.has_value());
}

// ============================================================================
// QuestionRequest serialization
// ============================================================================

TEST_CASE("QuestionRequest::to_json / from_json round-trip", "[core][tool][question]") {
    QuestionRequest req;
    req.id         = "req-001";
    req.session_id = "sess-abc";
    req.message_id = "msg-xyz";
    req.call_id    = "call-123";

    QuestionInfo q;
    q.question = "Test question?";
    q.header   = "Header";
    q.options  = {{"Yes", "Confirm"}, {"No", "Cancel"}};
    req.questions.push_back(q);

    auto j = req.to_json();
    REQUIRE(j["id"]          == "req-001");
    REQUIRE(j["session_id"]  == "sess-abc");
    REQUIRE(j["message_id"]  == "msg-xyz");
    REQUIRE(j["call_id"]     == "call-123");
    REQUIRE(j["questions"].size() == 1);

    auto restored = QuestionRequest::from_json(j);
    REQUIRE(restored.id         == req.id);
    REQUIRE(restored.session_id == req.session_id);
    REQUIRE(restored.message_id == req.message_id);
    REQUIRE(restored.call_id    == req.call_id);
    REQUIRE(restored.questions.size() == 1);
    REQUIRE(restored.questions[0].question == "Test question?");
}

// ============================================================================
// Question::ask / reply — happy path
// ============================================================================

TEST_CASE("Question::ask resolved by reply returns correct answers", "[core][tool][question]") {
    // We need the QuestionAskedEvent to fire and capture the request_id.
    std::string captured_request_id;
    bool event_received = false;

    auto sub = EventBus::instance().subscribe<session::QuestionAskedEvent>(
        session::QuestionAskedEvent::kEventName,
        [&](const Event<session::QuestionAskedEvent>& event) {
            captured_request_id = event.data.request.id;
            event_received = true;
        }
    );

    QuestionInfo q;
    q.question = "Pick one:";
    q.header   = "Choice";
    q.options  = {{"A", "First"}, {"B", "Second"}};

    std::vector<QuestionAnswer> result;
    bool ask_completed = false;

    // ask() blocks — run in a separate thread
    auto fut = std::async(std::launch::async, [&]() {
        result = Question::ask("sess-1", "msg-1", "call-1", {q});
        ask_completed = true;
    });

    // Wait for the event to be received (give the async thread time to publish)
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!event_received && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(event_received);
    REQUIRE_FALSE(captured_request_id.empty());

    // Deliver the answer from the "UI thread"
    std::vector<QuestionAnswer> answers = {{"A"}};
    bool replied = Question::reply(captured_request_id, answers);
    REQUIRE(replied);

    // Wait for ask() to unblock
    auto status = fut.wait_for(std::chrono::seconds(3));
    REQUIRE(status == std::future_status::ready);
    REQUIRE(ask_completed);
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].size() == 1);
    REQUIRE(result[0][0] == "A");

    EventBus::instance().unsubscribe(session::QuestionAskedEvent::kEventName, sub);
}

// ============================================================================
// Question::ask / reject — RejectedError path
// ============================================================================

TEST_CASE("Question::ask rejected by reject() throws RejectedError", "[core][tool][question]") {
    std::string captured_request_id;
    bool event_received = false;

    auto sub = EventBus::instance().subscribe<session::QuestionAskedEvent>(
        session::QuestionAskedEvent::kEventName,
        [&](const Event<session::QuestionAskedEvent>& event) {
            captured_request_id = event.data.request.id;
            event_received = true;
        }
    );

    QuestionInfo q;
    q.question = "Are you sure?";
    q.header   = "Confirm";
    q.options  = {{"Yes", "Proceed"}, {"No", "Cancel"}};

    bool threw_rejected = false;

    auto fut = std::async(std::launch::async, [&]() {
        try {
            Question::ask("sess-2", "msg-2", "call-2", {q});
        } catch (const Question::RejectedError&) {
            threw_rejected = true;
        }
    });

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!event_received && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(event_received);
    REQUIRE_FALSE(captured_request_id.empty());

    bool rejected = Question::reject(captured_request_id);
    REQUIRE(rejected);

    auto status = fut.wait_for(std::chrono::seconds(3));
    REQUIRE(status == std::future_status::ready);
    REQUIRE(threw_rejected);

    EventBus::instance().unsubscribe(session::QuestionAskedEvent::kEventName, sub);
}

// ============================================================================
// Question::reply / reject — unknown request_id returns false
// ============================================================================

TEST_CASE("Question::reply with unknown request_id returns false", "[core][tool][question]") {
    bool ok = Question::reply("non-existent-id", {});
    REQUIRE_FALSE(ok);
}

TEST_CASE("Question::reject with unknown request_id returns false", "[core][tool][question]") {
    bool ok = Question::reject("non-existent-id");
    REQUIRE_FALSE(ok);
}

// ============================================================================
// Question::list_pending
// ============================================================================

TEST_CASE("Question::list_pending returns pending requests", "[core][tool][question]") {
    std::string captured_id;
    bool got_event = false;

    auto sub = EventBus::instance().subscribe<session::QuestionAskedEvent>(
        session::QuestionAskedEvent::kEventName,
        [&](const Event<session::QuestionAskedEvent>& event) {
            captured_id = event.data.request.id;
            got_event   = true;
        }
    );

    QuestionInfo q;
    q.question = "List pending test?";
    q.header   = "Test";
    q.options  = {{"OK", "Fine"}};

    // Launch ask in background
    auto fut = std::async(std::launch::async, [&]() {
        try {
            Question::ask("sess-pending", "msg-p", "call-p", {q});
        } catch (...) {}
    });

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!got_event && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(got_event);

    // Check list_pending contains our request
    auto pending = Question::list_pending();
    bool found = false;
    for (const auto& r : pending) {
        if (r.id == captured_id) {
            found = true;
            REQUIRE(r.session_id == "sess-pending");
            break;
        }
    }
    REQUIRE(found);

    // Clean up
    Question::reject(captured_id);
    fut.wait_for(std::chrono::seconds(1));

    EventBus::instance().unsubscribe(session::QuestionAskedEvent::kEventName, sub);
}

// ============================================================================
// QuestionTool metadata
// ============================================================================

TEST_CASE("QuestionTool::name", "[core][tool][question]") {
    QuestionTool tool;
    REQUIRE(tool.name() == "question");
}

TEST_CASE("QuestionTool::description is non-empty", "[core][tool][question]") {
    QuestionTool tool;
    REQUIRE_FALSE(tool.description().empty());
}

TEST_CASE("QuestionTool::input_schema structure", "[core][tool][question]") {
    QuestionTool tool;
    auto schema = tool.input_schema();

    REQUIRE(schema["type"] == "object");
    REQUIRE(schema["properties"].contains("questions"));
    REQUIRE(schema["required"].is_array());

    bool has_questions = false;
    for (const auto& req : schema["required"]) {
        if (req == "questions") has_questions = true;
    }
    REQUIRE(has_questions);
}

// ============================================================================
// QuestionTool::execute — input validation
// ============================================================================

TEST_CASE("QuestionTool::execute with missing questions field returns error", "[core][tool][question]") {
    QuestionTool tool;
    ToolContext ctx;
    ctx.session_id  = "sess-exec";
    ctx.abort_flag  = std::make_shared<std::atomic<bool>>(false);

    nlohmann::json input = nlohmann::json::object();
    auto result = tool.execute(input, ctx);

    REQUIRE(result.is_error);
}

TEST_CASE("QuestionTool::execute with empty questions array returns error", "[core][tool][question]") {
    QuestionTool tool;
    ToolContext ctx;
    ctx.session_id  = "sess-exec";
    ctx.abort_flag  = std::make_shared<std::atomic<bool>>(false);

    nlohmann::json input = {{"questions", nlohmann::json::array()}};
    auto result = tool.execute(input, ctx);

    REQUIRE(result.is_error);
}

// ============================================================================
// QuestionTool::execute — successful reply path
// ============================================================================

TEST_CASE("QuestionTool::execute returns answers when user replies", "[core][tool][question]") {
    // Subscribe to QuestionAskedEvent and immediately reply
    std::string sub_id = EventBus::instance().subscribe<session::QuestionAskedEvent>(
        session::QuestionAskedEvent::kEventName,
        [](const Event<session::QuestionAskedEvent>& event) {
            // Reply from a separate thread to avoid re-entrancy issues
            std::thread([id = event.data.request.id]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                Question::reply(id, {{"Option A"}});
            }).detach();
        }
    );

    QuestionTool tool;
    ToolContext ctx;
    ctx.session_id = "sess-exec-2";
    ctx.message_id = "msg-exec-2";
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);

    nlohmann::json input = {
        {"questions", {{
            {"question", "Which option?"},
            {"header",   "Option"},
            {"options",  {{{"label", "Option A"}, {"description", "First option"}}}}
        }}}
    };

    auto result = tool.execute(input, ctx);

    // Unsubscribe before asserting to prevent handler from interfering with other tests
    EventBus::instance().unsubscribe(session::QuestionAskedEvent::kEventName, sub_id);

    REQUIRE_FALSE(result.is_error);
    REQUIRE(result.title == "Asked 1 question");
    REQUIRE(result.output.find("Option A") != std::string::npos);
    REQUIRE(result.metadata.contains("answers"));
}

// ============================================================================
// QuestionTool::execute — reject path (RejectedError propagation)
// ============================================================================

TEST_CASE("QuestionTool::execute throws RejectedError when user rejects", "[core][tool][question]") {
    // Subscribe to QuestionAskedEvent and immediately reject
    std::string sub_id = EventBus::instance().subscribe<session::QuestionAskedEvent>(
        session::QuestionAskedEvent::kEventName,
        [](const Event<session::QuestionAskedEvent>& event) {
            std::thread([id = event.data.request.id]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                Question::reject(id);
            }).detach();
        }
    );

    QuestionTool tool;
    ToolContext ctx;
    ctx.session_id = "sess-exec-3";
    ctx.message_id = "msg-exec-3";
    ctx.abort_flag = std::make_shared<std::atomic<bool>>(false);

    nlohmann::json input = {
        {"questions", {{
            {"question", "Proceed?"},
            {"header",   "Confirm"},
            {"options",  {{{"label", "Yes"}, {"description", "Go ahead"}}}}
        }}}
    };

    // execute() should propagate RejectedError (not catch it internally)
    REQUIRE_THROWS_AS(tool.execute(input, ctx), Question::RejectedError);

    // Unsubscribe to avoid handler leaking into subsequent tests
    EventBus::instance().unsubscribe(session::QuestionAskedEvent::kEventName, sub_id);
}

// ============================================================================
// ToolRegistry — QuestionTool opt-in registration
// ============================================================================

TEST_CASE("QuestionTool is NOT registered by default", "[core][tool][question]") {
    // Clear the registry to avoid state pollution from other tests
    ToolRegistry::instance().clear();
    ToolRegistry::instance().enable_question_tool(false);
    ToolRegistry::instance().register_builtin_tools();

    auto t = ToolRegistry::instance().get("question");
    REQUIRE(t == nullptr);
}

TEST_CASE("QuestionTool IS registered after enable_question_tool(true)", "[core][tool][question]") {
    ToolRegistry::instance().clear();
    ToolRegistry::instance().enable_question_tool(true);
    ToolRegistry::instance().register_builtin_tools();

    auto t = ToolRegistry::instance().get("question");
    REQUIRE(t != nullptr);
    REQUIRE(t->name() == "question");

    // Restore default state for other tests
    ToolRegistry::instance().clear();
    ToolRegistry::instance().enable_question_tool(false);
}
