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

TEST_CASE("json::has_path", "[utils][json]") {
    SECTION("existing path") {
        nlohmann::json data = {
            {"a", {
                {"b", {
                    {"c", 42}
                }}
            }}
        };

        REQUIRE(has_path(data, "a.b.c") == true);
        REQUIRE(has_path(data, "a.b") == true);
        REQUIRE(has_path(data, "a") == true);
    }

    SECTION("non-existing path") {
        nlohmann::json data = {{"a", 1}};

        REQUIRE(has_path(data, "a.b.c") == false);
        REQUIRE(has_path(data, "x") == false);
    }

    SECTION("empty path") {
        nlohmann::json data = {{"a", 1}};

        REQUIRE(has_path(data, "") == true);
    }
}

TEST_CASE("json::clone", "[utils][json]") {
    SECTION("clone object") {
        nlohmann::json original = {
            {"name", "John"},
            {"age", 30}
        };

        auto cloned = clone(original);

        REQUIRE(cloned == original);

        // Modify clone, original should be unchanged
        cloned["name"] = "Jane";
        REQUIRE(original["name"] == "John");
        REQUIRE(cloned["name"] == "Jane");
    }

    SECTION("clone array") {
        nlohmann::json original = {1, 2, 3, 4, 5};
        auto cloned = clone(original);

        REQUIRE(cloned == original);
    }
}

TEST_CASE("json::equals", "[utils][json]") {
    SECTION("equal objects") {
        nlohmann::json a = {{"key", "value"}};
        nlohmann::json b = {{"key", "value"}};

        REQUIRE(equals(a, b) == true);
    }

    SECTION("different objects") {
        nlohmann::json a = {{"key", "value1"}};
        nlohmann::json b = {{"key", "value2"}};

        REQUIRE(equals(a, b) == false);
    }

    SECTION("different types") {
        nlohmann::json a = {{"key", "value"}};
        nlohmann::json b = {1, 2, 3};

        REQUIRE(equals(a, b) == false);
    }
}

TEST_CASE("json::remove_path", "[utils][json]") {
    SECTION("remove nested key") {
        nlohmann::json data = {
            {"a", {
                {"b", {
                    {"c", 42}
                }}
            }}
        };

        REQUIRE(remove_path(data, "a.b.c") == true);
        REQUIRE_FALSE(data["a"]["b"].contains("c"));
    }

    SECTION("remove non-existing path") {
        nlohmann::json data = {{"a", 1}};

        REQUIRE(remove_path(data, "x.y.z") == false);
    }

    SECTION("remove from array") {
        nlohmann::json data = {
            {"items", {1, 2, 3, 4, 5}}
        };

        REQUIRE(remove_path(data, "items[2]") == true);
        REQUIRE(data["items"].size() == 4);
        REQUIRE(data["items"][2] == 4);
    }

    SECTION("empty path") {
        nlohmann::json data = {{"a", 1}};
        REQUIRE(remove_path(data, "") == false);
    }
}

TEST_CASE("json::pretty_print", "[utils][json]") {
    SECTION("print with indent") {
        nlohmann::json data = {{"name", "John"}, {"age", 30}};
        std::string result = pretty_print(data, 2);

        REQUIRE(result.find("\n") != std::string::npos);
        REQUIRE(result.find("  ") != std::string::npos);  // 2-space indent
    }

    SECTION("print with default indent") {
        nlohmann::json data = {{"key", "value"}};
        std::string result = pretty_print(data);

        REQUIRE(result.find("\n") != std::string::npos);
    }
}

TEST_CASE("json::split_path", "[utils][json]") {
    SECTION("simple path") {
        auto parts = split_path("a.b.c");

        REQUIRE(parts.size() == 3);
        REQUIRE(parts[0] == "a");
        REQUIRE(parts[1] == "b");
        REQUIRE(parts[2] == "c");
    }

    SECTION("path with array index") {
        auto parts = split_path("arr[0].field");

        REQUIRE(parts.size() == 3);
        REQUIRE(parts[0] == "arr");
        REQUIRE(parts[1] == "[0]");
        REQUIRE(parts[2] == "field");
    }

    SECTION("empty path") {
        auto parts = split_path("");
        REQUIRE(parts.empty());
    }
}

TEST_CASE("json::get_or", "[utils][json]") {
    SECTION("existing key returns value") {
        nlohmann::json data = {{"name", "John"}, {"age", 30}};

        REQUIRE(get_or<std::string>(data, "name", "default") == "John");
        REQUIRE(get_or<int>(data, "age", 0) == 30);
    }

    SECTION("non-existing key returns default") {
        nlohmann::json data = {{"name", "John"}};

        REQUIRE(get_or<std::string>(data, "missing", "default") == "default");
        REQUIRE(get_or<int>(data, "age", 99) == 99);
    }
}

TEST_CASE("json::validate_schema advanced", "[utils][json]") {
    SECTION("integer type") {
        nlohmann::json schema = R"({"type": "integer"})"_json;

        REQUIRE(validate_schema(42, schema) == true);
        REQUIRE(validate_schema(3.14, schema) == false);  // float is not integer
        REQUIRE(validate_schema("42", schema) == false);
    }

    SECTION("boolean type") {
        nlohmann::json schema = R"({"type": "boolean"})"_json;

        REQUIRE(validate_schema(true, schema) == true);
        REQUIRE(validate_schema(false, schema) == true);
        REQUIRE(validate_schema("true", schema) == false);
    }

    SECTION("null type") {
        nlohmann::json schema = R"({"type": "null"})"_json;

        REQUIRE(validate_schema(nullptr, schema) == true);
        REQUIRE(validate_schema(0, schema) == false);
    }

    SECTION("enum validation") {
        nlohmann::json schema = R"({
            "enum": ["red", "green", "blue"]
        })"_json;

        REQUIRE(validate_schema("red", schema) == true);
        REQUIRE(validate_schema("yellow", schema) == false);
        REQUIRE(validate_schema(42, schema) == false);
    }

    SECTION("array type") {
        nlohmann::json schema = R"({"type": "array"})"_json;

        REQUIRE(validate_schema({1, 2, 3}, schema) == true);
        REQUIRE(validate_schema("not array", schema) == false);
    }

    SECTION("invalid schema type") {
        nlohmann::json schema = "invalid";  // Not an object
        nlohmann::json data = {{"key", "value"}};

        REQUIRE(validate_schema(data, schema) == false);
    }
}

TEST_CASE("json::query advanced", "[utils][json]") {
    SECTION("nested array index") {
        nlohmann::json data = {
            {"matrix", {
                {1, 2, 3},
                {4, 5, 6}
            }}
        };

        auto result = query(data, "matrix[0][1]");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == 2);
    }

    SECTION("out of range index") {
        nlohmann::json data = {{"arr", {1, 2, 3}}};

        auto result = query(data, "arr[10]");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("invalid index format") {
        nlohmann::json data = {{"arr", {1, 2, 3}}};

        auto result = query(data, "arr[abc]");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("index on non-array") {
        nlohmann::json data = {{"obj", {{"key", "value"}}}};

        auto result = query(data, "obj[0]");
        REQUIRE_FALSE(result.has_value());
    }
}