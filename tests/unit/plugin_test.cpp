/// plugin_test.cpp — Unit tests for turbot::core::plugin::PluginManager
///
/// Covers:
///   - register_hook / handler_count
///   - trigger: no handlers (no-op)
///   - trigger: single handler modifies output
///   - trigger: multiple handlers called in registration order
///   - unregister_hook: by ID removes only that handler
///   - unregister_hook: unknown ID returns false
///   - clear: removes all handlers for one hook
///   - clear_all: removes handlers for all hooks
///   - Well-known kHook* constant: chat.params changes temperature
///   - trigger does not modify input (const-correctness)
///   - concurrent register + trigger (thread-safety smoke test)

#include <turbot/core/plugin/plugin.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <atomic>
#include <thread>
#include <vector>

using namespace turbot::core::plugin;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// RAII guard: clears the named hook on destruction.
struct HookGuard {
    std::string name;
    ~HookGuard() { PluginManager::instance().clear(name); }
};

/// RAII guard: clears ALL hooks on destruction.
struct AllHooksGuard {
    ~AllHooksGuard() { PluginManager::instance().clear_all(); }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("PluginManager: register_hook increments handler_count",
          "[core][plugin][register]") {
    AllHooksGuard guard;
    auto& pm = PluginManager::instance();

    REQUIRE(pm.handler_count(kHookChatParams) == 0);
    pm.register_hook(kHookChatParams, [](const nlohmann::json&, nlohmann::json&) {});
    REQUIRE(pm.handler_count(kHookChatParams) == 1);
    pm.register_hook(kHookChatParams, [](const nlohmann::json&, nlohmann::json&) {});
    REQUIRE(pm.handler_count(kHookChatParams) == 2);
}

TEST_CASE("PluginManager: trigger with no handlers is a no-op",
          "[core][plugin][trigger]") {
    AllHooksGuard guard;
    auto& pm = PluginManager::instance();

    nlohmann::json input = {{"session_id", "abc"}};
    nlohmann::json output = {{"temperature", 1.0}};
    REQUIRE_NOTHROW(pm.trigger("unknown.hook", input, output));
    REQUIRE(output["temperature"] == 1.0); // unchanged
}

TEST_CASE("PluginManager: single handler modifies output",
          "[core][plugin][trigger]") {
    AllHooksGuard guard;
    auto& pm = PluginManager::instance();

    pm.register_hook(kHookChatParams, [](const nlohmann::json&, nlohmann::json& out) {
        out["temperature"] = 0.5;
    });

    nlohmann::json input = {{"session_id", "s1"}};
    nlohmann::json output = {{"temperature", 1.0}, {"top_p", 1.0}};
    pm.trigger(kHookChatParams, input, output);

    REQUIRE_THAT(output["temperature"].get<double>(),
                 Catch::Matchers::WithinAbs(0.5, 1e-9));
    REQUIRE_THAT(output["top_p"].get<double>(),
                 Catch::Matchers::WithinAbs(1.0, 1e-9)); // untouched
}

TEST_CASE("PluginManager: multiple handlers called in registration order",
          "[core][plugin][trigger]") {
    AllHooksGuard guard;
    auto& pm = PluginManager::instance();

    std::vector<int> order;
    pm.register_hook(kHookChatParams, [&order](const nlohmann::json&, nlohmann::json&) {
        order.push_back(1);
    });
    pm.register_hook(kHookChatParams, [&order](const nlohmann::json&, nlohmann::json&) {
        order.push_back(2);
    });
    pm.register_hook(kHookChatParams, [&order](const nlohmann::json&, nlohmann::json&) {
        order.push_back(3);
    });

    nlohmann::json input, output;
    pm.trigger(kHookChatParams, input, output);

    REQUIRE(order == std::vector<int>{1, 2, 3});
}

TEST_CASE("PluginManager: last handler wins when both modify same key",
          "[core][plugin][trigger]") {
    AllHooksGuard guard;
    auto& pm = PluginManager::instance();

    pm.register_hook(kHookChatParams, [](const nlohmann::json&, nlohmann::json& out) {
        out["temperature"] = 0.1;
    });
    pm.register_hook(kHookChatParams, [](const nlohmann::json&, nlohmann::json& out) {
        out["temperature"] = 0.9; // overwrites previous
    });

    nlohmann::json input, output = {{"temperature", 1.0}};
    pm.trigger(kHookChatParams, input, output);

    REQUIRE_THAT(output["temperature"].get<double>(),
                 Catch::Matchers::WithinAbs(0.9, 1e-9));
}

TEST_CASE("PluginManager: unregister_hook removes only that handler",
          "[core][plugin][unregister]") {
    AllHooksGuard guard;
    auto& pm = PluginManager::instance();

    std::vector<int> called;
    pm.register_hook(kHookChatParams, [&called](const nlohmann::json&, nlohmann::json&) {
        called.push_back(1);
    });
    auto id2 = pm.register_hook(kHookChatParams, [&called](const nlohmann::json&, nlohmann::json&) {
        called.push_back(2);
    });
    pm.register_hook(kHookChatParams, [&called](const nlohmann::json&, nlohmann::json&) {
        called.push_back(3);
    });

    bool removed = pm.unregister_hook(kHookChatParams, id2);
    REQUIRE(removed);
    REQUIRE(pm.handler_count(kHookChatParams) == 2);

    nlohmann::json input, output;
    pm.trigger(kHookChatParams, input, output);

    REQUIRE(called == std::vector<int>{1, 3}); // handler 2 skipped
}

TEST_CASE("PluginManager: unregister_hook with unknown ID returns false",
          "[core][plugin][unregister]") {
    AllHooksGuard guard;
    auto& pm = PluginManager::instance();

    pm.register_hook(kHookChatParams, [](const nlohmann::json&, nlohmann::json&) {});
    // 999999 was never returned
    REQUIRE_FALSE(pm.unregister_hook(kHookChatParams, 999999u));
    REQUIRE(pm.handler_count(kHookChatParams) == 1); // still there
}

TEST_CASE("PluginManager: unregister_hook for unknown hook name returns false",
          "[core][plugin][unregister]") {
    AllHooksGuard guard;
    auto& pm = PluginManager::instance();
    REQUIRE_FALSE(pm.unregister_hook("no.such.hook", 0u));
}

TEST_CASE("PluginManager: clear removes all handlers for one hook",
          "[core][plugin][clear]") {
    AllHooksGuard guard;
    auto& pm = PluginManager::instance();

    pm.register_hook(kHookChatParams, [](const nlohmann::json&, nlohmann::json&) {});
    pm.register_hook(kHookChatHeaders, [](const nlohmann::json&, nlohmann::json&) {});

    pm.clear(kHookChatParams);

    REQUIRE(pm.handler_count(kHookChatParams) == 0);
    REQUIRE(pm.handler_count(kHookChatHeaders) == 1); // unaffected
}

TEST_CASE("PluginManager: clear_all removes all hooks",
          "[core][plugin][clear]") {
    auto& pm = PluginManager::instance();

    pm.register_hook(kHookChatParams,   [](const nlohmann::json&, nlohmann::json&) {});
    pm.register_hook(kHookChatHeaders,  [](const nlohmann::json&, nlohmann::json&) {});
    pm.register_hook(kHookSystemTransform, [](const nlohmann::json&, nlohmann::json&) {});

    pm.clear_all();

    REQUIRE(pm.handler_count(kHookChatParams)      == 0);
    REQUIRE(pm.handler_count(kHookChatHeaders)     == 0);
    REQUIRE(pm.handler_count(kHookSystemTransform) == 0);
}

TEST_CASE("PluginManager: kHookChatParams changes temperature in output",
          "[core][plugin][chat_params]") {
    AllHooksGuard guard;
    auto& pm = PluginManager::instance();

    pm.register_hook(kHookChatParams, [](const nlohmann::json& ctx, nlohmann::json& out) {
        // creative mode for a specific session
        if (ctx.value("session_id", "") == "creative") {
            out["temperature"] = 1.5;
        }
    });

    nlohmann::json ctx_creative = {{"session_id", "creative"}, {"model", "gpt-4"}};
    nlohmann::json ctx_default  = {{"session_id", "default"},  {"model", "gpt-4"}};
    nlohmann::json out1 = {{"temperature", 1.0}, {"top_p", 1.0}};
    nlohmann::json out2 = {{"temperature", 1.0}, {"top_p", 1.0}};

    pm.trigger(kHookChatParams, ctx_creative, out1);
    pm.trigger(kHookChatParams, ctx_default,  out2);

    REQUIRE_THAT(out1["temperature"].get<double>(),
                 Catch::Matchers::WithinAbs(1.5, 1e-9));
    REQUIRE_THAT(out2["temperature"].get<double>(),
                 Catch::Matchers::WithinAbs(1.0, 1e-9)); // unchanged
}

TEST_CASE("PluginManager: trigger does not modify input (const-correctness)",
          "[core][plugin][trigger]") {
    AllHooksGuard guard;
    auto& pm = PluginManager::instance();

    pm.register_hook(kHookChatParams, [](const nlohmann::json&, nlohmann::json& out) {
        out["temperature"] = 0.0;
    });

    const nlohmann::json original_input = {{"session_id", "immutable"}, {"x", 42}};
    nlohmann::json input_copy = original_input;
    nlohmann::json output = {{"temperature", 1.0}};

    pm.trigger(kHookChatParams, input_copy, output);

    REQUIRE(input_copy == original_input); // input must be untouched
}

TEST_CASE("PluginManager: system-transform hook appends to system array",
          "[core][plugin][system_transform]") {
    AllHooksGuard guard;
    auto& pm = PluginManager::instance();

    pm.register_hook(kHookSystemTransform,
                     [](const nlohmann::json&, nlohmann::json& out) {
                         out["system"].push_back("injected by plugin");
                     });

    nlohmann::json input = {{"session_id", "s"}, {"model", "m"}};
    nlohmann::json output = {{"system", nlohmann::json::array({"base prompt"})}};

    pm.trigger(kHookSystemTransform, input, output);

    REQUIRE(output["system"].size() == 2u);
    REQUIRE(output["system"][1].get<std::string>() == "injected by plugin");
}

TEST_CASE("PluginManager: session-compacting hook receives session_id",
          "[core][plugin][session_compacting]") {
    AllHooksGuard guard;
    auto& pm = PluginManager::instance();

    std::string captured_session;
    pm.register_hook(kHookSessionCompacting,
                     [&captured_session](const nlohmann::json& ctx, nlohmann::json& out) {
                         captured_session = ctx.value("session_id", "");
                         out["system_injection"] = "extra compaction context";
                     });

    nlohmann::json ctx = {{"session_id", "compact-sess"}};
    nlohmann::json out = {{"system_injection", ""}};
    pm.trigger(kHookSessionCompacting, ctx, out);

    REQUIRE(captured_session == "compact-sess");
    REQUIRE(out["system_injection"].get<std::string>() == "extra compaction context");
}

TEST_CASE("PluginManager: concurrent register and trigger are thread-safe",
          "[core][plugin][thread_safety]") {
    AllHooksGuard guard;
    auto& pm = PluginManager::instance();

    constexpr int kThreads  = 8;
    constexpr int kRoundsEach = 200;
    std::atomic<int> invocations{0};

    // Register a baseline handler before the threads start.
    pm.register_hook(kHookChatParams, [&invocations](const nlohmann::json&, nlohmann::json&) {
        invocations.fetch_add(1, std::memory_order_relaxed);
    });

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&pm, &invocations, t]() {
            for (int i = 0; i < kRoundsEach; ++i) {
                if (t % 2 == 0) {
                    // Even threads trigger the hook
                    nlohmann::json inp, out;
                    pm.trigger(kHookChatParams, inp, out);
                } else {
                    // Odd threads register and immediately unregister
                    auto id = pm.register_hook(kHookChatParams,
                        [&invocations](const nlohmann::json&, nlohmann::json&) {
                            invocations.fetch_add(1, std::memory_order_relaxed);
                        });
                    pm.unregister_hook(kHookChatParams, id);
                }
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }

    // Must not crash or deadlock.  We only assert that invocations > 0
    // (baseline handler was called at least once by even threads).
    REQUIRE(invocations.load() > 0);
}
