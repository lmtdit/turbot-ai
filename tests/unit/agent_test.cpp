#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/agent/builtin/build_agent.hpp>
#include <turbot/core/agent/builtin/plan_agent.hpp>
#include <turbot/core/agent/builtin/explore_agent.hpp>
#include <turbot/core/agent/builtin/configurable_agent.hpp>
#include <nlohmann/json.hpp>

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
    
    // Allow setting hidden for testing
    void set_hidden(bool hidden) { info_.hidden = hidden; }
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
        info.model = ModelRef{"gpt-4", "openai"};

        nlohmann::json j = info.to_json();
        
        REQUIRE(j["name"] == "test_agent");
        REQUIRE(j["description"] == "Test description");
        REQUIRE(j["mode"] == "primary");
        REQUIRE(j["native"] == true);
        REQUIRE(j["hidden"] == false);
        REQUIRE(j["model"]["model_id"] == "gpt-4");
        REQUIRE(j["model"]["provider_id"] == "openai");
    }

    SECTION("v2.0 extended fields serialization") {
        AgentInfo info;
        info.name = "extended_agent";
        info.description = "Agent with v2.0 fields";
        info.mode = AgentMode::Subagent;
        info.prompt = "You are a specialized agent for code review.";
        info.temperature = 0.7;
        info.top_p = 0.9;
        info.steps = 50;
        info.color = "#FF5733";
        info.variant = "high";

        nlohmann::json j = info.to_json();
        
        REQUIRE(j["name"] == "extended_agent");
        REQUIRE(j["prompt"] == "You are a specialized agent for code review.");
        REQUIRE(j["temperature"] == Catch::Approx(0.7));
        REQUIRE(j["top_p"] == Catch::Approx(0.9));
        REQUIRE(j["steps"] == 50);
        REQUIRE(j["color"] == "#FF5733");
        REQUIRE(j["variant"] == "high");
    }

    SECTION("v2.0 extended fields deserialization") {
        nlohmann::json j = {
            {"name", "test_extended"},
            {"description", "Test"},
            {"mode", "subagent"},
            {"prompt", "Custom system prompt"},
            {"temperature", 0.5},
            {"top_p", 0.95},
            {"steps", 20},
            {"color", "#00FF00"},
            {"variant", "low"}
        };

        AgentInfo info = AgentInfo::from_json(j);
        
        REQUIRE(info.name == "test_extended");
        REQUIRE(info.prompt == "Custom system prompt");
        REQUIRE(info.temperature == Catch::Approx(0.5));
        REQUIRE(info.top_p == Catch::Approx(0.95));
        REQUIRE(info.steps == 20);
        REQUIRE(info.color == "#00FF00");
        REQUIRE(info.variant == "low");
    }

    SECTION("v2.0 round trip with extended fields") {
        AgentInfo original;
        original.name = "round_trip_v2";
        original.description = "Round trip test v2.0";
        original.mode = AgentMode::Subagent;
        original.prompt = "Test prompt";
        original.temperature = 0.3;
        original.top_p = 0.8;
        original.steps = 10;
        original.color = "blue";
        original.variant = "medium";
        
        nlohmann::json j = original.to_json();
        AgentInfo restored = AgentInfo::from_json(j);
        
        REQUIRE(restored.name == original.name);
        REQUIRE(restored.description == original.description);
        REQUIRE(restored.mode == original.mode);
        REQUIRE(restored.prompt == original.prompt);
        REQUIRE(restored.temperature == Catch::Approx(*original.temperature));
        REQUIRE(restored.top_p == Catch::Approx(*original.top_p));
        REQUIRE(restored.steps == original.steps);
        REQUIRE(restored.color == original.color);
        REQUIRE(restored.variant == original.variant);
    }

    SECTION("v2.0 equality with extended fields") {
        AgentInfo a;
        a.name = "test";
        a.mode = AgentMode::Primary;
        a.prompt = "Same prompt";
        a.temperature = 0.5;
        a.steps = 10;
        
        AgentInfo b;
        b.name = "test";
        b.mode = AgentMode::Primary;
        b.prompt = "Same prompt";
        b.temperature = 0.5;
        b.steps = 10;
        
        AgentInfo c;
        c.name = "test";
        c.mode = AgentMode::Primary;
        c.prompt = "Different prompt";
        c.temperature = 0.5;
        c.steps = 10;
        
        REQUIRE(a == b);
        REQUIRE_FALSE(a == c);
    }

    SECTION("partial extended fields") {
        // Test that partial extended fields work correctly
        nlohmann::json j = {
            {"name", "partial_agent"},
            {"temperature", 0.8}
            // Other extended fields omitted
        };

        AgentInfo info = AgentInfo::from_json(j);
        
        REQUIRE(info.name == "partial_agent");
        REQUIRE(info.temperature == Catch::Approx(0.8));
        REQUIRE_FALSE(info.prompt.has_value());
        REQUIRE_FALSE(info.top_p.has_value());
        REQUIRE_FALSE(info.steps.has_value());
        REQUIRE_FALSE(info.color.has_value());
        REQUIRE_FALSE(info.variant.has_value());
    }

    SECTION("null extended fields") {
        nlohmann::json j = {
            {"name", "null_fields_agent"},
            {"prompt", nullptr},
            {"temperature", nullptr},
            {"steps", nullptr}
        };

        AgentInfo info = AgentInfo::from_json(j);
        
        REQUIRE(info.name == "null_fields_agent");
        REQUIRE_FALSE(info.prompt.has_value());
        REQUIRE_FALSE(info.temperature.has_value());
        REQUIRE_FALSE(info.steps.has_value());
    }
}

