#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/agent/builtin/build_agent.hpp>
#include <turbot/core/agent/builtin/plan_agent.hpp>
#include <turbot/core/agent/builtin/explore_agent.hpp>

using namespace turbot::core::agent;

// Test helper: custom agent for testing
class TestAgent : public Agent {
public:
    explicit TestAgent(const std::string& name, AgentMode mode = AgentMode::Primary) {
        info_.name = name;
        info_.description = "Test agent for unit testing";
        info_.mode = mode;
        info_.native = false;
    }

    [[nodiscard]] std::string name() const override { return info_.name; }
    [[nodiscard]] std::string description() const override { return "Test agent"; }
    
    [[nodiscard]] ExecuteResult execute(const ExecuteParams& params) override {
        return ExecuteResult::ok("Test output for: " + params.prompt);
    }
};

TEST_CASE("AgentMode conversion", "[core][agent][agent_mode]") {
    SECTION("to_string") {
        REQUIRE(agent_mode_to_string(AgentMode::Primary) == "primary");
        REQUIRE(agent_mode_to_string(AgentMode::Subagent) == "subagent");
        REQUIRE(agent_mode_to_string(AgentMode::All) == "all");
    }

    SECTION("from_string") {
        REQUIRE(string_to_agent_mode("primary") == AgentMode::Primary);
        REQUIRE(string_to_agent_mode("subagent") == AgentMode::Subagent);
        REQUIRE(string_to_agent_mode("all") == AgentMode::All);
    }

    SECTION("invalid string throws") {
        REQUIRE_THROWS_AS(string_to_agent_mode("invalid"), std::invalid_argument);
    }
}

TEST_CASE("AgentInfo serialization", "[core][agent][agent_info]") {
    SECTION("basic serialization") {
        AgentInfo info;
        info.name = "test_agent";
        info.description = "Test description";
        info.mode = AgentMode::Primary;
        info.native = true;
        info.hidden = false;
        info.model_id = "gpt-4";

        nlohmann::json j = info.to_json();
        
        REQUIRE(j["name"] == "test_agent");
        REQUIRE(j["description"] == "Test description");
        REQUIRE(j["mode"] == "primary");
        REQUIRE(j["native"] == true);
        REQUIRE(j["hidden"] == false);
        REQUIRE(j["model_id"] == "gpt-4");
    }

    SECTION("with permission rules") {
        using namespace turbot::core::permission;
        
        AgentInfo info;
        info.name = "test";
        info.permission = {
            PermissionRule{"read_file", "*", PermissionAction::Allow},
            PermissionRule{"write_file", "*", PermissionAction::Ask}
        };

        nlohmann::json j = info.to_json();
        
        REQUIRE(j["permission"].is_array());
        REQUIRE(j["permission"].size() == 2);
    }

    SECTION("deserialization") {
        nlohmann::json j = {
            {"name", "test_agent"},
            {"description", "Test"},
            {"mode", "subagent"},
            {"native", false},
            {"hidden", true},
            {"model_id", "gpt-3.5"}
        };

        AgentInfo info = AgentInfo::from_json(j);
        
        REQUIRE(info.name == "test_agent");
        REQUIRE(info.description == "Test");
        REQUIRE(info.mode == AgentMode::Subagent);
        REQUIRE(info.native == false);
        REQUIRE(info.hidden == true);
        REQUIRE(info.model_id == "gpt-3.5");
    }

    SECTION("round trip") {
        AgentInfo original;
        original.name = "round_trip";
        original.description = "Round trip test";
        original.mode = AgentMode::Subagent;
        original.native = true;
        
        nlohmann::json j = original.to_json();
        AgentInfo restored = AgentInfo::from_json(j);
        
        REQUIRE(restored.name == original.name);
        REQUIRE(restored.description == original.description);
        REQUIRE(restored.mode == original.mode);
        REQUIRE(restored.native == original.native);
    }

    SECTION("equality") {
        AgentInfo a;
        a.name = "test";
        a.mode = AgentMode::Primary;
        
        AgentInfo b;
        b.name = "test";
        b.mode = AgentMode::Primary;
        
        AgentInfo c;
        c.name = "other";
        c.mode = AgentMode::Primary;
        
        REQUIRE(a == b);
        REQUIRE_FALSE(a == c);
    }
}

