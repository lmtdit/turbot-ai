#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/session/usage_tracker.hpp>
#include <turbot/core/provider/provider.hpp>
#include <turbot/core/message/token_usage.hpp>

using namespace turbot::core::session;
using namespace turbot::core::provider;
using namespace turbot::core;
using namespace turbot::test;

// ==================== CostInfo 测试 ====================

TEST_CASE("UsageTracker.CostInfo.Defaults", "[UsageTracker]") {
    CostInfo info;
    REQUIRE(info.input_cost == 0.0);
    REQUIRE(info.output_cost == 0.0);
    REQUIRE(info.cache_read_cost == 0.0);
    REQUIRE(info.cache_write_cost == 0.0);
    REQUIRE(info.reasoning_cost == 0.0);
    REQUIRE(info.total_cost == 0.0);
}

TEST_CASE("UsageTracker.CostInfo.ToJson", "[UsageTracker]") {
    CostInfo info;
    info.input_cost = 0.01;
    info.output_cost = 0.02;
    info.total_cost = 0.03;
    
    nlohmann::json j = info.to_json();
    REQUIRE(j["input_cost"] == 0.01);
    REQUIRE(j["output_cost"] == 0.02);
    REQUIRE(j["total_cost"] == 0.03);
}

TEST_CASE("UsageTracker.CostInfo.FromJson", "[UsageTracker]") {
    nlohmann::json j = R"({
        "input_cost": 0.05,
        "output_cost": 0.10,
        "cache_read_cost": 0.01,
        "cache_write_cost": 0.02,
        "reasoning_cost": 0.03,
        "total_cost": 0.21
    })"_json;
    
    auto info = CostInfo::from_json(j);
    REQUIRE(info.input_cost == 0.05);
    REQUIRE(info.output_cost == 0.10);
    REQUIRE(info.cache_read_cost == 0.01);
    REQUIRE(info.cache_write_cost == 0.02);
    REQUIRE(info.reasoning_cost == 0.03);
    REQUIRE(info.total_cost == 0.21);
}

TEST_CASE("UsageTracker.CostInfo.PlusEquals", "[UsageTracker]") {
    CostInfo a;
    a.input_cost = 0.01;
    a.output_cost = 0.02;
    a.total_cost = 0.03;
    
    CostInfo b;
    b.input_cost = 0.05;
    b.output_cost = 0.10;
    b.total_cost = 0.15;
    
    a += b;
    REQUIRE(a.input_cost == 0.06);
    REQUIRE(a.output_cost == 0.12);
    REQUIRE(a.total_cost == 0.18);
}

TEST_CASE("UsageTracker.CostInfo.MinusEquals", "[UsageTracker]") {
    CostInfo a;
    a.input_cost = 0.10;
    a.output_cost = 0.20;
    a.total_cost = 0.30;
    
    CostInfo b;
    b.input_cost = 0.03;
    b.output_cost = 0.05;
    b.total_cost = 0.08;
    
    a -= b;
    REQUIRE(a.input_cost == 0.07);
    REQUIRE(a.output_cost == 0.15);
    REQUIRE(a.total_cost == 0.22);
}

// ==================== SessionUsage 测试 ====================

TEST_CASE("UsageTracker.SessionUsage.Defaults", "[UsageTracker]") {
    SessionUsage usage;
    REQUIRE(usage.message_count == 0);
    REQUIRE(usage.tool_calls == 0);
    REQUIRE(usage.request_count == 0);
}

TEST_CASE("UsageTracker.SessionUsage.ToJson", "[UsageTracker]") {
    SessionUsage usage;
    usage.tokens.input = 100;
    usage.tokens.output = 50;
    usage.cost.total_cost = 0.05;
    usage.message_count = 5;
    usage.tool_calls = 3;
    usage.request_count = 2;
    
    nlohmann::json j = usage.to_json();
    REQUIRE(j["tokens"]["input"] == 100);
    REQUIRE(j["tokens"]["output"] == 50);
    REQUIRE(j["cost"]["total_cost"] == 0.05);
    REQUIRE(j["message_count"] == 5);
    REQUIRE(j["tool_calls"] == 3);
    REQUIRE(j["request_count"] == 2);
}

TEST_CASE("UsageTracker.SessionUsage.FromJson", "[UsageTracker]") {
    nlohmann::json j = R"({
        "tokens": {"input": 200, "output": 100, "total": 300},
        "cost": {"total_cost": 0.10},
        "message_count": 10,
        "tool_calls": 5,
        "request_count": 3
    })"_json;
    
    auto usage = SessionUsage::from_json(j);
    REQUIRE(usage.tokens.input == 200);
    REQUIRE(usage.tokens.output == 100);
    REQUIRE(usage.cost.total_cost == 0.10);
    REQUIRE(usage.message_count == 10);
    REQUIRE(usage.tool_calls == 5);
    REQUIRE(usage.request_count == 3);
}

TEST_CASE("UsageTracker.SessionUsage.PlusEquals", "[UsageTracker]") {
    SessionUsage a;
    a.tokens.input = 100;
    a.message_count = 5;
    a.tool_calls = 2;
    
    SessionUsage b;
    b.tokens.input = 50;
    b.message_count = 3;
    b.tool_calls = 1;
    
    a += b;
    REQUIRE(a.tokens.input == 150);
    REQUIRE(a.message_count == 8);
    REQUIRE(a.tool_calls == 3);
}

// ==================== UsageTracker.calculate_usage 测试 ====================

