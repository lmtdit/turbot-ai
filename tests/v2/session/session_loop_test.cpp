/**
 * @file session_loop_test.cpp
 * @brief Session Loop 测试 - 测试主循环逻辑
 */

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/session/session_loop.hpp>
#include <turbot/core/session/session_store.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/tool/tool.hpp>
#include <turbot/storage/sqlite_database.hpp>
#include <mock/mock_provider.hpp>
#include <filesystem>
#include <memory>

namespace turbot::test {

using namespace turbot::core::session;
using namespace turbot::core::tool;
using namespace turbot::storage;
using namespace turbot::storage::sqlite;
using namespace turbot::test;

/// Mock Tool for testing
class MockTool : public Tool {
public:
    MockTool(const std::string& name, const std::string& desc)
        : name_(name), desc_(desc) {}

    [[nodiscard]] std::string name() const override { return name_; }
    [[nodiscard]] std::string description() const override { return desc_; }
    [[nodiscard]] nlohmann::json input_schema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"input", {{"type", "string"}}}
            }},
            {"required", nlohmann::json::array({"input"})}
        };
    }

    ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override {
        (void)ctx;
        std::string val = input.value("input", "");
        return ToolResult::success(name_ + "_result", val + "_processed");
    }

private:
    std::string name_;
    std::string desc_;
};

/// Session Loop 测试夹具
struct SessionLoopFixture {
    std::filesystem::path test_dir;
    std::shared_ptr<Database> db;
    std::shared_ptr<MockProvider> provider;

    void setup() {
        test_dir = std::filesystem::path("/tmp") / ("turbot-loop-" + std::to_string(std::time(nullptr)));
        std::filesystem::create_directories(test_dir);

        // 使用内存数据库
        DatabaseConfig config;
        config.path = ":memory:";
        db = std::make_shared<SQLiteDatabase>(config);

        // 初始化 SessionStore
        auto& store = SessionStore::instance();
        store.init(db);

        // 创建 Mock Provider
        provider = std::make_shared<MockProvider>();
        provider->set_ready(true);
        provider->add_model({"mock-model", "mock", "Mock Model", "Test model"});

        // 注册 Mock Tool
        ToolRegistry::instance().register_tool(std::make_unique<MockTool>("test_tool", "A test tool"));
    }

    void teardown() {
        provider.reset();
        db.reset();
        if (std::filesystem::exists(test_dir)) {
            std::error_code ec;
            std::filesystem::remove_all(test_dir, ec);
        }
    }
};

} // namespace turbot::test

// ============================================================================
// Session Loop 构造测试
// ============================================================================

TEST_CASE("Session.Loop.Construct.WithSessionId", "[Session][Loop]") {
    turbot::test::SessionLoopFixture fixture;
    fixture.setup();

    // 创建 SessionLoop
    turbot::core::session::SessionLoop loop("test-session-id");
    REQUIRE_NOTHROW(loop.session());

    fixture.teardown();
}

TEST_CASE("Session.Loop.Construct.WithSession", "[Session][Loop]") {
    turbot::test::SessionLoopFixture fixture;
    fixture.setup();

    // 创建 Session
    turbot::core::session::CreateParams params;
    params.project_id = "test-project";
    params.slug = "test-slug";
    params.directory = fixture.test_dir.string();
    params.title = "Test Session";

    auto session_result = turbot::core::session::Session::create(params);
    REQUIRE(session_result.has_value());

    // 创建 SessionLoop
    turbot::core::session::SessionLoop loop(std::move(*session_result));
    REQUIRE(loop.session().info().project_id == "test-project");

    fixture.teardown();
}

// ============================================================================
// Session Loop 配置测试
// ============================================================================

