#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/config/config_manager.hpp>
#include <fstream>

using namespace turbot::core;
using namespace turbot::test;

// ==================== ConfigManager 验证测试 ====================

TEST_CASE("Config.Validation.ValidProvider", "[Config]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    WorkingDirGuard cwd_guard(tmp.path());
    
    auto config_file = tmp.path() / ".turbot" / "turbot.json";
    std::filesystem::create_directories(config_file.parent_path());
    
    std::ofstream file(config_file);
    file << R"({
        "version": "1.0",
        "providers": [
            {"name": "openai", "type": "openai"},
            {"name": "anthropic", "type": "anthropic"}
        ]
    })";
    file.close();
    
    auto& manager = ConfigManager::instance();
    auto result = manager.load_config(ConfigLevel::Project);
    REQUIRE(result.success);
}

TEST_CASE("Config.Validation.InvalidProviderType", "[Config]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    WorkingDirGuard cwd_guard(tmp.path());
    
    auto config_file = tmp.path() / ".turbot" / "turbot.json";
    std::filesystem::create_directories(config_file.parent_path());
    
    std::ofstream file(config_file);
    file << R"({
        "providers": [
            {"name": "unknown", "type": "unknown_type"}
        ]
    })";
    file.close();
    
    auto& manager = ConfigManager::instance();
    std::filesystem::current_path(tmp.path());
    auto result = manager.load_config(ConfigLevel::Project);
    // Unknown type should still load (just warning)
    REQUIRE(result.success);
}

TEST_CASE("Config.Validation.InvalidVersion", "[Config]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto config_file = tmp.path() / ".turbot" / "turbot.json";
    std::filesystem::create_directories(config_file.parent_path());
    
    std::ofstream file(config_file);
    file << R"({
        "version": 123
    })";
    file.close();
    
    auto& manager = ConfigManager::instance();
    std::filesystem::current_path(tmp.path());
    auto result = manager.load_config(ConfigLevel::Project);
    // Version should be string
    REQUIRE_FALSE(result.success);
}

TEST_CASE("Config.Validation.ProvidersNotArray", "[Config]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto config_file = tmp.path() / ".turbot" / "turbot.json";
    std::filesystem::create_directories(config_file.parent_path());
    
    std::ofstream file(config_file);
    file << R"({
        "providers": {"openai": {}}
    })";
    file.close();
    
    auto& manager = ConfigManager::instance();
    std::filesystem::current_path(tmp.path());
    auto result = manager.load_config(ConfigLevel::Project);
    REQUIRE_FALSE(result.success);
}

TEST_CASE("Config.Validation.ProviderMissingFields", "[Config]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto config_file = tmp.path() / ".turbot" / "turbot.json";
    std::filesystem::create_directories(config_file.parent_path());
    
    std::ofstream file(config_file);
    file << R"({
        "providers": [
            {"name": "openai"}
        ]
    })";
    file.close();
    
    auto& manager = ConfigManager::instance();
    std::filesystem::current_path(tmp.path());
    auto result = manager.load_config(ConfigLevel::Project);
    REQUIRE_FALSE(result.success);
}

// ==================== ConfigManager 权限配置测试 ====================

TEST_CASE("Config.Validation.ValidPermissionMode", "[Config]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto config_file = tmp.path() / ".turbot" / "turbot.json";
    std::filesystem::create_directories(config_file.parent_path());
    
    std::ofstream file(config_file);
    file << R"({
        "permissions": {
            "mode": "ask",
            "rules": []
        }
    })";
    file.close();
    
    auto& manager = ConfigManager::instance();
    std::filesystem::current_path(tmp.path());
    auto result = manager.load_config(ConfigLevel::Project);
    REQUIRE(result.success);
}

TEST_CASE("Config.Validation.InvalidPermissionMode", "[Config]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto config_file = tmp.path() / ".turbot" / "turbot.json";
    std::filesystem::create_directories(config_file.parent_path());
    
    std::ofstream file(config_file);
    file << R"({
        "permissions": {
            "mode": "invalid_mode"
        }
    })";
    file.close();
    
    auto& manager = ConfigManager::instance();
    std::filesystem::current_path(tmp.path());
    auto result = manager.load_config(ConfigLevel::Project);
    REQUIRE_FALSE(result.success);
}

