#include <turbot/core/session/usage_tracker.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <nlohmann/json.hpp>

using namespace turbot::core;
using namespace turbot::core::session;

// ===== CostInfo Tests =====

TEST_CASE("CostInfo serialization", "[usage_tracker][cost]") {
    SECTION("to_json and from_json") {
        CostInfo original;
        original.input_cost = 0.01;
        original.output_cost = 0.03;
        original.cache_read_cost = 0.001;
        original.cache_write_cost = 0.002;
        original.reasoning_cost = 0.005;
        original.total_cost = 0.048;

        auto j = original.to_json();
        auto restored = CostInfo::from_json(j);

        REQUIRE(restored.input_cost == Catch::Approx(original.input_cost));
        REQUIRE(restored.output_cost == Catch::Approx(original.output_cost));
        REQUIRE(restored.cache_read_cost == Catch::Approx(original.cache_read_cost));
        REQUIRE(restored.cache_write_cost == Catch::Approx(original.cache_write_cost));
        REQUIRE(restored.reasoning_cost == Catch::Approx(original.reasoning_cost));
        REQUIRE(restored.total_cost == Catch::Approx(original.total_cost));
    }

    SECTION("operator+=") {
        CostInfo a;
        a.input_cost = 0.01;
        a.output_cost = 0.02;
        a.total_cost = 0.03;

        CostInfo b;
        b.input_cost = 0.005;
        b.output_cost = 0.01;
        b.total_cost = 0.015;

        a += b;

        REQUIRE(a.input_cost == Catch::Approx(0.015));
        REQUIRE(a.output_cost == Catch::Approx(0.03));
        REQUIRE(a.total_cost == Catch::Approx(0.045));
    }

    SECTION("operator-=") {
        CostInfo a;
        a.input_cost = 0.02;
        a.output_cost = 0.04;
        a.total_cost = 0.06;

        CostInfo b;
        b.input_cost = 0.01;
        b.output_cost = 0.02;
        b.total_cost = 0.03;

        a -= b;

        REQUIRE(a.input_cost == Catch::Approx(0.01));
        REQUIRE(a.output_cost == Catch::Approx(0.02));
        REQUIRE(a.total_cost == Catch::Approx(0.03));
    }
}

// ===== SessionUsage Tests =====

TEST_CASE("SessionUsage serialization", "[usage_tracker][session]") {
    SECTION("to_json and from_json") {
        SessionUsage original;
        original.tokens.input = 100;
        original.tokens.output = 50;
        original.cost.total_cost = 0.05;
        original.message_count = 5;
        original.tool_calls = 3;
        original.request_count = 2;

        auto j = original.to_json();
        auto restored = SessionUsage::from_json(j);

        REQUIRE(restored.tokens.input == 100);
        REQUIRE(restored.tokens.output == 50);
        REQUIRE(restored.cost.total_cost == Catch::Approx(0.05));
        REQUIRE(restored.message_count == 5);
        REQUIRE(restored.tool_calls == 3);
        REQUIRE(restored.request_count == 2);
    }

    SECTION("operator+=") {
        SessionUsage a;
        a.tokens.input = 100;
        a.message_count = 3;

        SessionUsage b;
        b.tokens.input = 50;
        b.message_count = 2;

        a += b;

        REQUIRE(a.tokens.input == 150);
        REQUIRE(a.message_count == 5);
    }
}

// ===== UsageTracker::calculate_usage Tests =====