TEST_CASE("AgentInfo validation", "[core][agent][agent_info][validation]") {
    SECTION("validate() returns true for valid values") {
        AgentInfo info;
        info.name = "valid_agent";
        info.temperature = 1.0;
        info.top_p = 0.9;
        info.steps = 10;
        
        REQUIRE(info.validate());
    }
    
    SECTION("validate() returns true for unset optional fields") {
        AgentInfo info;
        info.name = "minimal_agent";
        // No extended fields set
        
        REQUIRE(info.validate());
    }
    
    SECTION("validate() rejects temperature below minimum") {
        AgentInfo info;
        info.name = "test";
        info.temperature = -0.1;
        
        REQUIRE_FALSE(info.validate());
    }
    
    SECTION("validate() rejects temperature above maximum") {
        AgentInfo info;
        info.name = "test";
        info.temperature = 2.1;
        
        REQUIRE_FALSE(info.validate());
    }
    
    SECTION("validate() accepts temperature at boundaries") {
        AgentInfo info;
        info.name = "test";
        
        info.temperature = 0.0;
        REQUIRE(info.validate());
        
        info.temperature = 2.0;
        REQUIRE(info.validate());
    }
    
    SECTION("validate() rejects top_p below minimum") {
        AgentInfo info;
        info.name = "test";
        info.top_p = -0.1;
        
        REQUIRE_FALSE(info.validate());
    }
    
    SECTION("validate() rejects top_p above maximum") {
        AgentInfo info;
        info.name = "test";
        info.top_p = 1.1;
        
        REQUIRE_FALSE(info.validate());
    }
    
    SECTION("validate() accepts top_p at boundaries") {
        AgentInfo info;
        info.name = "test";
        
        info.top_p = 0.0;
        REQUIRE(info.validate());
        
        info.top_p = 1.0;
        REQUIRE(info.validate());
    }
    
    SECTION("validate() rejects steps below minimum") {
        AgentInfo info;
        info.name = "test";
        info.steps = 0;
        
        REQUIRE_FALSE(info.validate());
    }
    
    SECTION("validate() rejects negative steps") {
        AgentInfo info;
        info.name = "test";
        info.steps = -5;
        
        REQUIRE_FALSE(info.validate());
    }
    
    SECTION("validate() accepts steps at minimum boundary") {
        AgentInfo info;
        info.name = "test";
        info.steps = 1;
        
        REQUIRE(info.validate());
    }
}

