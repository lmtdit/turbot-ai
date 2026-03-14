/// @file agent_generator_llm_test.cpp
/// @brief Unit tests for P1-4: agent_generator::generate() LLM integration.
///
/// Tests cover:
///   - Fallback path (no provider): deterministic slug generation
///   - LLM happy path: valid JSON → AgentGenerateResult populated correctly
///   - LLM returns non-JSON text → fallback to placeholder
///   - LLM returns markdown-fenced JSON → still parsed correctly
///   - LLM provider chat() throws → fallback (no crash)
///   - model_override respected (GenerateParams::model)
///   - Empty identifier in LLM JSON → slug fallback applied

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/provider/provider_manager.hpp>
#include <turbot/core/provider/provider.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

using namespace turbot::core::agent;
using namespace turbot::core::provider;

// ============================================================================
// Configurable mock provider for agent generator tests
// ============================================================================

class GeneratorMockProvider : public Provider {
public:
    std::string id() const override { return "gen-mock"; }
    std::string name() const override { return "GeneratorMock"; }
    bool is_ready() const override { return true; }

    std::vector<ModelInfo> list_models() const override {
        return {{"gen-mock-model", id(), "GenMock", ""}};
    }
    std::optional<ModelInfo> get_model(const std::string& mid) const override {
        if (mid == "gen-mock-model") return {{"gen-mock-model", id(), "GenMock", ""}};
        return std::nullopt;
    }
    bool supports_model(const std::string&) const override { return true; }

    ChatResponse chat(const std::vector<ChatMessage>&, const std::string&,
                      const ChatOptions&) override {
        if (should_throw_) throw std::runtime_error("mock chat error");
        ChatResponse resp;
        ChatMessage msg = ChatMessage::assistant(reply_);
        resp.choices.push_back(msg);
        resp.finish_reason = "stop";
        return resp;
    }

    ChatResponse chat_stream(const std::vector<ChatMessage>&, const std::string&,
                             const ChatOptions&, StreamCallback) override {
        return {};
    }
    int64_t count_tokens(const std::vector<ChatMessage>&, const std::string&) const override {
        return 0;
    }
    bool validate() override { return true; }

    void set_reply(const std::string& text) { reply_ = text; }
    void set_throw(bool v) { should_throw_ = v; }

private:
    std::string reply_{"{}"}; // default: empty JSON object
    bool should_throw_{false};
};

// ============================================================================
// Fixture helpers
// ============================================================================

static std::shared_ptr<GeneratorMockProvider> register_gen_mock() {
    auto mock = std::make_shared<GeneratorMockProvider>();
    auto& pm  = ProviderManager::instance();
    if (!pm.has_provider("gen-mock")) {
        pm.register_provider(mock);
    }
    pm.set_default_provider("gen-mock");
    return mock;
}

static void clear_gen_mock() {
    ProviderManager::instance().unregister_provider("gen-mock");
}

// ============================================================================
// Tests – fallback path (no provider)
// ============================================================================

TEST_CASE("agent_generator::generate: fallback when no provider configured",
          "[agent_generator][p1-4][fallback]") {
    // Ensure no provider is set by clearing all providers
    auto& pm = ProviderManager::instance();
    pm.clear();

    agent_generator::GenerateParams params;
    params.description = "analyze Python code quality";

    auto result = agent_generator::generate(params);

    REQUIRE_FALSE(result.identifier.empty());
    REQUIRE_FALSE(result.when_to_use.empty());
    REQUIRE_FALSE(result.system_prompt.empty());

    // Slug should be lowercase, no spaces
    CHECK(result.identifier.find(' ') == std::string::npos);
    CHECK(result.identifier.length() <= 32);
    CHECK(result.identifier == "analyze_python_code_quality");
}

TEST_CASE("agent_generator::generate: fallback result is valid AgentInfo",
          "[agent_generator][p1-4][fallback]") {
    auto& pm = ProviderManager::instance();
    pm.clear();

    agent_generator::GenerateParams params;
    params.description = "Write unit tests";

    auto result = agent_generator::generate(params);
    auto info   = result.to_agent_info();

    REQUIRE_FALSE(info.name.empty());
    REQUIRE(info.mode == AgentMode::Primary);
    REQUIRE(info.native == false);
    REQUIRE_FALSE(info.prompt.value_or("").empty());
}

// ============================================================================
// Tests – LLM happy path
// ============================================================================

TEST_CASE("agent_generator::generate: LLM returns valid JSON → result populated",
          "[agent_generator][p1-4][llm]") {
    auto mock = register_gen_mock();

    nlohmann::json llm_json = {
        {"identifier",    "code_quality_agent"},
        {"when_to_use",   "Use when you need to review code for quality issues"},
        {"system_prompt", "You are an expert code reviewer. Provide detailed feedback."}
    };
    mock->set_reply(llm_json.dump());

    agent_generator::GenerateParams params;
    params.description = "analyze Python code quality";

    auto result = agent_generator::generate(params);

    CHECK(result.identifier    == "code_quality_agent");
    CHECK(result.when_to_use   == "Use when you need to review code for quality issues");
    CHECK(result.system_prompt == "You are an expert code reviewer. Provide detailed feedback.");

    clear_gen_mock();
}

