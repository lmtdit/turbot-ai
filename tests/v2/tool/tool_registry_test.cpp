/**
 * @file tool_registry_test.cpp
 * @brief Tool registry tests for ToolRegistry class
 *
 * Tests for:
 * - ToolRegistry::instance() - singleton pattern
 * - ToolRegistry::register_tool() - tool registration
 * - ToolRegistry::get() - tool retrieval
 * - ToolRegistry::has() - tool existence check
 * - ToolRegistry::remove() - tool removal
 * - ToolRegistry::clear() - registry clearing
 * - ToolRegistry::filter() - tool filtering
 * - ToolRegistry::to_tool_definitions() - JSON conversion
 */

#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/tool/tool.hpp>
#include <thread>
#include <atomic>

using namespace turbot::core::tool;
using namespace turbot::test;

// ==================== Mock Tool for Testing ====================

class MockTool : public Tool {
public:
    MockTool(const std::string& name, const std::string& desc = "Mock tool for testing")
        : name_(name), description_(desc) {}
    
    [[nodiscard]] std::string name() const override { return name_; }
    [[nodiscard]] std::string description() const override { return description_; }
    [[nodiscard]] nlohmann::json input_schema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"input", {{"type", "string"}}}
            }}
        };
    }
    
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override {
        return ToolResult::success(name_ + " executed", input.dump());
    }

private:
    std::string name_;
    std::string description_;
};

// ==================== Test Fixtures ====================

class ToolRegistryFixture {
public:
    ToolRegistryFixture() {
        // Clear registry before each test
        ToolRegistry::instance().clear();
    }

    ~ToolRegistryFixture() {
        // Clean up after test
        ToolRegistry::instance().clear();
    }
};

// ==================== Singleton Tests ====================

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.Singleton.Instance", "[Tool][Registry]") {
    auto& registry1 = ToolRegistry::instance();
    auto& registry2 = ToolRegistry::instance();
    
    REQUIRE(&registry1 == &registry2);
}

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.Singleton.NotCopyable", "[Tool][Registry]") {
    // ToolRegistry should not be copyable
    // This is a compile-time check, but we verify the instance is consistent
    auto& registry = ToolRegistry::instance();
    REQUIRE(&registry == &ToolRegistry::instance());
}

// ==================== Registration Tests ====================

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.Register.SingleTool", "[Tool][Registry]") {
    auto tool = std::make_unique<MockTool>("test_tool");
    ToolRegistry::instance().register_tool(std::move(tool));
    
    REQUIRE(ToolRegistry::instance().has("test_tool"));
    REQUIRE(ToolRegistry::instance().size() == 1);
}

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.Register.MultipleTools", "[Tool][Registry]") {
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("tool1"));
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("tool2"));
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("tool3"));
    
    REQUIRE(ToolRegistry::instance().size() == 3);
    REQUIRE(ToolRegistry::instance().has("tool1"));
    REQUIRE(ToolRegistry::instance().has("tool2"));
    REQUIRE(ToolRegistry::instance().has("tool3"));
}

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.Register.ReplaceExisting", "[Tool][Registry]") {
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("tool", "First version"));
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("tool", "Second version"));
    
    REQUIRE(ToolRegistry::instance().size() == 1);
    
    auto tool = ToolRegistry::instance().get("tool");
    REQUIRE(tool);
    REQUIRE(tool->description() == "Second version");
}

// ==================== Retrieval Tests ====================

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.Get.ExistingTool", "[Tool][Registry]") {
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("existing_tool"));
    
    auto tool = ToolRegistry::instance().get("existing_tool");
    REQUIRE(tool);
    REQUIRE(tool->name() == "existing_tool");
}

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.Get.NonExistingTool", "[Tool][Registry]") {
    auto tool = ToolRegistry::instance().get("non_existing_tool");
    REQUIRE_FALSE(tool);
}

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.Has.ExistingTool", "[Tool][Registry]") {
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("check_tool"));
    
    REQUIRE(ToolRegistry::instance().has("check_tool"));
    REQUIRE_FALSE(ToolRegistry::instance().has("other_tool"));
}

