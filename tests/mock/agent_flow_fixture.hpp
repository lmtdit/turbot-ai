#pragma once

/**
 * @file agent_flow_fixture.hpp
 * @brief Agent 推理全链路测试夹具
 *
 * 支持完整的 Agent 推理流程测试：
 * 用户发出 query -> Agent 收到请求 -> 解析用户请求 -> LLM 意图推理（可 mock）
 * -> 确立意图 -> Task 设计（可 mock）-> 调用执行 Agent -> 工具调用 -> 返回结果
 */

#include "mock_provider.hpp"
#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_loop.hpp>
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/agent/builtin/build_agent.hpp>
#include <turbot/core/agent/builtin/plan_agent.hpp>
#include <turbot/core/agent/builtin/explore_agent.hpp>
#include <turbot/core/tool/tool.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/tool/builtin/list_tool.hpp>
#include <turbot/core/tool/builtin/read_file_tool.hpp>
#include <turbot/core/tool/builtin/write_file_tool.hpp>
#include <turbot/core/tool/builtin/glob_tool.hpp>
#include <turbot/core/tool/builtin/grep_tool.hpp>
#include <turbot/core/permission/permission.hpp>
#include <turbot/utils/file_utils.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <functional>
#include <unordered_map>
#include <atomic>
#include <unistd.h>

namespace turbot::test {

// ============================================================================
// Agent Flow 场景类型
// ============================================================================

/// Agent 推理场景类型
enum class AgentFlowScenario {
    FileExploration,    ///< 文件探索场景（如：帮我看看 src 目录下什么）
    DocumentReorganization,  ///< 文档重组场景（如：按规范 README.md 整理 docs 目录）
    CodeRefactoring,    ///< 代码重构场景
    IntentClarification,  ///< 意图澄清场景
    MultiStepTask,      ///< 多步骤任务场景
};

/// 意图类型
enum class IntentType {
    FileList,           ///< 列出文件/目录
    FileRead,           ///< 读取文件
    FileWrite,          ///< 写入文件
    FileSearch,         ///< 搜索文件
    DocumentOrganize,   ///< 整理文档
    CodeAnalysis,       ///< 代码分析
    Unknown,            ///< 未知意图
};

/// 意图推理结果
struct IntentInferenceResult {
    IntentType type = IntentType::Unknown;
    std::string description;
    nlohmann::json entities;  ///< 提取的实体（路径、文件名等）
    bool needs_clarification = false;
    std::string clarification_question;
    double confidence = 0.0;
};

/// 任务设计结果
struct TaskDesignResult {
    std::string task_description;
    std::vector<std::string> steps;
    std::string agent_to_use;
    std::vector<std::string> tools_needed;
};

// ============================================================================
// Mock Intent Inference Engine
// ============================================================================

/// Mock 意图推理引擎
class MockIntentInferenceEngine {
public:
    /// 设置意图推理结果
    void set_intent_result(const std::string& query_pattern, const IntentInferenceResult& result) {
        intent_results_[query_pattern] = result;
    }

    /// 设置默认意图推理结果
    void set_default_intent_result(const IntentInferenceResult& result) {
        default_result_ = result;
    }

    /// 推理意图
    IntentInferenceResult infer_intent(const std::string& query) {
        // 尝试匹配模式
        for (const auto& [pattern, result] : intent_results_) {
            if (query.find(pattern) != std::string::npos) {
                call_count_++;
                last_query_ = query;
                return result;
            }
        }

        // 返回默认结果
        call_count_++;
        last_query_ = query;
        return default_result_;
    }

    /// 重置状态
    void reset() {
        intent_results_.clear();
        default_result_ = IntentInferenceResult{};
        call_count_ = 0;
        last_query_.clear();
    }

    int call_count() const { return call_count_; }
    const std::string& last_query() const { return last_query_; }

private:
    std::unordered_map<std::string, IntentInferenceResult> intent_results_;
    IntentInferenceResult default_result_;
    int call_count_ = 0;
    std::string last_query_;
};

// ============================================================================
// Mock Task Design Engine
// ============================================================================

/// Mock 任务设计引擎
class MockTaskDesignEngine {
public:
    /// 设置任务设计结果
    void set_task_design(IntentType intent, const TaskDesignResult& design) {
        task_designs_[intent] = design;
    }

    /// 设计任务
    TaskDesignResult design_task(const IntentInferenceResult& intent) {
        call_count_++;
        last_intent_ = intent;

        if (task_designs_.count(intent.type)) {
            return task_designs_[intent.type];
        }

        // 默认任务设计
        TaskDesignResult default_design;
        default_design.task_description = intent.description;
        default_design.agent_to_use = "build";
        return default_design;
    }

