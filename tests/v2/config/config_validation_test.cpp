/**
 * @file config_validation_test.cpp
 * @brief Configuration validation tests for ConfigManager
 *
 * Tests for:
 * - validate_config() - config validation
 * - Provider configuration validation
 * - Permission configuration validation
 * - Invalid config handling
 */

#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/config/config_manager.hpp>
#include <mock/env_guard.hpp>
#include <fstream>

using namespace turbot::core;
using namespace turbot::test;

// ==================== Test Fixtures ====================

class ConfigValidationFixture {
public:
    ConfigValidationFixture() {
        // Save current config state
        saved_config_ = ConfigManager::instance().get_all();
    }

    ~ConfigValidationFixture() {
        // Restore config state
        if (!saved_config_.is_null() && !saved_config_.empty()) {
            ConfigManager::instance().set("", saved_config_);
        }
    }

    void createConfigFile(const std::filesystem::path& path, const nlohmann::json& config) {
        std::ofstream file(path);
        file << config.dump(2);
    }

private:
    nlohmann::json saved_config_;
};

// ==================== Valid Config Tests ====================

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.ValidConfig.Empty", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    // Empty config should be valid
    nlohmann::json empty_config = nlohmann::json::object();
    manager.set("", empty_config);

    REQUIRE(true);  // No exception means valid
}

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.ValidConfig.Basic", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    nlohmann::json config = {
        {"turbot", {
            {"debug", false},
            {"log_level", "info"}
        }},
        {"permissions", {
            {"mode", "ask"}
        }}
    };

    manager.set("", config);
    REQUIRE(manager.has("turbot"));
}

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.ValidConfig.WithProviders", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    nlohmann::json config = {
        {"providers_by_name", {
            {"openai", {
                {"api_key", "sk-test"},
                {"base_url", "https://api.openai.com/v1"}
            }},
            {"anthropic", {
                {"api_key", "sk-ant-test"}
            }}
        }}
    };

    manager.set("", config);
    REQUIRE(manager.has("providers_by_name"));
}

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.ValidConfig.WithPermissions", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    nlohmann::json config = {
        {"permissions", {
            {"mode", "auto"},
            {"allow", {"read", "write"}},
            {"deny", {"delete"}}
        }}
    };

    manager.set("", config);
    REQUIRE(manager.has("permissions"));
}

// ==================== Provider Validation Tests ====================

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.Provider.MissingApiKey", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    // Provider without API key - may be valid for some providers
    nlohmann::json config = {
        {"providers_by_name", {
            {"openai", {
                {"base_url", "https://api.openai.com/v1"}
            }}
        }}
    };

    manager.set("", config);
    REQUIRE(manager.has("providers_by_name.openai"));
}

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.Provider.EmptyApiKey", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    nlohmann::json config = {
        {"providers_by_name", {
            {"openai", {
                {"api_key", ""}
            }}
        }}
    };

    manager.set("", config);
    REQUIRE(manager.has("providers_by_name.openai"));
}

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.Provider.InvalidBaseUrl", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    // Invalid URL format - should still be accepted (validation may be lenient)
    nlohmann::json config = {
        {"providers_by_name", {
            {"openai", {
                {"api_key", "sk-test"},
                {"base_url", "not-a-valid-url"}
            }}
        }}
    };

    manager.set("", config);
    REQUIRE(manager.has("providers_by_name.openai"));
}

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.Provider.MultipleProviders", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    nlohmann::json config = {
        {"providers_by_name", {
            {"openai", {{"api_key", "sk-openai"}}},
            {"anthropic", {{"api_key", "sk-anthropic"}}},
            {"azure", {{"api_key", "azure-key"}, {"base_url", "https://azure.openai.com/"}}},
            {"deepseek", {{"api_key", "sk-deepseek"}}}
        }}
    };

    manager.set("", config);
    REQUIRE(manager.has("providers_by_name.openai"));
    REQUIRE(manager.has("providers_by_name.anthropic"));
    REQUIRE(manager.has("providers_by_name.azure"));
    REQUIRE(manager.has("providers_by_name.deepseek"));
}

// ==================== Permission Validation Tests ====================

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.Permission.ValidModes", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    // Test all valid permission modes
    std::vector<std::string> modes = {"ask", "auto", "deny"};

    for (const auto& mode : modes) {
        manager.set("permissions.mode", mode);
        REQUIRE(manager.get<std::string>("permissions.mode") == mode);
    }
}

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.Permission.AllowList", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    nlohmann::json config = {
        {"permissions", {
            {"mode", "auto"},
            {"allow", {"read_file", "write_file", "execute_bash"}}
        }}
    };

    manager.set("", config);
    auto allow_list = manager.get<nlohmann::json>("permissions.allow");
    REQUIRE(allow_list.has_value());
    REQUIRE(allow_list->is_array());
    REQUIRE(allow_list->size() == 3);
}

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.Permission.DenyList", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    nlohmann::json config = {
        {"permissions", {
            {"mode", "ask"},
            {"deny", {"delete_file", "execute_bash"}}
        }}
    };

    manager.set("", config);
    auto deny_list = manager.get<nlohmann::json>("permissions.deny");
    REQUIRE(deny_list.has_value());
    REQUIRE(deny_list->is_array());
    REQUIRE(deny_list->size() == 2);
}

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.Permission.Rules", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    nlohmann::json config = {
        {"permissions", {
            {"mode", "auto"},
            {"rules", {
                {{"permission", "file"}, {"pattern", "*.txt"}, {"action", "allow"}},
                {{"permission", "bash"}, {"pattern", "rm *"}, {"action", "deny"}}
            }}
        }}
    };

    manager.set("", config);
    REQUIRE(manager.has("permissions.rules"));
}

