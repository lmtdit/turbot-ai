/// @file llm_provider_e2e_test.cpp
/// @brief End-to-end tests for LLM provider integration (P1-6).
///
/// Tests are gated behind the TURBOT_E2E environment variable to prevent
/// network requests in normal CI runs.  Activate with:
///   TURBOT_E2E=1 BAILIAN_API_KEY=<key> ctest --test-dir build/debug -R e2e

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/provider/provider_manager.hpp>
#include <turbot/core/provider/impl/bailian_provider.hpp>
#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_loop.hpp>
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/agent/builtin/build_agent.hpp>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <string>

using namespace turbot::core::provider;
using namespace turbot::core::session;
using namespace turbot::core;

// ─── Helper ───────────────────────────────────────────────────────────────────

static bool e2e_enabled() {
    const char* env = std::getenv("TURBOT_E2E");
    return env && std::string(env) == "1";
}

static std::string bailian_api_key() {
    const char* env = std::getenv("BAILIAN_API_KEY");
    return env ? std::string(env) : "";
}

// ─── E2E-LLM-01: BailianProvider direct call ──────────────────────────────────

TEST_CASE("E2E-LLM-01: BailianProvider direct chat call returns non-empty response",
          "[e2e][llm][bailian]") {
    if (!e2e_enabled()) {
        SKIP("Set TURBOT_E2E=1 to run LLM e2e tests");
    }

    const auto api_key = bailian_api_key();
    if (api_key.empty()) {
        SKIP("Set BAILIAN_API_KEY=<key> to run bailian e2e tests");
    }

    ProviderConfig config;
    config.api_key = api_key;

    BailianProvider provider{config};
    REQUIRE(provider.is_ready());

    std::vector<ChatMessage> messages;
    messages.push_back(ChatMessage::user("Say 'hello' in one word."));

    auto response = provider.chat(messages, "qwen3-coder-plus");

    CHECK_FALSE(response.is_error());
    CHECK_FALSE(response.get_text().empty());
    CHECK(response.finish_reason == "stop");
}

// ─── E2E-LLM-02: SessionLoop + BailianProvider full pipeline ──────────────────

TEST_CASE("E2E-LLM-02: SessionLoop + BailianProvider full pipeline returns Stop",
          "[e2e][llm][session-loop]") {
    if (!e2e_enabled()) {
        SKIP("Set TURBOT_E2E=1 to run LLM e2e tests");
    }

    const auto api_key = bailian_api_key();
    if (api_key.empty()) {
        SKIP("Set BAILIAN_API_KEY=<key> to run bailian e2e tests");
    }

    // Register provider
    ProviderConfig config;
    config.api_key = api_key;
    auto bailian = std::make_shared<BailianProvider>(config);

    auto& pm = ProviderManager::instance();
    pm.register_provider(bailian);
    pm.set_default_provider("bailian");

    // Create session
    CreateParams params;
    params.project_id = "e2e-test";
    params.slug       = "e2e-llm-02";
    params.directory  = "/tmp/turbot-e2e";
    params.title      = "E2E LLM Test";
    auto session_opt  = Session::create(params);
    REQUIRE(session_opt.has_value());

    // Create loop with provider
    SessionLoop loop{*session_opt};
    loop.set_provider(bailian.get());
    loop.set_model("qwen3-coder-plus");
    loop.set_agent(std::make_shared<agent::BuildAgent>());

    bool got_message = false;
    loop.set_on_message([&](const Message& msg) {
        if (msg.role() == Role::Assistant) {
            got_message = true;
        }
    });

    auto result = loop.run("What is 2 + 2? Reply with just the number.");

    CHECK(result == LoopResult::Stop);
    CHECK(got_message);
    CHECK_FALSE(loop.messages().empty());

    // Cleanup
    pm.unregister_provider("bailian");
}

// ─── E2E-LLM-03: ProviderManager default model selection ──────────────────────

TEST_CASE("E2E-LLM-03: ProviderManager lists BailianProvider models",
          "[e2e][llm][provider-manager]") {
    if (!e2e_enabled()) {
        SKIP("Set TURBOT_E2E=1 to run LLM e2e tests");
    }

    const auto api_key = bailian_api_key();
    if (api_key.empty()) {
        SKIP("Set BAILIAN_API_KEY=<key> to run bailian e2e tests");
    }

    ProviderConfig config;
    config.api_key = api_key;

    BailianProvider provider{config};
    auto models = provider.list_models();

    REQUIRE_FALSE(models.empty());
    // Verify at least one known model is present
    bool found = false;
    for (const auto& m : models) {
        if (m.id == "qwen3-coder-plus") { found = true; break; }
    }
    CHECK(found);
}
