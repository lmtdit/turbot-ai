/**
 * @file agent_generator_test.cpp
 * @brief Agent generator tests for agent_generator namespace
 *
 * Tests for:
 * - AgentGenerateResult::to_agent_info() - conversion to AgentInfo
 * - AgentGenerateResult::to_json() - JSON serialization
 * - agent_generator::generate() - fallback path (no provider)
 */

#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/agent/builtin/configurable_agent.hpp>
#include <turbot/core/provider/provider_manager.hpp>

using namespace turbot::core::agent;
using namespace turbot::core::provider;
using namespace turbot::test;

// ==================== AgentGenerateResult Tests ====================

TEST_CASE("Agent.Generator.Result.DefaultConstruction", "[Agent][Generator]") {
    AgentGenerateResult result;
    
    REQUIRE(result.identifier.empty());
    REQUIRE(result.when_to_use.empty());
    REQUIRE(result.system_prompt.empty());
}

TEST_CASE("Agent.Generator.Result.ToAgentInfo.Basic", "[Agent][Generator]") {
    AgentGenerateResult result;
    result.identifier = "test_agent";
    result.when_to_use = "For testing purposes";
    result.system_prompt = "You are a test agent.";
    
    auto info = result.to_agent_info();
    
    REQUIRE(info.name == "test_agent");
    REQUIRE(info.description == "For testing purposes");
    REQUIRE(info.prompt == "You are a test agent.");
}

TEST_CASE("Agent.Generator.Result.ToAgentInfo.Mode", "[Agent][Generator]") {
    AgentGenerateResult result;
    result.identifier = "custom_agent";
    result.when_to_use = "Custom agent description";
    result.system_prompt = "Custom system prompt";
    
    auto info = result.to_agent_info();
    
    // Default mode should be Primary
    REQUIRE(info.mode == AgentMode::Primary);
}

TEST_CASE("Agent.Generator.Result.ToAgentInfo.Native", "[Agent][Generator]") {
    AgentGenerateResult result;
    result.identifier = "generated_agent";
    result.when_to_use = "Generated agent";
    result.system_prompt = "You are a generated agent.";
    
    auto info = result.to_agent_info();
    
    // Generated agents are not native
    REQUIRE(info.native == false);
}

TEST_CASE("Agent.Generator.Result.ToAgentInfo.Hidden", "[Agent][Generator]") {
    AgentGenerateResult result;
    result.identifier = "visible_agent";
    result.when_to_use = "Visible agent";
    result.system_prompt = "You are visible.";
    
    auto info = result.to_agent_info();
    
    // Generated agents are not hidden by default
    REQUIRE(info.hidden == false);
}

TEST_CASE("Agent.Generator.Result.ToJson.Basic", "[Agent][Generator]") {
    AgentGenerateResult result;
    result.identifier = "my_agent";
    result.when_to_use = "Use for X";
    result.system_prompt = "You are X expert.";
    
    auto j = result.to_json();
    
    REQUIRE(j.contains("identifier"));
    REQUIRE(j.contains("when_to_use"));
    REQUIRE(j.contains("system_prompt"));
    REQUIRE(j["identifier"] == "my_agent");
    REQUIRE(j["when_to_use"] == "Use for X");
    REQUIRE(j["system_prompt"] == "You are X expert.");
}

TEST_CASE("Agent.Generator.Result.ToJson.AllFields", "[Agent][Generator]") {
    AgentGenerateResult result;
    result.identifier = "full_agent";
    result.when_to_use = "Full featured agent";
    result.system_prompt = "You are a full featured agent with many capabilities.";
    
    auto j = result.to_json();
    
    REQUIRE(j["identifier"] == "full_agent");
    REQUIRE(j["when_to_use"] == "Full featured agent");
    REQUIRE(j["system_prompt"] == "You are a full featured agent with many capabilities.");
    
    // Verify all required keys are present
    REQUIRE(j.size() == 3);
}

// ==================== agent_generator::generate Tests (Fallback) ====================

