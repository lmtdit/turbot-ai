#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <turbot/utils/json_utils.hpp>
#include <nlohmann/json.hpp>

using namespace turbot::utils::json;

TEST_CASE("json::validate_schema", "[utils][json]") {
    SECTION("valid schema") {
        nlohmann::json schema = R"({
            "type": "object",
            "properties": {
                "name": {"type": "string"},
                "age": {"type": "number"}
            },
            "required": ["name"]
        })"_json;

        nlohmann::json valid_data = {
            {"name", "John"},
            {"age", 30}
        };

        REQUIRE(validate_schema(valid_data, schema) == true);
    }

    SECTION("invalid schema - missing required field") {
        nlohmann::json schema = R"({
            "type": "object",
            "properties": {
                "name": {"type": "string"},
                "age": {"type": "number"}
            },
            "required": ["name"]
        })"_json;

        nlohmann::json invalid_data = {
            {"age", 30}
        };

        REQUIRE(validate_schema(invalid_data, schema) == false);
    }

    SECTION("empty schema always valid") {
        nlohmann::json schema = nlohmann::json::object();
        nlohmann::json data = {
            {"any", "data"}
        };

        REQUIRE(validate_schema(data, schema) == true);
    }
}

TEST_CASE("json::merge", "[utils][json]") {
    SECTION("simple merge") {
        nlohmann::json a = {{"a", 1}, {"b", 2}};
        nlohmann::json b = {{"b", 3}, {"c", 4}};

        auto result = merge(a, b);

        REQUIRE(result["a"] == 1);
        REQUIRE(result["b"] == 3);  // b被覆盖
        REQUIRE(result["c"] == 4);
    }

    SECTION("nested merge") {
        nlohmann::json a = {
            {"a", 1},
            {"nested", {{"x", 10}, {"y", 20}}}
        };
        nlohmann::json b = {
            {"b", 2},
            {"nested", {{"y", 30}, {"z", 40}}}
        };

        auto result = merge(a, b);

        REQUIRE(result["a"] == 1);
        REQUIRE(result["b"] == 2);
        REQUIRE(result["nested"]["x"] == 10);
        REQUIRE(result["nested"]["y"] == 30);  // nested.y被覆盖
        REQUIRE(result["nested"]["z"] == 40);
    }

    SECTION("array replace") {
        nlohmann::json a = {{"arr", {1, 2, 3}}};
        nlohmann::json b = {{"arr", {4, 5}}};

        auto result = merge(a, b);

        REQUIRE(result["arr"] == nlohmann::json({4, 5}));  // 数组被替换
    }
}

TEST_CASE("json::query", "[utils][json]") {
    SECTION("simple path") {
        nlohmann::json data = {
            {"a", {
                {"b", {
                    {"c", 42}
                }}
            }}
        };

        auto result = query(data, "a.b.c");

        REQUIRE(result.has_value());
        REQUIRE(result.value() == 42);
    }

    SECTION("array index") {
        nlohmann::json data = {
            {"arr", {10, 20, 30}}
        };

        auto result = query(data, "arr[1]");

        REQUIRE(result.has_value());
        REQUIRE(result.value() == 20);
    }

    SECTION("invalid path") {
        nlohmann::json data = {
            {"a", 1}
        };

        auto result = query(data, "a.b.c");

        REQUIRE_FALSE(result.has_value());
    }

    SECTION("empty path returns root") {
        nlohmann::json data = {{"a", 1}};
        auto result = query(data, "");

        REQUIRE(result.has_value());
        REQUIRE(result.value() == data);
    }
}

TEST_CASE("json::get_safe", "[utils][json]") {
    SECTION("existing key") {
        nlohmann::json data = {{"name", "John"}};

        auto result = get_safe<std::string>(data, "name");

        REQUIRE(result.has_value());
        REQUIRE(result.value() == "John");
    }

    SECTION("non-existing key") {
        nlohmann::json data = {{"name", "John"}};

        auto result = get_safe<std::string>(data, "age");

        REQUIRE_FALSE(result.has_value());
    }

    SECTION("type mismatch") {
        nlohmann::json data = {{"age", "30"}};  // string instead of int

        auto result = get_safe<int>(data, "age");

        REQUIRE_FALSE(result.has_value());
    }

    SECTION("get with default value") {
        nlohmann::json data = {{"name", "John"}};

        auto age = get_safe<int>(data, "age");
        REQUIRE_FALSE(age.has_value());

        // 使用可选值的.value_or()方法
        auto default_age = age.value_or(0);
        REQUIRE(default_age == 0);
    }
}