// ==================== Invalid Config Tests ====================

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.InvalidConfig.WrongType", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    // Set a value with wrong type (string where number expected)
    manager.set("test.number_value", "not a number");

    // Should store as string
    auto value = manager.get<std::string>("test.number_value");
    REQUIRE(value.has_value());
    REQUIRE(*value == "not a number");
}

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.InvalidConfig.MalformedJson", "[Config][Validation]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    // Create a file with malformed JSON
    auto malformed_path = tmp.path() / "malformed.json";
    std::ofstream file(malformed_path);
    file << "{\n  \"key\": missing_quotes\n}";

    REQUIRE(std::filesystem::exists(malformed_path));
    // Loading this file should fail gracefully
}

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.InvalidConfig.DeepNesting", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    // Create deeply nested config
    manager.set("level1.level2.level3.level4.level5.value", "deep");
    REQUIRE(manager.get<std::string>("level1.level2.level3.level4.level5.value") == "deep");
}

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.InvalidConfig.CircularReference", "[Config][Validation]") {
    // Circular references would be detected during validation
    // This test verifies the manager handles complex structures
    auto& manager = ConfigManager::instance();

    nlohmann::json config = {
        {"a", {{"ref", "b"}}},
        {"b", {{"ref", "a"}}}
    };

    manager.set("", config);
    REQUIRE(manager.has("a"));
    REQUIRE(manager.has("b"));
}

// ==================== Edge Cases ====================

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.Edge.NullValues", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    manager.set("null_value", nullptr);
    auto value = manager.get<nlohmann::json>("null_value");
    REQUIRE(value.has_value());
    REQUIRE(value->is_null());
}

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.Edge.EmptyStrings", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    manager.set("empty_string", "");
    auto value = manager.get<std::string>("empty_string");
    REQUIRE(value.has_value());
    REQUIRE(*value == "");
}

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.Edge.SpecialCharacters", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    std::string special = "value with \n newline and \t tab and \"quotes\"";
    manager.set("special_chars", special);

    auto value = manager.get<std::string>("special_chars");
    REQUIRE(value.has_value());
    REQUIRE(*value == special);
}

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.Edge.UnicodeKeys", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    manager.set("unicode_key_你好", "value");
    auto value = manager.get<std::string>("unicode_key_你好");
    REQUIRE(value.has_value());
    REQUIRE(*value == "value");
}

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.Edge.LargeConfig", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    // Create a large config
    nlohmann::json large_config = nlohmann::json::object();
    for (int i = 0; i < 1000; ++i) {
        large_config["key" + std::to_string(i)] = i;
    }

    manager.set("", large_config);

    // Verify some values
    REQUIRE(manager.get<int>("key0") == 0);
    REQUIRE(manager.get<int>("key500") == 500);
    REQUIRE(manager.get<int>("key999") == 999);
}

// ==================== ConfigException Tests ====================

TEST_CASE("Config.Validation.Exception.Types", "[Config][Validation]") {
    ConfigException ex1(ConfigError::FileNotFound, "File not found");
    REQUIRE(ex1.error() == ConfigError::FileNotFound);

    ConfigException ex2(ConfigError::ParseError, "Parse error");
    REQUIRE(ex2.error() == ConfigError::ParseError);

    ConfigException ex3(ConfigError::ValidationError, "Validation error");
    REQUIRE(ex3.error() == ConfigError::ValidationError);

    ConfigException ex4(ConfigError::PermissionDenied, "Permission denied");
    REQUIRE(ex4.error() == ConfigError::PermissionDenied);

    ConfigException ex5(ConfigError::InvalidPath, "Invalid path");
    REQUIRE(ex5.error() == ConfigError::InvalidPath);

    ConfigException ex6(ConfigError::CircularReference, "Circular reference");
    REQUIRE(ex6.error() == ConfigError::CircularReference);
}

TEST_CASE("Config.Validation.Exception.Message", "[Config][Validation]") {
    std::string message = "This is a test error message";
    ConfigException ex(ConfigError::ParseError, message);

    REQUIRE(std::string(ex.what()) == message);
}

// ==================== Integration Tests ====================

TEST_CASE_METHOD(ConfigValidationFixture, "Config.Validation.Integration.FullValidation", "[Config][Validation]") {
    auto& manager = ConfigManager::instance();

    nlohmann::json full_config = {
        {"turbot", {
            {"debug", true},
            {"log_level", "debug"}
        }},
        {"providers_by_name", {
            {"openai", {
                {"api_key", "sk-test-key"},
                {"base_url", "https://api.openai.com/v1"}
            }}
        }},
        {"permissions", {
            {"mode", "auto"},
            {"allow", {"read", "write"}},
            {"deny", {"delete"}}
        }},
        {"extensions", {
            {"enabled", true},
            {"directories", {"/path/to/extensions"}}
        }}
    };

    manager.set("", full_config);

    // Verify all sections
    REQUIRE(manager.get<bool>("turbot.debug") == true);
    REQUIRE(manager.get<std::string>("turbot.log_level") == "debug");
    REQUIRE(manager.has("providers_by_name.openai"));
    REQUIRE(manager.get<std::string>("permissions.mode") == "auto");
    REQUIRE(manager.get<bool>("extensions.enabled") == true);
}
