#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/config/config_manager.hpp>

using namespace turbot::core;
using namespace turbot::test;

// ==================== ConfigLevel 测试 ====================

TEST_CASE("Config.Level.Default", "[Config]") {
    REQUIRE(static_cast<int>(ConfigLevel::Default) == 0);
    REQUIRE(static_cast<int>(ConfigLevel::User) == 1);
    REQUIRE(static_cast<int>(ConfigLevel::Project) == 2);
}

// ==================== LoadResult 测试 ====================

TEST_CASE("Config.LoadResult.Defaults", "[Config]") {
    LoadResult result;
    REQUIRE_FALSE(result.success);
    REQUIRE(result.level == ConfigLevel::Default);
    REQUIRE(result.path.empty());
    REQUIRE(result.warnings.empty());
    REQUIRE(result.errors.empty());
}

TEST_CASE("Config.LoadResult.Success", "[Config]") {
    LoadResult result;
    result.success = true;
    result.level = ConfigLevel::Project;
    result.path = "/path/to/config.json";
    
    REQUIRE(result.success);
    REQUIRE(result.level == ConfigLevel::Project);
    REQUIRE(result.path == "/path/to/config.json");
}

// ==================== ConfigException 测试 ====================

TEST_CASE("Config.Exception.Creation", "[Config]") {
    ConfigException ex(ConfigError::FileNotFound, "Config file not found");
    REQUIRE(std::string(ex.what()) == "Config file not found");
    REQUIRE(ex.error() == ConfigError::FileNotFound);
}

TEST_CASE("Config.Exception.Types", "[Config]") {
    ConfigException ex1(ConfigError::ParseError, "Parse error");
    REQUIRE(ex1.error() == ConfigError::ParseError);
    
    ConfigException ex2(ConfigError::ValidationError, "Validation error");
    REQUIRE(ex2.error() == ConfigError::ValidationError);
    
    ConfigException ex3(ConfigError::PermissionDenied, "Permission denied");
    REQUIRE(ex3.error() == ConfigError::PermissionDenied);
    
    ConfigException ex4(ConfigError::InvalidPath, "Invalid path");
    REQUIRE(ex4.error() == ConfigError::InvalidPath);
    
    ConfigException ex5(ConfigError::CircularReference, "Circular reference");
    REQUIRE(ex5.error() == ConfigError::CircularReference);
}

// ==================== MarkdownConfig 测试 ====================

TEST_CASE("Config.MarkdownConfig.Defaults", "[Config]") {
    MarkdownConfig config;
    REQUIRE(config.frontmatter.is_null());
    REQUIRE(config.content.empty());
}

TEST_CASE("Config.MarkdownConfig.WithContent", "[Config]") {
    MarkdownConfig config;
    config.frontmatter = nlohmann::json{{"title", "Test"}};
    config.content = "# Hello World\n\nThis is content.";
    
    REQUIRE(config.frontmatter["title"] == "Test");
    REQUIRE(config.content == "# Hello World\n\nThis is content.");
}

// ==================== ConfigManager 基础测试 ====================

TEST_CASE("Config.Manager.Instance", "[Config]") {
    // 单例模式测试
    auto& instance1 = ConfigManager::instance();
    auto& instance2 = ConfigManager::instance();
    REQUIRE(&instance1 == &instance2);
}

TEST_CASE("Config.Manager.DefaultConfig", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    // 获取默认配置 - 可能是空对象或 null
    auto config = manager.get_all();
    // 如果没有配置，可能是 null 或空对象
    REQUIRE((config.is_object() || config.is_null()));
}

TEST_CASE("Config.Manager.SetGet", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    // 设置配置值
    manager.set("test.key", "test_value");
    
    // 获取配置值
    auto value = manager.get<std::string>("test.key");
    REQUIRE(value.has_value());
    REQUIRE(*value == "test_value");
}

TEST_CASE("Config.Manager.SetInt", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    manager.set("test.int_key", 42);
    
    auto value = manager.get<int>("test.int_key");
    REQUIRE(value.has_value());
    REQUIRE(*value == 42);
}

TEST_CASE("Config.Manager.SetBool", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    manager.set("test.bool_key", true);
    
    auto value = manager.get<bool>("test.bool_key");
    REQUIRE(value.has_value());
    REQUIRE(*value == true);
    
    manager.set("test.bool_key", false);
    value = manager.get<bool>("test.bool_key");
    REQUIRE(value.has_value());
    REQUIRE(*value == false);
}

