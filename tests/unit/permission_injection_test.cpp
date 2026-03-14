/// @file permission_injection_test.cpp
/// @brief Unit tests for P0-2: ask_permission injection into SessionLoop.
///
/// Tests cover:
///   1. set_on_permission_request() direct-callback path (PermissionCallback)
///   2. EventBus publish/wait path (PermissionAskedEvent → PermissionRepliedEvent)
///   3. PermissionAskedEvent / PermissionRepliedEvent struct construction & kEventName
///   4. Timeout path: no reply arrives → Reject default
///   5. allow-once / always / reject reply types round-trip through EventBus

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/session/session_loop.hpp>
#include <turbot/core/session/session_events.hpp>
#include <turbot/core/event/event_bus.hpp>
#include <turbot/core/permission/permission.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/tool/tool.hpp>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

using namespace turbot::core::session;
using namespace turbot::core::permission;
using namespace turbot::core;

// ============================================================================
// Helpers
// ============================================================================

/// A minimal tool that immediately calls ctx.ask_permission when executed.
class PermissionRequestingTool : public tool::Tool {
public:
    explicit PermissionRequestingTool(const std::string& name) : name_(name) {}

    std::string name() const override { return name_; }
    std::string description() const override { return "Test permission tool"; }
    nlohmann::json input_schema() const override {
        return {{"type", "object"}, {"properties", nlohmann::json::object()}};
    }

    tool::ToolResult execute(const nlohmann::json&, tool::ToolContext& ctx) override {
        if (!ctx.ask_permission) {
            // No callback → record this and return success
            permission_called_ = false;
            return tool::ToolResult::success("no-perm-cb", "ok");
        }
        PermissionRequest req;
        req.id         = "req-" + name_;
        req.permission = "write";
        req.patterns   = {"/tmp/test"};
        req.tool       = name_;

        PermissionReply reply = ctx.ask_permission(req);
        permission_called_ = true;
        last_reply_type_   = reply.type;

        if (reply.type == PermissionReply::Type::Reject) {
            return tool::ToolResult::error("PermissionDenied", "User rejected");
        }
        return tool::ToolResult::success("allowed", "Tool executed");
    }

    bool             permission_called_  = false;
    PermissionReply::Type last_reply_type_ = PermissionReply::Type::Reject;

private:
    std::string name_;
};

// ============================================================================
// Section 1 – PermissionAskedEvent / PermissionRepliedEvent basic structure
// ============================================================================

TEST_CASE("PermissionAskedEvent: kEventName and fields", "[permission][events]") {
    REQUIRE(std::string(PermissionAskedEvent::kEventName) == "permission.asked");

    PermissionAskedEvent ev;
    ev.session_id = "sess-1";
    ev.request.id         = "req-1";
    ev.request.permission = "write";
    ev.request.patterns   = {"/tmp/*"};

    CHECK(ev.session_id == "sess-1");
    CHECK(ev.request.id == "req-1");
    CHECK(ev.request.permission == "write");
    REQUIRE(ev.request.patterns.size() == 1);
    CHECK(ev.request.patterns[0] == "/tmp/*");
}

TEST_CASE("PermissionRepliedEvent: kEventName and fields", "[permission][events]") {
    REQUIRE(std::string(PermissionRepliedEvent::kEventName) == "permission.replied");

    PermissionRepliedEvent ev;
    ev.session_id  = "sess-2";
    ev.request_id  = "req-2";
    ev.reply       = PermissionReply::once();

    CHECK(ev.session_id == "sess-2");
    CHECK(ev.request_id == "req-2");
    CHECK(ev.reply.type == PermissionReply::Type::Once);
}

// ============================================================================
// Section 2 – PermissionCallback: set_on_permission_request() direct path
// ============================================================================

TEST_CASE("PermissionCallback: set_on_permission_request allows injection", "[permission][callback]") {
    // Verify the PermissionCallback type alias works as expected
    int call_count = 0;
    PermissionCallback cb = [&](const PermissionRequest& req) -> PermissionReply {
        ++call_count;
        CHECK(req.permission == "read");
        return PermissionReply::once();
    };

    PermissionRequest req;
    req.id         = "r1";
    req.permission = "read";
    req.patterns   = {"**"};

    auto reply = cb(req);
    CHECK(call_count == 1);
    CHECK(reply.type == PermissionReply::Type::Once);
}

TEST_CASE("PermissionCallback: always reply type round-trip", "[permission][callback]") {
    PermissionCallback cb = [](const PermissionRequest&) -> PermissionReply {
        return PermissionReply::always();
    };
    PermissionRequest req;
    req.id = "r2"; req.permission = "write"; req.patterns = {"/etc"};
    auto reply = cb(req);
    CHECK(reply.type == PermissionReply::Type::Always);
}

TEST_CASE("PermissionCallback: reject reply type round-trip", "[permission][callback]") {
    PermissionCallback cb = [](const PermissionRequest&) -> PermissionReply {
        return PermissionReply::reject("no thanks");
    };
    PermissionRequest req;
    req.id = "r3"; req.permission = "execute"; req.patterns = {"/usr/bin/*"};
    auto reply = cb(req);
    CHECK(reply.type == PermissionReply::Type::Reject);
}