    /// 重置状态
    void reset() {
        task_designs_.clear();
        call_count_ = 0;
    }

    int call_count() const { return call_count_; }

private:
    std::unordered_map<IntentType, TaskDesignResult> task_designs_;
    int call_count_ = 0;
    IntentInferenceResult last_intent_;
};

// ============================================================================
// Agent Flow Fixture
// ============================================================================

/// Agent 推理流程测试夹具
struct AgentFlowFixture {
    // === 核心组件 ===
    std::unique_ptr<core::session::Session> session;
    std::shared_ptr<core::agent::Agent> agent;
    std::shared_ptr<MockProvider> provider;
    std::unique_ptr<core::session::SessionLoop> loop;
    std::filesystem::path test_dir;

    // === Mock 引擎 ===
    MockIntentInferenceEngine intent_engine;
    MockTaskDesignEngine task_engine;

    // === 执行追踪 ===
    std::vector<std::string> executed_tools;
    std::vector<nlohmann::json> tool_inputs;
    std::vector<core::tool::ToolResult> tool_results;
    std::vector<std::string> messages_log;
    std::atomic<int> step_count_{0};

    // === 权限控制 ===
    bool auto_approve_permissions = true;

    /// 初始化测试环境
    void setup() {
        // 创建测试目录（包含 PID 避免冲突）
        test_dir = std::filesystem::temp_directory_path() / (
            "turbot-agent-flow-" + std::to_string(std::time(nullptr)) +
            "-" + std::to_string(getpid())
        );
        std::filesystem::create_directories(test_dir);

        // 创建 Session
        core::session::CreateParams params;
        params.project_id = "agent-flow-test";
        params.slug = "test-session";
        params.directory = test_dir.string();
        params.title = "Agent Flow Test";

        auto session_result = core::session::Session::create(params);
        REQUIRE(session_result.has_value());
        session = std::make_unique<core::session::Session>(std::move(*session_result));

        // 注册内置 Agent
        auto& registry = core::agent::AgentRegistry::instance();
        registry.clear();
        registry.register_agent(std::make_shared<core::agent::BuildAgent>());
        registry.register_agent(std::make_shared<core::agent::PlanAgent>());
        registry.register_agent(std::make_shared<core::agent::ExploreAgent>());

        agent = registry.get("build");
        REQUIRE(agent != nullptr);

        // 创建 Mock Provider
        provider = MockProviderBuilder()
            .with_api_key("mock-api-key")
            .with_model(core::provider::ModelInfo{
                .id = "mock-model",
                .provider_id = "mock",
                .name = "Mock Model",
                .description = "A mock model for testing",
                .capabilities = {.temperature = true, .tool_call = true, .streaming = true},
                .context_window = 128000
            })
            .build();

        // 创建 SessionLoop
        loop = std::make_unique<core::session::SessionLoop>(*session);
        loop->set_agent(agent);
        loop->set_provider(provider.get());
        loop->set_model("mock-model");

        // 设置回调
        setup_callbacks();

        // 注册内置工具
        register_builtin_tools();
    }

    /// 清理测试环境
    void teardown() {
        loop.reset();
        session.reset();
        provider.reset();

        // 清理测试目录
        if (std::filesystem::exists(test_dir)) {
            std::error_code ec;
            std::filesystem::remove_all(test_dir, ec);
        }

        // 清理注册表
        core::agent::AgentRegistry::instance().clear();
        core::tool::ToolRegistry::instance().clear();
    }

    /// 设置回调
    void setup_callbacks() {
        // 工具调用回调
        loop->set_on_tool_call([this](const std::string& tool_name,
                                       const std::string& call_id,
                                       const nlohmann::json& input) {
            executed_tools.push_back(tool_name);
            tool_inputs.push_back(input);
            messages_log.push_back("Tool call: " + tool_name + " [" + call_id + "]");
        });

        // 工具结果回调
        loop->set_on_tool_result([this](const std::string& tool_name,
                                         const std::string& call_id,
                                         const core::tool::ToolResult& result) {
            tool_results.push_back(result);
            messages_log.push_back("Tool result: " + tool_name + " [" + call_id + "] " +
                                   (!result.is_error ? "SUCCESS" : "ERROR"));
        });

        // 消息回调
        loop->set_on_message([this](const core::Message& msg) {
            messages_log.push_back("Message: " + std::string(role_to_string(msg.role())) + " - " +
                                   msg.get_text());
        });

        // 步骤回调
        loop->set_on_step([this](const core::session::StepInfo& info) {
            step_count_++;
        });

        // 权限回调
        loop->set_on_permission_request([this](const core::permission::PermissionRequest& req) {
            if (auto_approve_permissions) {
                return core::permission::PermissionReply::once();
            }
            return core::permission::PermissionReply::reject();
        });
    }

