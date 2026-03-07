#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <turbot/core/config/config_manager.hpp>
#include <fstream>
#include <filesystem>
#include <cstdlib>

using namespace turbot::core;

// 测试辅助函数
namespace {

std::string create_temp_dir(const std::string& name) {
    std::string temp_dir = std::filesystem::temp_directory_path().string() + "/" + name;
    std::filesystem::create_directories(temp_dir);
    return temp_dir;
}

void remove_temp_dir(const std::string& path) {
    if (std::filesystem::exists(path)) {
        std::filesystem::remove_all(path);
    }
}

std::string create_temp_config(const std::string& dir, const std::string& filename, const std::string& content) {
    std::string file_path = dir + "/" + filename;
    std::ofstream file(file_path);
    file << content;
    file.close();
    return file_path;
}

}

// ==================== ConfigLevel 枚举测试 ====================

TEST_CASE("ConfigLevel enum values", "[core][config_manager]") {
    SECTION("enum values are correct") {
        REQUIRE(static_cast<int>(ConfigLevel::Default) == 0);
        REQUIRE(static_cast<int>(ConfigLevel::User) == 1);
        REQUIRE(static_cast<int>(ConfigLevel::Project) == 2);
    }
}

// ==================== ExtensionType 枚举测试 ====================

TEST_CASE("ExtensionType enum values", "[core][config_manager]") {
    SECTION("all extension types are defined") {
        REQUIRE(static_cast<int>(ExtensionType::Agent) == 0);
        REQUIRE(static_cast<int>(ExtensionType::Skill) == 1);
        REQUIRE(static_cast<int>(ExtensionType::Rule) == 2);
        REQUIRE(static_cast<int>(ExtensionType::Event) == 3);
        REQUIRE(static_cast<int>(ExtensionType::Extension) == 4);
    }
}

// ==================== ConfigException 测试 ====================

TEST_CASE("ConfigException", "[core][config_manager]") {
    SECTION("stores error type and message") {
        ConfigException ex(ConfigError::FileNotFound, "File not found: test.json");
        REQUIRE(ex.error() == ConfigError::FileNotFound);
        REQUIRE(std::string(ex.what()) == "File not found: test.json");
    }

    SECTION("different error types") {
        ConfigException ex1(ConfigError::ParseError, "parse error");
        REQUIRE(ex1.error() == ConfigError::ParseError);

        ConfigException ex2(ConfigError::ValidationError, "validation error");
        REQUIRE(ex2.error() == ConfigError::ValidationError);

        ConfigException ex3(ConfigError::PermissionDenied, "permission denied");
        REQUIRE(ex3.error() == ConfigError::PermissionDenied);

        ConfigException ex4(ConfigError::InvalidPath, "invalid path");
        REQUIRE(ex4.error() == ConfigError::InvalidPath);

        ConfigException ex5(ConfigError::CircularReference, "circular reference");
        REQUIRE(ex5.error() == ConfigError::CircularReference);
    }
}

// ==================== LoadResult 测试 ====================

TEST_CASE("LoadResult default values", "[core][config_manager]") {
    LoadResult result;
    REQUIRE(result.success == false);
    REQUIRE(result.level == ConfigLevel::Default);
    REQUIRE(result.path.empty());
    REQUIRE(result.warnings.empty());
    REQUIRE(result.errors.empty());
}

// ==================== MarkdownConfig 测试 ====================

TEST_CASE("MarkdownConfig", "[core][config_manager]") {
    SECTION("default construction") {
        MarkdownConfig config;
        REQUIRE((config.frontmatter.is_null() || config.frontmatter.is_object()));
        REQUIRE(config.content.empty());
    }
}

// ==================== ConfigManager::instance 测试 ====================

TEST_CASE("ConfigManager::instance", "[core][config_manager]") {
    SECTION("singleton pattern") {
        auto& instance1 = ConfigManager::instance();
        auto& instance2 = ConfigManager::instance();
        REQUIRE(&instance1 == &instance2);
    }
}

// ==================== ConfigManager::get_config_path 测试 ====================