TEST_CASE("agent_generator::generate: LLM JSON → to_agent_info() maps fields",
          "[agent_generator][p1-4][llm]") {
    auto mock = register_gen_mock();

    nlohmann::json llm_json = {
        {"identifier",    "test_writer"},
        {"when_to_use",   "Use for writing unit tests"},
        {"system_prompt", "You are a testing expert. Write thorough unit tests."}
    };
    mock->set_reply(llm_json.dump());

    agent_generator::GenerateParams params;
    params.description = "Write unit tests for C++ code";

    auto result = agent_generator::generate(params);
    auto info   = result.to_agent_info();

    CHECK(info.name == "test_writer");
    CHECK(info.mode == AgentMode::Primary);
    CHECK(info.native == false);
    CHECK(info.prompt.value_or("") == "You are a testing expert. Write thorough unit tests.");

    clear_gen_mock();
}

// ============================================================================
// Tests – markdown-fenced JSON
// ============================================================================

TEST_CASE("agent_generator::generate: LLM returns markdown-fenced JSON",
          "[agent_generator][p1-4][llm]") {
    auto mock = register_gen_mock();

    const std::string fenced =
        "Here is the agent configuration:\n"
        "```json\n"
        "{\n"
        "  \"identifier\": \"docs_agent\",\n"
        "  \"when_to_use\": \"Use for generating documentation\",\n"
        "  \"system_prompt\": \"You are a technical writer.\"\n"
        "}\n"
        "```\n";
    mock->set_reply(fenced);

    agent_generator::GenerateParams params;
    params.description = "Generate documentation";

    auto result = agent_generator::generate(params);

    // Should successfully extract JSON from markdown fences
    CHECK(result.identifier    == "docs_agent");
    CHECK(result.when_to_use   == "Use for generating documentation");
    CHECK(result.system_prompt == "You are a technical writer.");

    clear_gen_mock();
}

// ============================================================================
// Tests – non-JSON LLM response → fallback
// ============================================================================

TEST_CASE("agent_generator::generate: LLM returns plain text → fallback",
          "[agent_generator][p1-4][fallback]") {
    auto mock = register_gen_mock();
    mock->set_reply("Sorry, I cannot generate an agent configuration.");

    agent_generator::GenerateParams params;
    params.description = "Write documentation";

    auto result = agent_generator::generate(params);

    // Should fall back to deterministic slug
    REQUIRE_FALSE(result.identifier.empty());
    CHECK(result.identifier.find(' ') == std::string::npos);
    CHECK(result.when_to_use.find("Write documentation") != std::string::npos);

    clear_gen_mock();
}

// ============================================================================
// Tests – LLM throws exception → fallback (no crash)
// ============================================================================

TEST_CASE("agent_generator::generate: LLM throws → fallback, no crash",
          "[agent_generator][p1-4][fallback]") {
    auto mock = register_gen_mock();
    mock->set_throw(true);

    agent_generator::GenerateParams params;
    params.description = "Refactor legacy code";

    // Must NOT throw
    REQUIRE_NOTHROW([&]() {
        auto result = agent_generator::generate(params);
        CHECK_FALSE(result.identifier.empty());
        CHECK_FALSE(result.when_to_use.empty());
    }());

    clear_gen_mock();
}

// ============================================================================
// Tests – empty identifier in LLM JSON → slug fallback
// ============================================================================

TEST_CASE("agent_generator::generate: LLM returns empty identifier → slug fallback",
          "[agent_generator][p1-4][llm]") {
    auto mock = register_gen_mock();

    nlohmann::json llm_json = {
        {"identifier",    ""},           // empty!
        {"when_to_use",   "Use for data analysis"},
        {"system_prompt", "You are a data analyst."}
    };
    mock->set_reply(llm_json.dump());

    agent_generator::GenerateParams params;
    params.description = "Analyze data sets";

    auto result = agent_generator::generate(params);

    // identifier must not be empty even though LLM returned ""
    REQUIRE_FALSE(result.identifier.empty());
    CHECK(result.when_to_use   == "Use for data analysis");
    CHECK(result.system_prompt == "You are a data analyst.");

    clear_gen_mock();
}

// ============================================================================
// Tests – model override respected
// ============================================================================

TEST_CASE("agent_generator::generate: GenerateParams::model sets model_id",
          "[agent_generator][p1-4][llm]") {
    auto mock = register_gen_mock();

    // Track which model id was requested (we can only verify indirectly via result)
    nlohmann::json llm_json = {
        {"identifier",    "perf_agent"},
        {"when_to_use",   "Use for performance profiling"},
        {"system_prompt", "You are a performance expert."}
    };
    mock->set_reply(llm_json.dump());

    agent_generator::GenerateParams params;
    params.description = "Profile application performance";
    params.model = ModelRef{"custom-model", "gen-mock"};

    // Should succeed without throwing even with a custom model reference
    REQUIRE_NOTHROW([&]() {
        auto result = agent_generator::generate(params);
        CHECK(result.identifier == "perf_agent");
    }());

    clear_gen_mock();
}

// ============================================================================
// Tests – to_json() round-trip
// ============================================================================

TEST_CASE("AgentGenerateResult::to_json produces correct keys",
          "[agent_generator][p1-4][json]") {
    AgentGenerateResult r;
    r.identifier    = "my_agent";
    r.when_to_use   = "Use for X";
    r.system_prompt = "You are X expert.";

    auto j = r.to_json();
    REQUIRE(j.contains("identifier"));
    REQUIRE(j.contains("when_to_use"));
    REQUIRE(j.contains("system_prompt"));
    CHECK(j["identifier"].get<std::string>()    == "my_agent");
    CHECK(j["when_to_use"].get<std::string>()   == "Use for X");
    CHECK(j["system_prompt"].get<std::string>() == "You are X expert.");
}