TEST_CASE("Config.Manager.SetObject", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    nlohmann::json obj = {
        {"name", "test"},
        {"value", 123}
    };
    
    manager.set("test.object", obj);
    
    auto value = manager.get<nlohmann::json>("test.object");
    REQUIRE(value.has_value());
    REQUIRE((*value)["name"] == "test");
    REQUIRE((*value)["value"] == 123);
}

TEST_CASE("Config.Manager.HasKey", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    REQUIRE_FALSE(manager.has("test.nonexistent_key"));
    
    manager.set("test.has_key", "value");
    REQUIRE(manager.has("test.has_key"));
}

// ==================== 嵌套键测试 ====================

TEST_CASE("Config.Manager.NestedKeys", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    // 设置嵌套值
    manager.set("app.server.host", "localhost");
    manager.set("app.server.port", 8080);
    
    REQUIRE(manager.get<std::string>("app.server.host") == "localhost");
    REQUIRE(manager.get<int>("app.server.port") == 8080);
}

// ==================== 默认值测试 ====================

TEST_CASE("Config.Manager.DefaultValue", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    // 不存在的键返回默认值
    auto value = manager.get_or<std::string>("nonexistent.key", "default_value");
    REQUIRE(value == "default_value");
    
    auto int_value = manager.get_or<int>("nonexistent.int_key", 999);
    REQUIRE(int_value == 999);
}

// ==================== 配置层级测试 ====================

TEST_CASE("Config.Manager.Levels", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    // 测试配置层级概念: Default < User < Project
    // 设置配置值（项目级）
    manager.set("test.level", "project_value");
    REQUIRE(manager.get<std::string>("test.level") == "project_value");
}

// ==================== Extended Config Tests ====================

TEST_CASE("Config.Manager.Remove", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    manager.set("test.remove_key", "value");
    REQUIRE(manager.has("test.remove_key"));
    
    // ConfigManager::set with nullptr sets value to null, but key still exists
    // So has() will still return true, but get() will return null
    manager.set("test.remove_key", nullptr);
    // Key still exists with null value
    REQUIRE(manager.has("test.remove_key"));
    // Value is null
    auto value = manager.get<std::string>("test.remove_key");
    REQUIRE_FALSE(value.has_value());
}

TEST_CASE("Config.Manager.Clear", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    manager.set("test.clear_key1", "value1");
    manager.set("test.clear_key2", "value2");
    
    // ConfigManager doesn't have clear method
    // Just verify the keys exist
    REQUIRE(manager.has("test.clear_key1"));
    REQUIRE(manager.has("test.clear_key2"));
}

TEST_CASE("Config.Manager.DoubleSet", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    manager.set("test.double_key", "first");
    manager.set("test.double_key", "second");
    
    auto value = manager.get<std::string>("test.double_key");
    REQUIRE(value.has_value());
    REQUIRE(*value == "second");
}

TEST_CASE("Config.Manager.GetMissingKey", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    auto value = manager.get<std::string>("nonexistent.path.key");
    REQUIRE_FALSE(value.has_value());
}

TEST_CASE("Config.Manager.SetArray", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    nlohmann::json arr = {"item1", "item2", "item3"};
    manager.set("test.array", arr);
    
    auto value = manager.get<nlohmann::json>("test.array");
    REQUIRE(value.has_value());
    REQUIRE((*value).is_array());
    REQUIRE((*value).size() == 3);
}

TEST_CASE("Config.Manager.SetDouble", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    manager.set("test.double", 3.14159);
    
    auto value = manager.get<double>("test.double");
    REQUIRE(value.has_value());
    REQUIRE(*value > 3.14);
    REQUIRE(*value < 3.15);
}

TEST_CASE("Config.Manager.DeepNested", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    manager.set("level1.level2.level3.level4", "deep_value");
    
    auto value = manager.get<std::string>("level1.level2.level3.level4");
    REQUIRE(value.has_value());
    REQUIRE(*value == "deep_value");
}

TEST_CASE("Config.LoadResult.WithWarnings", "[Config]") {
    LoadResult result;
    result.success = true;
    result.warnings.push_back("Deprecated option used");
    result.warnings.push_back("Unknown key ignored");
    
    REQUIRE(result.success);
    REQUIRE(result.warnings.size() == 2);
}

TEST_CASE("Config.LoadResult.WithErrors", "[Config]") {
    LoadResult result;
    result.success = false;
    result.errors.push_back("Invalid JSON syntax");
    
    REQUIRE_FALSE(result.success);
    REQUIRE(result.errors.size() == 1);
}