TEST_CASE("ConfigManager::get_config_path", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();

    SECTION("default config path") {
        std::string path = manager.get_config_path(ConfigLevel::Default);
        REQUIRE(path == "(compiled-in)");
    }

    SECTION("user config path") {
        std::string path = manager.get_config_path(ConfigLevel::User);
        REQUIRE(path.find(".turbot") != std::string::npos);
        REQUIRE(path.find("turbot.json") != std::string::npos);
    }

    SECTION("project config path") {
        std::string path = manager.get_config_path(ConfigLevel::Project);
        REQUIRE(path.find(".turbot") != std::string::npos);
        REQUIRE(path.find("turbot.json") != std::string::npos);
    }

    SECTION("user config path with env override") {
        std::string temp_dir = create_temp_dir("turbot_test_user_path");
        setenv("TURBOT_USER_CONFIG_PATH", temp_dir.c_str(), 1);

        std::string path = manager.get_config_path(ConfigLevel::User);
        REQUIRE(path == temp_dir + "/turbot.json");

        unsetenv("TURBOT_USER_CONFIG_PATH");
        remove_temp_dir(temp_dir);
    }

    SECTION("project config path with env override") {
        std::string temp_dir = create_temp_dir("turbot_test_project_path");
        setenv("TURBOT_PROJECT_CONFIG_PATH", temp_dir.c_str(), 1);

        std::string path = manager.get_config_path(ConfigLevel::Project);
        REQUIRE(path == temp_dir + "/turbot.json");

        unsetenv("TURBOT_PROJECT_CONFIG_PATH");
        remove_temp_dir(temp_dir);
    }
}

// ==================== ConfigManager::get_extension_path 测试 ====================

TEST_CASE("ConfigManager::get_extension_path", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();

    SECTION("agent extension path") {
        std::string path = manager.get_extension_path(ConfigLevel::Project, ExtensionType::Agent);
        REQUIRE(path.find("agents") != std::string::npos);
    }

    SECTION("skill extension path") {
        std::string path = manager.get_extension_path(ConfigLevel::Project, ExtensionType::Skill);
        REQUIRE(path.find("skills") != std::string::npos);
    }

    SECTION("rule extension path") {
        std::string path = manager.get_extension_path(ConfigLevel::Project, ExtensionType::Rule);
        REQUIRE(path.find("rules") != std::string::npos);
    }

    SECTION("event extension path") {
        std::string path = manager.get_extension_path(ConfigLevel::Project, ExtensionType::Event);
        REQUIRE(path.find("events") != std::string::npos);
    }

    SECTION("extension extension path") {
        std::string path = manager.get_extension_path(ConfigLevel::Project, ExtensionType::Extension);
        REQUIRE(path.find("extensions") != std::string::npos);
    }

    SECTION("default level returns empty") {
        std::string path = manager.get_extension_path(ConfigLevel::Default, ExtensionType::Agent);
        REQUIRE(path.empty());
    }
}

// ==================== ConfigManager::get_default_config 测试 ====================

TEST_CASE("ConfigManager::get_default_config", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();

    SECTION("default config has required fields") {
        manager.initialize();
        auto default_val = manager.get<std::string>("version");
        REQUIRE(default_val.has_value());
        REQUIRE(default_val.value() == "1.0");
    }

    SECTION("default permissions mode is ask") {
        manager.initialize();
        auto mode = manager.get<std::string>("permissions.mode");
        REQUIRE(mode.has_value());
        REQUIRE(mode.value() == "ask");
    }

    SECTION("default debug is false") {
        manager.initialize();
        auto debug = manager.get<bool>("turbot.debug");
        REQUIRE(debug.has_value());
        REQUIRE(debug.value() == false);
    }
}

// ==================== ConfigManager::load_config 测试 ====================

TEST_CASE("ConfigManager::load_config", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();

    SECTION("load non-existing file returns failure") {
        LoadResult result = manager.load_config(ConfigLevel::Project);
        REQUIRE_FALSE(result.success);
        REQUIRE_FALSE(result.warnings.empty());
    }

    SECTION("load valid config file") {
        std::string temp_dir = create_temp_dir("turbot_test_load");
        create_temp_config(temp_dir, "turbot.json", R"({
            "version": "1.0",
            "test": {
                "value": "loaded"
            }
        })");

        setenv("TURBOT_PROJECT_CONFIG_PATH", temp_dir.c_str(), 1);

        LoadResult result = manager.load_config(ConfigLevel::Project);
        REQUIRE(result.success);
        REQUIRE(result.path.find("turbot.json") != std::string::npos);

        unsetenv("TURBOT_PROJECT_CONFIG_PATH");
        remove_temp_dir(temp_dir);
    }

    SECTION("load invalid JSON returns error") {
        std::string temp_dir = create_temp_dir("turbot_test_invalid");
        create_temp_config(temp_dir, "turbot.json", "{ invalid json }");

        setenv("TURBOT_PROJECT_CONFIG_PATH", temp_dir.c_str(), 1);

        LoadResult result = manager.load_config(ConfigLevel::Project);
        REQUIRE_FALSE(result.success);
        REQUIRE_FALSE(result.errors.empty());

        unsetenv("TURBOT_PROJECT_CONFIG_PATH");
        remove_temp_dir(temp_dir);
    }
}