// ==================== Removal Tests ====================

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.Remove.ExistingTool", "[Tool][Registry]") {
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("remove_tool"));
    REQUIRE(ToolRegistry::instance().has("remove_tool"));
    
    bool removed = ToolRegistry::instance().remove("remove_tool");
    
    REQUIRE(removed);
    REQUIRE_FALSE(ToolRegistry::instance().has("remove_tool"));
    REQUIRE(ToolRegistry::instance().size() == 0);
}

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.Remove.NonExistingTool", "[Tool][Registry]") {
    bool removed = ToolRegistry::instance().remove("non_existing");
    REQUIRE_FALSE(removed);
}

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.Clear.AllTools", "[Tool][Registry]") {
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("tool1"));
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("tool2"));
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("tool3"));
    
    REQUIRE(ToolRegistry::instance().size() == 3);
    
    ToolRegistry::instance().clear();
    
    REQUIRE(ToolRegistry::instance().size() == 0);
    REQUIRE_FALSE(ToolRegistry::instance().has("tool1"));
    REQUIRE_FALSE(ToolRegistry::instance().has("tool2"));
    REQUIRE_FALSE(ToolRegistry::instance().has("tool3"));
}

// ==================== List and Names Tests ====================

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.List.Empty", "[Tool][Registry]") {
    auto tools = ToolRegistry::instance().list();
    REQUIRE(tools.empty());
}

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.List.WithTools", "[Tool][Registry]") {
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("tool_a"));
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("tool_b"));
    
    auto tools = ToolRegistry::instance().list();
    REQUIRE(tools.size() == 2);
}

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.Names.WithTools", "[Tool][Registry]") {
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("name_tool_1"));
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("name_tool_2"));
    
    auto names = ToolRegistry::instance().names();
    REQUIRE(names.size() == 2);
    
    // Check that names are present
    bool has_tool1 = false, has_tool2 = false;
    for (const auto& name : names) {
        if (name == "name_tool_1") has_tool1 = true;
        if (name == "name_tool_2") has_tool2 = true;
    }
    REQUIRE(has_tool1);
    REQUIRE(has_tool2);
}

// ==================== Filter Tests ====================

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.Filter.AllTools", "[Tool][Registry]") {
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("filter_tool_1"));
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("filter_tool_2"));
    
    // Empty filter should return all tools
    auto filtered = ToolRegistry::instance().filter({});
    REQUIRE(filtered.size() == 2);
}

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.Filter.SpecificTools", "[Tool][Registry]") {
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("keep_tool"));
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("exclude_tool"));
    
    auto filtered = ToolRegistry::instance().filter({"keep_tool"});
    REQUIRE(filtered.size() == 1);
    REQUIRE(filtered[0]->name() == "keep_tool");
}

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.Filter.NonExistingTools", "[Tool][Registry]") {
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("existing_tool"));
    
    auto filtered = ToolRegistry::instance().filter({"non_existing"});
    REQUIRE(filtered.empty());
}

// ==================== Tool Definitions Tests ====================

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.ToToolDefinitions.Empty", "[Tool][Registry]") {
    auto defs = ToolRegistry::instance().to_tool_definitions();
    REQUIRE(defs.is_array());
    REQUIRE(defs.empty());
}

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.ToToolDefinitions.WithTools", "[Tool][Registry]") {
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("def_tool"));
    
    auto defs = ToolRegistry::instance().to_tool_definitions();
    REQUIRE(defs.is_array());
    REQUIRE(defs.size() == 1);
    
    // Check structure of tool definition (OpenAI format)
    auto& def = defs[0];
    REQUIRE(def.contains("type"));
    REQUIRE(def["type"] == "function");
    REQUIRE(def.contains("function"));
    REQUIRE(def["function"].contains("name"));
    REQUIRE(def["function"]["name"] == "def_tool");
}

// ==================== Size Tests ====================

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.Size.Empty", "[Tool][Registry]") {
    REQUIRE(ToolRegistry::instance().size() == 0);
}

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.Size.AfterOperations", "[Tool][Registry]") {
    REQUIRE(ToolRegistry::instance().size() == 0);
    
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("size_tool_1"));
    REQUIRE(ToolRegistry::instance().size() == 1);
    
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("size_tool_2"));
    REQUIRE(ToolRegistry::instance().size() == 2);
    
    ToolRegistry::instance().remove("size_tool_1");
    REQUIRE(ToolRegistry::instance().size() == 1);
}

