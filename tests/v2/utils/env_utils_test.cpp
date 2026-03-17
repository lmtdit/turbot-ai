#include <catch2/catch_test_macros.hpp>
#include <turbot/utils/env_utils.hpp>
#include "mock/env_guard.hpp"

using namespace turbot::utils;

// ==================== get_env Tests ====================

TEST_CASE("EnvUtils.GetEnv.Existing", "[Utils][Env]") {
    set_env("TURBOT_TEST_VAR", "test_value");
    
    std::string value = get_env("TURBOT_TEST_VAR");
    REQUIRE(value == "test_value");
    
    unset_env("TURBOT_TEST_VAR");
}

TEST_CASE("EnvUtils.GetEnv.NonExistent", "[Utils][Env]") {
    std::string value = get_env("TURBOT_NONEXISTENT_VAR_12345");
    REQUIRE(value.empty());
}

TEST_CASE("EnvUtils.GetEnv.EmptyValue", "[Utils][Env]") {
    set_env("TURBOT_EMPTY_VAR", "");
    
    std::string value = get_env("TURBOT_EMPTY_VAR");
    // Empty value still returns empty string
    REQUIRE(value.empty());
    
    unset_env("TURBOT_EMPTY_VAR");
}

// ==================== get_env_or Tests ====================

TEST_CASE("EnvUtils.GetEnvOr.Existing", "[Utils][Env]") {
    set_env("TURBOT_TEST_VAR2", "actual_value");
    
    std::string value = get_env_or("TURBOT_TEST_VAR2", "default_value");
    REQUIRE(value == "actual_value");
    
    unset_env("TURBOT_TEST_VAR2");
}

TEST_CASE("EnvUtils.GetEnvOr.NonExistent", "[Utils][Env]") {
    std::string value = get_env_or("TURBOT_NONEXISTENT_VAR_67890", "default_value");
    REQUIRE(value == "default_value");
}

TEST_CASE("EnvUtils.GetEnvOr.EmptyValue", "[Utils][Env]") {
    set_env("TURBOT_EMPTY_VAR2", "");
    
    std::string value = get_env_or("TURBOT_EMPTY_VAR2", "default_value");
    // Empty value is still considered "set"
    REQUIRE(value.empty());
    
    unset_env("TURBOT_EMPTY_VAR2");
}

// ==================== set_env Tests ====================

TEST_CASE("EnvUtils.SetEnv.Basic", "[Utils][Env]") {
    set_env("TURBOT_SET_TEST", "new_value");
    
    REQUIRE(get_env("TURBOT_SET_TEST") == "new_value");
    
    unset_env("TURBOT_SET_TEST");
}

TEST_CASE("EnvUtils.SetEnv.Overwrite", "[Utils][Env]") {
    set_env("TURBOT_OVERWRITE_TEST", "original");
    set_env("TURBOT_OVERWRITE_TEST", "updated");
    
    REQUIRE(get_env("TURBOT_OVERWRITE_TEST") == "updated");
    
    unset_env("TURBOT_OVERWRITE_TEST");
}

TEST_CASE("EnvUtils.SetEnv.SpecialChars", "[Utils][Env]") {
    set_env("TURBOT_SPECIAL_TEST", "value with spaces and !@#$%");
    
    REQUIRE(get_env("TURBOT_SPECIAL_TEST") == "value with spaces and !@#$%");
    
    unset_env("TURBOT_SPECIAL_TEST");
}

// ==================== unset_env Tests ====================

TEST_CASE("EnvUtils.UnsetEnv.Basic", "[Utils][Env]") {
    set_env("TURBOT_UNSET_TEST", "value");
    unset_env("TURBOT_UNSET_TEST");
    
    REQUIRE(get_env("TURBOT_UNSET_TEST").empty());
}

TEST_CASE("EnvUtils.UnsetEnv.NonExistent", "[Utils][Env]") {
    // Unsetting non-existent should not throw
    REQUIRE_NOTHROW(unset_env("TURBOT_NONEXISTENT_UNSET"));
}

// ==================== has_env Tests ====================

TEST_CASE("EnvUtils.HasEnv.Existing", "[Utils][Env]") {
    set_env("TURBOT_HAS_TEST", "value");
    
    REQUIRE(has_env("TURBOT_HAS_TEST"));
    
    unset_env("TURBOT_HAS_TEST");
}

TEST_CASE("EnvUtils.HasEnv.NonExistent", "[Utils][Env]") {
    REQUIRE_FALSE(has_env("TURBOT_NONEXISTENT_HAS"));
}

TEST_CASE("EnvUtils.HasEnv.EmptyValue", "[Utils][Env]") {
    set_env("TURBOT_EMPTY_HAS", "");
    
    // Empty value is still "set"
    REQUIRE(has_env("TURBOT_EMPTY_HAS"));
    
    unset_env("TURBOT_EMPTY_HAS");
}