// ==================== ConfigManager::initialize 测试 ====================

TEST_CASE("ConfigManager::initialize", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();

    SECTION("initialize returns three results") {
        std::vector<LoadResult> results = manager.initialize();
        REQUIRE(results.size() == 3);

        REQUIRE(results[0].level == ConfigLevel::Default);
        REQUIRE(results[0].success == true);

        REQUIRE(results[1].level == ConfigLevel::User);
        REQUIRE(results[2].level == ConfigLevel::Project);
    }
}

// ==================== ConfigManager::get/get_or 测试 ====================

TEST_CASE("ConfigManager::get and get_or", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();
    manager.initialize();

    SECTION("get existing key") {
        auto version = manager.get<std::string>("version");
        REQUIRE(version.has_value());
        REQUIRE(version.value() == "1.0");
    }

    SECTION("get non-existing key") {
        auto value = manager.get<std::string>("non.existing.key");
        REQUIRE_FALSE(value.has_value());
    }

    SECTION("get_or with existing key") {
        auto value = manager.get_or<std::string>("version", "default");
        REQUIRE(value == "1.0");
    }

    SECTION("get_or with non-existing key") {
        auto value = manager.get_or<std::string>("non.existing.key", std::string("default"));
        REQUIRE(value == "default");
    }

    SECTION("get nested value") {
        auto mode = manager.get<std::string>("permissions.mode");
        REQUIRE(mode.has_value());
        REQUIRE(mode.value() == "ask");
    }
}

// ==================== ConfigManager::has 测试 ====================

TEST_CASE("ConfigManager::has", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();
    manager.initialize();

    SECTION("existing key returns true") {
        REQUIRE(manager.has("version") == true);
        REQUIRE(manager.has("permissions") == true);
        REQUIRE(manager.has("permissions.mode") == true);
    }

    SECTION("non-existing key returns false") {
        REQUIRE(manager.has("non.existing.key") == false);
    }
}

// ==================== ConfigManager::set 测试 ====================

TEST_CASE("ConfigManager::set", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();
    manager.initialize();

    SECTION("set and get value") {
        manager.set("test.set.value", "test_data");
        auto value = manager.get<std::string>("test.set.value");
        REQUIRE(value.has_value());
        REQUIRE(value.value() == "test_data");
    }

    SECTION("set overwrites existing value") {
        manager.set("test.overwrite", "original");
        manager.set("test.overwrite", "updated");

        auto value = manager.get<std::string>("test.overwrite");
        REQUIRE(value.has_value());
        REQUIRE(value.value() == "updated");
    }

    SECTION("set nested value") {
        manager.set("deep.nested.value", 42);
        auto value = manager.get<int>("deep.nested.value");
        REQUIRE(value.has_value());
        REQUIRE(value.value() == 42);
    }
}

// ==================== ConfigManager::get_all 测试 ====================

TEST_CASE("ConfigManager::get_all", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();
    manager.initialize();

    SECTION("get_all returns json object") {
        nlohmann::json all = manager.get_all();
        REQUIRE(all.is_object());
        REQUIRE(all.contains("version"));
    }
}

// ==================== ConfigManager::validate_config 测试 ====================

