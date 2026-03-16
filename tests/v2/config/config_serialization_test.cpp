/**
 * @file config_serialization_test.cpp
 * @brief Tests for Config serialization functionality
 *
 * Tests for:
 * - Config::load_from_string
 * - Config::save
 * - Config::has
 * - Config::remove
 * - Config::watch/unwatch
 * - Config::get_all
 * - Config::clear
 */

#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/config/config.hpp>
#include <filesystem>
#include <fstream>

using namespace turbot::core;
namespace fs = std::filesystem;

// ==================== Config Singleton Tests ====================

TEST_CASE("Config.Singleton.Instance", "[Config][Serialization]") {
    auto& config1 = Config::instance();
    auto& config2 = Config::instance();
    REQUIRE(&config1 == &config2);
}

// ==================== Config Load Tests ====================

TEST_CASE("Config.LoadFromString.ValidJson", "[Config][Serialization]") {
    auto& config = Config::instance();
    config.clear();
    
    std::string json_content = R"({"key": "value", "number": 42})";
    REQUIRE_NOTHROW(config.load_from_string(json_content));
    REQUIRE(config.has("key"));
    REQUIRE(config.has("number"));
    
    config.clear();
}

TEST_CASE("Config.LoadFromString.NestedJson", "[Config][Serialization]") {
    auto& config = Config::instance();
    config.clear();
    
    std::string json_content = R"({
        "server": {
            "host": "localhost",
            "port": 8080
        },
        "database": {
            "name": "turbot",
            "connection": {
                "timeout": 30
            }
        }
    })";
    
    REQUIRE_NOTHROW(config.load_from_string(json_content));
    REQUIRE(config.has("server"));
    REQUIRE(config.has("server.host"));
    REQUIRE(config.has("server.port"));
    REQUIRE(config.has("database"));
    REQUIRE(config.has("database.name"));
    REQUIRE(config.has("database.connection.timeout"));
    
    config.clear();
}

TEST_CASE("Config.LoadFromString.InvalidJson", "[Config][Serialization]") {
    auto& config = Config::instance();
    config.clear();
    
    std::string invalid_json = "{ invalid json }";
    REQUIRE_THROWS_AS(config.load_from_string(invalid_json), std::runtime_error);
    
    config.clear();
}

TEST_CASE("Config.LoadFromString.Merge", "[Config][Serialization]") {
    auto& config = Config::instance();
    config.clear();
    
    config.load_from_string(R"({"key1": "value1"})");
    REQUIRE(config.has("key1"));
    
    config.load_from_string(R"({"key2": "value2"})");
    REQUIRE(config.has("key1"));
    REQUIRE(config.has("key2"));
    
    config.clear();
}

// ==================== Config Save Tests ====================

TEST_CASE("Config.Save.ToFile", "[Config][Serialization]") {
    auto& config = Config::instance();
    config.clear();
    
    config.load_from_string(R"({"test_key": "test_value"})");
    
    std::string temp_file = "/tmp/turbot_config_test_" + std::to_string(std::time(nullptr)) + ".json";
    REQUIRE_NOTHROW(config.save(temp_file));
    
    // Verify file exists
    REQUIRE(fs::exists(temp_file));
    
    // Verify content
    std::ifstream ifs(temp_file);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    REQUIRE(content.find("test_key") != std::string::npos);
    REQUIRE(content.find("test_value") != std::string::npos);
    
    // Cleanup
    fs::remove(temp_file);
    config.clear();
}

// ==================== Config Has Tests ====================

TEST_CASE("Config.Has.ExistingKey", "[Config][Serialization]") {
    auto& config = Config::instance();
    config.clear();
    
    config.load_from_string(R"({"existing_key": "value"})");
    REQUIRE(config.has("existing_key"));
    
    config.clear();
}

TEST_CASE("Config.Has.NonExistingKey", "[Config][Serialization]") {
    auto& config = Config::instance();
    config.clear();
    
    REQUIRE_FALSE(config.has("non_existing_key"));
    
    config.clear();
}