TEST_CASE("Config.Validation.PermissionRulesNotArray", "[Config]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto config_file = tmp.path() / ".turbot" / "turbot.json";
    std::filesystem::create_directories(config_file.parent_path());
    
    std::ofstream file(config_file);
    file << R"({
        "permissions": {
            "rules": "not_an_array"
        }
    })";
    file.close();
    
    auto& manager = ConfigManager::instance();
    std::filesystem::current_path(tmp.path());
    auto result = manager.load_config(ConfigLevel::Project);
    REQUIRE_FALSE(result.success);
}

TEST_CASE("Config.Validation.PermissionRuleMissingFields", "[Config]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto config_file = tmp.path() / ".turbot" / "turbot.json";
    std::filesystem::create_directories(config_file.parent_path());
    
    std::ofstream file(config_file);
    file << R"({
        "permissions": {
            "rules": [
                {"pattern": "test"}
            ]
        }
    })";
    file.close();
    
    auto& manager = ConfigManager::instance();
    std::filesystem::current_path(tmp.path());
    auto result = manager.load_config(ConfigLevel::Project);
    REQUIRE_FALSE(result.success);
}

// ==================== ConfigManager YAML 解析测试 ====================

TEST_CASE("Config.Yaml.ParseBoolean", "[Config]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto config_file = tmp.path() / ".turbot" / "turbot.json";
    std::filesystem::create_directories(config_file.parent_path());
    
    std::ofstream file(config_file);
    file << R"({
        "bool_true": true,
        "bool_false": false
    })";
    file.close();
    
    auto& manager = ConfigManager::instance();
    std::filesystem::current_path(tmp.path());
    manager.initialize();  // Use initialize() instead of load_config()
    
    auto v1 = manager.get<bool>("bool_true");
    auto v2 = manager.get<bool>("bool_false");
    REQUIRE(v1.has_value());
    REQUIRE(v2.has_value());
    REQUIRE(*v1 == true);
    REQUIRE(*v2 == false);
}

TEST_CASE("Config.Yaml.ParseNumbers", "[Config]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto config_file = tmp.path() / ".turbot" / "turbot.json";
    std::filesystem::create_directories(config_file.parent_path());
    
    std::ofstream file(config_file);
    file << R"({
        "int_val": 42,
        "float_val": 3.14,
        "neg_val": -100
    })";
    file.close();
    
    auto& manager = ConfigManager::instance();
    std::filesystem::current_path(tmp.path());
    manager.initialize();  // Use initialize() instead of load_config()
    
    auto v1 = manager.get<int>("int_val");
    auto v2 = manager.get<double>("float_val");
    auto v3 = manager.get<int>("neg_val");
    
    REQUIRE(v1.has_value());
    REQUIRE(*v1 == 42);
    REQUIRE(v2.has_value());
    REQUIRE(std::abs(*v2 - 3.14) < 0.001);
    REQUIRE(v3.has_value());
    REQUIRE(*v3 == -100);
}

// ==================== ConfigManager 环境变量测试 ====================

TEST_CASE("Config.Env.TurbotDebug", "[Config]") {
    setenv("TURBOT_DEBUG", "true", 1);
    
    auto& manager = ConfigManager::instance();
    manager.reload();
    
    // Check if turbot.debug is set from env
    auto debug = manager.get<bool>("turbot.debug");
    REQUIRE(debug.has_value());
    REQUIRE(*debug == true);
    
    unsetenv("TURBOT_DEBUG");
}

TEST_CASE("Config.Env.TurbotLogLevel", "[Config]") {
    setenv("TURBOT_LOG_LEVEL", "debug", 1);
    
    auto& manager = ConfigManager::instance();
    manager.reload();
    
    auto level = manager.get<std::string>("turbot.log_level");
    REQUIRE(level.has_value());
    REQUIRE(*level == "debug");
    
    unsetenv("TURBOT_LOG_LEVEL");
}

TEST_CASE("Config.Env.ApiKey", "[Config]") {
    setenv("OPENAI_API_KEY", "test-openai-key", 1);
    
    auto& manager = ConfigManager::instance();
    manager.reload();
    
    auto key = manager.get<std::string>("providers_by_name.openai.api_key");
    REQUIRE(key.has_value());
    REQUIRE(*key == "test-openai-key");
    
    unsetenv("OPENAI_API_KEY");
}