TEST_CASE("UsageTracker calculate_usage", "[usage_tracker][calculate]") {
    SECTION("OpenAI format") {
        nlohmann::json raw_usage = {
            {"prompt_tokens", 100},
            {"completion_tokens", 50}
        };

        auto usage = UsageTracker::calculate_usage(raw_usage);

        REQUIRE(usage.input == 100);
        REQUIRE(usage.output == 50);
        REQUIRE(usage.reasoning == 0);
        REQUIRE(usage.total() == 150);
    }

    SECTION("Anthropic format") {
        nlohmann::json raw_usage = {
            {"input_tokens", 200},
            {"output_tokens", 100}
        };

        auto usage = UsageTracker::calculate_usage(raw_usage);

        REQUIRE(usage.input == 200);
        REQUIRE(usage.output == 100);
        REQUIRE(usage.total() == 300);
    }

    SECTION("CamelCase format") {
        nlohmann::json raw_usage = {
            {"promptTokens", 150},
            {"completionTokens", 75}
        };

        auto usage = UsageTracker::calculate_usage(raw_usage);

        REQUIRE(usage.input == 150);
        REQUIRE(usage.output == 75);
    }

    SECTION("With metadata - reasoning tokens") {
        nlohmann::json raw_usage = {
            {"prompt_tokens", 100},
            {"completion_tokens", 50}
        };
        nlohmann::json metadata = {
            {"reasoning_tokens", 25}
        };

        auto usage = UsageTracker::calculate_usage(raw_usage, metadata);

        REQUIRE(usage.input == 100);
        REQUIRE(usage.output == 50);
        REQUIRE(usage.reasoning == 25);
    }

    SECTION("With cache tokens") {
        nlohmann::json raw_usage = {
            {"input_tokens", 1000},
            {"output_tokens", 500},
            {"cache_read_input_tokens", 300},
            {"cache_creation_input_tokens", 100}
        };

        auto usage = UsageTracker::calculate_usage(raw_usage);

        REQUIRE(usage.input == 1000);
        REQUIRE(usage.output == 500);
        REQUIRE(usage.cache.read == 300);
        REQUIRE(usage.cache.write == 100);
    }

    SECTION("Cache tokens from metadata") {
        nlohmann::json raw_usage = {
            {"prompt_tokens", 500},
            {"completion_tokens", 200}
        };
        nlohmann::json metadata = {
            {"cache_read_tokens", 150},
            {"cache_write_tokens", 50}
        };

        auto usage = UsageTracker::calculate_usage(raw_usage, metadata);

        REQUIRE(usage.cache.read == 150);
        REQUIRE(usage.cache.write == 50);
    }
}

// ===== UsageTracker::calculate_cost Tests =====

TEST_CASE("UsageTracker calculate_cost", "[usage_tracker][cost]") {
    SECTION("Basic cost calculation") {
        TokenUsage usage;
        usage.input = 1000;
        usage.output = 500;

        nlohmann::json pricing = {
            {"input", 0.01},      // $0.01 per 1K input tokens
            {"output", 0.03}      // $0.03 per 1K output tokens
        };

        auto cost = UsageTracker::calculate_cost(pricing, usage);

        REQUIRE(cost.input_cost == Catch::Approx(0.01));   // 1K * $0.01
        REQUIRE(cost.output_cost == Catch::Approx(0.015)); // 0.5K * $0.03
        REQUIRE(cost.total_cost == Catch::Approx(0.025));
    }

    SECTION("Cost with cache tokens") {
        TokenUsage usage;
        usage.input = 1000;
        usage.output = 500;
        usage.cache.read = 500;
        usage.cache.write = 200;

        nlohmann::json pricing = {
            {"input", 0.01},
            {"output", 0.03},
            {"cache_read", 0.003},
            {"cache_write", 0.005}
        };

        auto cost = UsageTracker::calculate_cost(pricing, usage);

        REQUIRE(cost.input_cost == Catch::Approx(0.01));
        REQUIRE(cost.output_cost == Catch::Approx(0.015));
        REQUIRE(cost.cache_read_cost == Catch::Approx(0.0015));  // 0.5K * $0.003
        REQUIRE(cost.cache_write_cost == Catch::Approx(0.001));  // 0.2K * $0.005
        REQUIRE(cost.total_cost == Catch::Approx(0.0275));
    }

    SECTION("Cost with reasoning tokens") {
        TokenUsage usage;
        usage.input = 1000;
        usage.output = 500;
        usage.reasoning = 300;

        nlohmann::json pricing = {
            {"input", 0.01},
            {"output", 0.03},
            {"reasoning", 0.02}
        };

        auto cost = UsageTracker::calculate_cost(pricing, usage);

        REQUIRE(cost.reasoning_cost == Catch::Approx(0.006));  // 0.3K * $0.02
    }

    SECTION("Cost with ModelInfo") {
        provider::ModelInfo model;
        model.id = "claude-3-opus";
        model.pricing = {
            {"input", 0.015},
            {"output", 0.075}
        };

        TokenUsage usage;
        usage.input = 1000;
        usage.output = 500;

        auto cost = UsageTracker::calculate_cost(model, usage);

        REQUIRE(cost.input_cost == Catch::Approx(0.015));
        REQUIRE(cost.output_cost == Catch::Approx(0.0375));
    }

    SECTION("Empty pricing returns zero cost") {
        TokenUsage usage;
        usage.input = 1000;
        usage.output = 500;

        auto cost = UsageTracker::calculate_cost(nlohmann::json::object(), usage);

        REQUIRE(cost.input_cost == Catch::Approx(0.0));
        REQUIRE(cost.output_cost == Catch::Approx(0.0));
        REQUIRE(cost.total_cost == Catch::Approx(0.0));
    }
}

