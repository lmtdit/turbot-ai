#include <catch2/catch_test_macros.hpp>
#include <turbot/utils/json_utils.hpp>
#include <nlohmann/json.hpp>

using namespace turbot::utils::json;
using json_t = nlohmann::json;

// ==================== validate_schema Tests ====================

TEST_CASE("JsonUtils.ValidateSchema.ValidObject", "[Utils][Json]") {
    json_t data = {{"name", "John"}, {"age", 30}};
    json_t schema = {
        {"type", "object"},
        {"properties", {
            {"name", {{"type", "string"}}},
            {"age", {{"type", "integer"}}}
        }},
        {"required", {"name"}}
    };
    
    REQUIRE(validate_schema(data, schema));
}

TEST_CASE("JsonUtils.ValidateSchema.InvalidType", "[Utils][Json]") {
    json_t data = {{"name", 123}};  // Should be string
    json_t schema = {
        {"type", "object"},
        {"properties", {
            {"name", {{"type", "string"}}}
        }}
    };
    
    REQUIRE_FALSE(validate_schema(data, schema));
}

TEST_CASE("JsonUtils.ValidateSchema.MissingRequired", "[Utils][Json]") {
    json_t data = {{"age", 30}};
    json_t schema = {
        {"type", "object"},
        {"properties", {
            {"name", {{"type", "string"}}}
        }},
        {"required", {"name"}}
    };
    
    REQUIRE_FALSE(validate_schema(data, schema));
}

TEST_CASE("JsonUtils.ValidateSchema.Array", "[Utils][Json]") {
    json_t data = json_t::array({1, 2, 3});
    json_t schema = {
        {"type", "array"},
        {"items", {{"type", "integer"}}}
    };
    
    REQUIRE(validate_schema(data, schema));
}

TEST_CASE("JsonUtils.ValidateSchema.EmptySchema", "[Utils][Json]") {
    json_t data = {{"key", "value"}};
    json_t schema = json_t::object();
    
    // Empty schema should accept anything
    REQUIRE(validate_schema(data, schema));
}

// ==================== merge Tests ====================

TEST_CASE("JsonUtils.Merge.Basic", "[Utils][Json]") {
    json_t a = {{"key1", "value1"}};
    json_t b = {{"key2", "value2"}};
    
    json_t result = merge(a, b);
    
    REQUIRE(result["key1"] == "value1");
    REQUIRE(result["key2"] == "value2");
}

TEST_CASE("JsonUtils.Merge.Override", "[Utils][Json]") {
    json_t a = {{"key", "original"}};
    json_t b = {{"key", "updated"}};
    
    json_t result = merge(a, b);
    
    REQUIRE(result["key"] == "updated");
}

TEST_CASE("JsonUtils.Merge.DeepMerge", "[Utils][Json]") {
    json_t a = {{"nested", {{"key1", "value1"}}}};
    json_t b = {{"nested", {{"key2", "value2"}}}};
    
    json_t result = merge(a, b);
    
    REQUIRE(result["nested"]["key1"] == "value1");
    REQUIRE(result["nested"]["key2"] == "value2");
}

TEST_CASE("JsonUtils.Merge.EmptyObjects", "[Utils][Json]") {
    json_t a = json_t::object();
    json_t b = json_t::object();
    
    json_t result = merge(a, b);
    
    REQUIRE(result.is_object());
    REQUIRE(result.empty());
}

TEST_CASE("JsonUtils.Merge.WithArray", "[Utils][Json]") {
    json_t a = {{"arr", json_t::array({1, 2})}};
    json_t b = {{"arr", json_t::array({3, 4})}};
    
    json_t result = merge(a, b);
    
    // Arrays are replaced, not concatenated
    REQUIRE(result["arr"] == json_t::array({3, 4}));
}

// ==================== query Tests ====================