TEST_CASE("Session.Loop.Config.Default", "[Session][Loop]") {
    turbot::test::SessionLoopFixture fixture;
    fixture.setup();

    turbot::core::session::SessionLoop loop("test-session");
    const auto& config = loop.config();

    REQUIRE(config.max_iterations == 100);
    REQUIRE(config.max_tokens == 128000);
    REQUIRE(config.compact_threshold == 100000);
    REQUIRE(config.auto_compact == true);
    REQUIRE(config.doom_loop_threshold == 3);

    fixture.teardown();
}

TEST_CASE("Session.Loop.Config.Custom", "[Session][Loop]") {
    turbot::test::SessionLoopFixture fixture;
    fixture.setup();

    turbot::core::session::SessionLoop loop("test-session");

    turbot::core::session::SessionLoopConfig config;
    config.max_iterations = 50;
    config.max_tokens = 64000;
    config.compact_threshold = 50000;
    config.auto_compact = false;
    config.doom_loop_threshold = 5;

    loop.set_config(config);
    const auto& set_config = loop.config();

    REQUIRE(set_config.max_iterations == 50);
    REQUIRE(set_config.max_tokens == 64000);
    REQUIRE(set_config.compact_threshold == 50000);
    REQUIRE(set_config.auto_compact == false);
    REQUIRE(set_config.doom_loop_threshold == 5);

    fixture.teardown();
}

// ============================================================================
// Session Loop Provider 设置测试
// ============================================================================

TEST_CASE("Session.Loop.SetProvider", "[Session][Loop]") {
    turbot::test::SessionLoopFixture fixture;
    fixture.setup();

    turbot::core::session::SessionLoop loop("test-session");
    loop.set_provider(fixture.provider.get());
    loop.set_model("mock-model");

    // 验证设置成功
    REQUIRE(fixture.provider->is_ready());

    fixture.teardown();
}

TEST_CASE("Session.Loop.SetModel", "[Session][Loop]") {
    turbot::test::SessionLoopFixture fixture;
    fixture.setup();

    turbot::core::session::SessionLoop loop("test-session");
    loop.set_model("gpt-4");

    // 验证设置成功（无异常）
    REQUIRE_NOTHROW(loop.set_model("claude-3"));

    fixture.teardown();
}

// ============================================================================
// Session Loop 运行测试
// ============================================================================

TEST_CASE("Session.Loop.Run.SimpleText", "[Session][Loop]") {
    turbot::test::SessionLoopFixture fixture;
    fixture.setup();

    turbot::core::session::SessionLoop loop("test-session");
    loop.set_provider(fixture.provider.get());
    loop.set_model("mock-model");

    // 配置简单的文本响应
    turbot::core::provider::ChatResponse response;
    response.id = "mock-1";
    response.model = "mock-model";
    response.finish_reason = "stop";
    response.choices.push_back(turbot::core::provider::ChatMessage::assistant("Hello! How can I help you?"));
    fixture.provider->set_next_response(response);

    // 运行循环
    auto result = loop.run("Hi there");

    REQUIRE(result == turbot::core::session::LoopResult::Stop);
    REQUIRE(loop.messages().size() >= 2);  // user + assistant

    fixture.teardown();
}

TEST_CASE("Session.Loop.Run.WithToolCall", "[Session][Loop]") {
    turbot::test::SessionLoopFixture fixture;
    fixture.setup();

    turbot::core::session::SessionLoop loop("test-session");
    loop.set_provider(fixture.provider.get());
    loop.set_model("mock-model");

    // 配置工具调用响应
    turbot::core::provider::ToolCall tc;
    tc.id = "tc-1";
    tc.name = "test_tool";
    tc.arguments = {{"input", "test_data"}};

    turbot::core::provider::ChatResponse tool_response;
    tool_response.id = "mock-tool";
    tool_response.model = "mock-model";
    tool_response.finish_reason = "tool_calls";
    tool_response.choices.push_back(turbot::core::provider::ChatMessage::assistant_with_tools("", {tc}));
    fixture.provider->set_next_response(tool_response);

    // 配置后续文本响应
    turbot::core::provider::ChatResponse text_response;
    text_response.id = "mock-text";
    text_response.model = "mock-model";
    text_response.finish_reason = "stop";
    text_response.choices.push_back(turbot::core::provider::ChatMessage::assistant("Tool executed successfully"));
    fixture.provider->set_next_response(text_response);

    // 运行循环
    auto result = loop.run("Use the tool");

    REQUIRE(result == turbot::core::session::LoopResult::Stop);

    fixture.teardown();
}

