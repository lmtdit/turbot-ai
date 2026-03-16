/**
 * @file agent_loader_test.cpp
 * @brief Agent loader tests for agent_loader namespace
 *
 * Tests for:
 * - initialize_builtin_agents() - built-in agent initialization
 * - load_from_config() - loading agents from JSON config
 * - reload() - reloading all agents
 */

#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/agent/builtin/configurable_agent.hpp>
#include <turbot/core/permission/permission.hpp>

using namespace turbot::core::agent;
using namespace turbot::core::permission;
using namespace turbot::test;

// ==================== Test Fixtures ====================

class AgentLoaderFixture {
public:
    AgentLoaderFixture() {
        // Ensure valid current directory
        try {
            std::filesystem::current_path(std::filesystem::temp_directory_path());
        } catch (...) {}
        
        // Clear registry before each test
        AgentRegistry::instance().clear();
    }

    ~AgentLoaderFixture() {
        // Clean up after test
        AgentRegistry::instance().clear();
    }
};

// ==================== initialize_builtin_agents Tests ====================

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.InitializeBuiltin.ReturnsCount", "[Agent][Loader]") {
    auto count = agent_loader::initialize_builtin_agents();
    
    // Should initialize at least 5 built-in agents
    REQUIRE(count >= 5);
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.InitializeBuiltin.HasBuild", "[Agent][Loader]") {
    agent_loader::initialize_builtin_agents();
    
    REQUIRE(AgentRegistry::instance().has("build"));
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.InitializeBuiltin.HasPlan", "[Agent][Loader]") {
    agent_loader::initialize_builtin_agents();
    
    REQUIRE(AgentRegistry::instance().has("plan"));
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.InitializeBuiltin.HasExplore", "[Agent][Loader]") {
    agent_loader::initialize_builtin_agents();
    
    REQUIRE(AgentRegistry::instance().has("explore"));
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.InitializeBuiltin.HasGeneral", "[Agent][Loader]") {
    agent_loader::initialize_builtin_agents();
    
    REQUIRE(AgentRegistry::instance().has("general"));
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.InitializeBuiltin.HasCompaction", "[Agent][Loader]") {
    agent_loader::initialize_builtin_agents();
    
    REQUIRE(AgentRegistry::instance().has("compaction"));
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.InitializeBuiltin.HasTitle", "[Agent][Loader]") {
    agent_loader::initialize_builtin_agents();
    
    REQUIRE(AgentRegistry::instance().has("title"));
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.InitializeBuiltin.HasSummary", "[Agent][Loader]") {
    agent_loader::initialize_builtin_agents();
    
    REQUIRE(AgentRegistry::instance().has("summary"));
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.InitializeBuiltin.BuildIsPrimary", "[Agent][Loader]") {
    agent_loader::initialize_builtin_agents();
    
    auto build = AgentRegistry::instance().get("build");
    REQUIRE(build);
    REQUIRE(build->mode() == AgentMode::Primary);
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.InitializeBuiltin.ExploreIsSubagent", "[Agent][Loader]") {
    agent_loader::initialize_builtin_agents();
    
    auto explore = AgentRegistry::instance().get("explore");
    REQUIRE(explore);
    REQUIRE(explore->mode() == AgentMode::Subagent);
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.InitializeBuiltin.CompactionIsHidden", "[Agent][Loader]") {
    agent_loader::initialize_builtin_agents();
    
    auto compaction = AgentRegistry::instance().get("compaction");
    REQUIRE(compaction);
    REQUIRE(compaction->is_hidden());
}