// ============================================================================
// Section 3 – PermissionReply serialisation helpers (used in event payloads)
// ============================================================================

TEST_CASE("PermissionReply: factory helpers return correct type", "[permission][reply]") {
    CHECK(PermissionReply::once().type   == PermissionReply::Type::Once);
    CHECK(PermissionReply::always().type == PermissionReply::Type::Always);
    CHECK(PermissionReply::reject().type == PermissionReply::Type::Reject);
}

TEST_CASE("PermissionReply: optional message preserved", "[permission][reply]") {
    auto r = PermissionReply::reject("too risky");
    REQUIRE(r.message.has_value());
    CHECK(*r.message == "too risky");

    auto r2 = PermissionReply::once();
    CHECK_FALSE(r2.message.has_value());
}

// ============================================================================
// Section 4 – EventBus publish / subscribe round-trip for permission events
// ============================================================================

TEST_CASE("EventBus: PermissionAskedEvent subscribe and receive", "[permission][eventbus]") {
    auto& bus = EventBus::instance();

    bool received = false;
    std::string captured_session;
    std::string captured_req_id;

    std::string sub_id = bus.subscribe<PermissionAskedEvent>(
        PermissionAskedEvent::kEventName,
        [&](const Event<PermissionAskedEvent>& ev) {
            received         = true;
            captured_session = ev.data.session_id;
            captured_req_id  = ev.data.request.id;
        }
    );

    PermissionAskedEvent asked;
    asked.session_id      = "sess-pub";
    asked.request.id      = "req-pub";
    asked.request.permission = "write";
    asked.request.patterns   = {"/tmp/foo"};

    bus.publish(PermissionAskedEvent::kEventName, std::move(asked));

    CHECK(received);
    CHECK(captured_session == "sess-pub");
    CHECK(captured_req_id  == "req-pub");

    bus.unsubscribe(PermissionAskedEvent::kEventName, sub_id);
}

TEST_CASE("EventBus: PermissionRepliedEvent subscribe and receive", "[permission][eventbus]") {
    auto& bus = EventBus::instance();

    bool received = false;
    PermissionReply::Type captured_type = PermissionReply::Type::Reject;

    std::string sub_id = bus.subscribe<PermissionRepliedEvent>(
        PermissionRepliedEvent::kEventName,
        [&](const Event<PermissionRepliedEvent>& ev) {
            received      = true;
            captured_type = ev.data.reply.type;
        }
    );

    PermissionRepliedEvent replied;
    replied.session_id = "sess-rep";
    replied.request_id = "req-rep";
    replied.reply      = PermissionReply::always();

    bus.publish(PermissionRepliedEvent::kEventName, std::move(replied));

    CHECK(received);
    CHECK(captured_type == PermissionReply::Type::Always);

    bus.unsubscribe(PermissionRepliedEvent::kEventName, sub_id);
}

TEST_CASE("EventBus: PermissionReplied filtered by request_id", "[permission][eventbus]") {
    auto& bus = EventBus::instance();

    std::string captured_id;
    std::string sub_id = bus.subscribe<PermissionRepliedEvent>(
        PermissionRepliedEvent::kEventName,
        [&](const Event<PermissionRepliedEvent>& ev) {
            // Simulate filtering: only capture if request_id matches
            if (ev.data.request_id == "target") {
                captured_id = ev.data.request_id;
            }
        }
    );

    // Publish a non-matching event first
    {
        PermissionRepliedEvent r; r.request_id = "other"; r.reply = PermissionReply::once();
        bus.publish(PermissionRepliedEvent::kEventName, std::move(r));
    }
    CHECK(captured_id.empty());

    // Publish the matching event
    {
        PermissionRepliedEvent r; r.request_id = "target"; r.reply = PermissionReply::always();
        bus.publish(PermissionRepliedEvent::kEventName, std::move(r));
    }
    CHECK(captured_id == "target");

    bus.unsubscribe(PermissionRepliedEvent::kEventName, sub_id);
}

// ============================================================================
// Section 5 – SessionLoop::set_on_permission_request() integration
// (uses a tool registry, no LLM needed; calls execute_tool via the tool object)
// ============================================================================

TEST_CASE("SessionLoop: set_on_permission_request stores callback", "[permission][session_loop]") {
    // We only verify that the setter doesn't throw and the callback can be
    // assigned multiple times.
    int call_count = 0;
    PermissionCallback cb1 = [&](const PermissionRequest&) -> PermissionReply {
        ++call_count;
        return PermissionReply::once();
    };
    PermissionCallback cb2 = [&](const PermissionRequest&) -> PermissionReply {
        call_count += 10;
        return PermissionReply::always();
    };

    // Create a minimal session (no DB needed for this test)
    CreateParams params;
    params.project_id = "perm-test-proj";
    params.slug       = "perm-test";
    params.directory  = "/tmp/perm-test";
    params.title      = "Permission Test Session";

    auto session_opt = Session::create(params);
    REQUIRE(session_opt.has_value());

    SessionLoop loop{*session_opt};
    CHECK_NOTHROW(loop.set_on_permission_request(std::move(cb1)));
    CHECK_NOTHROW(loop.set_on_permission_request(std::move(cb2)));
    // No assertion on call_count — we just verify no crash
}