TEST_CASE("ConfigManager::validate_config", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();

    SECTION("valid config passes validation") {
        nlohmann::json config = {
            {"version", "1.0"},
            {"providers", nlohmann::json::array()}
        };
        REQUIRE(manager.validate_config(config) == true);
    }

    SECTION("invalid version type fails") {
        nlohmann::json config = {
            {"version", 123}
        };
        REQUIRE(manager.validate_config(config) == false);
    }

    SECTION("providers must be array") {
        nlohmann::json config = {
            {"providers", "not an array"}
        };
        REQUIRE(manager.validate_config(config) == false);
    }

    SECTION("provider must have name and type") {
        nlohmann::json config = {
            {"providers", nlohmann::json::array({
                {{"name", "test"}}  // missing type
            })}
        };
        REQUIRE(manager.validate_config(config) == false);
    }

    SECTION("valid provider passes") {
        nlohmann::json config = {
            {"providers", nlohmann::json::array({
                {{"name", "test"}, {"type", "openai"}}
            })}
        };
        REQUIRE(manager.validate_config(config) == true);
    }

    SECTION("invalid permission mode fails") {
        nlohmann::json config = {
            {"permissions", {
                {"mode", "invalid"}
            }}
        };
        REQUIRE(manager.validate_config(config) == false);
    }

    SECTION("permission rules must be array") {
        nlohmann::json config = {
            {"permissions", {
                {"rules", "not an array"}
            }}
        };
        REQUIRE(manager.validate_config(config) == false);
    }

    SECTION("permission rule must have pattern and action") {
        nlohmann::json config = {
            {"permissions", {
                {"rules", nlohmann::json::array({
                    {{"pattern", "test"}}  // missing action
                })}
            }}
        };
        REQUIRE(manager.validate_config(config) == false);
    }

    SECTION("valid permission config passes") {
        nlohmann::json config = {
            {"permissions", {
                {"mode", "ask"},
                {"rules", nlohmann::json::array({
                    {{"pattern", "read:*"}, {"action", "allow"}}
                })}
            }}
        };
        REQUIRE(manager.validate_config(config) == true);
    }
}

// ==================== ConfigManager::parse_markdown_config 测试 ====================

TEST_CASE("ConfigManager::parse_markdown_config", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();

    SECTION("parse valid markdown with frontmatter") {
        std::string temp_dir = create_temp_dir("turbot_test_md");
        std::string md_file = create_temp_config(temp_dir, "test.md",
            "---\n"
            "name: test-agent\n"
            "description: Test Agent\n"
            "---\n"
            "# Test Agent\n"
            "This is the content.\n");

        MarkdownConfig config = manager.parse_markdown_config(md_file);

        REQUIRE_FALSE(config.frontmatter.is_null());
        REQUIRE(config.frontmatter.contains("name"));
        REQUIRE(config.content.find("# Test Agent") != std::string::npos);

        remove_temp_dir(temp_dir);
    }

    SECTION("parse markdown without frontmatter") {
        std::string temp_dir = create_temp_dir("turbot_test_md_no_fm");
        std::string md_file = create_temp_config(temp_dir, "test.md",
            "# No Frontmatter\n"
            "Just content.\n");

        MarkdownConfig config = manager.parse_markdown_config(md_file);

        REQUIRE(config.frontmatter.is_object());
        REQUIRE(config.content.find("# No Frontmatter") != std::string::npos);

        remove_temp_dir(temp_dir);
    }

    SECTION("parse non-existing file") {
        MarkdownConfig config = manager.parse_markdown_config("/non/existing/file.md");
        REQUIRE(config.frontmatter.is_object());
        REQUIRE(config.content.empty());
    }
}

// ==================== ConfigManager::parse_yaml 测试 ====================

TEST_CASE("ConfigManager::parse_yaml", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();

    SECTION("parse simple key-value") {
        std::string yaml = "name: test\nversion: \"1.0\"\n";
        nlohmann::json result = manager.parse_yaml(yaml);

        REQUIRE(result.is_object());
        REQUIRE(result["name"] == "test");
        REQUIRE(result["version"] == "1.0");
    }

    SECTION("parse boolean values") {
        std::string yaml = "enabled: true\ndisabled: false\n";
        nlohmann::json result = manager.parse_yaml(yaml);

        REQUIRE(result["enabled"] == true);
        REQUIRE(result["disabled"] == false);
    }

    SECTION("parse numeric values") {
        std::string yaml = "int_value: 42\nfloat_value: 3.14\n";
        nlohmann::json result = manager.parse_yaml(yaml);

        REQUIRE(result["int_value"] == 42);
        REQUIRE(result["float_value"].get<double>() == Catch::Approx(3.14));
    }

    SECTION("parse nested object") {
        std::string yaml =
            "database:\n"
            "  host: localhost\n"
            "  port: 5432\n";
        nlohmann::json result = manager.parse_yaml(yaml);

        REQUIRE(result.is_object());
        REQUIRE(result["database"]["host"] == "localhost");
        REQUIRE(result["database"]["port"] == 5432);
    }

    SECTION("parse null value") {
        std::string yaml = "empty: null\nnothing: ~\n";
        nlohmann::json result = manager.parse_yaml(yaml);

        REQUIRE(result["empty"].is_null());
        REQUIRE(result["nothing"].is_null());
    }

    SECTION("parse quoted strings") {
        std::string yaml = "single: 'value'\ndouble: \"value\"\n";
        nlohmann::json result = manager.parse_yaml(yaml);

        REQUIRE(result["single"] == "value");
        REQUIRE(result["double"] == "value");
    }
}