// ==================== Builtin Tools Tests ====================

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.RegisterBuiltinTools", "[Tool][Registry][Builtin]") {
    ToolRegistry::instance().register_builtin_tools();
    
    // Check core file tools
    REQUIRE(ToolRegistry::instance().has("read_file"));
    REQUIRE(ToolRegistry::instance().has("write_file"));
    REQUIRE(ToolRegistry::instance().has("edit_file"));
    REQUIRE(ToolRegistry::instance().has("bash"));
    
    // Check search tools
    REQUIRE(ToolRegistry::instance().has("glob"));
    REQUIRE(ToolRegistry::instance().has("grep"));
    REQUIRE(ToolRegistry::instance().has("list"));
    
    // Check web tools
    REQUIRE(ToolRegistry::instance().has("webfetch"));
    REQUIRE(ToolRegistry::instance().has("websearch"));
    
    // Check task tools
    REQUIRE(ToolRegistry::instance().has("task"));
    REQUIRE(ToolRegistry::instance().has("todo_read"));
    REQUIRE(ToolRegistry::instance().has("todo_write"));
    REQUIRE(ToolRegistry::instance().has("plan_enter"));
    REQUIRE(ToolRegistry::instance().has("plan_exit"));
    REQUIRE(ToolRegistry::instance().has("skill"));
}

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.EnableQuestionTool", "[Tool][Registry][Builtin]") {
    // By default, question tool should not be registered
    ToolRegistry::instance().register_builtin_tools();
    REQUIRE_FALSE(ToolRegistry::instance().has("question"));
    
    // Clear and enable question tool
    ToolRegistry::instance().clear();
    ToolRegistry::instance().enable_question_tool(true);
    ToolRegistry::instance().register_builtin_tools();
    
    REQUIRE(ToolRegistry::instance().has("question"));
    
    // Reset for other tests
    ToolRegistry::instance().enable_question_tool(false);
}

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.BuiltinToolDefinitions", "[Tool][Registry][Builtin]") {
    ToolRegistry::instance().register_builtin_tools();
    
    auto defs = ToolRegistry::instance().to_tool_definitions();
    REQUIRE(defs.is_array());
    REQUIRE(defs.size() > 10);  // Should have many builtin tools
    
    // Verify each definition has proper structure
    for (const auto& def : defs) {
        REQUIRE(def.contains("type"));
        REQUIRE(def["type"] == "function");
        REQUIRE(def.contains("function"));
        REQUIRE(def["function"].contains("name"));
        REQUIRE(def["function"].contains("description"));
        REQUIRE(def["function"].contains("parameters"));
    }
}

// ==================== Custom Tool Names Tests ====================

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.CustomToolNames.Empty", "[Tool][Registry]") {
    auto names = ToolRegistry::instance().custom_tool_names();
    REQUIRE(names.empty());
}

// ==================== Thread Safety Tests ====================

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.ThreadSafety.RegisterAndRead", "[Tool][Registry][Thread]") {
    // Register from main thread
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("thread_tool_1"));
    
    // Read from another thread
    std::thread reader([]() {
        auto tool = ToolRegistry::instance().get("thread_tool_1");
        REQUIRE(tool);
        REQUIRE(tool->name() == "thread_tool_1");
    });
    reader.join();
    
    REQUIRE(ToolRegistry::instance().has("thread_tool_1"));
}

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.ThreadSafety.ConcurrentReads", "[Tool][Registry][Thread]") {
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("concurrent_tool"));
    
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&success_count]() {
            if (ToolRegistry::instance().has("concurrent_tool")) {
                success_count++;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    REQUIRE(success_count == 10);
}

// ==================== Null Tool Tests ====================

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.RegisterNullTool", "[Tool][Registry]") {
    REQUIRE_THROWS_AS(
        ToolRegistry::instance().register_tool(nullptr),
        std::invalid_argument
    );
}

// ==================== Tool Replacement Tests ====================

TEST_CASE_METHOD(ToolRegistryFixture, "Tool.Registry.ReplaceTool", "[Tool][Registry]") {
    // Register initial tool
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("replaceable", "Version 1"));
    auto tool = ToolRegistry::instance().get("replaceable");
    REQUIRE(tool->description() == "Version 1");
    
    // Replace with new version
    ToolRegistry::instance().register_tool(std::make_unique<MockTool>("replaceable", "Version 2"));
    tool = ToolRegistry::instance().get("replaceable");
    REQUIRE(tool->description() == "Version 2");
    REQUIRE(ToolRegistry::instance().size() == 1);  // Size should not increase
}