TEST_CASE("Agent.Generator.Generate.FallbackNoProvider", "[Agent][Generator]") {
    // Clear provider manager to test fallback
    auto& pm = ProviderManager::instance();
    pm.clear();
    
    agent_generator::GenerateParams params;
    params.description = "analyze Python code quality";
    
    auto result = agent_generator::generate(params);
    
    // Should return a valid result even without provider
    REQUIRE_FALSE(result.identifier.empty());
    REQUIRE_FALSE(result.when_to_use.empty());
    REQUIRE_FALSE(result.system_prompt.empty());
}

TEST_CASE("Agent.Generator.Generate.Fallback.IdentifierSlugified", "[Agent][Generator]") {
    auto& pm = ProviderManager::instance();
    pm.clear();
    
    agent_generator::GenerateParams params;
    params.description = "Code Review and Optimization";
    
    auto result = agent_generator::generate(params);
    
    // Identifier should be slugified (no spaces)
    REQUIRE(result.identifier.find(' ') == std::string::npos);
}

TEST_CASE("Agent.Generator.Generate.Fallback.DescriptionIncluded", "[Agent][Generator]") {
    auto& pm = ProviderManager::instance();
    pm.clear();
    
    agent_generator::GenerateParams params;
    params.description = "Database migration assistant";
    
    auto result = agent_generator::generate(params);
    
    // The description should be referenced in the result
    REQUIRE(result.when_to_use.find("Database migration assistant") != std::string::npos);
}

TEST_CASE("Agent.Generator.Generate.Fallback.SystemPromptIncluded", "[Agent][Generator]") {
    auto& pm = ProviderManager::instance();
    pm.clear();
    
    agent_generator::GenerateParams params;
    params.description = "API documentation generator";
    
    auto result = agent_generator::generate(params);
    
    // The system prompt should reference the description
    REQUIRE(result.system_prompt.find("API documentation generator") != std::string::npos);
}

TEST_CASE("Agent.Generator.Generate.Fallback.WithModel", "[Agent][Generator]") {
    auto& pm = ProviderManager::instance();
    pm.clear();
    
    agent_generator::GenerateParams params;
    params.description = "Test agent with model";
    params.model = ModelRef{"gpt-4", "openai"};
    
    auto result = agent_generator::generate(params);
    
    // Should still return a valid result
    REQUIRE_FALSE(result.identifier.empty());
    REQUIRE_FALSE(result.when_to_use.empty());
    REQUIRE_FALSE(result.system_prompt.empty());
}

TEST_CASE("Agent.Generator.Generate.Fallback.EmptyDescription", "[Agent][Generator]") {
    auto& pm = ProviderManager::instance();
    pm.clear();
    
    agent_generator::GenerateParams params;
    params.description = "";
    
    auto result = agent_generator::generate(params);
    
    // Should handle empty description gracefully
    // The identifier might be empty or a placeholder
    REQUIRE(result.system_prompt.length() > 0);
}

// ==================== AgentGenerateResult Round-trip Tests ====================

TEST_CASE("Agent.Generator.RoundTrip.ToAgentInfoToJson", "[Agent][Generator]") {
    AgentGenerateResult result;
    result.identifier = "round_trip_agent";
    result.when_to_use = "For round trip testing";
    result.system_prompt = "You are a round trip test agent.";
    
    // Convert to AgentInfo
    auto info = result.to_agent_info();
    
    // Verify conversion
    REQUIRE(info.name == "round_trip_agent");
    REQUIRE(info.description == "For round trip testing");
    REQUIRE(info.prompt == "You are a round trip test agent.");
    
    // Convert AgentInfo to JSON
    auto j = info.to_json();
    
    REQUIRE(j["name"] == "round_trip_agent");
    REQUIRE(j["description"] == "For round trip testing");
    REQUIRE(j["prompt"] == "You are a round trip test agent.");
}

