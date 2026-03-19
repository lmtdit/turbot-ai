#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/permission/permission.hpp>

using namespace turbot::core::agent;
using namespace turbot::core::permission;
using namespace turbot::test;

// ==================== AgentMode 转换测试 ====================

TEST_CASE("Agent.Mode.ToString", "[Agent]") {
    REQUIRE(agent_mode_to_string(AgentMode::Primary) == "primary");
    REQUIRE(agent_mode_to_string(AgentMode::Subagent) == "subagent");
    REQUIRE(agent_mode_to_string(AgentMode::All) == "all");
}

TEST_CASE("Agent.Mode.FromString", "[Agent]") {
    REQUIRE(string_to_agent_mode("primary") == AgentMode::Primary);
    REQUIRE(string_to_agent_mode("subagent") == AgentMode::Subagent);
    REQUIRE(string_to_agent_mode("all") == AgentMode::All);
}

TEST_CASE("Agent.Mode.InvalidString", "[Agent]") {
    REQUIRE_THROWS_AS(string_to_agent_mode("invalid"), std::invalid_argument);
}

// ==================== ModelRef 测试 ====================

TEST_CASE("Agent.ModelRef.Defaults.Enhanced", "[Agent]") {
    ModelRef ref;
    REQUIRE(ref.model_id.empty());
    REQUIRE(ref.provider_id.empty());
}

TEST_CASE("Agent.ModelRef.JsonSerialization.Enhanced", "[Agent]") {
    ModelRef ref;
    ref.model_id = "claude-3-opus";
    ref.provider_id = "anthropic";
    
    nlohmann::json j = ref.to_json();
    REQUIRE(j["model_id"] == "claude-3-opus");
    REQUIRE(j["provider_id"] == "anthropic");
    
    auto restored = ModelRef::from_json(j);
    REQUIRE(restored.model_id == "claude-3-opus");
    REQUIRE(restored.provider_id == "anthropic");
}

// ==================== AgentInfo 基础测试 ====================

TEST_CASE("Agent.Info.Defaults.Enhanced", "[Agent]") {
    AgentInfo info;
    REQUIRE(info.name.empty());
    REQUIRE_FALSE(info.description.has_value());
    REQUIRE(info.mode == AgentMode::Primary);
    REQUIRE(info.native == false);
    REQUIRE(info.hidden == false);
    REQUIRE(info.disable == false);
    REQUIRE_FALSE(info.model.has_value());
    REQUIRE_FALSE(info.prompt.has_value());
    REQUIRE_FALSE(info.temperature.has_value());
    REQUIRE_FALSE(info.top_p.has_value());
    REQUIRE_FALSE(info.steps.has_value());
}

TEST_CASE("Agent.Info.WithBasicFields", "[Agent]") {
    AgentInfo info;
    info.name = "test-agent";
    info.description = "Test agent description";
    info.mode = AgentMode::Subagent;
    info.native = true;
    info.hidden = true;
    
    REQUIRE(info.name == "test-agent");
    REQUIRE(info.description == "Test agent description");
    REQUIRE(info.mode == AgentMode::Subagent);
    REQUIRE(info.native == true);
    REQUIRE(info.hidden == true);
}

// ==================== AgentInfo 验证测试 ====================

TEST_CASE("Agent.Info.Validate.Temperature", "[Agent]") {
    AgentInfo info;
    info.name = "temp-test";
    
    // Valid temperature
    info.temperature = 0.5;
    REQUIRE(info.validate());
    
    info.temperature = 0.0;
    REQUIRE(info.validate());
    
    info.temperature = 2.0;
    REQUIRE(info.validate());
    
    // Invalid temperature
    info.temperature = -0.1;
    REQUIRE_FALSE(info.validate());
    
    info.temperature = 2.1;
    REQUIRE_FALSE(info.validate());
}

TEST_CASE("Agent.Info.Validate.TopP", "[Agent]") {
    AgentInfo info;
    info.name = "topp-test";
    
    // Valid top_p
    info.top_p = 0.5;
    REQUIRE(info.validate());
    
    info.top_p = 0.0;
    REQUIRE(info.validate());
    
    info.top_p = 1.0;
    REQUIRE(info.validate());
    
    // Invalid top_p
    info.top_p = -0.1;
    REQUIRE_FALSE(info.validate());
    
    info.top_p = 1.1;
    REQUIRE_FALSE(info.validate());
}

TEST_CASE("Agent.Info.Validate.Steps", "[Agent]") {
    AgentInfo info;
    info.name = "steps-test";
    
    // Valid steps
    info.steps = 1;
    REQUIRE(info.validate());
    
    info.steps = 100;
    REQUIRE(info.validate());
    
    // Invalid steps
    info.steps = 0;
    REQUIRE_FALSE(info.validate());
    
    info.steps = -1;
    REQUIRE_FALSE(info.validate());
}