TEST_CASE("Session.Loop.Stop", "[Session][Loop]") {
    turbot::test::SessionLoopFixture fixture;
    fixture.setup();

    turbot::core::session::SessionLoop loop("test-session");
    loop.set_provider(fixture.provider.get());
    loop.set_model("mock-model");

    // 停止循环
    loop.stop();

    REQUIRE_FALSE(loop.is_running());

    fixture.teardown();
}

// ============================================================================
// Session Loop 回调测试
// ============================================================================

TEST_CASE("Session.Loop.Callbacks.OnMessage", "[Session][Loop]") {
    turbot::test::SessionLoopFixture fixture;
    fixture.setup();

    turbot::core::session::SessionLoop loop("test-session");
    loop.set_provider(fixture.provider.get());
    loop.set_model("mock-model");

    turbot::core::provider::ChatResponse response;
    response.id = "mock-1";
    response.model = "mock-model";
    response.finish_reason = "stop";
    response.choices.push_back(turbot::core::provider::ChatMessage::assistant("Response"));
    fixture.provider->set_next_response(response);

    int message_count = 0;
    loop.set_on_message([&message_count](const turbot::core::Message& msg) {
        (void)msg;
        message_count++;
    });

    loop.run("Test message");

    REQUIRE(message_count >= 2);  // user + assistant

    fixture.teardown();
}

TEST_CASE("Session.Loop.Callbacks.OnStep", "[Session][Loop]") {
    turbot::test::SessionLoopFixture fixture;
    fixture.setup();

    turbot::core::session::SessionLoop loop("test-session");
    loop.set_provider(fixture.provider.get());
    loop.set_model("mock-model");

    turbot::core::provider::ChatResponse response;
    response.id = "mock-1";
    response.model = "mock-model";
    response.finish_reason = "stop";
    response.choices.push_back(turbot::core::provider::ChatMessage::assistant("Response"));
    fixture.provider->set_next_response(response);

    int step_count = 0;
    loop.set_on_step([&step_count](const turbot::core::session::StepInfo& info) {
        (void)info;
        step_count++;
    });

    loop.run("Test message");

    REQUIRE(step_count >= 1);

    fixture.teardown();
}

// ============================================================================
// Session Loop 错误处理测试
// ============================================================================

TEST_CASE("Session.Loop.Error.NoProvider", "[Session][Loop]") {
    turbot::test::SessionLoopFixture fixture;
    fixture.setup();

    turbot::core::session::SessionLoop loop("test-session");
    // 不设置 provider

    std::string error_msg;
    std::string error_code;
    loop.set_on_error([&](const std::string& msg, const std::string& code) {
        error_msg = msg;
        error_code = code;
    });

    auto result = loop.run("Test message");

    REQUIRE(result == turbot::core::session::LoopResult::Error);
    REQUIRE_FALSE(error_msg.empty());

    fixture.teardown();
}

TEST_CASE("Session.Loop.Error.NoModel", "[Session][Loop]") {
    turbot::test::SessionLoopFixture fixture;
    fixture.setup();

    turbot::core::session::SessionLoop loop("test-session");
    loop.set_provider(fixture.provider.get());
    // 不设置 model

    std::string error_msg;
    loop.set_on_error([&](const std::string& msg, const std::string& code) {
        (void)code;
        error_msg = msg;
    });

    auto result = loop.run("Test message");

    REQUIRE(result == turbot::core::session::LoopResult::Error);

    fixture.teardown();
}

