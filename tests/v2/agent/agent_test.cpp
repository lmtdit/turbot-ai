#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/agent/agent.hpp>

using namespace turbot::core::agent;
using namespace turbot::test;

// ==================== ModelRef 测试 ====================

TEST_CASE("Agent.ModelRef.Defaults", "[Agent]") {
    ModelRef ref;
    REQUIRE(ref.model_id.empty());
    REQUIRE(ref.provider_id.empty());
}

TEST_CASE("Agent.ModelRef.WithValues", "[Agent]") {
    ModelRef ref;
    ref.model_id = "claude-3-opus";
    ref.provider_id = "anthropic";
    
    REQUIRE(ref.model_id == "claude-3-opus");
    REQUIRE(ref.provider_id == "anthropic");
}

TEST_CASE("Agent.ModelRef.JsonSerialization", "[Agent]") {
    ModelRef ref;
    ref.model_id = "gpt-4";
    ref.provider_id = "openai";
    
    nlohmann::json j = ref.to_json();
    REQUIRE(j["model_id"] == "gpt-4");
    REQUIRE(j["provider_id"] == "openai");
    
    auto restored = ModelRef::from_json(j);
    REQUIRE(restored.model_id == ref.model_id);
    REQUIRE(restored.provider_id == ref.provider_id);
}

TEST_CASE("Agent.ModelRef.Equality", "[Agent]") {
    ModelRef ref1{"gpt-4", "openai"};
    ModelRef ref2{"gpt-4", "openai"};
    ModelRef ref3{"claude-3", "anthropic"};
    
    REQUIRE(ref1 == ref2);
    REQUIRE_FALSE(ref1 == ref3);
}

// ==================== AgentInfo 测试 ====================

TEST_CASE("Agent.Info.Defaults", "[Agent]") {
    AgentInfo info;
    REQUIRE(info.name.empty());
    REQUIRE_FALSE(info.model.has_value());
    REQUIRE_FALSE(info.prompt.has_value());
    REQUIRE(info.mode == AgentMode::Primary);
    REQUIRE_FALSE(info.native);
    REQUIRE_FALSE(info.hidden);
    REQUIRE_FALSE(info.disable);
}

TEST_CASE("Agent.Info.WithValues", "[Agent]") {
    AgentInfo info;
    info.name = "build";
    info.model = ModelRef{"claude-3-opus", "anthropic"};
    info.prompt = "You are a helpful assistant";
    info.mode = AgentMode::Primary;
    
    REQUIRE(info.name == "build");
    REQUIRE(info.model.has_value());
    REQUIRE(info.model->model_id == "claude-3-opus");
    REQUIRE(info.prompt == "You are a helpful assistant");
}

TEST_CASE("Agent.Info.JsonSerialization", "[Agent]") {
    AgentInfo info;
    info.name = "test-agent";
    info.model = ModelRef{"gpt-4", "openai"};
    info.prompt = "Test prompt";
    
    nlohmann::json j = info.to_json();
    REQUIRE(j["name"] == "test-agent");
    REQUIRE(j["model"]["model_id"] == "gpt-4");
    REQUIRE(j["prompt"] == "Test prompt");
    
    auto restored = AgentInfo::from_json(j);
    REQUIRE(restored.name == info.name);
    REQUIRE(restored.model->model_id == info.model->model_id);
}

TEST_CASE("Agent.Info.Validation", "[Agent]") {
    AgentInfo info;
    info.name = "valid-agent";
    info.temperature = 0.7;
    info.top_p = 0.9;
    info.steps = 10;
    
    REQUIRE(info.validate());
}

// ==================== AgentMode 测试 ====================

TEST_CASE("Agent.Mode.Conversion", "[Agent]") {
    REQUIRE(agent_mode_to_string(AgentMode::Primary) == "primary");
    REQUIRE(agent_mode_to_string(AgentMode::Subagent) == "subagent");
    REQUIRE(agent_mode_to_string(AgentMode::All) == "all");
    
    REQUIRE(string_to_agent_mode("primary") == AgentMode::Primary);
    REQUIRE(string_to_agent_mode("subagent") == AgentMode::Subagent);
    REQUIRE(string_to_agent_mode("all") == AgentMode::All);
}

// ==================== AgentRegistry 测试 ====================

TEST_CASE("Agent.Registry.Instance", "[Agent]") {
    auto& registry1 = AgentRegistry::instance();
    auto& registry2 = AgentRegistry::instance();
    REQUIRE(&registry1 == &registry2);
}

TEST_CASE("Agent.Registry.Names", "[Agent]") {
    auto& registry = AgentRegistry::instance();
    
    auto names = registry.names();
    // 验证返回的是名称列表
    for (const auto& name : names) {
        REQUIRE_FALSE(name.empty());
    }
}