TEST_CASE("UsageTracker.CalculateUsage.OpenAIFormat", "[UsageTracker]") {
    nlohmann::json raw_usage = R"({
        "prompt_tokens": 100,
        "completion_tokens": 50
    })"_json;
    
    auto usage = UsageTracker::calculate_usage(raw_usage, std::nullopt);
    REQUIRE(usage.input == 100);
    REQUIRE(usage.output == 50);
}

TEST_CASE("UsageTracker.CalculateUsage.AnthropicFormat", "[UsageTracker]") {
    nlohmann::json raw_usage = R"({
        "input_tokens": 200,
        "output_tokens": 100
    })"_json;
    
    auto usage = UsageTracker::calculate_usage(raw_usage, std::nullopt);
    REQUIRE(usage.input == 200);
    REQUIRE(usage.output == 100);
}

TEST_CASE("UsageTracker.CalculateUsage.CamelCaseFormat", "[UsageTracker]") {
    nlohmann::json raw_usage = R"({
        "promptTokens": 150,
        "completionTokens": 75
    })"_json;
    
    auto usage = UsageTracker::calculate_usage(raw_usage, std::nullopt);
    REQUIRE(usage.input == 150);
    REQUIRE(usage.output == 75);
}

TEST_CASE("UsageTracker.CalculateUsage.WithMetadata", "[UsageTracker]") {
    nlohmann::json raw_usage = R"({
        "input_tokens": 100,
        "output_tokens": 50
    })"_json;
    
    nlohmann::json metadata = R"({
        "reasoning_tokens": 20,
        "cache_read_tokens": 30,
        "cache_write_tokens": 10
    })"_json;
    
    auto usage = UsageTracker::calculate_usage(raw_usage, metadata);
    REQUIRE(usage.input == 100);
    REQUIRE(usage.output == 50);
    REQUIRE(usage.reasoning == 20);
    REQUIRE(usage.cache.read == 30);
    REQUIRE(usage.cache.write == 10);
}

TEST_CASE("UsageTracker.CalculateUsage.AnthropicCacheTokens", "[UsageTracker]") {
    nlohmann::json raw_usage = R"({
        "input_tokens": 100,
        "output_tokens": 50,
        "cache_read_input_tokens": 40,
        "cache_creation_input_tokens": 15
    })"_json;
    
    auto usage = UsageTracker::calculate_usage(raw_usage, std::nullopt);
    REQUIRE(usage.input == 100);
    REQUIRE(usage.output == 50);
    REQUIRE(usage.cache.read == 40);
    REQUIRE(usage.cache.write == 15);
}

TEST_CASE("UsageTracker.CalculateUsage.Empty", "[UsageTracker]") {
    nlohmann::json raw_usage = nlohmann::json::object();
    
    auto usage = UsageTracker::calculate_usage(raw_usage, std::nullopt);
    REQUIRE(usage.input == 0);
    REQUIRE(usage.output == 0);
}

// ==================== TokenUsage 测试 ====================

TEST_CASE("UsageTracker.TokenUsage.Defaults", "[UsageTracker]") {
    TokenUsage usage;
    REQUIRE(usage.input == 0);
    REQUIRE(usage.output == 0);
    REQUIRE(usage.total() == 0);
    REQUIRE(usage.reasoning == 0);
    REQUIRE(usage.cache.read == 0);
    REQUIRE(usage.cache.write == 0);
}

TEST_CASE("UsageTracker.TokenUsage.ToJson", "[UsageTracker]") {
    TokenUsage usage;
    usage.input = 100;
    usage.output = 50;
    usage.reasoning = 20;
    usage.cache.read = 30;
    usage.cache.write = 10;
    
    nlohmann::json j = usage.to_json();
    REQUIRE(j["input"] == 100);
    REQUIRE(j["output"] == 50);
    REQUIRE(j["reasoning"] == 20);
    REQUIRE(j["cache"]["read"] == 30);
    REQUIRE(j["cache"]["write"] == 10);
}

TEST_CASE("UsageTracker.TokenUsage.FromJson", "[UsageTracker]") {
    nlohmann::json j = R"({
        "input": 200,
        "output": 100,
        "reasoning": 25,
        "cache": {"read": 40, "write": 20}
    })"_json;
    
    auto usage = TokenUsage::from_json(j);
    REQUIRE(usage.input == 200);
    REQUIRE(usage.output == 100);
    REQUIRE(usage.total() == 385);
    REQUIRE(usage.reasoning == 25);
    REQUIRE(usage.cache.read == 40);
    REQUIRE(usage.cache.write == 20);
}

TEST_CASE("UsageTracker.TokenUsage.PlusEquals", "[UsageTracker]") {
    TokenUsage a;
    a.input = 100;
    a.output = 50;
    
    TokenUsage b;
    b.input = 50;
    b.output = 25;
    
    a += b;
    REQUIRE(a.input == 150);
    REQUIRE(a.output == 75);
    REQUIRE(a.total() == 225);
}

TEST_CASE("UsageTracker.TokenUsage.Cost", "[UsageTracker]") {
    TokenUsage usage;
    usage.input = 1000;
    usage.output = 500;
    usage.cache.read = 200;
    
    nlohmann::json pricing = R"({
        "input": 0.01,
        "output": 0.03,
        "cache_read": 0.005
    })"_json;
    
    double cost = usage.cost(pricing);
    // 1000 * 0.01 + 500 * 0.03 + 200 * 0.005 = 10 + 15 + 1 = 26
    REQUIRE(cost == Catch::Approx(26.0));
}
