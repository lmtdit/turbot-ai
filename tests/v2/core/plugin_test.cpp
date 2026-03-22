/**
 * @file plugin_test.cpp
 * @brief Tests for PluginManager functionality
 */

#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/plugin/plugin.hpp>

using namespace turbot::core::plugin;
using namespace turbot::test;

// ==================== PluginManager Singleton Tests ====================

TEST_CASE("PluginManager.Instance", "[Core][Plugin]") {
    auto& instance1 = PluginManager::instance();
    auto& instance2 = PluginManager::instance();
    
    REQUIRE(&instance1 == &instance2);
}

// ==================== Register/Unregister Hook Tests ====================

TEST_CASE("PluginManager.RegisterHook", "[Core][Plugin]") {
    auto& manager = PluginManager::instance();
    manager.clear_all();
    
    bool called = false;
    auto id = manager.register_hook("test_hook", [&](const nlohmann::json& input, nlohmann::json& output) {
        called = true;
    });
    
    // ID starts from 0 and increments; >= 0 is valid
    REQUIRE(id >= 0);
    REQUIRE(manager.handler_count("test_hook") == 1);
    
    manager.clear_all();
}

TEST_CASE("PluginManager.RegisterMultipleHooks", "[Core][Plugin]") {
    auto& manager = PluginManager::instance();
    manager.clear_all();
    
    auto id1 = manager.register_hook("hook1", [](const nlohmann::json&, nlohmann::json&) {});
    auto id2 = manager.register_hook("hook2", [](const nlohmann::json&, nlohmann::json&) {});
    auto id3 = manager.register_hook("hook1", [](const nlohmann::json&, nlohmann::json&) {});  // Same hook name
    
    REQUIRE(id1 != id2);
    REQUIRE(id2 != id3);
    REQUIRE(manager.handler_count("hook1") == 2);
    REQUIRE(manager.handler_count("hook2") == 1);
    
    manager.clear_all();
}

TEST_CASE("PluginManager.UnregisterHook", "[Core][Plugin]") {
    auto& manager = PluginManager::instance();
    manager.clear_all();
    
    auto id = manager.register_hook("test_hook", [](const nlohmann::json&, nlohmann::json&) {});
    REQUIRE(manager.handler_count("test_hook") == 1);
    
    bool result = manager.unregister_hook("test_hook", id);
    REQUIRE(result);
    REQUIRE(manager.handler_count("test_hook") == 0);
    
    manager.clear_all();
}

TEST_CASE("PluginManager.UnregisterHook.NonExistent", "[Core][Plugin]") {
    auto& manager = PluginManager::instance();
    manager.clear_all();
    
    bool result = manager.unregister_hook("nonexistent_hook", 999);
    REQUIRE_FALSE(result);
}

TEST_CASE("PluginManager.UnregisterHook.WrongId", "[Core][Plugin]") {
    auto& manager = PluginManager::instance();
    manager.clear_all();
    
    manager.register_hook("test_hook", [](const nlohmann::json&, nlohmann::json&) {});
    
    bool result = manager.unregister_hook("test_hook", 99999);
    REQUIRE_FALSE(result);
    REQUIRE(manager.handler_count("test_hook") == 1);
    
    manager.clear_all();
}

// ==================== Trigger Tests ====================

TEST_CASE("PluginManager.Trigger", "[Core][Plugin]") {
    auto& manager = PluginManager::instance();
    manager.clear_all();
    
    std::string received_input;
    manager.register_hook("test_hook", [&](const nlohmann::json& input, nlohmann::json& output) {
        received_input = input.value("message", "");
        output["result"] = "processed";
    });
    
    nlohmann::json input = {{"message", "hello"}};
    nlohmann::json output;
    
    manager.trigger("test_hook", input, output);
    
    REQUIRE(received_input == "hello");
    REQUIRE(output["result"] == "processed");
    
    manager.clear_all();
}

TEST_CASE("PluginManager.Trigger.MultipleHandlers", "[Core][Plugin]") {
    auto& manager = PluginManager::instance();
    manager.clear_all();
    
    int call_count = 0;
    
    manager.register_hook("multi_hook", [&](const nlohmann::json&, nlohmann::json& output) {
        call_count++;
        output["count"] = call_count;
    });
    
    manager.register_hook("multi_hook", [&](const nlohmann::json&, nlohmann::json& output) {
        call_count++;
        output["count"] = call_count;
    });
    
    nlohmann::json input;
    nlohmann::json output;
    
    manager.trigger("multi_hook", input, output);
    
    REQUIRE(call_count == 2);
    
    manager.clear_all();
}

TEST_CASE("PluginManager.Trigger.NonExistentHook", "[Core][Plugin]") {
    auto& manager = PluginManager::instance();
    manager.clear_all();
    
    nlohmann::json input = {{"test", "value"}};
    nlohmann::json output;
    
    // Should not throw
    REQUIRE_NOTHROW(manager.trigger("nonexistent_hook", input, output));
}

TEST_CASE("PluginManager.Trigger.EmptyHandler", "[Core][Plugin]") {
    auto& manager = PluginManager::instance();
    manager.clear_all();
    
    // Register an empty handler (default-constructed)
    HookHandler empty_handler;
    manager.register_hook("empty_hook", empty_handler);
    
    nlohmann::json input;
    nlohmann::json output;
    
    // Should not throw due to empty handler check
    REQUIRE_NOTHROW(manager.trigger("empty_hook", input, output));
    
    manager.clear_all();
}

// ==================== Clear Tests ====================

TEST_CASE("PluginManager.Clear", "[Core][Plugin]") {
    auto& manager = PluginManager::instance();
    manager.clear_all();
    
    manager.register_hook("hook1", [](const nlohmann::json&, nlohmann::json&) {});
    manager.register_hook("hook2", [](const nlohmann::json&, nlohmann::json&) {});
    
    manager.clear("hook1");
    
    REQUIRE(manager.handler_count("hook1") == 0);
    REQUIRE(manager.handler_count("hook2") == 1);
    
    manager.clear_all();
}

TEST_CASE("PluginManager.ClearAll", "[Core][Plugin]") {
    auto& manager = PluginManager::instance();
    
    manager.register_hook("hook1", [](const nlohmann::json&, nlohmann::json&) {});
    manager.register_hook("hook2", [](const nlohmann::json&, nlohmann::json&) {});
    manager.register_hook("hook3", [](const nlohmann::json&, nlohmann::json&) {});
    
    manager.clear_all();
    
    REQUIRE(manager.handler_count("hook1") == 0);
    REQUIRE(manager.handler_count("hook2") == 0);
    REQUIRE(manager.handler_count("hook3") == 0);
}

// ==================== Handler Count Tests ====================

TEST_CASE("PluginManager.HandlerCount", "[Core][Plugin]") {
    auto& manager = PluginManager::instance();
    manager.clear_all();
    
    REQUIRE(manager.handler_count("nonexistent") == 0);
    
    manager.register_hook("test", [](const nlohmann::json&, nlohmann::json&) {});
    REQUIRE(manager.handler_count("test") == 1);
    
    manager.register_hook("test", [](const nlohmann::json&, nlohmann::json&) {});
    REQUIRE(manager.handler_count("test") == 2);
    
    manager.clear_all();
}