    /// 注册内置工具
    void register_builtin_tools() {
        auto& registry = core::tool::ToolRegistry::instance();
        registry.register_tool(std::make_unique<core::tool::builtin::ListTool>());
        registry.register_tool(std::make_unique<core::tool::builtin::ReadFileTool>());
        registry.register_tool(std::make_unique<core::tool::builtin::WriteFileTool>());
        registry.register_tool(std::make_unique<core::tool::builtin::GlobTool>());
        registry.register_tool(std::make_unique<core::tool::builtin::GrepTool>());
    }

    // === 文件系统操作 ===

    /// 创建测试文件
    void create_test_file(const std::string& relative_path, const std::string& content) {
        auto full_path = test_dir / relative_path;
        std::filesystem::create_directories(full_path.parent_path());
        std::ofstream file(full_path);
        file << content;
    }

    /// 创建测试目录
    void create_test_directory(const std::string& relative_path) {
        std::filesystem::create_directories(test_dir / relative_path);
    }

    /// 读取测试文件
    std::string read_test_file(const std::string& relative_path) {
        auto full_path = test_dir / relative_path;
        return turbot::utils::read_file(full_path.string()).value_or("");
    }

    /// 检查文件是否存在
    bool file_exists(const std::string& relative_path) {
        return std::filesystem::exists(test_dir / relative_path);
    }

    // === 场景设置 ===

    /// 设置文件探索场景
    void setup_file_exploration_scenario(const std::string& target_dir) {
        // 创建目录结构
        create_test_directory(target_dir + "/subdir1");
        create_test_directory(target_dir + "/subdir2");
        create_test_file(target_dir + "/file1.txt", "Content of file1");
        create_test_file(target_dir + "/file2.md", "# File 2\nContent here");
        create_test_file(target_dir + "/subdir1/nested.txt", "Nested file");
        create_test_file(target_dir + "/subdir2/deep.txt", "Deep file");

        // 设置意图推理结果
        IntentInferenceResult intent;
        intent.type = IntentType::FileList;
        intent.description = "列出目录内容";
        intent.entities = {{"path", target_dir}};
        intent.needs_clarification = false;
        intent.confidence = 0.95;
        intent_engine.set_intent_result("看看", intent);
        intent_engine.set_intent_result("目录", intent);
        intent_engine.set_intent_result("src", intent);

        // 设置任务设计
        TaskDesignResult design;
        design.task_description = "列出指定目录的文件和子目录";
        design.steps = {"使用 list 工具列出目录内容"};
        design.agent_to_use = "build";
        design.tools_needed = {"list"};
        task_engine.set_task_design(IntentType::FileList, design);

        // 配置 Mock Provider 返回工具调用
        setup_provider_for_list_tool(target_dir);
    }

    /// 设置文档重组场景
    void setup_document_reorganization_scenario(const std::string& target_dir) {
        // 创建文档目录结构
        create_test_directory(target_dir);
        create_test_file(target_dir + "/intro.md", "# Introduction\nIntro content");
        create_test_file(target_dir + "/guide.md", "# Guide\nGuide content");
        create_test_file(target_dir + "/api.md", "# API Reference\nAPI docs");
        create_test_file(target_dir + "/misc.txt", "Some misc notes");

        // 创建参考 README
        create_test_file("README.md", R"(
# Project README

## Documentation Structure
- docs/intro.md - Introduction
- docs/guide.md - User Guide
- docs/api.md - API Reference

## Naming Convention
All documentation files should use lowercase with hyphens.
)");

        // 设置意图推理结果
        IntentInferenceResult intent;
        intent.type = IntentType::DocumentOrganize;
        intent.description = "按规范整理文档目录";
        intent.entities = {
            {"target_dir", target_dir},
            {"reference_file", "README.md"}
        };
        intent.needs_clarification = false;
        intent.confidence = 0.88;
        intent_engine.set_intent_result("整理", intent);
        intent_engine.set_intent_result("规范", intent);
        intent_engine.set_intent_result("README", intent);

        // 设置任务设计
        TaskDesignResult design;
        design.task_description = "按照 README.md 规范整理文档目录";
        design.steps = {
            "读取 README.md 了解规范",
            "列出目标目录内容",
            "分析文件命名是否符合规范",
            "重命名或移动不符合规范的文件"
        };
        design.agent_to_use = "build";
        design.tools_needed = {"read_file", "list", "write_file"};
        task_engine.set_task_design(IntentType::DocumentOrganize, design);
    }