// ==================== load_from_config Tests ====================

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.LoadFromConfig.EmptyConfig", "[Agent][Loader]") {
    nlohmann::json config = nlohmann::json::object();
    
    auto count = agent_loader::load_from_config(config);
    REQUIRE(count == 0);
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.LoadFromConfig.InvalidType", "[Agent][Loader]") {
    nlohmann::json config = "not an object";
    
    auto count = agent_loader::load_from_config(config);
    REQUIRE(count == 0);
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.LoadFromConfig.SingleAgent", "[Agent][Loader]") {
    nlohmann::json config = {
        {"custom-agent", {
            {"description", "A custom test agent"},
            {"mode", "primary"},
            {"prompt", "You are a custom agent."}
        }}
    };
    
    auto count = agent_loader::load_from_config(config);
    REQUIRE(count == 1);
    REQUIRE(AgentRegistry::instance().has("custom-agent"));
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.LoadFromConfig.MultipleAgents", "[Agent][Loader]") {
    nlohmann::json config = {
        {"agent-one", {
            {"description", "First agent"},
            {"mode", "primary"}
        }},
        {"agent-two", {
            {"description", "Second agent"},
            {"mode", "subagent"}
        }}
    };
    
    auto count = agent_loader::load_from_config(config);
    REQUIRE(count == 2);
    REQUIRE(AgentRegistry::instance().has("agent-one"));
    REQUIRE(AgentRegistry::instance().has("agent-two"));
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.LoadFromConfig.WithModel", "[Agent][Loader]") {
    nlohmann::json config = {
        {"model-agent", {
            {"description", "Agent with model"},
            {"model", {
                {"model_id", "gpt-4"},
                {"provider_id", "openai"}
            }}
        }}
    };
    
    auto count = agent_loader::load_from_config(config);
    REQUIRE(count == 1);
    
    auto agent = AgentRegistry::instance().get("model-agent");
    REQUIRE(agent);
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.LoadFromConfig.WithTemperature", "[Agent][Loader]") {
    nlohmann::json config = {
        {"temp-agent", {
            {"description", "Agent with temperature"},
            {"temperature", 0.7}
        }}
    };
    
    auto count = agent_loader::load_from_config(config);
    REQUIRE(count == 1);
    REQUIRE(AgentRegistry::instance().has("temp-agent"));
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.LoadFromConfig.WithDisable", "[Agent][Loader]") {
    // First, initialize built-in agents
    agent_loader::initialize_builtin_agents();
    REQUIRE(AgentRegistry::instance().has("build"));
    
    // Now load config that disables an agent
    nlohmann::json config = {
        {"build", {
            {"disable", true}
        }}
    };
    
    agent_loader::load_from_config(config);
    
    // Build should be unregistered
    REQUIRE_FALSE(AgentRegistry::instance().has("build"));
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.LoadFromConfig.InvalidTemperature", "[Agent][Loader]") {
    nlohmann::json config = {
        {"invalid-temp", {
            {"description", "Agent with invalid temperature"},
            {"temperature", 3.0}  // Invalid: > 2.0
        }}
    };
    
    auto count = agent_loader::load_from_config(config);
    // Should skip invalid agent
    REQUIRE(count == 0);
}

// ==================== reload Tests ====================

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.Reload.ClearsAndReinitializes", "[Agent][Loader]") {
    // Initialize first time
    auto count1 = agent_loader::initialize_builtin_agents();
    REQUIRE(count1 >= 5);
    
    // Add a custom agent
    AgentInfo info;
    info.name = "custom";
    info.description = "Custom agent";
    AgentRegistry::instance().register_agent(std::make_shared<ConfigurableAgent>(info));
    REQUIRE(AgentRegistry::instance().has("custom"));
    
    // Reload
    auto count2 = agent_loader::reload();
    
    // Should have same count as initial
    REQUIRE(count2 == count1);
    
    // Custom agent should be gone
    REQUIRE_FALSE(AgentRegistry::instance().has("custom"));
    
    // Built-in agents should be back
    REQUIRE(AgentRegistry::instance().has("build"));
}

TEST_CASE_METHOD(AgentLoaderFixture, "Agent.Loader.Reload.ReturnsCount", "[Agent][Loader]") {
    auto count = agent_loader::reload();
    REQUIRE(count >= 5);
}