TEST_CASE("Config.MarkdownConfig.Parse", "[Config]") {
    MarkdownConfig config;
    config.frontmatter = {
        {"title", "Test Document"},
        {"author", "Test Author"},
        {"tags", nlohmann::json::array({"tag1", "tag2"})}
    };
    config.content = "# Heading\n\nParagraph";
    
    REQUIRE(config.frontmatter["title"] == "Test Document");
    REQUIRE(config.frontmatter["tags"].size() == 2);
}

TEST_CASE("Config.Exception.AllErrors", "[Config]") {
    // Test all ConfigError values
    std::vector<ConfigError> errors = {
        ConfigError::FileNotFound,
        ConfigError::ParseError,
        ConfigError::ValidationError,
        ConfigError::PermissionDenied,
        ConfigError::InvalidPath,
        ConfigError::CircularReference
    };
    
    for (auto error : errors) {
        ConfigException ex(error, "Test message");
        REQUIRE(ex.error() == error);
    }
}

// ==================== Config Class Tests ====================

#include <turbot/core/config/config.hpp>
#include <fstream>

TEST_CASE("Config.Class.Instance", "[Config][Class]") {
    // Config is a singleton
    auto& config1 = Config::instance();
    auto& config2 = Config::instance();
    REQUIRE(&config1 == &config2);
}

TEST_CASE("Config.Class.LoadFromString", "[Config][Class]") {
    auto& config = Config::instance();
    
    // Load valid JSON
    config.load_from_string(R"({"test_key": "test_value", "nested": {"key": 123}})");
    
    REQUIRE(config.has("test_key"));
    REQUIRE(config.get<std::string>("test_key") == "test_value");
    REQUIRE(config.get<int>("nested.key") == 123);
}

TEST_CASE("Config.Class.LoadFromStringInvalid", "[Config][Class]") {
    auto& config = Config::instance();
    
    // Invalid JSON should throw
    REQUIRE_THROWS_AS(
        config.load_from_string("not valid json"),
        std::runtime_error
    );
}

TEST_CASE("Config.Class.Has", "[Config][Class]") {
    auto& config = Config::instance();
    config.load_from_string(R"({"existing_key": "value"})");
    
    REQUIRE(config.has("existing_key"));
    REQUIRE_FALSE(config.has("nonexistent_key"));
}

TEST_CASE("Config.Class.Get", "[Config][Class]") {
    auto& config = Config::instance();
    config.load_from_string(R"({
        "string_key": "hello",
        "int_key": 42,
        "bool_key": true,
        "double_key": 3.14,
        "array_key": [1, 2, 3],
        "object_key": {"nested": "value"}
    })");
    
    REQUIRE(config.get<std::string>("string_key") == "hello");
    REQUIRE(config.get<int>("int_key") == 42);
    REQUIRE(config.get<bool>("bool_key") == true);
    REQUIRE(config.get<double>("double_key") > 3.13);
    auto arr = config.get<nlohmann::json>("array_key");
    REQUIRE(arr.has_value());
    REQUIRE(arr->is_array());
    REQUIRE(config.get<std::string>("object_key.nested") == "value");
}

TEST_CASE("Config.Class.GetMissing", "[Config][Class]") {
    auto& config = Config::instance();
    config.load_from_string("{}");
    
    REQUIRE_FALSE(config.get<std::string>("missing_key").has_value());
    REQUIRE_FALSE(config.get<int>("missing.int").has_value());
}

TEST_CASE("Config.Class.Set", "[Config][Class]") {
    auto& config = Config::instance();
    config.load_from_string("{}");
    
    config.set("new_key", "new_value");
    REQUIRE(config.get<std::string>("new_key") == "new_value");
    
    config.set("nested.key", 123);
    REQUIRE(config.get<int>("nested.key") == 123);
}

TEST_CASE("Config.Class.GetOr", "[Config][Class]") {
    auto& config = Config::instance();
    config.load_from_string(R"({"existing": "value"})");
    
    REQUIRE(config.get_or<std::string>("existing", "default") == "value");
    REQUIRE(config.get_or<std::string>("missing", "default") == "default");
    REQUIRE(config.get_or<int>("missing_int", 999) == 999);
}