    /// 设置意图澄清场景
    void setup_intent_clarification_scenario() {
        IntentInferenceResult intent;
        intent.type = IntentType::Unknown;
        intent.description = "无法确定用户意图";
        intent.needs_clarification = true;
        intent.clarification_question = "请问您具体想要做什么？";
        intent.confidence = 0.3;
        intent_engine.set_default_intent_result(intent);
    }

    // === Provider 配置 ===

    /// 配置 Provider 返回 list 工具调用
    void setup_provider_for_list_tool(const std::string& path) {
        core::provider::ToolCall tc;
        tc.id = "tc-list-1";
        tc.name = "list";
        tc.arguments = {{"path", path}};

        core::provider::ChatResponse response;
        response.id = "mock-list";
        response.model = "mock-model";
        response.finish_reason = "tool_calls";
        response.choices.push_back(
            core::provider::ChatMessage::assistant_with_tools("", {tc})
        );

        provider->set_next_response(response);
    }

    /// 配置 Provider 返回 read_file 工具调用
    void setup_provider_for_read_file(const std::string& path) {
        core::provider::ToolCall tc;
        tc.id = "tc-read-1";
        tc.name = "read_file";
        tc.arguments = {{"path", path}};

        core::provider::ChatResponse response;
        response.id = "mock-read";
        response.model = "mock-model";
        response.finish_reason = "tool_calls";
        response.choices.push_back(
            core::provider::ChatMessage::assistant_with_tools("", {tc})
        );

        provider->set_next_response(response);
    }

    /// 配置 Provider 返回多个工具调用
    void setup_provider_for_multi_tool_call(
        const std::vector<std::pair<std::string, nlohmann::json>>& tools
    ) {
        std::vector<core::provider::ToolCall> tool_calls;
        for (const auto& [name, args] : tools) {
            tool_calls.push_back({
                .id = "tc-" + name + "-" + std::to_string(tool_calls.size()),
                .name = name,
                .arguments = args
            });
        }

        core::provider::ChatResponse response;
        response.id = "mock-multi";
        response.model = "mock-model";
        response.finish_reason = "tool_calls";
        response.choices.push_back(
            core::provider::ChatMessage::assistant_with_tools("", tool_calls)
        );

        provider->set_next_response(response);
    }

    /// 配置 Provider 返回最终文本响应
    void setup_provider_for_final_response(const std::string& text) {
        core::provider::ChatResponse response;
        response.id = "mock-final";
        response.model = "mock-model";
        response.finish_reason = "stop";
        response.choices.push_back(core::provider::ChatMessage::assistant(text));

        provider->set_next_response(response);
    }

    /// 配置 Provider 返回工具调用序列
    void setup_provider_for_tool_sequence(
        const std::vector<std::pair<std::string, nlohmann::json>>& tools
    ) {
        for (const auto& [name, args] : tools) {
            core::provider::ToolCall tc;
            tc.id = "tc-" + name;
            tc.name = name;
            tc.arguments = args;

            core::provider::ChatResponse response;
            response.id = "mock-seq-" + name;
            response.model = "mock-model";
            response.finish_reason = "tool_calls";
            response.choices.push_back(
                core::provider::ChatMessage::assistant_with_tools("", {tc})
            );

            provider->set_next_response(response);
        }
        // 最后返回最终响应
        setup_provider_for_final_response("任务已完成。");
    }

    // === 执行和验证 ===

    /// 执行用户查询
    core::session::LoopResult execute_query(const std::string& query) {
        // 推理意图
        auto intent = intent_engine.infer_intent(query);

        // 设计任务
        auto task = task_engine.design_task(intent);

        // 运行 SessionLoop
        return loop->run(query);
    }

    /// 验证工具被调用
    bool verify_tool_called(const std::string& tool_name) const {
        return std::find(executed_tools.begin(), executed_tools.end(), tool_name)
               != executed_tools.end();
    }

    /// 验证工具调用次数
    int tool_call_count(const std::string& tool_name) const {
        return std::count(executed_tools.begin(), executed_tools.end(), tool_name);
    }

    /// 获取工具输入
    std::optional<nlohmann::json> get_tool_input(const std::string& tool_name, int index = 0) const {
        int count = 0;
        for (size_t i = 0; i < executed_tools.size(); ++i) {
            if (executed_tools[i] == tool_name) {
                if (count == index) {
                    return tool_inputs[i];
                }
                count++;
            }
        }
        return std::nullopt;
    }

    /// 重置执行状态
    void reset_execution_state() {
        executed_tools.clear();
        tool_inputs.clear();
        tool_results.clear();
        messages_log.clear();
        step_count_ = 0;
        provider->reset();
    }
};

/// RAII 包装器
struct AgentFlowTest : public AgentFlowFixture {
    AgentFlowTest() { setup(); }
    ~AgentFlowTest() { teardown(); }
};

} // namespace turbot::test
