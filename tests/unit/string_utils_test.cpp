#include <catch2/catch_test_macros.hpp>
#include <turbot/utils/string_utils.hpp>
#include <turbot/utils/env_utils.hpp>
#include <cstdlib>

using namespace turbot::utils;

TEST_CASE("string::split", "[utils][string]") {
    SECTION("basic split") {
        auto result = split("a,b,c", ",");

        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == "a");
        REQUIRE(result[1] == "b");
        REQUIRE(result[2] == "c");
    }

    SECTION("empty string") {
        auto result = split("", ",");

        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == "");
    }

    SECTION("delimiter not found") {
        auto result = split("abc", ",");

        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == "abc");
    }

    SECTION("consecutive delimiters") {
        auto result = split("a,,b,,c", ",");

        REQUIRE(result.size() == 5);
        REQUIRE(result[0] == "a");
        REQUIRE(result[1] == "");
        REQUIRE(result[2] == "b");
        REQUIRE(result[3] == "");
        REQUIRE(result[4] == "c");
    }

    SECTION("empty delimiter") {
        auto result = split("abc", "");

        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == "abc");
    }
}

TEST_CASE("string::join", "[utils][string]") {
    SECTION("basic join") {
        std::vector<std::string> parts = {"a", "b", "c"};
        auto result = join(parts, ",");

        REQUIRE(result == "a,b,c");
    }

    SECTION("empty vector") {
        std::vector<std::string> parts;
        auto result = join(parts, ",");

        REQUIRE(result == "");
    }

    SECTION("single element") {
        std::vector<std::string> parts = {"a"};
        auto result = join(parts, ",");

        REQUIRE(result == "a");
    }

    SECTION("empty delimiter") {
        std::vector<std::string> parts = {"a", "b", "c"};
        auto result = join(parts, "");

        REQUIRE(result == "abc");
    }
}

TEST_CASE("string::replace_all", "[utils][string]") {
    SECTION("basic replacement") {
        std::string result = replace_all("hello world", "world", "turbot");

        REQUIRE(result == "hello turbot");
    }

    SECTION("multiple occurrences") {
        std::string result = replace_all("hello world world", "world", "turbot");

        REQUIRE(result == "hello turbot turbot");
    }

    SECTION("no occurrences") {
        std::string result = replace_all("hello world", "foo", "bar");

        REQUIRE(result == "hello world");
    }

    SECTION("empty replacement") {
        std::string result = replace_all("hello world", "world", "");

        REQUIRE(result == "hello ");
    }
}

TEST_CASE("string::trim", "[utils][string]") {
    SECTION("trim both sides") {
        std::string result = trim("  hello  ");

        REQUIRE(result == "hello");
    }

    SECTION("trim left") {
        std::string result = trim_left("  hello  ");

        REQUIRE(result == "hello  ");
    }

    SECTION("trim right") {
        std::string result = trim_right("  hello  ");

        REQUIRE(result == "  hello");
    }

    SECTION("no spaces") {
        std::string result = trim("hello");

        REQUIRE(result == "hello");
    }

    SECTION("only spaces") {
        std::string result = trim("   ");

        REQUIRE(result == "");
    }

    SECTION("empty string") {
        std::string result = trim("");

        REQUIRE(result == "");
    }
}

TEST_CASE("string::generate_uuid", "[utils][string]") {
    SECTION("generate unique UUIDs") {
        std::string uuid1 = generate_uuid();
        std::string uuid2 = generate_uuid();

        REQUIRE(uuid1 != uuid2);
        REQUIRE(uuid1.length() == 36);  // UUID格式: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        REQUIRE(uuid2.length() == 36);
    }

    SECTION("UUID format") {
        std::string uuid = generate_uuid();

        // 检查格式: 8-4-4-4-12
        REQUIRE(uuid[8] == '-');
        REQUIRE(uuid[13] == '-');
        REQUIRE(uuid[18] == '-');
        REQUIRE(uuid[23] == '-');
    }
}