TEST_CASE("ExecuteResult", "[core][agent][execute_result]") {
    SECTION("success result") {
        auto result = ExecuteResult::ok("Output text", {{"key", "value"}});
        
        REQUIRE(result.is_success == true);
        REQUIRE(result.output == "Output text");
        REQUIRE(result.metadata["key"] == "value");
        REQUIRE_FALSE(result.error_message.has_value());
    }

    SECTION("error result") {
        auto result = ExecuteResult::error("Something went wrong");
        
        REQUIRE(result.is_success == false);
        REQUIRE(result.error_message == "Something went wrong");
        REQUIRE(result.output.empty());
    }
}

TEST_CASE("Agent base class", "[core][agent][agent]") {
    SECTION("to_tool_definition") {
        TestAgent agent("test_tool");
        nlohmann::json def = agent.to_tool_definition();
        
        REQUIRE(def["type"] == "function");
        REQUIRE(def["function"]["name"] == "test_tool");
        REQUIRE(def["function"]["description"] == "Test agent");
    }

    SECTION("info accessor") {
        TestAgent agent("info_test");
        
        REQUIRE(agent.info().name == "info_test");
        REQUIRE(agent.info().mode == AgentMode::Primary);
    }
}

TEST_CASE("AgentRegistry", "[core][agent][agent_registry]") {
    // Clear registry before each test
    AgentRegistry::instance().clear();

    SECTION("register and get") {
        auto agent = std::make_shared<TestAgent>("registered");
        
        REQUIRE(AgentRegistry::instance().register_agent(agent));
        REQUIRE(AgentRegistry::instance().has("registered"));
        
        auto retrieved = AgentRegistry::instance().get("registered");
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->name() == "registered");
    }

    SECTION("duplicate registration fails") {
        auto agent1 = std::make_shared<TestAgent>("duplicate");
        auto agent2 = std::make_shared<TestAgent>("duplicate");
        
        REQUIRE(AgentRegistry::instance().register_agent(agent1));
        REQUIRE_FALSE(AgentRegistry::instance().register_agent(agent2));
    }

    SECTION("null registration fails") {
        REQUIRE_FALSE(AgentRegistry::instance().register_agent(nullptr));
    }

    SECTION("unregister") {
        auto agent = std::make_shared<TestAgent>("to_remove");
        AgentRegistry::instance().register_agent(agent);
        
        REQUIRE(AgentRegistry::instance().unregister_agent("to_remove"));
        REQUIRE_FALSE(AgentRegistry::instance().has("to_remove"));
        REQUIRE_FALSE(AgentRegistry::instance().unregister_agent("nonexistent"));
    }

    SECTION("list agents") {
        AgentRegistry::instance().register_agent(std::make_shared<TestAgent>("agent1"));
        AgentRegistry::instance().register_agent(std::make_shared<TestAgent>("agent2"));
        AgentRegistry::instance().register_agent(std::make_shared<TestAgent>("agent3", AgentMode::Subagent));
        
        auto all = AgentRegistry::instance().list();
        REQUIRE(all.size() == 3);
        
        auto primary = AgentRegistry::instance().list_by_mode(AgentMode::Primary);
        REQUIRE(primary.size() == 2);
        
        auto subagents = AgentRegistry::instance().list_by_mode(AgentMode::Subagent);
        REQUIRE(subagents.size() == 1);
    }

    SECTION("names") {
        AgentRegistry::instance().register_agent(std::make_shared<TestAgent>("name1"));
        AgentRegistry::instance().register_agent(std::make_shared<TestAgent>("name2"));
        
        auto names = AgentRegistry::instance().names();
        REQUIRE(names.size() == 2);
        REQUIRE(std::find(names.begin(), names.end(), "name1") != names.end());
        REQUIRE(std::find(names.begin(), names.end(), "name2") != names.end());
    }

    SECTION("size") {
        REQUIRE(AgentRegistry::instance().size() == 0);
        
        AgentRegistry::instance().register_agent(std::make_shared<TestAgent>("size1"));
        REQUIRE(AgentRegistry::instance().size() == 1);
        
        AgentRegistry::instance().register_agent(std::make_shared<TestAgent>("size2"));
        REQUIRE(AgentRegistry::instance().size() == 2);
    }

    SECTION("clear") {
        AgentRegistry::instance().register_agent(std::make_shared<TestAgent>("clear1"));
        AgentRegistry::instance().register_agent(std::make_shared<TestAgent>("clear2"));
        
        AgentRegistry::instance().clear();
        REQUIRE(AgentRegistry::instance().size() == 0);
    }
}