TEST_CASE("Agent.Info.Validate.MultipleFields", "[Agent]") {
    AgentInfo info;
    info.name = "multi-validate";
    info.temperature = 0.7;
    info.top_p = 0.9;
    info.steps = 10;
    
    REQUIRE(info.validate());
}

// ==================== AgentInfo JSON 序列化测试 ====================

TEST_CASE("Agent.Info.JsonSerialization.Basic", "[Agent]") {
    AgentInfo info;
    info.name = "json-agent";
    info.description = "JSON test agent";
    info.mode = AgentMode::Primary;
    info.native = true;
    
    nlohmann::json j = info.to_json();
    REQUIRE(j["name"] == "json-agent");
    REQUIRE(j["description"] == "JSON test agent");
    REQUIRE(j["mode"] == "primary");
    REQUIRE(j["native"] == true);
    
    auto restored = AgentInfo::from_json(j);
    REQUIRE(restored.name == "json-agent");
    REQUIRE(restored.description == "JSON test agent");
    REQUIRE(restored.mode == AgentMode::Primary);
    REQUIRE(restored.native == true);
}

TEST_CASE("Agent.Info.JsonSerialization.WithModel", "[Agent]") {
    AgentInfo info;
    info.name = "model-agent";
    info.model = ModelRef{"gpt-4", "openai"};
    
    nlohmann::json j = info.to_json();
    REQUIRE(j.contains("model"));
    REQUIRE(j["model"]["model_id"] == "gpt-4");
    
    auto restored = AgentInfo::from_json(j);
    REQUIRE(restored.model.has_value());
    REQUIRE(restored.model->model_id == "gpt-4");
    REQUIRE(restored.model->provider_id == "openai");
}

TEST_CASE("Agent.Info.JsonSerialization.WithPrompt", "[Agent]") {
    AgentInfo info;
    info.name = "prompt-agent";
    info.prompt = "You are a helpful assistant.";
    
    nlohmann::json j = info.to_json();
    REQUIRE(j["prompt"] == "You are a helpful assistant.");
    
    auto restored = AgentInfo::from_json(j);
    REQUIRE(restored.prompt == "You are a helpful assistant.");
}

TEST_CASE("Agent.Info.JsonSerialization.WithTemperature", "[Agent]") {
    AgentInfo info;
    info.name = "temp-agent";
    info.temperature = 0.8;
    
    nlohmann::json j = info.to_json();
    REQUIRE(j["temperature"] == 0.8);
    
    auto restored = AgentInfo::from_json(j);
    REQUIRE(restored.temperature == 0.8);
}

TEST_CASE("Agent.Info.JsonSerialization.WithTopP", "[Agent]") {
    AgentInfo info;
    info.name = "topp-agent";
    info.top_p = 0.95;
    
    nlohmann::json j = info.to_json();
    REQUIRE(j["top_p"] == 0.95);
    
    auto restored = AgentInfo::from_json(j);
    REQUIRE(restored.top_p == 0.95);
}

TEST_CASE("Agent.Info.JsonSerialization.WithSteps", "[Agent]") {
    AgentInfo info;
    info.name = "steps-agent";
    info.steps = 25;
    
    nlohmann::json j = info.to_json();
    REQUIRE(j["steps"] == 25);
    
    auto restored = AgentInfo::from_json(j);
    REQUIRE(restored.steps == 25);
}

TEST_CASE("Agent.Info.JsonSerialization.WithColor", "[Agent]") {
    AgentInfo info;
    info.name = "color-agent";
    info.color = "#FF5500";
    
    nlohmann::json j = info.to_json();
    REQUIRE(j["color"] == "#FF5500");
    
    auto restored = AgentInfo::from_json(j);
    REQUIRE(restored.color == "#FF5500");
}

TEST_CASE("Agent.Info.JsonSerialization.WithVariant", "[Agent]") {
    AgentInfo info;
    info.name = "variant-agent";
    info.variant = "extended";
    
    nlohmann::json j = info.to_json();
    REQUIRE(j["variant"] == "extended");
    
    auto restored = AgentInfo::from_json(j);
    REQUIRE(restored.variant == "extended");
}

TEST_CASE("Agent.Info.JsonSerialization.WithDisable", "[Agent]") {
    AgentInfo info;
    info.name = "disable-agent";
    info.disable = true;
    
    nlohmann::json j = info.to_json();
    REQUIRE(j["disable"] == true);
    
    auto restored = AgentInfo::from_json(j);
    REQUIRE(restored.disable == true);
}

TEST_CASE("Agent.Info.JsonSerialization.WithPermission", "[Agent]") {
    AgentInfo info;
    info.name = "perm-agent";
    info.permission = {
        PermissionRule{"bash", "*", PermissionAction::Allow},
        PermissionRule{"edit", "*", PermissionAction::Ask}
    };
    
    nlohmann::json j = info.to_json();
    REQUIRE(j.contains("permission"));
    REQUIRE(j["permission"].size() == 2);
    
    auto restored = AgentInfo::from_json(j);
    REQUIRE(restored.permission.size() == 2);
}