// ===== UsageTracker Instance Methods Tests =====

TEST_CASE("UsageTracker session management", "[usage_tracker][session]") {
    UsageTracker tracker;

    SECTION("add_usage basic") {
        TokenUsage usage{100, 50, 0, {0, 0}};
        tracker.add_usage("session-1", usage);

        auto session = tracker.get_session_usage("session-1");
        REQUIRE(session.tokens.input == 100);
        REQUIRE(session.tokens.output == 50);
        REQUIRE(session.message_count == 0);
    }

    SECTION("add_usage with cost") {
        TokenUsage usage{100, 50, 0, {0, 0}};
        CostInfo cost;
        cost.total_cost = 0.05;

        tracker.add_usage("session-1", usage, cost);

        auto session = tracker.get_session_usage("session-1");
        REQUIRE(session.cost.total_cost == Catch::Approx(0.05));
    }

    SECTION("add_usage with model") {
        provider::ModelInfo model;
        model.pricing = {{"input", 0.01}, {"output", 0.03}};

        TokenUsage usage{1000, 500, 0, {0, 0}};
        tracker.add_usage("session-1", usage, model);

        auto session = tracker.get_session_usage("session-1");
        REQUIRE(session.cost.input_cost == Catch::Approx(0.01));
        REQUIRE(session.cost.output_cost == Catch::Approx(0.015));
    }

    SECTION("accumulate multiple usages") {
        tracker.add_usage("session-1", {100, 50, 0, {0, 0}});
        tracker.add_usage("session-1", {200, 100, 0, {0, 0}});

        auto session = tracker.get_session_usage("session-1");
        REQUIRE(session.tokens.input == 300);
        REQUIRE(session.tokens.output == 150);
    }

    SECTION("increment counts") {
        tracker.increment_message_count("session-1");
        tracker.increment_message_count("session-1");
        tracker.increment_tool_calls("session-1");
        tracker.increment_request_count("session-1");
        tracker.increment_request_count("session-1");
        tracker.increment_request_count("session-1");

        auto session = tracker.get_session_usage("session-1");
        REQUIRE(session.message_count == 2);
        REQUIRE(session.tool_calls == 1);
        REQUIRE(session.request_count == 3);
    }

    SECTION("total usage across sessions") {
        tracker.add_usage("session-1", {100, 50, 0, {0, 0}});
        tracker.add_usage("session-2", {200, 100, 0, {0, 0}});

        auto total = tracker.get_total_usage();
        REQUIRE(total.tokens.input == 300);
        REQUIRE(total.tokens.output == 150);
    }

    SECTION("has_session") {
        REQUIRE_FALSE(tracker.has_session("session-1"));

        tracker.add_usage("session-1", {100, 50, 0, {0, 0}});

        REQUIRE(tracker.has_session("session-1"));
        REQUIRE_FALSE(tracker.has_session("session-2"));
    }

    SECTION("get_session_ids") {
        tracker.add_usage("session-1", {100, 50, 0, {0, 0}});
        tracker.add_usage("session-2", {200, 100, 0, {0, 0}});

        auto ids = tracker.get_session_ids();
        REQUIRE(ids.size() == 2);
    }

    SECTION("reset_session") {
        tracker.add_usage("session-1", {100, 50, 0, {0, 0}});
        tracker.increment_message_count("session-1");

        tracker.reset_session("session-1");

        auto session = tracker.get_session_usage("session-1");
        REQUIRE(session.tokens.input == 0);
        REQUIRE(session.message_count == 0);
        REQUIRE_FALSE(tracker.has_session("session-1"));
    }

    SECTION("reset_session updates total") {
        tracker.add_usage("session-1", {100, 50, 0, {0, 0}});
        tracker.add_usage("session-2", {200, 100, 0, {0, 0}});

        auto total_before = tracker.get_total_usage();
        REQUIRE(total_before.tokens.input == 300);

        tracker.reset_session("session-1");

        auto total_after = tracker.get_total_usage();
        REQUIRE(total_after.tokens.input == 200);
    }

    SECTION("reset_all") {
        tracker.add_usage("session-1", {100, 50, 0, {0, 0}});
        tracker.add_usage("session-2", {200, 100, 0, {0, 0}});

        tracker.reset_all();

        REQUIRE(tracker.session_count() == 0);
        auto total = tracker.get_total_usage();
        REQUIRE(total.tokens.input == 0);
    }

    SECTION("session_count") {
        REQUIRE(tracker.session_count() == 0);

        tracker.add_usage("session-1", {100, 50, 0, {0, 0}});
        REQUIRE(tracker.session_count() == 1);

        tracker.add_usage("session-2", {200, 100, 0, {0, 0}});
        REQUIRE(tracker.session_count() == 2);
    }
}