TEST_CASE("JsonUtils.Query.SimpleKey", "[Utils][Json]") {
    json_t data = {{"name", "John"}};
    
    auto result = query(data, "name");
    
    REQUIRE(result.has_value());
    REQUIRE(result.value() == "John");
}

TEST_CASE("JsonUtils.Query.NestedPath", "[Utils][Json]") {
    json_t data = {
        {"user", {
            {"profile", {
                {"name", "John"}
            }}
        }}
    };
    
    auto result = query(data, "user.profile.name");
    
    REQUIRE(result.has_value());
    REQUIRE(result.value() == "John");
}

TEST_CASE("JsonUtils.Query.ArrayIndex", "[Utils][Json]") {
    json_t data = {
        {"items", json_t::array({1, 2, 3})}
    };
    
    auto result = query(data, "items[1]");
    
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 2);
}

TEST_CASE("JsonUtils.Query.NonExistentKey", "[Utils][Json]") {
    json_t data = {{"name", "John"}};
    
    auto result = query(data, "nonexistent");
    
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("JsonUtils.Query.NonExistentPath", "[Utils][Json]") {
    json_t data = {{"name", "John"}};
    
    auto result = query(data, "user.profile.name");
    
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("JsonUtils.Query.InvalidArrayIndex", "[Utils][Json]") {
    json_t data = {{"items", json_t::array({1, 2, 3})}};
    
    auto result = query(data, "items[10]");
    
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("JsonUtils.Query.MixedPath", "[Utils][Json]") {
    json_t data = {
        {"users", json_t::array({
            {{"name", "John"}},
            {{"name", "Jane"}}
        })}
    };
    
    auto result = query(data, "users[1].name");
    
    REQUIRE(result.has_value());
    REQUIRE(result.value() == "Jane");
}

// ==================== get_safe Tests ====================

TEST_CASE("JsonUtils.GetSafe.Existing", "[Utils][Json]") {
    json_t data = {{"key", "value"}};
    
    auto result = get_safe<std::string>(data, "key");
    
    REQUIRE(result.has_value());
    REQUIRE(result.value() == "value");
}

TEST_CASE("JsonUtils.GetSafe.NonExistent", "[Utils][Json]") {
    json_t data = {{"key", "value"}};
    
    auto result = get_safe<std::string>(data, "nonexistent");
    
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("JsonUtils.GetSafe.TypeMismatch", "[Utils][Json]") {
    json_t data = {{"key", 123}};
    
    auto result = get_safe<std::string>(data, "key");
    
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("JsonUtils.GetSafe.Integer", "[Utils][Json]") {
    json_t data = {{"count", 42}};
    
    auto result = get_safe<int>(data, "count");
    
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 42);
}

TEST_CASE("JsonUtils.GetSafe.Boolean", "[Utils][Json]") {
    json_t data = {{"enabled", true}};
    
    auto result = get_safe<bool>(data, "enabled");
    
    REQUIRE(result.has_value());
    REQUIRE(result.value() == true);
}

// ==================== get_or Tests ====================

TEST_CASE("JsonUtils.GetOr.Existing", "[Utils][Json]") {
    json_t data = {{"key", "value"}};
    
    std::string result = get_or(data, "key", std::string("default"));
    
    REQUIRE(result == "value");
}

TEST_CASE("JsonUtils.GetOr.NonExistent", "[Utils][Json]") {
    json_t data = {{"key", "value"}};
    
    std::string result = get_or(data, "nonexistent", std::string("default"));
    
    REQUIRE(result == "default");
}

TEST_CASE("JsonUtils.GetOr.Integer", "[Utils][Json]") {
    json_t data = {{"count", 42}};
    
    int result = get_or(data, "count", 0);
    
    REQUIRE(result == 42);
}

TEST_CASE("JsonUtils.GetOr.DefaultInteger", "[Utils][Json]") {
    json_t data = json_t::object();
    
    int result = get_or(data, "missing", -1);
    
    REQUIRE(result == -1);
}

// ==================== split_path Tests ====================

TEST_CASE("JsonUtils.SplitPath.Simple", "[Utils][Json]") {
    std::vector<std::string> parts = split_path("key");
    
    REQUIRE(parts.size() == 1);
    REQUIRE(parts[0] == "key");
}

TEST_CASE("JsonUtils.SplitPath.Nested", "[Utils][Json]") {
    std::vector<std::string> parts = split_path("a.b.c");
    
    REQUIRE(parts.size() == 3);
    REQUIRE(parts[0] == "a");
    REQUIRE(parts[1] == "b");
    REQUIRE(parts[2] == "c");
}

TEST_CASE("JsonUtils.SplitPath.WithArrayIndex", "[Utils][Json]") {
    std::vector<std::string> parts = split_path("items[0]");
    
    REQUIRE(parts.size() == 2);
    REQUIRE(parts[0] == "items");
    REQUIRE(parts[1] == "[0]");
}

TEST_CASE("JsonUtils.SplitPath.Complex", "[Utils][Json]") {
    std::vector<std::string> parts = split_path("users[1].profile.name");
    
    REQUIRE(parts.size() == 4);
    REQUIRE(parts[0] == "users");
    REQUIRE(parts[1] == "[1]");
    REQUIRE(parts[2] == "profile");
    REQUIRE(parts[3] == "name");
}

TEST_CASE("JsonUtils.SplitPath.Empty", "[Utils][Json]") {
    std::vector<std::string> parts = split_path("");
    
    REQUIRE(parts.empty());
}

// ==================== has_path Tests ====================

TEST_CASE("JsonUtils.HasPath.Existing", "[Utils][Json]") {
    json_t data = {{"key", "value"}};
    
    REQUIRE(has_path(data, "key"));
}

TEST_CASE("JsonUtils.HasPath.NestedExisting", "[Utils][Json]") {
    json_t data = {{"user", {{"name", "John"}}}};
    
    REQUIRE(has_path(data, "user.name"));
}

TEST_CASE("JsonUtils.HasPath.NonExistent", "[Utils][Json]") {
    json_t data = {{"key", "value"}};
    
    REQUIRE_FALSE(has_path(data, "nonexistent"));
}

TEST_CASE("JsonUtils.HasPath.ArrayIndex", "[Utils][Json]") {
    json_t data = {{"items", json_t::array({1, 2, 3})}};
    
    REQUIRE(has_path(data, "items[0]"));
    REQUIRE_FALSE(has_path(data, "items[10]"));
}

// ==================== clone Tests ====================

TEST_CASE("JsonUtils.Clone.Object", "[Utils][Json]") {
    json_t original = {{"key", "value"}, {"nested", {{"num", 42}}}};
    
    json_t cloned = clone(original);
    
    REQUIRE(cloned == original);
    
    // Modify original, cloned should be unchanged
    original["key"] = "modified";
    REQUIRE(cloned["key"] == "value");
}

TEST_CASE("JsonUtils.Clone.Array", "[Utils][Json]") {
    json_t original = json_t::array({1, 2, 3});
    
    json_t cloned = clone(original);
    
    REQUIRE(cloned == original);
}

TEST_CASE("JsonUtils.Clone.DeepCopy", "[Utils][Json]") {
    json_t original = {
        {"nested", {
            {"deep", {
                {"value", 123}
            }}
        }}
    };
    
    json_t cloned = clone(original);
    
    // Modify deeply nested value in original
    original["nested"]["deep"]["value"] = 456;
    
    REQUIRE(cloned["nested"]["deep"]["value"] == 123);
}

// ==================== equals Tests ====================

TEST_CASE("JsonUtils.Equals.SameObject", "[Utils][Json]") {
    json_t a = {{"key", "value"}};
    json_t b = {{"key", "value"}};
    
    REQUIRE(equals(a, b));
}

TEST_CASE("JsonUtils.Equals.DifferentValues", "[Utils][Json]") {
    json_t a = {{"key", "value1"}};
    json_t b = {{"key", "value2"}};
    
    REQUIRE_FALSE(equals(a, b));
}

TEST_CASE("JsonUtils.Equals.DifferentKeys", "[Utils][Json]") {
    json_t a = {{"key1", "value"}};
    json_t b = {{"key2", "value"}};
    
    REQUIRE_FALSE(equals(a, b));
}

TEST_CASE("JsonUtils.Equals.Arrays", "[Utils][Json]") {
    json_t a = json_t::array({1, 2, 3});
    json_t b = json_t::array({1, 2, 3});
    json_t c = json_t::array({1, 2, 4});
    
    REQUIRE(equals(a, b));
    REQUIRE_FALSE(equals(a, c));
}

TEST_CASE("JsonUtils.Equals.NullAndEmpty", "[Utils][Json]") {
    json_t null_json = json_t(nullptr);
    json_t empty_obj = json_t::object();
    
    REQUIRE_FALSE(equals(null_json, empty_obj));
}

// ==================== remove_path Tests ====================

TEST_CASE("JsonUtils.RemovePath.SimpleKey", "[Utils][Json]") {
    json_t data = {{"key1", "value1"}, {"key2", "value2"}};
    
    bool removed = remove_path(data, "key1");
    
    REQUIRE(removed);
    REQUIRE_FALSE(data.contains("key1"));
    REQUIRE(data.contains("key2"));
}

TEST_CASE("JsonUtils.RemovePath.NestedPath", "[Utils][Json]") {
    json_t data = {
        {"user", {
            {"name", "John"},
            {"age", 30}
        }}
    };
    
    bool removed = remove_path(data, "user.name");
    
    REQUIRE(removed);
    REQUIRE_FALSE(data["user"].contains("name"));
    REQUIRE(data["user"].contains("age"));
}

TEST_CASE("JsonUtils.RemovePath.NonExistent", "[Utils][Json]") {
    json_t data = {{"key", "value"}};
    
    bool removed = remove_path(data, "nonexistent");
    
    REQUIRE_FALSE(removed);
}

TEST_CASE("JsonUtils.RemovePath.ArrayElement", "[Utils][Json]") {
    json_t data = {{"items", json_t::array({1, 2, 3})}};
    
    bool removed = remove_path(data, "items[1]");
    
    REQUIRE(removed);
    REQUIRE(data["items"] == json_t::array({1, 3}));
}

// ==================== pretty_print Tests ====================

TEST_CASE("JsonUtils.PrettyPrint.Basic", "[Utils][Json]") {
    json_t data = {{"key", "value"}};
    
    std::string pretty = pretty_print(data);
    
    REQUIRE(pretty.find("\"key\"") != std::string::npos);
    REQUIRE(pretty.find("\"value\"") != std::string::npos);
}

TEST_CASE("JsonUtils.PrettyPrint.CustomIndent", "[Utils][Json]") {
    json_t data = {{"key", "value"}};
    
    std::string pretty2 = pretty_print(data, 2);
    std::string pretty4 = pretty_print(data, 4);
    
    // 4-space indent should have more spaces
    REQUIRE(pretty4.find("    \"key\"") != std::string::npos);
}

TEST_CASE("JsonUtils.PrettyPrint.Array", "[Utils][Json]") {
    json_t data = json_t::array({1, 2, 3});
    
    std::string pretty = pretty_print(data);
    
    REQUIRE(pretty.find("[") != std::string::npos);
    REQUIRE(pretty.find("]") != std::string::npos);
}

TEST_CASE("JsonUtils.PrettyPrint.Empty", "[Utils][Json]") {
    json_t data = json_t::object();
    
    std::string pretty = pretty_print(data);
    
    REQUIRE(pretty.find("{}") != std::string::npos);
}