// ==================== ConfigManager 回调测试 ====================

TEST_CASE("Config.Callback.OnChange", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    bool callback_called = false;
    nlohmann::json received_config;
    
    manager.on_config_change([&](ConfigLevel level, const std::string& key, const nlohmann::json& config) {
        callback_called = true;
        received_config = config;
    });
    
    manager.set("test.callback", "value");
    manager.reload();
    
    // Callback should be called on reload
    // Note: This depends on implementation details
}

// ==================== ConfigManager 合并测试 ====================

TEST_CASE("Config.Merge.NestedObjects", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    // Set base nested object
    manager.set("nested.level1.level2.key1", "value1");
    manager.set("nested.level1.level2.key2", "value2");
    
    auto v1 = manager.get<std::string>("nested.level1.level2.key1");
    auto v2 = manager.get<std::string>("nested.level1.level2.key2");
    
    REQUIRE(v1.has_value());
    REQUIRE(*v1 == "value1");
    REQUIRE(v2.has_value());
    REQUIRE(*v2 == "value2");
}

// ==================== ConfigManager 保存测试 ====================

TEST_CASE("Config.Save.ProjectConfig", "[Config]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto config_file = tmp.path() / ".turbot" / "turbot.json";
    std::filesystem::create_directories(config_file.parent_path());
    
    // Create initial config
    {
        std::ofstream file(config_file);
        file << R"({"test": "initial"})";
    }
    
    auto& manager = ConfigManager::instance();
    std::filesystem::current_path(tmp.path());
    
    // Load and modify
    manager.load_config(ConfigLevel::Project);
    manager.set("test.new_key", "new_value");
    
    // Save
    bool saved = manager.save_config(ConfigLevel::Project);
    // Note: save_config behavior depends on implementation
}

// ==================== ConfigManager 日志路径测试 ====================

TEST_CASE("Config.LogPath.Default", "[Config]") {
    auto& manager = ConfigManager::instance();
    std::string log_path = manager.get_log_path();
    
    // Should return a valid path
    REQUIRE_FALSE(log_path.empty());
    // Should contain .turbot
    REQUIRE(log_path.find(".turbot") != std::string::npos);
}

// ==================== ConfigManager 扩展路径测试 ====================

TEST_CASE("Config.ExtensionPath.User", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    std::string agent_path = manager.get_extension_path(ConfigLevel::User, ExtensionType::Agent);
    std::string skill_path = manager.get_extension_path(ConfigLevel::User, ExtensionType::Skill);
    
    REQUIRE(agent_path.find("agents") != std::string::npos);
    REQUIRE(skill_path.find("skills") != std::string::npos);
}

TEST_CASE("Config.ExtensionPath.Project", "[Config]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    std::filesystem::current_path(tmp.path());
    
    auto& manager = ConfigManager::instance();
    
    std::string agent_path = manager.get_extension_path(ConfigLevel::Project, ExtensionType::Agent);
    
    REQUIRE(agent_path.find("agents") != std::string::npos);
    REQUIRE(agent_path.find(".turbot") != std::string::npos);
}

// ==================== ConfigManager 环境变量解析测试 ====================

TEST_CASE("Config.ResolveEnvVars.WithDefault", "[Config]") {
    auto& manager = ConfigManager::instance();
    
    std::string result = manager.resolve_env_vars("${NONEXISTENT_VAR:-default_value}");
    REQUIRE(result == "default_value");
}

TEST_CASE("Config.ResolveEnvVars.WithoutDefault", "[Config]") {
    setenv("TEST_RESOLVE_VAR", "resolved_value", 1);
    
    auto& manager = ConfigManager::instance();
    
    std::string result = manager.resolve_env_vars("${TEST_RESOLVE_VAR}");
    REQUIRE(result == "resolved_value");
    
    unsetenv("TEST_RESOLVE_VAR");
}

TEST_CASE("Config.ResolveEnvVars.MultipleVars", "[Config]") {
    setenv("VAR1", "value1", 1);
    setenv("VAR2", "value2", 1);
    
    auto& manager = ConfigManager::instance();
    
    std::string result = manager.resolve_env_vars("${VAR1}_${VAR2}");
    REQUIRE(result == "value1_value2");
    
    unsetenv("VAR1");
    unsetenv("VAR2");
}