TEST_CASE("string::base64", "[utils][string]") {
    SECTION("encode and decode") {
        std::string original = "Hello, World!";
        std::string encoded = base64_encode(original);
        std::string decoded = base64_decode(encoded);

        REQUIRE(original == decoded);
    }

    SECTION("encode empty string") {
        std::string original = "";
        std::string encoded = base64_encode(original);
        std::string decoded = base64_decode(encoded);

        REQUIRE(original == decoded);
    }

    SECTION("encode simple string") {
        std::string original = "test";
        std::string encoded = base64_encode(original);

        REQUIRE(encoded == "dGVzdA==");
    }
}

TEST_CASE("string::wildcard_match", "[utils][string]") {
    SECTION("exact match") {
        REQUIRE(wildcard_match("hello", "hello") == true);
    }

    SECTION("wildcard at end") {
        REQUIRE(wildcard_match("hello*", "hello world") == true);
        REQUIRE(wildcard_match("hello*", "hello") == true);
        REQUIRE(wildcard_match("hello*", "hi") == false);
    }

    SECTION("wildcard at start") {
        REQUIRE(wildcard_match("*world", "hello world") == true);
        REQUIRE(wildcard_match("*world", "world") == true);
        REQUIRE(wildcard_match("*world", "hello") == false);
    }

    SECTION("wildcard in middle") {
        REQUIRE(wildcard_match("hello*world", "hello beautiful world") == true);
        REQUIRE(wildcard_match("hello*world", "hello world") == true);
        REQUIRE(wildcard_match("hello*world", "hello") == false);
    }

    SECTION("multiple wildcards") {
        REQUIRE(wildcard_match("*hello*", "say hello to the world") == true);
        REQUIRE(wildcard_match("*a*b*", "xabcy") == true);
    }

    SECTION("empty pattern") {
        REQUIRE(wildcard_match("", "") == true);
        REQUIRE(wildcard_match("", "anything") == false);
    }

    SECTION("question mark wildcard") {
        REQUIRE(wildcard_match("h?llo", "hello") == true);
        REQUIRE(wildcard_match("h?llo", "hallo") == true);
        REQUIRE(wildcard_match("h?llo", "hllo") == false);
        REQUIRE(wildcard_match("??", "ab") == true);
    }

    SECTION("special regex characters") {
        REQUIRE(wildcard_match("file.txt", "file.txt") == true);
        REQUIRE(wildcard_match("test[1]", "test[1]") == true);
        REQUIRE(wildcard_match("(a|b)", "(a|b)") == true);
    }

    SECTION("case insensitive") {
        REQUIRE(wildcard_match("HELLO", "hello") == true);
        REQUIRE(wildcard_match("Hello*", "hello world") == true);
    }
}

TEST_CASE("string::to_lower", "[utils][string]") {
    SECTION("basic conversion") {
        REQUIRE(to_lower("HELLO") == "hello");
        REQUIRE(to_lower("Hello World") == "hello world");
    }

    SECTION("already lowercase") {
        REQUIRE(to_lower("hello") == "hello");
    }

    SECTION("empty string") {
        REQUIRE(to_lower("") == "");
    }

    SECTION("numbers and special chars") {
        REQUIRE(to_lower("ABC123!@#") == "abc123!@#");
    }
}

TEST_CASE("string::to_upper", "[utils][string]") {
    SECTION("basic conversion") {
        REQUIRE(to_upper("hello") == "HELLO");
        REQUIRE(to_upper("Hello World") == "HELLO WORLD");
    }

    SECTION("already uppercase") {
        REQUIRE(to_upper("HELLO") == "HELLO");
    }

    SECTION("empty string") {
        REQUIRE(to_upper("") == "");
    }

    SECTION("numbers and special chars") {
        REQUIRE(to_upper("abc123!@#") == "ABC123!@#");
    }
}