TEST_CASE("Agent.Generator.RoundTrip.ToJsonToAgentInfo", "[Agent][Generator]") {
    AgentGenerateResult result;
    result.identifier = "json_round_trip";
    result.when_to_use = "JSON round trip agent";
    result.system_prompt = "Testing JSON serialization.";
    
    // Convert to JSON
    auto j = result.to_json();
    
    // Verify JSON structure
    REQUIRE(j["identifier"] == "json_round_trip");
    REQUIRE(j["when_to_use"] == "JSON round trip agent");
    REQUIRE(j["system_prompt"] == "Testing JSON serialization.");
    
    // Convert to AgentInfo
    auto info = result.to_agent_info();
    
    REQUIRE(info.name == "json_round_trip");
    REQUIRE(info.description == "JSON round trip agent");
    REQUIRE(info.prompt == "Testing JSON serialization.");
}

// ==================== Edge Cases ====================

TEST_CASE("Agent.Generator.Result.SpecialCharacters", "[Agent][Generator]") {
    AgentGenerateResult result;
    result.identifier = "special_agent_123";
    result.when_to_use = "Agent with special chars: @#$%^&*()";
    result.system_prompt = "You handle special characters: <>&\"'";
    
    auto info = result.to_agent_info();
    
    REQUIRE(info.name == "special_agent_123");
    REQUIRE(info.description == "Agent with special chars: @#$%^&*()");
    REQUIRE(info.prompt == "You handle special characters: <>&\"'");
}

TEST_CASE("Agent.Generator.Result.UnicodeCharacters", "[Agent][Generator]") {
    AgentGenerateResult result;
    result.identifier = "unicode_agent";
    result.when_to_use = "Unicode: 你好世界";
    result.system_prompt = "Emoji: 🚀🎯💻";
    
    auto info = result.to_agent_info();
    
    REQUIRE(info.name == "unicode_agent");
    REQUIRE(info.description == "Unicode: 你好世界");
    REQUIRE(info.prompt == "Emoji: 🚀🎯💻");
}

TEST_CASE("Agent.Generator.Result.LongContent", "[Agent][Generator]") {
    AgentGenerateResult result;
    result.identifier = "long_agent";
    result.when_to_use = "This is a very long description that goes on and on";
    result.system_prompt = "This is a very long system prompt that contains many words and sentences. "
                           "It is designed to test how the agent generator handles longer content. "
                           "The prompt should be preserved exactly as provided.";
    
    auto info = result.to_agent_info();
    
    REQUIRE(info.name == "long_agent");
    REQUIRE(info.description.has_value());
    REQUIRE(info.description->size() > 50);
    REQUIRE(info.prompt.has_value());
    REQUIRE(info.prompt->size() > 100);
}

TEST_CASE("Agent.Generator.Result.MultilinePrompt", "[Agent][Generator]") {
    AgentGenerateResult result;
    result.identifier = "multiline_agent";
    result.when_to_use = "Multiline agent";
    result.system_prompt = "Line 1\nLine 2\nLine 3\n\nParagraph 2";
    
    auto info = result.to_agent_info();
    
    REQUIRE(info.prompt == "Line 1\nLine 2\nLine 3\n\nParagraph 2");
}

// ==================== Validation Tests ====================

TEST_CASE("Agent.Generator.Result.ToAgentInfo.Validates", "[Agent][Generator]") {
    AgentGenerateResult result;
    result.identifier = "valid_agent";
    result.when_to_use = "Valid agent description";
    result.system_prompt = "You are a valid agent.";
    
    auto info = result.to_agent_info();
    
    // AgentInfo should pass validation
    REQUIRE(info.validate());
}

TEST_CASE("Agent.Generator.Result.ToAgentInfo.CanBeRegistered", "[Agent][Generator]") {
    AgentGenerateResult result;
    result.identifier = "registerable_agent";
    result.when_to_use = "Can be registered";
    result.system_prompt = "You can be registered.";
    
    auto info = result.to_agent_info();
    
    // Clear registry and register the agent
    AgentRegistry::instance().clear();
    
    auto agent = std::make_shared<ConfigurableAgent>(info);
    AgentRegistry::instance().register_agent(agent);
    
    REQUIRE(AgentRegistry::instance().has("registerable_agent"));
    
    auto retrieved = AgentRegistry::instance().get("registerable_agent");
    REQUIRE(retrieved);
    REQUIRE(retrieved->name() == "registerable_agent");
}