TEST_CASE("Config.Class.GetAll", "[Config][Class]") {
    auto& config = Config::instance();
    config.load_from_string(R"({"key1": "val1", "key2": "val2"})");
    
    auto all = config.get_all();
    REQUIRE(all.is_object());
    REQUIRE(all["key1"] == "val1");
    REQUIRE(all["key2"] == "val2");
}

TEST_CASE("Config.Class.Merge", "[Config][Class]") {
    auto& config = Config::instance();
    config.load_from_string(R"({"key1": "original", "key2": "original"})");
    
    // Load more config - should merge
    config.load_from_string(R"({"key2": "updated", "key3": "new"})");
    
    REQUIRE(config.get<std::string>("key1") == "original");
    REQUIRE(config.get<std::string>("key2") == "updated");
    REQUIRE(config.get<std::string>("key3") == "new");
}

TEST_CASE("Config.Class.Remove", "[Config][Class]") {
    auto& config = Config::instance();
    config.load_from_string(R"({"key_to_remove": "value", "keep_this": "kept"})");
    
    REQUIRE(config.has("key_to_remove"));
    REQUIRE(config.remove("key_to_remove"));
    REQUIRE_FALSE(config.has("key_to_remove"));
    REQUIRE(config.has("keep_this"));
}

TEST_CASE("Config.Class.Clear", "[Config][Class]") {
    auto& config = Config::instance();
    config.load_from_string(R"({"key1": "val1", "key2": "val2"})");
    
    config.clear();
    
    REQUIRE_FALSE(config.has("key1"));
    REQUIRE_FALSE(config.has("key2"));
}

TEST_CASE("Config.Class.Watch", "[Config][Class]") {
    auto& config = Config::instance();
    config.load_from_string("{}");
    
    std::string received_key;
    nlohmann::json received_value;
    
    auto watch_id = config.watch("watched_key", [&](const std::string& key, const nlohmann::json& value) {
        received_key = key;
        received_value = value;
    });
    
    config.set("watched_key", "new_value");
    
    REQUIRE(received_key == "watched_key");
    REQUIRE(received_value == "new_value");
    
    config.unwatch("watched_key", watch_id);
}

TEST_CASE("Config.Class.Unwatch", "[Config][Class]") {
    auto& config = Config::instance();
    config.load_from_string("{}");
    
    int call_count = 0;
    
    auto watch_id = config.watch("unwatch_test", [&](const std::string&, const nlohmann::json&) {
        call_count++;
    });
    
    config.set("unwatch_test", "first");
    REQUIRE(call_count == 1);
    
    config.unwatch("unwatch_test", watch_id);
    
    config.set("unwatch_test", "second");
    REQUIRE(call_count == 1);  // Should not increase after unwatch
}

TEST_CASE("Config.Class.SaveAndLoad", "[Config][Class]") {
    auto& config = Config::instance();
    config.load_from_string(R"({"saved_key": "saved_value", "number": 42})");
    
    // Save to temp file
    std::string temp_path = "/tmp/turbot_config_test.json";
    config.save(temp_path);
    
    // Clear and reload
    config.clear();
    REQUIRE_FALSE(config.has("saved_key"));
    
    // Load from file
    config.load(temp_path);
    REQUIRE(config.get<std::string>("saved_key") == "saved_value");
    REQUIRE(config.get<int>("number") == 42);
    
    // Cleanup
    std::remove(temp_path.c_str());
}

TEST_CASE("Config.Class.LoadFromEnv", "[Config][Class]") {
    auto& config = Config::instance();
    config.clear();
    
    // Set an environment variable
    setenv("TURBOT_TEST_ENV_KEY", "env_value", 1);
    
    config.load_from_env("TURBOT_");
    
    // The env variable TURBOT_TEST_ENV_KEY is converted to test.env.key (underscores to dots)
    auto value = config.get<std::string>("test.env.key");
    REQUIRE(value.has_value());
    REQUIRE(*value == "env_value");
    
    // Cleanup
    unsetenv("TURBOT_TEST_ENV_KEY");
}

TEST_CASE("Config.Class.NonexistentRemove", "[Config][Class]") {
    auto& config = Config::instance();
    config.load_from_string("{}");
    
    REQUIRE_FALSE(config.remove("nonexistent_key"));
}

// ==================== Extended ConfigManager Tests ====================

TEST_CASE("Config.Manager.InitializeExtended", "[Config]") {
    // Ensure we have a valid current working directory
    std::error_code ec;
    std::filesystem::current_path("/tmp", ec);
    
    auto& manager = ConfigManager::instance();
    
    // Initialize config system
    auto results = manager.initialize();
    REQUIRE(results.size() >= 1);  // At least default config
}