TEST_CASE("BuildAgent", "[core][agent][builtin][build_agent]") {
    SECTION("construction") {
        BuildAgent agent;
        
        REQUIRE(agent.name() == "build");
        REQUIRE(agent.info().mode == AgentMode::Primary);
        REQUIRE(agent.info().native == true);
        REQUIRE_FALSE(agent.info().hidden);
    }

    SECTION("execute") {
        BuildAgent agent;
        ExecuteParams params;
        params.session_id = "test_session";
        params.prompt = "Build this project";
        
        auto result = agent.execute(params);
        
        REQUIRE(result.is_success);
        REQUIRE_FALSE(result.output.empty());
        REQUIRE(result.metadata["agent"] == "build");
    }

    SECTION("default permissions") {
        BuildAgent agent;
        const auto& perms = agent.info().permission;
        
        REQUIRE_FALSE(perms.empty());
        // Check that there's an allow-all rule
        bool has_allow_all = false;
        for (const auto& rule : perms) {
            if (rule.permission == "*" && rule.pattern == "*") {
                has_allow_all = true;
            }
        }
        REQUIRE(has_allow_all);
    }
}

TEST_CASE("PlanAgent", "[core][agent][builtin][plan_agent]") {
    SECTION("construction") {
        PlanAgent agent;
        
        REQUIRE(agent.name() == "plan");
        REQUIRE(agent.info().mode == AgentMode::Primary);
        REQUIRE(agent.info().native == true);
    }

    SECTION("execute") {
        PlanAgent agent;
        ExecuteParams params;
        params.session_id = "plan_session";
        params.prompt = "Create a plan";
        
        auto result = agent.execute(params);
        
        REQUIRE(result.is_success);
        REQUIRE(result.metadata["agent"] == "plan");
        REQUIRE(result.metadata["mode"] == "analysis");
    }
}

TEST_CASE("ExploreAgent", "[core][agent][builtin][explore_agent]") {
    SECTION("construction") {
        ExploreAgent agent;
        
        REQUIRE(agent.name() == "explore");
        REQUIRE(agent.info().mode == AgentMode::Primary);
        REQUIRE(agent.info().native == true);
    }

    SECTION("execute") {
        ExploreAgent agent;
        ExecuteParams params;
        params.session_id = "explore_session";
        params.prompt = "Explore this code";
        
        auto result = agent.execute(params);
        
        REQUIRE(result.is_success);
        REQUIRE(result.metadata["agent"] == "explore");
        REQUIRE(result.metadata["mode"] == "exploration");
    }

    SECTION("read-only permissions") {
        ExploreAgent agent;
        const auto& perms = agent.info().permission;
        
        REQUIRE_FALSE(perms.empty());
        // Explore agent should have a deny-all rule at the end
        bool has_deny_all = false;
        for (const auto& rule : perms) {
            if (rule.permission == "*" && rule.pattern == "*" && 
                rule.action == turbot::core::permission::PermissionAction::Deny) {
                has_deny_all = true;
            }
        }
        REQUIRE(has_deny_all);
    }
}