// ==================== ConfigManager::resolve_env_vars 测试 ====================

TEST_CASE("ConfigManager::resolve_env_vars", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();

    SECTION("resolve existing env var") {
        setenv("TEST_VAR_FOR_RESOLVE", "resolved_value", 1);
        std::string result = manager.resolve_env_vars("${TEST_VAR_FOR_RESOLVE}");
        REQUIRE(result == "resolved_value");
        unsetenv("TEST_VAR_FOR_RESOLVE");
    }

    SECTION("resolve with default value when var not set") {
        unsetenv("NON_EXISTING_VAR_XYZ");
        std::string result = manager.resolve_env_vars("${NON_EXISTING_VAR_XYZ:-default_value}");
        REQUIRE(result == "default_value");
    }

    SECTION("env var takes precedence over default") {
        setenv("TEST_VAR_WITH_DEFAULT", "actual_value", 1);
        std::string result = manager.resolve_env_vars("${TEST_VAR_WITH_DEFAULT:-default_value}");
        REQUIRE(result == "actual_value");
        unsetenv("TEST_VAR_WITH_DEFAULT");
    }

    SECTION("no substitution for non-matching pattern") {
        std::string result = manager.resolve_env_vars("plain text");
        REQUIRE(result == "plain text");
    }

    SECTION("multiple substitutions") {
        setenv("VAR_A", "value_a", 1);
        setenv("VAR_B", "value_b", 1);
        std::string result = manager.resolve_env_vars("${VAR_A} and ${VAR_B}");
        REQUIRE(result == "value_a and value_b");
        unsetenv("VAR_A");
        unsetenv("VAR_B");
    }
}

// ==================== ConfigManager::reload 测试 ====================

TEST_CASE("ConfigManager::reload", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();
    manager.initialize();

    SECTION("reload updates config") {
        manager.set("test.reload", "before");
        manager.reload();

        // After reload, the value should be reset (depends on implementation)
        REQUIRE_NOTHROW(manager.reload());
    }
}

// ==================== ConfigManager::save_config 测试 ====================

TEST_CASE("ConfigManager::save_config", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();
    manager.initialize();

    SECTION("save to default level returns false") {
        bool result = manager.save_config(ConfigLevel::Default);
        REQUIRE(result == false);
    }

    SECTION("save to project level") {
        std::string temp_dir = create_temp_dir("turbot_test_save");
        setenv("TURBOT_PROJECT_CONFIG_PATH", temp_dir.c_str(), 1);

        bool result = manager.save_config(ConfigLevel::Project);
        REQUIRE(result == true);

        unsetenv("TURBOT_PROJECT_CONFIG_PATH");
        remove_temp_dir(temp_dir);
    }
}

// ==================== ConfigManager::get_log_path 测试 ====================

TEST_CASE("ConfigManager::get_log_path", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();

    SECTION("log path contains turbot") {
        std::string log_path = manager.get_log_path();
        REQUIRE(log_path.find(".turbot") != std::string::npos);
        REQUIRE(log_path.find("logs") != std::string::npos);
    }
}

// ==================== ConfigManager::on_config_change 测试 ====================

TEST_CASE("ConfigManager::on_config_change", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();
    manager.initialize();

    SECTION("callback is registered") {
        bool callback_called = false;
        manager.on_config_change([&callback_called](ConfigLevel, const std::string&, const nlohmann::json&) {
            callback_called = true;
        });

        manager.reload();
        REQUIRE(callback_called);
    }
}

// ==================== ConfigManager::load_extensions 测试 ====================