// ===== Integration Tests =====

TEST_CASE("UsageTracker full workflow", "[usage_tracker][integration]") {
    UsageTracker tracker;

    // Simulate a conversation
    provider::ModelInfo model;
    model.id = "gpt-4";
    model.pricing = {
        {"input", 0.03},
        {"output", 0.06}
    };

    // First message
    tracker.add_usage("conv-1", {500, 200, 0, {0, 0}}, model);
    tracker.increment_message_count("conv-1");
    tracker.increment_request_count("conv-1");

    // Second message (with tool call)
    tracker.add_usage("conv-1", {800, 300, 0, {0, 0}}, model);
    tracker.increment_message_count("conv-1");
    tracker.increment_tool_calls("conv-1");
    tracker.increment_request_count("conv-1");

    // Check session stats
    auto session = tracker.get_session_usage("conv-1");
    REQUIRE(session.tokens.input == 1300);
    REQUIRE(session.tokens.output == 500);
    REQUIRE(session.message_count == 2);
    REQUIRE(session.tool_calls == 1);
    REQUIRE(session.request_count == 2);

    // Check cost calculation
    // input: 1.3K * $0.03 = $0.039
    // output: 0.5K * $0.06 = $0.03
    // total: $0.069
    REQUIRE(session.cost.input_cost == Catch::Approx(0.039));
    REQUIRE(session.cost.output_cost == Catch::Approx(0.03));
    REQUIRE(session.cost.total_cost == Catch::Approx(0.069));

    // Check total
    auto total = tracker.get_total_usage();
    REQUIRE(total.tokens.input == 1300);
    REQUIRE(total.cost.total_cost == Catch::Approx(0.069));
}