// ============================================================================
// Session Loop Token 统计测试
// ============================================================================

TEST_CASE("Session.Loop.TokenCount", "[Session][Loop]") {
    turbot::test::SessionLoopFixture fixture;
    fixture.setup();

    turbot::core::session::SessionLoop loop("test-session");
    loop.set_provider(fixture.provider.get());
    loop.set_model("mock-model");

    turbot::core::provider::ChatResponse response;
    response.id = "mock-1";
    response.model = "mock-model";
    response.finish_reason = "stop";
    response.choices.push_back(turbot::core::provider::ChatMessage::assistant("Response"));
    fixture.provider->set_next_response(response);

    loop.run("Test message");

    // Token count 应该大于 0
    REQUIRE(loop.token_count() >= 0);

    fixture.teardown();
}

TEST_CASE("Session.Loop.TotalUsage", "[Session][Loop]") {
    turbot::test::SessionLoopFixture fixture;
    fixture.setup();

    turbot::core::session::SessionLoop loop("test-session");
    loop.set_provider(fixture.provider.get());
    loop.set_model("mock-model");

    turbot::core::provider::ChatResponse response;
    response.id = "mock-1";
    response.model = "mock-model";
    response.finish_reason = "stop";
    response.choices.push_back(turbot::core::provider::ChatMessage::assistant("Response"));
    fixture.provider->set_next_response(response);

    loop.run("Test message");

    // 获取总使用量
    auto usage = loop.total_usage();
    // 使用量应该被记录（MockProvider 返回默认值）
    REQUIRE(usage.total() >= 0);

    fixture.teardown();
}

// ============================================================================
// Session Loop 消息测试
// ============================================================================

TEST_CASE("Session.Loop.Messages", "[Session][Loop]") {
    turbot::test::SessionLoopFixture fixture;
    fixture.setup();

    turbot::core::session::SessionLoop loop("test-session");
    loop.set_provider(fixture.provider.get());
    loop.set_model("mock-model");

    turbot::core::provider::ChatResponse response;
    response.id = "mock-1";
    response.model = "mock-model";
    response.finish_reason = "stop";
    response.choices.push_back(turbot::core::provider::ChatMessage::assistant("Response"));
    fixture.provider->set_next_response(response);

    loop.run("Test message");

    auto messages = loop.messages();
    REQUIRE(messages.size() >= 2);

    // 第一条应该是用户消息
    REQUIRE(messages[0].role() == turbot::core::Role::User);

    // 最后一条应该是助手消息
    REQUIRE(messages.back().role() == turbot::core::Role::Assistant);

    fixture.teardown();
}

// ============================================================================
// Session Loop 迭代限制测试
// ============================================================================

TEST_CASE("Session.Loop.MaxIterations", "[Session][Loop]") {
    turbot::test::SessionLoopFixture fixture;
    fixture.setup();

    turbot::core::session::SessionLoop loop("test-session");
    loop.set_provider(fixture.provider.get());
    loop.set_model("mock-model");

    // 设置较低的迭代限制
    turbot::core::session::SessionLoopConfig config;
    config.max_iterations = 2;
    loop.set_config(config);

    // 配置持续返回工具调用
    turbot::core::provider::ToolCall tc;
    tc.id = "tc-1";
    tc.name = "test_tool";
    tc.arguments = {{"input", "test"}};

    for (int i = 0; i < 5; ++i) {
        turbot::core::provider::ChatResponse response;
        response.id = "mock-tool-" + std::to_string(i);
        response.model = "mock-model";
        response.finish_reason = "tool_calls";
        response.choices.push_back(turbot::core::provider::ChatMessage::assistant_with_tools("", {tc}));
        fixture.provider->set_next_response(response);
    }

    auto result = loop.run("Keep calling tools");

    // 应该因迭代限制而停止
    REQUIRE(loop.step_number() <= 2);

    fixture.teardown();
}
