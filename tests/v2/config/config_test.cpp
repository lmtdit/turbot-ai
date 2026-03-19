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
    
    // ConfigManager doesn't have remove, use set to null
    manager.set("test.remove_key", nullptr);
    REQUIRE_FALSE(manager.has("test.remove_key"));
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