TEST_CASE("string::starts_with", "[utils][string]") {
    SECTION("basic starts with") {
        REQUIRE(starts_with("hello world", "hello") == true);
        REQUIRE(starts_with("hello world", "world") == false);
    }

    SECTION("empty prefix") {
        REQUIRE(starts_with("hello", "") == true);
    }

    SECTION("prefix longer than string") {
        REQUIRE(starts_with("hi", "hello") == false);
    }

    SECTION("exact match") {
        REQUIRE(starts_with("hello", "hello") == true);
    }

    SECTION("empty string") {
        REQUIRE(starts_with("", "") == true);
        REQUIRE(starts_with("", "a") == false);
    }
}

TEST_CASE("string::ends_with", "[utils][string]") {
    SECTION("basic ends with") {
        REQUIRE(ends_with("hello world", "world") == true);
        REQUIRE(ends_with("hello world", "hello") == false);
    }

    SECTION("empty suffix") {
        REQUIRE(ends_with("hello", "") == true);
    }

    SECTION("suffix longer than string") {
        REQUIRE(ends_with("hi", "hello") == false);
    }

    SECTION("exact match") {
        REQUIRE(ends_with("hello", "hello") == true);
    }

    SECTION("empty string") {
        REQUIRE(ends_with("", "") == true);
        REQUIRE(ends_with("", "a") == false);
    }
}

TEST_CASE("string::split with char delimiter", "[utils][string]") {
    SECTION("basic split") {
        auto result = split("a,b,c", ',');

        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == "a");
        REQUIRE(result[1] == "b");
        REQUIRE(result[2] == "c");
    }

    SECTION("empty string") {
        auto result = split("", ',');

        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == "");
    }

    SECTION("delimiter not found") {
        auto result = split("abc", ',');

        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == "abc");
    }
}

TEST_CASE("string::join with char delimiter", "[utils][string]") {
    SECTION("basic join") {
        std::vector<std::string> parts = {"a", "b", "c"};
        auto result = join(parts, ',');

        REQUIRE(result == "a,b,c");
    }

    SECTION("empty vector") {
        std::vector<std::string> parts;
        auto result = join(parts, ',');

        REQUIRE(result == "");
    }

    SECTION("single element") {
        std::vector<std::string> parts = {"a"};
        auto result = join(parts, ',');

        REQUIRE(result == "a");
    }
}

// ============================================================================
// env_utils Tests
// ============================================================================

TEST_CASE("env_utils::get_env", "[utils][env]") {
    SECTION("existing variable returns value") {
        set_env("TURBOT_TEST_GET", "hello");
        REQUIRE(get_env("TURBOT_TEST_GET") == "hello");
        unset_env("TURBOT_TEST_GET");
    }

    SECTION("non-existing variable returns empty string") {
        unset_env("TURBOT_TEST_NONEXIST");
        REQUIRE(get_env("TURBOT_TEST_NONEXIST") == "");
    }

    SECTION("empty value variable returns empty string") {
        set_env("TURBOT_TEST_EMPTY", "");
        REQUIRE(get_env("TURBOT_TEST_EMPTY") == "");
        unset_env("TURBOT_TEST_EMPTY");
    }
}

TEST_CASE("env_utils::get_env_or", "[utils][env]") {
    SECTION("existing variable returns value") {
        set_env("TURBOT_TEST_OR", "world");
        REQUIRE(get_env_or("TURBOT_TEST_OR", "default") == "world");
        unset_env("TURBOT_TEST_OR");
    }

    SECTION("non-existing variable returns default") {
        unset_env("TURBOT_TEST_OR_MISSING");
        REQUIRE(get_env_or("TURBOT_TEST_OR_MISSING", "mydefault") == "mydefault");
    }
}

TEST_CASE("env_utils::set_env and has_env", "[utils][env]") {
    SECTION("set creates variable") {
        unset_env("TURBOT_TEST_SET");
        REQUIRE_FALSE(has_env("TURBOT_TEST_SET"));
        set_env("TURBOT_TEST_SET", "value123");
        REQUIRE(has_env("TURBOT_TEST_SET"));
        REQUIRE(get_env("TURBOT_TEST_SET") == "value123");
        unset_env("TURBOT_TEST_SET");
    }

    SECTION("set overwrites existing variable") {
        set_env("TURBOT_TEST_OVERWRITE", "first");
        set_env("TURBOT_TEST_OVERWRITE", "second");
        REQUIRE(get_env("TURBOT_TEST_OVERWRITE") == "second");
        unset_env("TURBOT_TEST_OVERWRITE");
    }
}