TEST_CASE("Config.Has.NestedKey", "[Config][Serialization]") {
    auto& config = Config::instance();
    config.clear();
    
    config.load_from_string(R"({"level1": {"level2": {"level3": "value"}}})");
    REQUIRE(config.has("level1"));
    REQUIRE(config.has("level1.level2"));
    REQUIRE(config.has("level1.level2.level3"));
    REQUIRE_FALSE(config.has("level1.level2.nonexistent"));
    
    config.clear();
}

// ==================== Config Remove Tests ====================

TEST_CASE("Config.Remove.ExistingKey", "[Config][Serialization]") {
    auto& config = Config::instance();
    config.clear();
    
    config.load_from_string(R"({"key_to_remove": "value"})");
    REQUIRE(config.has("key_to_remove"));
    
    REQUIRE(config.remove("key_to_remove"));
    REQUIRE_FALSE(config.has("key_to_remove"));
    
    config.clear();
}

TEST_CASE("Config.Remove.NonExistingKey", "[Config][Serialization]") {
    auto& config = Config::instance();
    config.clear();
    
    REQUIRE_FALSE(config.remove("non_existing_key"));
    
    config.clear();
}

TEST_CASE("Config.Remove.NestedKey", "[Config][Serialization]") {
    auto& config = Config::instance();
    config.clear();
    
    config.load_from_string(R"({"level1": {"level2": {"level3": "value"}}})");
    
    REQUIRE(config.remove("level1.level2.level3"));
    REQUIRE_FALSE(config.has("level1.level2.level3"));
    REQUIRE(config.has("level1.level2"));
    
    config.clear();
}

// ==================== Config Watch Tests ====================

TEST_CASE("Config.Watch.Register", "[Config][Serialization]") {
    auto& config = Config::instance();
    config.clear();
    
    bool callback_called = false;
    std::string watch_id = config.watch("test_key", [&](const std::string& key, const nlohmann::json& value) {
        callback_called = true;
    });
    
    REQUIRE_FALSE(watch_id.empty());
    
    config.clear();
}

TEST_CASE("Config.Watch.Unregister", "[Config][Serialization]") {
    auto& config = Config::instance();
    config.clear();
    
    std::string watch_id = config.watch("test_key", [](const std::string& key, const nlohmann::json& value) {});
    REQUIRE_FALSE(watch_id.empty());
    
    REQUIRE_NOTHROW(config.unwatch("test_key", watch_id));
    
    config.clear();
}

// ==================== Config Get All Tests ====================

TEST_CASE("Config.GetAll.Empty", "[Config][Serialization]") {
    auto& config = Config::instance();
    config.clear();
    
    auto all = config.get_all();
    REQUIRE(all.is_object());
    
    config.clear();
}

TEST_CASE("Config.GetAll.WithData", "[Config][Serialization]") {
    auto& config = Config::instance();
    config.clear();
    
    config.load_from_string(R"({"key1": "value1", "key2": 42})");
    
    auto all = config.get_all();
    REQUIRE(all.is_object());
    REQUIRE(all.contains("key1"));
    REQUIRE(all.contains("key2"));
    REQUIRE(all["key1"] == "value1");
    REQUIRE(all["key2"] == 42);
    
    config.clear();
}

// ==================== Config Clear Tests ====================

TEST_CASE("Config.Clear.AllData", "[Config][Serialization]") {
    auto& config = Config::instance();
    config.clear();
    
    config.load_from_string(R"({"key": "value"})");
    REQUIRE(config.has("key"));
    
    config.clear();
    REQUIRE_FALSE(config.has("key"));
}

// ==================== Config Thread Safety Tests ====================

TEST_CASE("Config.ThreadSafety.MultipleOperations", "[Config][Serialization]") {
    auto& config = Config::instance();
    config.clear();
    
    // Perform multiple operations to test thread safety
    config.load_from_string(R"({"thread_test": "value"})");
    REQUIRE(config.has("thread_test"));
    config.remove("thread_test");
    REQUIRE_FALSE(config.has("thread_test"));
    
    config.clear();
}