TEST_CASE("AgentInfo deserialization validation", "[core][agent][agent_info][validation]") {
    SECTION("from_json throws for invalid temperature") {
        nlohmann::json j = {
            {"name", "test"},
            {"temperature", 3.0}  // Invalid: > 2.0
        };
        
        REQUIRE_THROWS_AS(AgentInfo::from_json(j), std::out_of_range);
    }
    
    SECTION("from_json throws for negative temperature") {
        nlohmann::json j = {
            {"name", "test"},
            {"temperature", -0.5}  // Invalid: < 0.0
        };
        
        REQUIRE_THROWS_AS(AgentInfo::from_json(j), std::out_of_range);
    }
    
    SECTION("from_json throws for invalid top_p") {
        nlohmann::json j = {
            {"name", "test"},
            {"top_p", 1.5}  // Invalid: > 1.0
        };
        
        REQUIRE_THROWS_AS(AgentInfo::from_json(j), std::out_of_range);
    }
    
    SECTION("from_json throws for invalid steps") {
        nlohmann::json j = {
            {"name", "test"},
            {"steps", 0}  // Invalid: < 1
        };
        
        REQUIRE_THROWS_AS(AgentInfo::from_json(j), std::out_of_range);
    }
    
    SECTION("from_json throws for negative steps") {
        nlohmann::json j = {
            {"name", "test"},
            {"steps", -10}
        };
        
        REQUIRE_THROWS_AS(AgentInfo::from_json(j), std::out_of_range);
    }
    
    SECTION("from_json accepts boundary values") {
        nlohmann::json j = {
            {"name", "boundary_test"},
            {"temperature", 0.0},
            {"top_p", 1.0},
            {"steps", 1}
        };
        
        AgentInfo info = AgentInfo::from_json(j);
        REQUIRE(info.temperature == Catch::Approx(0.0));
        REQUIRE(info.top_p == Catch::Approx(1.0));
        REQUIRE(info.steps == 1);
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
            {"model", {
                {"model_id", "gpt-3.5"},
                {"provider_id", "openai"}
            }}
        };

        AgentInfo info = AgentInfo::from_json(j);
        
        REQUIRE(info.name == "test_agent");
        REQUIRE(info.description == "Test");
        REQUIRE(info.mode == AgentMode::Subagent);
        REQUIRE(info.native == false);
        REQUIRE(info.hidden == true);
        REQUIRE(info.model.has_value());
        REQUIRE(info.model->model_id == "gpt-3.5");
        REQUIRE(info.model->provider_id == "openai");
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
        REQUIRE(agent.info().mode == AgentMode::Subagent);
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

// ============================================================================
// Agent Registry Extended Tests (v2.0)
// ============================================================================

TEST_CASE("AgentRegistry::default_agent", "[core][agent][registry]") {
    AgentRegistry::instance().clear();
    
    SECTION("empty registry returns empty string") {
        REQUIRE(AgentRegistry::instance().default_agent().empty());
    }
    
    SECTION("returns build agent when available") {
        AgentRegistry::instance().register_agent(std::make_shared<BuildAgent>());
        REQUIRE(AgentRegistry::instance().default_agent() == "build");
    }
    
    SECTION("returns first visible primary agent") {
        auto hidden_agent = std::make_shared<TestAgent>("hidden_agent");
        hidden_agent->set_hidden(true);
        AgentRegistry::instance().register_agent(hidden_agent);
        
        auto visible_agent = std::make_shared<TestAgent>("visible_agent");
        AgentRegistry::instance().register_agent(visible_agent);
        
        REQUIRE(AgentRegistry::instance().default_agent() == "visible_agent");
    }
    
    SECTION("skips subagent") {
        auto subagent = std::make_shared<TestAgent>("sub_agent", AgentMode::Subagent);
        AgentRegistry::instance().register_agent(subagent);
        
        auto primary = std::make_shared<TestAgent>("primary_agent");
        AgentRegistry::instance().register_agent(primary);
        
        REQUIRE(AgentRegistry::instance().default_agent() == "primary_agent");
    }
    
    AgentRegistry::instance().clear();
}

TEST_CASE("AgentRegistry::list_visible", "[core][agent][registry]") {
    AgentRegistry::instance().clear();
    
    auto visible1 = std::make_shared<TestAgent>("visible1");
    auto visible2 = std::make_shared<TestAgent>("visible2");
    auto hidden = std::make_shared<TestAgent>("hidden");
    hidden->set_hidden(true);
    
    AgentRegistry::instance().register_agent(visible1);
    AgentRegistry::instance().register_agent(visible2);
    AgentRegistry::instance().register_agent(hidden);
    
    auto list = AgentRegistry::instance().list_visible();
    REQUIRE(list.size() == 2);
    
    AgentRegistry::instance().clear();
}

TEST_CASE("AgentRegistry::list_primary", "[core][agent][registry]") {
    AgentRegistry::instance().clear();
    
    auto primary1 = std::make_shared<TestAgent>("primary1");
    auto primary2 = std::make_shared<TestAgent>("primary2");
    auto subagent = std::make_shared<TestAgent>("subagent", AgentMode::Subagent);
    auto hidden_primary = std::make_shared<TestAgent>("hidden_primary");
    hidden_primary->set_hidden(true);
    
    AgentRegistry::instance().register_agent(primary1);
    AgentRegistry::instance().register_agent(primary2);
    AgentRegistry::instance().register_agent(subagent);
    AgentRegistry::instance().register_agent(hidden_primary);
    
    auto list = AgentRegistry::instance().list_primary();
    REQUIRE(list.size() == 2);
    
    AgentRegistry::instance().clear();
}

// ============================================================================
// Agent Loader Tests (v2.0)
// ============================================================================

TEST_CASE("agent_loader::initialize_builtin_agents", "[core][agent][loader]") {
    AgentRegistry::instance().clear();
    
    size_t count = agent_loader::initialize_builtin_agents();
    
    REQUIRE(count >= 3);  // At least build, plan, explore
    REQUIRE(AgentRegistry::instance().has("build"));
    REQUIRE(AgentRegistry::instance().has("plan"));
    REQUIRE(AgentRegistry::instance().has("explore"));
    
    AgentRegistry::instance().clear();
}

TEST_CASE("agent_loader::reload", "[core][agent][loader]") {
    AgentRegistry::instance().clear();
    
    // Add a custom agent
    AgentRegistry::instance().register_agent(std::make_shared<TestAgent>("custom"));
    REQUIRE(AgentRegistry::instance().size() == 1);
    
    // Reload should clear and reinitialize built-in agents
    size_t count = agent_loader::reload();
    REQUIRE(count >= 3);
    REQUIRE_FALSE(AgentRegistry::instance().has("custom"));
    REQUIRE(AgentRegistry::instance().has("build"));
    
    AgentRegistry::instance().clear();
}


TEST_CASE("agent_generator::generate", "[core][agent][generator]") {
    agent_generator::GenerateParams params;
    params.description = "Code review and optimization";
    
    auto result = agent_generator::generate(params);
    
    REQUIRE_FALSE(result.identifier.empty());
    REQUIRE_FALSE(result.when_to_use.empty());
    REQUIRE_FALSE(result.system_prompt.empty());
    
    // Check that identifier is slugified
    REQUIRE(result.identifier.find(' ') == std::string::npos);
}

TEST_CASE("AgentGenerateResult::to_agent_info", "[core][agent][generator]") {
    AgentGenerateResult result;
    result.identifier = "test_agent";
    result.when_to_use = "For testing purposes";
    result.system_prompt = "You are a test agent.";
    
    auto info = result.to_agent_info();
    
    REQUIRE(info.name == "test_agent");
    REQUIRE(info.description == "For testing purposes");
    REQUIRE(info.prompt == "You are a test agent.");
    REQUIRE(info.mode == AgentMode::Primary);
    REQUIRE(info.native == false);
}

// ============================================================================
// agent_loader::load_from_config Tests
// ============================================================================

TEST_CASE("agent_loader::load_from_config - basic", "[core][agent][loader]") {
    AgentRegistry::instance().clear();

    nlohmann::json config = {
        {"custom_agent", {
            {"description", "A custom agent for testing"},
            {"mode", "primary"},
            {"hidden", false},
            {"prompt", "You are a custom agent."}
        }}
    };

    size_t count = agent_loader::load_from_config(config);
    REQUIRE(count == 1);
    REQUIRE(AgentRegistry::instance().has("custom_agent"));

    auto agent = AgentRegistry::instance().get("custom_agent");
    REQUIRE(agent != nullptr);
    REQUIRE(agent->info().description == "A custom agent for testing");
    REQUIRE(agent->info().mode == AgentMode::Primary);

    AgentRegistry::instance().clear();
}

TEST_CASE("agent_loader::load_from_config - subagent mode", "[core][agent][loader]") {
    AgentRegistry::instance().clear();

    nlohmann::json config = {
        {"sub_agent", {
            {"description", "A subagent"},
            {"mode", "subagent"},
            {"hidden", true}
        }}
    };

    size_t count = agent_loader::load_from_config(config);
    REQUIRE(count == 1);

    auto agent = AgentRegistry::instance().get("sub_agent");
    REQUIRE(agent != nullptr);
    REQUIRE(agent->info().mode == AgentMode::Subagent);
    REQUIRE(agent->info().hidden == true);

    AgentRegistry::instance().clear();
}

TEST_CASE("agent_loader::load_from_config - temperature and steps", "[core][agent][loader]") {
    AgentRegistry::instance().clear();

    nlohmann::json config = {
        {"tuned_agent", {
            {"description", "Agent with tuned params"},
            {"temperature", 0.3},
            {"top_p", 0.9},
            {"steps", 10}
        }}
    };

    size_t count = agent_loader::load_from_config(config);
    REQUIRE(count == 1);

    auto agent = AgentRegistry::instance().get("tuned_agent");
    REQUIRE(agent != nullptr);
    REQUIRE(agent->info().temperature.has_value());
    REQUIRE(agent->info().temperature.value() == Catch::Approx(0.3));
    REQUIRE(agent->info().top_p.has_value());
    REQUIRE(agent->info().top_p.value() == Catch::Approx(0.9));
    REQUIRE(agent->info().steps.has_value());
    REQUIRE(agent->info().steps.value() == 10);

    AgentRegistry::instance().clear();
}

TEST_CASE("agent_loader::load_from_config - with model", "[core][agent][loader]") {
    AgentRegistry::instance().clear();

    nlohmann::json config = {
        {"model_agent", {
            {"description", "Agent with preferred model"},
            {"model", {
                {"model_id", "claude-3-opus"},
                {"provider_id", "anthropic"}
            }}
        }}
    };

    size_t count = agent_loader::load_from_config(config);
    REQUIRE(count == 1);

    auto agent = AgentRegistry::instance().get("model_agent");
    REQUIRE(agent != nullptr);
    REQUIRE(agent->info().model.has_value());
    REQUIRE(agent->info().model->model_id == "claude-3-opus");
    REQUIRE(agent->info().model->provider_id == "anthropic");

    AgentRegistry::instance().clear();
}

TEST_CASE("agent_loader::load_from_config - color and variant", "[core][agent][loader]") {
    AgentRegistry::instance().clear();

    nlohmann::json config = {
        {"styled_agent", {
            {"description", "Agent with styling"},
            {"color", "#FF5733"},
            {"variant", "high"}
        }}
    };

    size_t count = agent_loader::load_from_config(config);
    REQUIRE(count == 1);

    auto agent = AgentRegistry::instance().get("styled_agent");
    REQUIRE(agent != nullptr);
    REQUIRE(agent->info().color == "#FF5733");
    REQUIRE(agent->info().variant == "high");

    AgentRegistry::instance().clear();
}

TEST_CASE("agent_loader::load_from_config - disable removes agent", "[core][agent][loader]") {
    AgentRegistry::instance().clear();
    // Register a known agent first
    agent_loader::initialize_builtin_agents();
    REQUIRE(AgentRegistry::instance().has("build"));

    // disable: true should remove it
    nlohmann::json config = {
        {"build", {{"disable", true}}}
    };

    agent_loader::load_from_config(config);
    REQUIRE_FALSE(AgentRegistry::instance().has("build"));

    AgentRegistry::instance().clear();
}

TEST_CASE("agent_loader::load_from_config - skip existing agent", "[core][agent][loader]") {
    AgentRegistry::instance().clear();
    agent_loader::initialize_builtin_agents();
    size_t initial_size = AgentRegistry::instance().size();

    // Try to add agent with same name as existing — should skip
    nlohmann::json config = {
        {"build", {
            {"description", "A different build agent"}
        }}
    };

    size_t count = agent_loader::load_from_config(config);
    REQUIRE(count == 0); // Skipped because already exists
    REQUIRE(AgentRegistry::instance().size() == initial_size);

    AgentRegistry::instance().clear();
}

TEST_CASE("agent_loader::load_from_config - invalid config returns 0", "[core][agent][loader]") {
    AgentRegistry::instance().clear();

    // Non-object config
    REQUIRE(agent_loader::load_from_config(nlohmann::json::array()) == 0);
    REQUIRE(agent_loader::load_from_config("string") == 0);
    REQUIRE(agent_loader::load_from_config(42) == 0);

    AgentRegistry::instance().clear();
}

TEST_CASE("agent_loader::load_from_config - non-object entry skipped", "[core][agent][loader]") {
    AgentRegistry::instance().clear();

    nlohmann::json config = {
        {"valid_agent", {
            {"description", "valid"}
        }},
        {"invalid_entry", "just a string"},  // non-object: should skip
        {"another_invalid", 42}
    };

    size_t count = agent_loader::load_from_config(config);
    REQUIRE(count == 1); // Only valid_agent
    REQUIRE(AgentRegistry::instance().has("valid_agent"));

    AgentRegistry::instance().clear();
}

TEST_CASE("agent_loader::load_from_config - multiple agents", "[core][agent][loader]") {
    AgentRegistry::instance().clear();

    nlohmann::json config = {
        {"agent_alpha", {{"description", "Alpha"}, {"mode", "primary"}}},
        {"agent_beta",  {{"description", "Beta"},  {"mode", "subagent"}}},
        {"agent_gamma", {{"description", "Gamma"}, {"hidden", true}}}
    };

    size_t count = agent_loader::load_from_config(config);
    REQUIRE(count == 3);
    REQUIRE(AgentRegistry::instance().has("agent_alpha"));
    REQUIRE(AgentRegistry::instance().has("agent_beta"));
    REQUIRE(AgentRegistry::instance().has("agent_gamma"));

    REQUIRE(AgentRegistry::instance().get("agent_beta")->info().mode == AgentMode::Subagent);

    AgentRegistry::instance().clear();
}

TEST_CASE("AgentGenerateResult::to_json", "[core][agent][generator]") {
    AgentGenerateResult result;
    result.identifier = "my_agent";
    result.when_to_use = "When you need help";
    result.system_prompt = "Be helpful.";

    nlohmann::json j = result.to_json();
    REQUIRE(j["identifier"] == "my_agent");
    REQUIRE(j["when_to_use"] == "When you need help");
    REQUIRE(j["system_prompt"] == "Be helpful.");
}

TEST_CASE("agent_generator::generate - long description truncated", "[core][agent][generator]") {
    agent_generator::GenerateParams params;
    // >32 chars to trigger truncation
    params.description = "This is a very long description that exceeds the 32 character limit";

    auto result = agent_generator::generate(params);
    REQUIRE(result.identifier.length() <= 32);
}

// ============================================================================
// General Agent Tests (v4.1)
// ============================================================================

TEST_CASE("GeneralAgent registration", "[core][agent][builtin][general_agent]") {
    AgentRegistry::instance().clear();
    
    // Register general agent manually (as done in agent_loader.cpp)
    using namespace turbot::core::permission;
    AgentInfo info;
    info.name = "general";
    info.description = "General-purpose subagent (no todo tools)";
    info.mode = AgentMode::Subagent;
    info.native = true;
    info.hidden = false;
    info.permission.push_back(PermissionRule{"todoread", "*", PermissionAction::Deny});
    info.permission.push_back(PermissionRule{"todowrite", "*", PermissionAction::Deny});
    info.permission.push_back(PermissionRule{"*", "*", PermissionAction::Allow});
    
    auto agent = std::make_shared<ConfigurableAgent>(std::move(info));
    REQUIRE(AgentRegistry::instance().register_agent(agent));
    
    // Verify registration
    REQUIRE(AgentRegistry::instance().has("general"));
    auto retrieved = AgentRegistry::instance().get("general");
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->name() == "general");
    REQUIRE(retrieved->mode() == AgentMode::Subagent);
    REQUIRE_FALSE(retrieved->is_hidden());
    
    // Verify permission: todoread/todowrite denied
    const auto& perms = retrieved->info().permission;
    bool todoread_denied = false;
    bool todowrite_denied = false;
    for (const auto& rule : perms) {
        if (rule.permission == "todoread" && rule.action == PermissionAction::Deny) {
            todoread_denied = true;
        }
        if (rule.permission == "todowrite" && rule.action == PermissionAction::Deny) {
            todowrite_denied = true;
        }
    }
    REQUIRE(todoread_denied);
    REQUIRE(todowrite_denied);
    
    AgentRegistry::instance().clear();
}