TEST_CASE("env_utils::unset_env", "[utils][env]") {
    SECTION("unset removes variable") {
        set_env("TURBOT_TEST_UNSET", "toremove");
        REQUIRE(has_env("TURBOT_TEST_UNSET"));
        unset_env("TURBOT_TEST_UNSET");
        REQUIRE_FALSE(has_env("TURBOT_TEST_UNSET"));
    }

    SECTION("unset non-existing variable is safe") {
        unset_env("TURBOT_TEST_UNSET_SAFE");
        REQUIRE_NOTHROW(unset_env("TURBOT_TEST_UNSET_SAFE"));
    }
}

TEST_CASE("env_utils::env_key_to_config_key", "[utils][env]") {
    SECTION("normal conversion strips prefix and lowercases") {
        // TURBOT_LOG_LEVEL -> log.level
        REQUIRE(env_key_to_config_key("TURBOT_LOG_LEVEL", "TURBOT_") == "log.level");
    }

    SECTION("nested key with multiple underscores") {
        REQUIRE(env_key_to_config_key("TURBOT_HTTP_TIMEOUT_MS", "TURBOT_") == "http.timeout.ms");
    }

    SECTION("key already lowercase") {
        REQUIRE(env_key_to_config_key("PREFIX_key_name", "PREFIX_") == "key.name");
    }

    SECTION("missing prefix returns empty string") {
        REQUIRE(env_key_to_config_key("OTHER_KEY", "TURBOT_") == "");
    }

    SECTION("key equal to prefix returns empty string") {
        REQUIRE(env_key_to_config_key("TURBOT_", "TURBOT_") == "");
    }

    SECTION("key shorter than prefix returns empty string") {
        REQUIRE(env_key_to_config_key("SHORT", "TURBOT_") == "");
    }

    SECTION("empty prefix returns empty string (prefix guard)") {
        // starts_with("") is always true, but size <= 0 guard applies
        REQUIRE(env_key_to_config_key("", "") == "");
    }
}

TEST_CASE("env_utils::resolve_env_refs", "[utils][env]") {
    SECTION("no references returns as-is") {
        REQUIRE(resolve_env_refs("plain string") == "plain string");
    }

    SECTION("${VAR} with set variable") {
        set_env("TURBOT_RESOLVE_VAR", "resolved");
        REQUIRE(resolve_env_refs("prefix_${TURBOT_RESOLVE_VAR}_suffix") == "prefix_resolved_suffix");
        unset_env("TURBOT_RESOLVE_VAR");
    }

    SECTION("${VAR} with unset variable uses empty string") {
        unset_env("TURBOT_RESOLVE_MISSING");
        REQUIRE(resolve_env_refs("x${TURBOT_RESOLVE_MISSING}y") == "xy");
    }

    SECTION("${VAR:-default} with unset variable uses default") {
        unset_env("TURBOT_RESOLVE_DEF");
        REQUIRE(resolve_env_refs("${TURBOT_RESOLVE_DEF:-fallback}") == "fallback");
    }

    SECTION("${VAR:-default} with set variable uses value") {
        set_env("TURBOT_RESOLVE_SET", "actual");
        REQUIRE(resolve_env_refs("${TURBOT_RESOLVE_SET:-fallback}") == "actual");
        unset_env("TURBOT_RESOLVE_SET");
    }

    SECTION("multiple references in one string") {
        set_env("TURBOT_A", "foo");
        set_env("TURBOT_B", "bar");
        REQUIRE(resolve_env_refs("${TURBOT_A}-${TURBOT_B}") == "foo-bar");
        unset_env("TURBOT_A");
        unset_env("TURBOT_B");
    }

    SECTION("empty string returns empty") {
        REQUIRE(resolve_env_refs("") == "");
    }
}