// ==================== env_key_to_config_key Tests ====================

TEST_CASE("EnvUtils.EnvKeyToConfigKey.Basic", "[Utils][Env]") {
    std::string config_key = env_key_to_config_key("TURBOT_DATABASE_HOST", "TURBOT_");
    REQUIRE(config_key == "database.host");
}

TEST_CASE("EnvUtils.EnvKeyToConfigKey.MultiLevel", "[Utils][Env]") {
    std::string config_key = env_key_to_config_key("TURBOT_SERVER_HTTP_PORT", "TURBOT_");
    REQUIRE(config_key == "server.http.port");
}

TEST_CASE("EnvUtils.EnvKeyToConfigKey.NoPrefix", "[Utils][Env]") {
    std::string config_key = env_key_to_config_key("DATABASE_HOST", "");
    REQUIRE(config_key == "database.host");
}

TEST_CASE("EnvUtils.EnvKeyToConfigKey.SinglePart", "[Utils][Env]") {
    std::string config_key = env_key_to_config_key("TURBOT_PORT", "TURBOT_");
    REQUIRE(config_key == "port");
}

TEST_CASE("EnvUtils.EnvKeyToConfigKey.LowercaseConversion", "[Utils][Env]") {
    std::string config_key = env_key_to_config_key("TURBOT_DATABASE_HOSTNAME", "TURBOT_");
    REQUIRE(config_key == "database.hostname");
}

// ==================== resolve_env_refs Tests ====================

TEST_CASE("EnvUtils.ResolveEnvRefs.Basic", "[Utils][Env]") {
    set_env("TURBOT_HOME", "/home/user");
    
    std::string result = resolve_env_refs("${TURBOT_HOME}/config");
    REQUIRE(result == "/home/user/config");
    
    unset_env("TURBOT_HOME");
}

TEST_CASE("EnvUtils.ResolveEnvRefs.Multiple", "[Utils][Env]") {
    set_env("TURBOT_VAR1", "hello");
    set_env("TURBOT_VAR2", "world");
    
    std::string result = resolve_env_refs("${TURBOT_VAR1}_${TURBOT_VAR2}");
    REQUIRE(result == "hello_world");
    
    unset_env("TURBOT_VAR1");
    unset_env("TURBOT_VAR2");
}

TEST_CASE("EnvUtils.ResolveEnvRefs.WithDefault", "[Utils][Env]") {
    std::string result = resolve_env_refs("${TURBOT_NONEXISTENT_VAR:-default_value}");
    REQUIRE(result == "default_value");
}

TEST_CASE("EnvUtils.ResolveEnvRefs.DefaultNotUsed", "[Utils][Env]") {
    set_env("TURBOT_EXISTING_VAR", "actual");
    
    std::string result = resolve_env_refs("${TURBOT_EXISTING_VAR:-default}");
    REQUIRE(result == "actual");
    
    unset_env("TURBOT_EXISTING_VAR");
}

TEST_CASE("EnvUtils.ResolveEnvRefs.NoRefs", "[Utils][Env]") {
    std::string result = resolve_env_refs("plain string without refs");
    REQUIRE(result == "plain string without refs");
}

TEST_CASE("EnvUtils.ResolveEnvRefs.EmptyString", "[Utils][Env]") {
    std::string result = resolve_env_refs("");
    REQUIRE(result.empty());
}

TEST_CASE("EnvUtils.ResolveEnvRefs.NestedBraces", "[Utils][Env]") {
    set_env("TURBOT_NESTED", "value");
    
    // Simple nested case - the inner braces are part of the value
    std::string result = resolve_env_refs("${TURBOT_NESTED}");
    REQUIRE(result == "value");
    
    unset_env("TURBOT_NESTED");
}

TEST_CASE("EnvUtils.ResolveEnvRefs.EmptyDefault", "[Utils][Env]") {
    std::string result = resolve_env_refs("${TURBOT_NONEXISTENT_EMPTY:-}");
    REQUIRE(result.empty());
}

// ==================== Integration Tests ====================

TEST_CASE("EnvUtils.Integration.RoundTrip", "[Utils][Env]") {
    turbot::test::EnvGuard guard;
    
    set_env("TURBOT_INTEGRATION_TEST", "test_value");
    REQUIRE(has_env("TURBOT_INTEGRATION_TEST"));
    REQUIRE(get_env("TURBOT_INTEGRATION_TEST") == "test_value");
    REQUIRE(get_env_or("TURBOT_INTEGRATION_TEST", "default") == "test_value");
    
    unset_env("TURBOT_INTEGRATION_TEST");
    REQUIRE_FALSE(has_env("TURBOT_INTEGRATION_TEST"));
    REQUIRE(get_env("TURBOT_INTEGRATION_TEST").empty());
    REQUIRE(get_env_or("TURBOT_INTEGRATION_TEST", "default") == "default");
}