TEST_CASE("Agent.Info.JsonSerialization.WithOptions", "[Agent]") {
    AgentInfo info;
    info.name = "options-agent";
    info.options = R"({
        "custom_option": "value",
        "nested": {"key": "val"}
    })"_json;
    
    nlohmann::json j = info.to_json();
    REQUIRE(j.contains("options"));
    REQUIRE(j["options"]["custom_option"] == "value");
    
    auto restored = AgentInfo::from_json(j);
    REQUIRE(restored.options["custom_option"] == "value");
}

// ==================== AgentInfo 完整测试 ====================

TEST_CASE("Agent.Info.FullSerialization", "[Agent]") {
    AgentInfo info;
    info.name = "full-agent";
    info.description = "Full featured agent";
    info.mode = AgentMode::Subagent;
    info.native = true;
    info.hidden = false;
    info.disable = false;
    info.model = ModelRef{"claude-3-opus", "anthropic"};
    info.prompt = "You are a coding assistant.";
    info.temperature = 0.7;
    info.top_p = 0.9;
    info.steps = 30;
    info.color = "#00FF00";
    info.variant = "standard";
    
    nlohmann::json j = info.to_json();
    
    auto restored = AgentInfo::from_json(j);
    REQUIRE(restored.name == "full-agent");
    REQUIRE(restored.description == "Full featured agent");
    REQUIRE(restored.mode == AgentMode::Subagent);
    REQUIRE(restored.native == true);
    REQUIRE(restored.model->model_id == "claude-3-opus");
    REQUIRE(restored.prompt == "You are a coding assistant.");
    REQUIRE(restored.temperature == 0.7);
    REQUIRE(restored.top_p == 0.9);
    REQUIRE(restored.steps == 30);
    REQUIRE(restored.color == "#00FF00");
    REQUIRE(restored.variant == "standard");
    REQUIRE(restored.validate());
}

// ==================== AgentRegistry 测试 ====================

TEST_CASE("Agent.Registry.Instance.Enhanced", "[Agent]") {
    auto& registry1 = AgentRegistry::instance();
    auto& registry2 = AgentRegistry::instance();
    REQUIRE(&registry1 == &registry2);
}

TEST_CASE("Agent.Registry.List", "[Agent]") {
    // 确保有有效的当前目录
    try {
        std::filesystem::current_path(std::filesystem::path("/tmp"));
    } catch (...) {}
    
    auto& registry = AgentRegistry::instance();
    
    // Clear and initialize built-in agents
    registry.clear();
    agent_loader::initialize_builtin_agents();
    
    auto agents = registry.list();
    REQUIRE_FALSE(agents.empty());
    
    // Should have at least build, plan, explore
    REQUIRE(registry.has("build"));
}

TEST_CASE("Agent.Registry.Clear", "[Agent]") {
    // 确保有有效的当前目录
    try {
        std::filesystem::current_path(std::filesystem::path("/tmp"));
    } catch (...) {}
    
    auto& registry = AgentRegistry::instance();
    
    // Initialize agents
    agent_loader::initialize_builtin_agents();
    REQUIRE(registry.size() > 0);
    
    registry.clear();
    REQUIRE(registry.size() == 0);
}

TEST_CASE("Agent.Registry.ListByMode", "[Agent]") {
    // 确保有有效的当前目录
    try {
        std::filesystem::current_path(std::filesystem::path("/tmp"));
    } catch (...) {}
    
    auto& registry = AgentRegistry::instance();
    registry.clear();
    agent_loader::initialize_builtin_agents();
    
    auto primary_agents = registry.list_by_mode(AgentMode::Primary);
    REQUIRE_FALSE(primary_agents.empty());
    
    // All returned agents should be primary mode
    for (const auto& agent : primary_agents) {
        REQUIRE(agent->mode() == AgentMode::Primary);
    }
}

TEST_CASE("Agent.Registry.ListVisible", "[Agent]") {
    // 确保有有效的当前目录
    try {
        std::filesystem::current_path(std::filesystem::path("/tmp"));
    } catch (...) {}
    
    auto& registry = AgentRegistry::instance();
    registry.clear();
    agent_loader::initialize_builtin_agents();
    
    auto visible_agents = registry.list_visible();
    REQUIRE_FALSE(visible_agents.empty());
    
    // All returned agents should be non-hidden
    for (const auto& agent : visible_agents) {
        REQUIRE_FALSE(agent->is_hidden());
    }
}

TEST_CASE("Agent.Registry.DefaultAgent", "[Agent]") {
    // 确保有有效的当前目录
    try {
        std::filesystem::current_path(std::filesystem::path("/tmp"));
    } catch (...) {}
    
    auto& registry = AgentRegistry::instance();
    registry.clear();
    agent_loader::initialize_builtin_agents();
    
    auto default_name = registry.default_agent();
    REQUIRE_FALSE(default_name.empty());
    REQUIRE(registry.has(default_name));
}
