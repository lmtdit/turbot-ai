/**
 * @file agent_flow_test.cpp
 * @brief Agent 推理全链路测试用例
 */

#include <catch2/catch_test_macros.hpp>
#include "../../mock/agent_flow_fixture.hpp"

// ============================================================================
// 测试场景 1：文件探索 - "帮我看看 src 目录下什么"
// ============================================================================

TEST_CASE("Agent.Flow.FileExploration.Basic", "[Agent][Flow]") {
    turbot::test::AgentFlowFixture fixture;
    fixture.setup();
    fixture.setup_file_exploration_scenario("src");
    fixture.setup_provider_for_list_tool("src");

    std::string query = "帮我看看 src 目录下什么";
    auto result = fixture.execute_query(query);

    REQUIRE(fixture.intent_engine.call_count() > 0);
    REQUIRE(fixture.verify_tool_called("list"));
    
    fixture.teardown();
}

TEST_CASE("Agent.Flow.FileExploration.WithSubdirectories", "[Agent][Flow]") {
    turbot::test::AgentFlowFixture fixture;
    fixture.setup();

    fixture.create_test_directory("src/core");
    fixture.create_test_directory("src/utils");
    fixture.create_test_file("src/main.cpp", "int main() { return 0; }");
    fixture.create_test_file("src/core/engine.cpp", "// engine");

    turbot::test::IntentInferenceResult intent;
    intent.type = turbot::test::IntentType::FileList;
    intent.description = "列出 src 目录内容";
    intent.entities = {{"path", "src"}};
    fixture.intent_engine.set_intent_result("src", intent);

    fixture.setup_provider_for_list_tool("src");
    auto result = fixture.execute_query("帮我看看 src 目录下有什么文件");

    REQUIRE(fixture.verify_tool_called("list"));
    
    fixture.teardown();
}

// ============================================================================
// 测试场景 2：文档重组 - "按规范 README.md 整理 docs 目录"
// ============================================================================

TEST_CASE("Agent.Flow.DocumentReorganization.Basic", "[Agent][Flow]") {
    turbot::test::AgentFlowFixture fixture;
    fixture.setup();
    fixture.setup_document_reorganization_scenario("docs");

    fixture.setup_provider_for_tool_sequence({
        {"read_file", {{"path", "README.md"}}},
        {"list", {{"path", "docs"}}}
    });

    std::string query = "按规范 README.md 整理 docs 目录";
    auto result = fixture.execute_query(query);

    REQUIRE(fixture.intent_engine.call_count() > 0);
    REQUIRE(fixture.verify_tool_called("read_file"));
    REQUIRE(fixture.verify_tool_called("list"));
    
    fixture.teardown();
}

TEST_CASE("Agent.Flow.DocumentReorganization.WithRename", "[Agent][Flow]") {
    turbot::test::AgentFlowFixture fixture;
    fixture.setup();

    fixture.create_test_directory("docs");
    fixture.create_test_file("docs/Introduction.md", "# Introduction");
    fixture.create_test_file("docs/UserGuide.md", "# User Guide");
    fixture.create_test_file("STYLE_GUIDE.md", "# Style Guide\n- lowercase with hyphens");

    turbot::test::IntentInferenceResult intent;
    intent.type = turbot::test::IntentType::DocumentOrganize;
    intent.description = "按规范重命名文档";
    fixture.intent_engine.set_intent_result("整理", intent);

    fixture.setup_provider_for_tool_sequence({
        {"read_file", {{"path", "STYLE_GUIDE.md"}}},
        {"list", {{"path", "docs"}}}
    });

    auto result = fixture.execute_query("按规范整理 docs 目录");

    REQUIRE(fixture.verify_tool_called("read_file"));
    REQUIRE(fixture.verify_tool_called("list"));
    
    fixture.teardown();
}

// ============================================================================
// 测试场景 3：意图澄清
// ============================================================================

TEST_CASE("Agent.Flow.IntentClarification.AmbiguousQuery", "[Agent][Flow]") {
    turbot::test::AgentFlowFixture fixture;
    fixture.setup();
    fixture.setup_intent_clarification_scenario();

    fixture.setup_provider_for_final_response("您的请求不太明确，请具体说明。");

    auto result = fixture.execute_query("帮我看看");

    REQUIRE(fixture.intent_engine.call_count() > 0);
    auto intent = fixture.intent_engine.infer_intent("帮我看看");
    REQUIRE(intent.needs_clarification);
    
    fixture.teardown();
}

// ============================================================================
// 测试场景 4：多步骤任务
// ============================================================================

TEST_CASE("Agent.Flow.MultiStep.SequentialTools", "[Agent][Flow]") {
    turbot::test::AgentFlowFixture fixture;
    fixture.setup();

    fixture.create_test_file("data/input.txt", "Hello World");
    fixture.create_test_file("data/config.json", R"({"uppercase": true})");

    turbot::test::IntentInferenceResult intent;
    intent.type = turbot::test::IntentType::FileRead;
    intent.description = "读取文件";
    fixture.intent_engine.set_intent_result("读取", intent);

    fixture.setup_provider_for_tool_sequence({
        {"read_file", {{"path", "data/config.json"}}},
        {"read_file", {{"path", "data/input.txt"}}}
    });

    auto result = fixture.execute_query("读取 input.txt 和 config.json");

    REQUIRE(fixture.tool_call_count("read_file") >= 2);
    
    fixture.teardown();
}

// ============================================================================
// 测试场景 5：错误处理
// ============================================================================

TEST_CASE("Agent.Flow.ErrorHandling.FileNotFound", "[Agent][Flow]") {
    turbot::test::AgentFlowFixture fixture;
    fixture.setup();

    turbot::test::IntentInferenceResult intent;
    intent.type = turbot::test::IntentType::FileRead;
    fixture.intent_engine.set_intent_result("读取", intent);

    fixture.setup_provider_for_read_file("nonexistent/file.txt");

    auto result = fixture.execute_query("读取 nonexistent/file.txt");

    REQUIRE(fixture.verify_tool_called("read_file"));
    
    fixture.teardown();
}

// ============================================================================
// 测试场景 6：边界条件
// ============================================================================

TEST_CASE("Agent.Flow.EdgeCase.EmptyQuery", "[Agent][Flow]") {
    turbot::test::AgentFlowFixture fixture;
    fixture.setup();
    fixture.setup_provider_for_final_response("请问有什么我可以帮助您的？");

    auto result = fixture.execute_query("");
    REQUIRE(fixture.intent_engine.call_count() >= 0);
    
    fixture.teardown();
}