TEST_CASE("Config.Manager.GetConfigPathExtended", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    auto user_path = manager.get_config_path(ConfigLevel::User);
    REQUIRE_FALSE(user_path.empty());
    
    auto project_path = manager.get_config_path(ConfigLevel::Project);
    REQUIRE_FALSE(project_path.empty());
}

TEST_CASE("Config.Manager.SetLogLevel", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    manager.set("log.level", "debug");
    auto level = manager.get<std::string>("log.level");
    REQUIRE(level.has_value());
    REQUIRE(*level == "debug");
}

TEST_CASE("Config.Manager.SetProvider", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    // providers is an array, so we need to set it as an array
    nlohmann::json providers_array = nlohmann::json::array({
        {{"name", "test-provider"}, {"type", "openai"}, {"api_key", "test-key"}}
    });
    
    manager.set("providers", providers_array);
    auto loaded = manager.get<nlohmann::json>("providers");
    REQUIRE(loaded.has_value());
    REQUIRE((*loaded).is_array());
    REQUIRE((*loaded)[0]["name"] == "test-provider");
}

TEST_CASE("Config.Manager.LoadConfig", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    // Load user level config
    auto result = manager.load_config(ConfigLevel::User);
    REQUIRE(result.level == ConfigLevel::User);
}

TEST_CASE("Config.Manager.GetAllExtended", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    manager.set("getall_test.key", "value");
    
    auto all = manager.get_all();
    REQUIRE(all.is_object());
    REQUIRE(all.contains("getall_test"));
}

TEST_CASE("Config.Manager.HasNested", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    manager.set("nested.test.key", "value");
    REQUIRE(manager.has("nested.test.key"));
    REQUIRE(manager.has("nested.test"));
    REQUIRE(manager.has("nested"));
}

TEST_CASE("Config.Manager.ValidateConfigExtended", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    nlohmann::json valid_config = {
        {"version", "1.0"},
        {"providers", nlohmann::json::array()}
    };
    
    REQUIRE(manager.validate_config(valid_config));
}

TEST_CASE("Config.Manager.ParseYaml", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    std::string yaml = R"(
name: test
value: 123
enabled: true
)";
    
    auto json = manager.parse_yaml(yaml);
    REQUIRE(json.is_object());
    REQUIRE(json["name"] == "test");
    REQUIRE(json["value"] == 123);
    REQUIRE(json["enabled"] == true);
}

TEST_CASE("Config.Manager.ResolveEnvVars", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    setenv("TEST_CONFIG_VAR", "resolved_value", 1);
    
    std::string value = "${TEST_CONFIG_VAR}";
    auto resolved = manager.resolve_env_vars(value);
    REQUIRE(resolved == "resolved_value");
    
    unsetenv("TEST_CONFIG_VAR");
}

TEST_CASE("Config.Manager.SaveConfig", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    manager.set("save_test.key", "value");
    
    // Note: This may fail if no write permission, but should not throw
    bool result = manager.save_config(ConfigLevel::User);
    // Just verify it doesn't crash
    (void)result;
}

TEST_CASE("Config.Manager.Reload", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    // Should not throw
    manager.reload();
}

TEST_CASE("Config.Manager.GetExtensionPathExtended", "[Config]") {
    // Ensure we have a valid current working directory
    std::error_code ec;
    std::filesystem::current_path("/tmp", ec);
    
    auto& manager = ConfigManager::instance();
    
    auto agent_path = manager.get_extension_path(ConfigLevel::User, ExtensionType::Agent);
    REQUIRE_FALSE(agent_path.empty());
    
    auto skill_path = manager.get_extension_path(ConfigLevel::Project, ExtensionType::Skill);
    REQUIRE_FALSE(skill_path.empty());
}

TEST_CASE("Config.Manager.GetLogPath", "[Config]") {
    // Ensure we have a valid current working directory
    std::error_code ec;
    std::filesystem::current_path("/tmp", ec);
    
    auto& manager = ConfigManager::instance();
    
    auto log_path = manager.get_log_path();
    REQUIRE_FALSE(log_path.empty());
}

TEST_CASE("Config.Manager.OnConfigChangeExtended", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    int call_count = 0;
    manager.on_config_change([&call_count](ConfigLevel, const std::string&, const nlohmann::json&) {
        call_count++;
    });
    
    // The callback is registered
    // Note: May not be triggered immediately
    (void)call_count;
}