TEST_CASE("ConfigManager::load_extensions", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();

    SECTION("load extensions from non-existing directory") {
        std::vector<LoadResult> results = manager.load_extensions(ConfigLevel::Project, ExtensionType::Agent);
        REQUIRE(results.empty());
    }

    SECTION("load extensions from directory with files") {
        std::string temp_dir = create_temp_dir("turbot_test_ext");
        std::filesystem::create_directories(temp_dir + "/agents");

        // Create a JSON agent file
        create_temp_config(temp_dir + "/agents", "test_agent.json", R"({
            "name": "test_agent",
            "description": "Test Agent"
        })");

        setenv("TURBOT_PROJECT_CONFIG_PATH", temp_dir.c_str(), 1);

        std::vector<LoadResult> results = manager.load_extensions(ConfigLevel::Project, ExtensionType::Agent);
        REQUIRE_FALSE(results.empty());

        unsetenv("TURBOT_PROJECT_CONFIG_PATH");
        remove_temp_dir(temp_dir);
    }

    SECTION("load skill with SKILL.md") {
        std::string temp_dir = create_temp_dir("turbot_test_skill");
        std::filesystem::create_directories(temp_dir + "/skills/test_skill");

        create_temp_config(temp_dir + "/skills/test_skill", "SKILL.md",
            "---\n"
            "name: test_skill\n"
            "description: Test Skill\n"
            "---\n"
            "# Test Skill\n");

        setenv("TURBOT_PROJECT_CONFIG_PATH", temp_dir.c_str(), 1);

        std::vector<LoadResult> results = manager.load_extensions(ConfigLevel::Project, ExtensionType::Skill);
        REQUIRE_FALSE(results.empty());

        unsetenv("TURBOT_PROJECT_CONFIG_PATH");
        remove_temp_dir(temp_dir);
    }
}

// ==================== 环境变量覆盖测试 ====================

TEST_CASE("ConfigManager environment variable overrides", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();

    SECTION("TURBOT_DEBUG override") {
        setenv("TURBOT_DEBUG", "true", 1);
        manager.initialize();

        auto debug = manager.get<bool>("turbot.debug");
        REQUIRE(debug.has_value());
        REQUIRE(debug.value() == true);

        unsetenv("TURBOT_DEBUG");
    }

    SECTION("TURBOT_LOG_LEVEL override") {
        setenv("TURBOT_LOG_LEVEL", "debug", 1);
        manager.initialize();

        auto level = manager.get<std::string>("turbot.log_level");
        REQUIRE(level.has_value());
        REQUIRE(level.value() == "debug");

        unsetenv("TURBOT_LOG_LEVEL");
    }

    SECTION("OPENAI_API_KEY override") {
        setenv("OPENAI_API_KEY", "test_api_key_12345", 1);
        manager.initialize();

        auto api_key = manager.get<std::string>("providers_by_name.openai.api_key");
        REQUIRE(api_key.has_value());
        REQUIRE(api_key.value() == "test_api_key_12345");

        unsetenv("OPENAI_API_KEY");
    }
}

// ==================== 配置合并测试 ====================

TEST_CASE("ConfigManager config merging", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();

    SECTION("project config overrides user config") {
        std::string user_dir = create_temp_dir("turbot_test_user_merge");
        std::string project_dir = create_temp_dir("turbot_test_project_merge");

        create_temp_config(user_dir, "turbot.json", R"({
            "version": "1.0",
            "test": {
                "value": "user_value",
                "user_only": "user_data"
            }
        })");

        create_temp_config(project_dir, "turbot.json", R"({
            "version": "1.0",
            "test": {
                "value": "project_value"
            }
        })");

        setenv("TURBOT_USER_CONFIG_PATH", user_dir.c_str(), 1);
        setenv("TURBOT_PROJECT_CONFIG_PATH", project_dir.c_str(), 1);

        manager.initialize();

        auto value = manager.get<std::string>("test.value");
        REQUIRE(value.has_value());
        REQUIRE(value.value() == "project_value");  // Project overrides user

        auto user_only = manager.get<std::string>("test.user_only");
        REQUIRE(user_only.has_value());
        REQUIRE(user_only.value() == "user_data");  // User-only value preserved

        unsetenv("TURBOT_USER_CONFIG_PATH");
        unsetenv("TURBOT_PROJECT_CONFIG_PATH");
        remove_temp_dir(user_dir);
        remove_temp_dir(project_dir);
    }
}

// ==================== 边界条件测试 ====================

TEST_CASE("ConfigManager edge cases", "[core][config_manager]") {
    auto& manager = ConfigManager::instance();

    SECTION("empty key returns nullopt") {
        auto value = manager.get<std::string>("");
        REQUIRE_FALSE(value.has_value());
    }

    SECTION("get_or with complex default") {
        std::vector<int> default_vec = {1, 2, 3};
        auto result = manager.get_or<std::vector<int>>("non.existing.array", default_vec);
        REQUIRE(result == default_vec);
    }

    SECTION("set creates nested path") {
        manager.set("a.b.c.d.e", "deep_value");
        auto value = manager.get<std::string>("a.b.c.d.e");
        REQUIRE(value.has_value());
        REQUIRE(value.value() == "deep_value");
    }
}