// ============================================================================
// Compaction Agent Tests (v4.1)
// ============================================================================

TEST_CASE("CompactionAgent registration", "[core][agent][builtin][compaction_agent]") {
    AgentRegistry::instance().clear();
    
    // Register compaction agent manually (as done in agent_loader.cpp)
    using namespace turbot::core::permission;
    AgentInfo info;
    info.name = "compaction";
    info.description = "Internal agent used during context-window compaction";
    info.mode = AgentMode::Primary;
    info.native = true;
    info.hidden = true;
    info.permission.push_back(PermissionRule{"*", "*", PermissionAction::Deny});
    
    auto agent = std::make_shared<ConfigurableAgent>(std::move(info));
    REQUIRE(AgentRegistry::instance().register_agent(agent));
    
    // Verify registration
    REQUIRE(AgentRegistry::instance().has("compaction"));
    auto retrieved = AgentRegistry::instance().get("compaction");
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->name() == "compaction");
    REQUIRE(retrieved->mode() == AgentMode::Primary);
    REQUIRE(retrieved->is_hidden());
    
    // Verify permission: all tools denied
    const auto& perms = retrieved->info().permission;
    bool all_denied = false;
    for (const auto& rule : perms) {
        if (rule.permission == "*" && rule.pattern == "*" && rule.action == PermissionAction::Deny) {
            all_denied = true;
        }
    }
    REQUIRE(all_denied);
    
    // Verify not in visible list
    auto visible = AgentRegistry::instance().list_visible();
    bool found_in_visible = false;
    for (const auto& a : visible) {
        if (a->name() == "compaction") {
            found_in_visible = true;
        }
    }
    REQUIRE_FALSE(found_in_visible);
    
    AgentRegistry::instance().clear();
}

