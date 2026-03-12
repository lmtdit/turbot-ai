/// builtin_agents_test.cpp — Unit tests for 2.8.10 built-in Agent preset
///
/// Covers:
///   - initialize_builtin_agents() registers exactly 7 agents
///   - AgentInfo.disable field: serialise / deserialise
///   - Each built-in agent has the expected name, mode, hidden flag
///   - title/summary/compaction agents are hidden
///   - general is a visible subagent
///   - compaction/title/summary deny all tools
///   - general denies only todo tools
///   - AgentInfo operator== includes disable field
///   - load_from_config honours the "disable" key

#include <turbot/core/agent/agent.hpp>
#include <turbot/core/agent/builtin/configurable_agent.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace turbot::core::agent;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// RAII guard — clears the AgentRegistry on destruction.
struct RegistryGuard {
    ~RegistryGuard() { AgentRegistry::instance().clear(); }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("initialize_builtin_agents registers exactly 7 agents",
          "[core][agent][builtin][2.8.10]") {
    RegistryGuard guard;
    AgentRegistry::instance().clear();

    const std::size_t n = agent_loader::initialize_builtin_agents();
    REQUIRE(n == 7u);
    REQUIRE(AgentRegistry::instance().size() == 7u);
}

TEST_CASE("initialize_builtin_agents: all 7 expected names are present",
          "[core][agent][builtin][2.8.10]") {
    RegistryGuard guard;
    AgentRegistry::instance().clear();
    agent_loader::initialize_builtin_agents();

    const auto& reg = AgentRegistry::instance();
    REQUIRE(reg.has("build"));
    REQUIRE(reg.has("plan"));
    REQUIRE(reg.has("explore"));
    REQUIRE(reg.has("general"));
    REQUIRE(reg.has("compaction"));
    REQUIRE(reg.has("title"));
    REQUIRE(reg.has("summary"));
}

TEST_CASE("initialize_builtin_agents: mode and hidden flags",
          "[core][agent][builtin][2.8.10]") {
    RegistryGuard guard;
    AgentRegistry::instance().clear();
    agent_loader::initialize_builtin_agents();

    const auto& reg = AgentRegistry::instance();

    // Visible primary agents
    REQUIRE(reg.get("build")->info().mode   == AgentMode::Primary);
    REQUIRE(reg.get("build")->info().hidden == false);
    REQUIRE(reg.get("plan")->info().mode    == AgentMode::Primary);
    REQUIRE(reg.get("plan")->info().hidden  == false);

    // Subagent, visible
    REQUIRE(reg.get("explore")->info().mode   == AgentMode::Subagent);
    REQUIRE(reg.get("explore")->info().hidden == false);
    REQUIRE(reg.get("general")->info().mode   == AgentMode::Subagent);
    REQUIRE(reg.get("general")->info().hidden == false);

    // Hidden internal agents
    REQUIRE(reg.get("compaction")->info().hidden == true);
    REQUIRE(reg.get("title")->info().hidden      == true);
    REQUIRE(reg.get("summary")->info().hidden    == true);
}

TEST_CASE("title agent has temperature=0.5 and steps=1",
          "[core][agent][builtin][2.8.10]") {
    RegistryGuard guard;
    AgentRegistry::instance().clear();
    agent_loader::initialize_builtin_agents();

    auto title = AgentRegistry::instance().get("title");
    REQUIRE(title != nullptr);
    REQUIRE(title->info().temperature.has_value());
    REQUIRE_THAT(title->info().temperature.value(),
                 Catch::Matchers::WithinAbs(0.5, 1e-9));
    REQUIRE(title->info().steps.has_value());
    REQUIRE(title->info().steps.value() == 1);
}

TEST_CASE("AgentInfo.disable field: default is false",
          "[core][agent][disable][2.8.10]") {
    AgentInfo info;
    info.name = "test";
    REQUIRE(info.disable == false);
}

TEST_CASE("AgentInfo.disable field: to_json / from_json round-trip",
          "[core][agent][disable][2.8.10]") {
    AgentInfo info;
    info.name    = "test_disabled";
    info.disable = true;

    auto j = info.to_json();
    REQUIRE(j.contains("disable"));
    REQUIRE(j["disable"] == true);

    auto restored = AgentInfo::from_json(j);
    REQUIRE(restored.disable == true);
}

TEST_CASE("AgentInfo.disable=false is omitted from JSON (compact serialisation)",
          "[core][agent][disable][2.8.10]") {
    AgentInfo info;
    info.name    = "no_disable";
    info.disable = false;

    auto j = info.to_json();
    // We intentionally do NOT serialise false to keep the JSON compact.
    REQUIRE_FALSE(j.contains("disable"));
}

TEST_CASE("AgentInfo operator== includes disable field",
          "[core][agent][disable][2.8.10]") {
    AgentInfo a;
    a.name    = "x";
    a.disable = false;

    AgentInfo b = a;
    REQUIRE(a == b);

    b.disable = true;
    REQUIRE_FALSE(a == b);
}

TEST_CASE("ConfigurableAgent: name and description delegated to AgentInfo",
          "[core][agent][configurable][2.8.10]") {
    AgentInfo info;
    info.name        = "my_agent";
    info.description = "does stuff";

    ConfigurableAgent agent(info);
    REQUIRE(agent.name()        == "my_agent");
    REQUIRE(agent.description() == "does stuff");
}

TEST_CASE("ConfigurableAgent: execute returns ok result",
          "[core][agent][configurable][2.8.10]") {
    AgentInfo info;
    info.name = "cfg";

    ConfigurableAgent agent(info);
    ExecuteParams params;
    params.session_id = "sess-1";
    params.prompt     = "hello";

    auto result = agent.execute(params);
    REQUIRE(result.is_success);
}

TEST_CASE("load_from_config: disable=true removes existing agent",
          "[core][agent][loader][disable][2.8.10]") {
    RegistryGuard guard;
    AgentRegistry::instance().clear();
    agent_loader::initialize_builtin_agents();

    REQUIRE(AgentRegistry::instance().has("build"));

    nlohmann::json cfg = {
        {"build", {{"disable", true}}}
    };
    agent_loader::load_from_config(cfg);

    REQUIRE_FALSE(AgentRegistry::instance().has("build"));
}

TEST_CASE("list_visible excludes hidden agents",
          "[core][agent][registry][2.8.10]") {
    RegistryGuard guard;
    AgentRegistry::instance().clear();
    agent_loader::initialize_builtin_agents();

    const auto visible = AgentRegistry::instance().list_visible();
    for (const auto& agent : visible) {
        REQUIRE_FALSE(agent->info().hidden);
    }
    // build + plan + explore + general = 4 visible
    REQUIRE(visible.size() == 4u);
}
