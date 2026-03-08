#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <turbot/core/llm/tool_schema.hpp>

using namespace turbot::core::llm;

TEST_CASE("SchemaType conversion", "[core][llm][tool_schema][schema_type]") {
    SECTION("to_string") {
        REQUIRE(schema_type_to_string(SchemaType::String) == "string");
        REQUIRE(schema_type_to_string(SchemaType::Number) == "number");
        REQUIRE(schema_type_to_string(SchemaType::Integer) == "integer");
        REQUIRE(schema_type_to_string(SchemaType::Boolean) == "boolean");
        REQUIRE(schema_type_to_string(SchemaType::Object) == "object");
        REQUIRE(schema_type_to_string(SchemaType::Array) == "array");
        REQUIRE(schema_type_to_string(SchemaType::Null) == "null");
    }

    SECTION("from_string") {
        REQUIRE(string_to_schema_type("string") == SchemaType::String);
        REQUIRE(string_to_schema_type("number") == SchemaType::Number);
        REQUIRE(string_to_schema_type("integer") == SchemaType::Integer);
        REQUIRE(string_to_schema_type("boolean") == SchemaType::Boolean);
        REQUIRE(string_to_schema_type("object") == SchemaType::Object);
        REQUIRE(string_to_schema_type("array") == SchemaType::Array);
        REQUIRE(string_to_schema_type("null") == SchemaType::Null);
    }

    SECTION("invalid string throws") {
        REQUIRE_THROWS_AS(string_to_schema_type("invalid"), std::invalid_argument);
    }
}

TEST_CASE("ParameterSchema", "[core][llm][tool_schema][parameter_schema]") {
    SECTION("basic to_json_schema") {
        ParameterSchema schema;
        schema.type = SchemaType::String;
        schema.description = "A test parameter";

        nlohmann::json j = schema.to_json_schema();
        
        REQUIRE(j["type"] == "string");
        REQUIRE(j["description"] == "A test parameter");
    }

    SECTION("number with constraints") {
        ParameterSchema schema;
        schema.type = SchemaType::Number;
        schema.description = "A number parameter";
        schema.minimum = 0.0;
        schema.maximum = 100.0;

        nlohmann::json j = schema.to_json_schema();
        
        REQUIRE(j["type"] == "number");
        REQUIRE(j["minimum"] == Catch::Approx(0.0));
        REQUIRE(j["maximum"] == Catch::Approx(100.0));
    }

    SECTION("string with constraints") {
        ParameterSchema schema;
        schema.type = SchemaType::String;
        schema.min_length = 1;
        schema.max_length = 100;
        schema.pattern = "^[a-z]+$";

        nlohmann::json j = schema.to_json_schema();
        
        REQUIRE(j["minLength"] == 1);
        REQUIRE(j["maxLength"] == 100);
        REQUIRE(j["pattern"] == "^[a-z]+$");
    }

    SECTION("enum values") {
        ParameterSchema schema;
        schema.type = SchemaType::String;
        schema.enum_values = nlohmann::json::array({"option1", "option2", "option3"});

        nlohmann::json j = schema.to_json_schema();
        
        REQUIRE(j["enum"].is_array());
        REQUIRE(j["enum"].size() == 3);
        REQUIRE(j["enum"][0] == "option1");
    }

    SECTION("round trip") {
        ParameterSchema original;
        original.type = SchemaType::Integer;
        original.description = "Test integer";
        original.minimum = 1;
        original.maximum = 10;

        nlohmann::json j = original.to_json_schema();
        ParameterSchema restored = ParameterSchema::from_json_schema(j);
        
        REQUIRE(restored.type == SchemaType::Integer);
        REQUIRE(restored.description == "Test integer");
        REQUIRE(restored.minimum == Catch::Approx(1.0));
        REQUIRE(restored.maximum == Catch::Approx(10.0));
    }
}

TEST_CASE("ToolSchema builder", "[core][llm][tool_schema][builder]") {
    SECTION("basic construction") {
        ToolSchema schema("test_tool", "A test tool");
        
        REQUIRE(schema.name() == "test_tool");
        REQUIRE(schema.description() == "A test tool");
    }

    SECTION("fluent interface") {
        auto schema = ToolSchema()
            .set_name("fluent_tool")
            .set_description("Built with fluent interface");
        
        REQUIRE(schema.name() == "fluent_tool");
        REQUIRE(schema.description() == "Built with fluent interface");
    }

    SECTION("add_string_param") {
        ToolSchema schema("test", "test");
        schema.add_string_param("name", "The name parameter", true);
        
        REQUIRE(schema.parameters().count("name") == 1);
        REQUIRE(schema.parameters().at("name").type == SchemaType::String);
        REQUIRE(schema.required_params().size() == 1);
        REQUIRE(schema.required_params()[0] == "name");
    }

    SECTION("add_number_param") {
        ToolSchema schema("test", "test");
        schema.add_number_param("count", "The count", true, std::nullopt, 0.0, 100.0);
        
        REQUIRE(schema.parameters().count("count") == 1);
        REQUIRE(schema.parameters().at("count").type == SchemaType::Number);
        REQUIRE(schema.parameters().at("count").minimum == Catch::Approx(0.0));
        REQUIRE(schema.parameters().at("count").maximum == Catch::Approx(100.0));
    }

    SECTION("add_integer_param") {
        ToolSchema schema("test", "test");
        schema.add_integer_param("id", "The ID", true, std::nullopt, 1, 9999);
        
        REQUIRE(schema.parameters().count("id") == 1);
        REQUIRE(schema.parameters().at("id").type == SchemaType::Integer);
    }

    SECTION("add_boolean_param") {
        ToolSchema schema("test", "test");
        schema.add_boolean_param("enabled", "Enable flag", true);
        
        REQUIRE(schema.parameters().count("enabled") == 1);
        REQUIRE(schema.parameters().at("enabled").type == SchemaType::Boolean);
    }

    SECTION("add_enum_param") {
        ToolSchema schema("test", "test");
        schema.add_enum_param("status", "Status value", {"active", "inactive", "pending"}, true);
        
        REQUIRE(schema.parameters().count("status") == 1);
        REQUIRE(schema.parameters().at("status").enum_values.has_value());
        REQUIRE(schema.parameters().at("status").enum_values->size() == 3);
    }

    SECTION("optional parameter") {
        ToolSchema schema("test", "test");
        schema.add_string_param("optional_param", "Optional", false);
        
        REQUIRE(schema.parameters().count("optional_param") == 1);
        REQUIRE(schema.required_params().empty());
    }
}

TEST_CASE("ToolSchema output formats", "[core][llm][tool_schema][output]") {
    ToolSchema schema("read_file", "Read a file from disk");
    schema.add_string_param("path", "The file path to read", true);
    schema.add_integer_param("lines", "Number of lines to read", false);

    SECTION("OpenAI format") {
        nlohmann::json j = schema.to_openai_tool();
        
        REQUIRE(j["type"] == "function");
        REQUIRE(j["function"]["name"] == "read_file");
        REQUIRE(j["function"]["description"] == "Read a file from disk");
        REQUIRE(j["function"]["parameters"]["type"] == "object");
        REQUIRE(j["function"]["parameters"]["properties"]["path"]["type"] == "string");
        REQUIRE(j["function"]["parameters"]["properties"]["lines"]["type"] == "integer");
        REQUIRE(j["function"]["parameters"]["required"].is_array());
        REQUIRE(j["function"]["parameters"]["required"][0] == "path");
    }

    SECTION("Anthropic format") {
        nlohmann::json j = schema.to_anthropic_tool();
        
        REQUIRE(j["name"] == "read_file");
        REQUIRE(j["description"] == "Read a file from disk");
        REQUIRE(j["input_schema"]["type"] == "object");
        REQUIRE(j["input_schema"]["properties"]["path"]["type"] == "string");
    }

    SECTION("to_json returns OpenAI format") {
        nlohmann::json j = schema.to_json();
        
        REQUIRE(j["type"] == "function");
        REQUIRE(j["function"]["name"] == "read_file");
    }
}

TEST_CASE("ToolSchema from_json", "[core][llm][tool_schema][deserialization]") {
    SECTION("from OpenAI format") {
        nlohmann::json j = {
            {"type", "function"},
            {"function", {
                {"name", "test_tool"},
                {"description", "A test tool"},
                {"parameters", {
                    {"type", "object"},
                    {"properties", {
                        {"arg1", {{"type", "string"}, {"description", "First arg"}}},
                        {"arg2", {{"type", "integer"}, {"description", "Second arg"}}}
                    }},
                    {"required", {"arg1"}}
                }}
            }}
        };

        ToolSchema schema = ToolSchema::from_json(j);
        
        REQUIRE(schema.name() == "test_tool");
        REQUIRE(schema.description() == "A test tool");
        REQUIRE(schema.parameters().size() == 2);
        REQUIRE(schema.required_params().size() == 1);
        REQUIRE(schema.required_params()[0] == "arg1");
    }

    SECTION("from Anthropic format") {
        nlohmann::json j = {
            {"name", "anthropic_tool"},
            {"description", "Anthropic format tool"},
            {"input_schema", {
                {"type", "object"},
                {"properties", {
                    {"input", {{"type", "string"}, {"description", "Input value"}}}
                }},
                {"required", {"input"}}
            }}
        };

        ToolSchema schema = ToolSchema::from_json(j);
        
        REQUIRE(schema.name() == "anthropic_tool");
        REQUIRE(schema.description() == "Anthropic format tool");
        REQUIRE(schema.parameters().count("input") == 1);
    }

    SECTION("from_tool_definition success") {
        nlohmann::json j = {
            {"type", "function"},
            {"function", {
                {"name", "valid_tool"},
                {"description", "Valid tool"},
                {"parameters", {{"type", "object"}, {"properties", nlohmann::json::object()}}}
            }}
        };

        auto schema = ToolSchema::from_tool_definition(j);
        REQUIRE(schema.has_value());
        REQUIRE(schema->name() == "valid_tool");
    }
}

TEST_CASE("schema_utils validation", "[core][llm][tool_schema][validation]") {
    SECTION("validate string type") {
        ParameterSchema schema;
        schema.type = SchemaType::String;
        
        REQUIRE(schema_utils::validate_against_schema("hello", schema));
        REQUIRE_FALSE(schema_utils::validate_against_schema(123, schema));
    }

    SECTION("validate number type") {
        ParameterSchema schema;
        schema.type = SchemaType::Number;
        
        REQUIRE(schema_utils::validate_against_schema(3.14, schema));
        REQUIRE(schema_utils::validate_against_schema(42, schema)); // int converts to number
        REQUIRE_FALSE(schema_utils::validate_against_schema("string", schema));
    }

    SECTION("validate integer type") {
        ParameterSchema schema;
        schema.type = SchemaType::Integer;
        
        REQUIRE(schema_utils::validate_against_schema(42, schema));
        REQUIRE_FALSE(schema_utils::validate_against_schema(3.14, schema));
        REQUIRE_FALSE(schema_utils::validate_against_schema("string", schema));
    }

    SECTION("validate boolean type") {
        ParameterSchema schema;
        schema.type = SchemaType::Boolean;
        
        REQUIRE(schema_utils::validate_against_schema(true, schema));
        REQUIRE(schema_utils::validate_against_schema(false, schema));
        REQUIRE_FALSE(schema_utils::validate_against_schema("true", schema));
    }

    SECTION("validate with enum") {
        ParameterSchema schema;
        schema.type = SchemaType::String;
        schema.enum_values = nlohmann::json::array({"a", "b", "c"});
        
        REQUIRE(schema_utils::validate_against_schema("a", schema));
        REQUIRE(schema_utils::validate_against_schema("b", schema));
        REQUIRE_FALSE(schema_utils::validate_against_schema("d", schema));
    }

    SECTION("validate with range") {
        ParameterSchema schema;
        schema.type = SchemaType::Number;
        schema.minimum = 0.0;
        schema.maximum = 100.0;
        
        REQUIRE(schema_utils::validate_against_schema(50.0, schema));
        REQUIRE(schema_utils::validate_against_schema(0.0, schema));
        REQUIRE(schema_utils::validate_against_schema(100.0, schema));
        REQUIRE_FALSE(schema_utils::validate_against_schema(-1.0, schema));
        REQUIRE_FALSE(schema_utils::validate_against_schema(101.0, schema));
    }

    SECTION("validate with string length") {
        ParameterSchema schema;
        schema.type = SchemaType::String;
        schema.min_length = 1;
        schema.max_length = 10;
        
        REQUIRE(schema_utils::validate_against_schema("hello", schema));
        REQUIRE_FALSE(schema_utils::validate_against_schema("", schema));
        REQUIRE_FALSE(schema_utils::validate_against_schema("this is too long", schema));
    }

    SECTION("get_validation_error") {
        ParameterSchema schema;
        schema.type = SchemaType::Integer;
        
        std::string error = schema_utils::get_validation_error("not_an_int", schema);
        REQUIRE_FALSE(error.empty());
    }
}

TEST_CASE("Complex tool schema", "[core][llm][tool_schema][complex]") {
    SECTION("complete tool definition") {
        ToolSchema schema("execute_code", "Execute code in a sandbox");
        schema.add_string_param("language", "Programming language", true)
               .add_string_param("code", "Code to execute", true)
               .add_integer_param("timeout", "Timeout in seconds", false, std::nullopt, 1, 300)
               .add_boolean_param("verbose", "Enable verbose output", false, false);

        nlohmann::json j = schema.to_openai_tool();
        
        REQUIRE(j["function"]["name"] == "execute_code");
        REQUIRE(j["function"]["parameters"]["properties"]["language"]["type"] == "string");
        REQUIRE(j["function"]["parameters"]["properties"]["code"]["type"] == "string");
        REQUIRE(j["function"]["parameters"]["properties"]["timeout"]["type"] == "integer");
        REQUIRE(j["function"]["parameters"]["properties"]["timeout"]["minimum"] == 1);
        REQUIRE(j["function"]["parameters"]["properties"]["timeout"]["maximum"] == 300);
        REQUIRE(j["function"]["parameters"]["properties"]["verbose"]["type"] == "boolean");
        REQUIRE(j["function"]["parameters"]["properties"]["verbose"]["default"] == false);
        
        // Check required array has 2 items (language and code)
        REQUIRE(j["function"]["parameters"]["required"].size() == 2);
    }
}

TEST_CASE("Nested schema validation", "[core][llm][tool_schema][validation][nested]") {
    SECTION("validate array of strings") {
        ParameterSchema item_schema;
        item_schema.type = SchemaType::String;
        
        ParameterSchema schema;
        schema.type = SchemaType::Array;
        schema.items = std::make_shared<ParameterSchema>(item_schema);
        
        REQUIRE(schema_utils::validate_against_schema(
            nlohmann::json::array({"a", "b", "c"}), schema));
        REQUIRE_FALSE(schema_utils::validate_against_schema(
            nlohmann::json::array({"a", 123, "c"}), schema));
    }

    SECTION("validate array of integers") {
        ParameterSchema item_schema;
        item_schema.type = SchemaType::Integer;
        item_schema.minimum = 0.0;
        item_schema.maximum = 100.0;
        
        ParameterSchema schema;
        schema.type = SchemaType::Array;
        schema.items = std::make_shared<ParameterSchema>(item_schema);
        
        REQUIRE(schema_utils::validate_against_schema(
            nlohmann::json::array({1, 50, 100}), schema));
        REQUIRE_FALSE(schema_utils::validate_against_schema(
            nlohmann::json::array({1, -5, 100}), schema));
        REQUIRE_FALSE(schema_utils::validate_against_schema(
            nlohmann::json::array({1, 150}), schema));
    }

    SECTION("validate nested object") {
        auto nested_schema = std::make_shared<ParameterSchema>();
        nested_schema->type = SchemaType::String;
        
        ParameterSchema schema;
        schema.type = SchemaType::Object;
        schema.properties["name"] = nested_schema;
        schema.properties["value"] = std::make_shared<ParameterSchema>();
        schema.properties["value"]->type = SchemaType::Integer;
        
        REQUIRE(schema_utils::validate_against_schema(
            nlohmann::json{{"name", "test"}, {"value", 42}}, schema));
        REQUIRE_FALSE(schema_utils::validate_against_schema(
            nlohmann::json{{"name", 123}, {"value", 42}}, schema));
        REQUIRE_FALSE(schema_utils::validate_against_schema(
            nlohmann::json{{"name", "test"}, {"value", "not_int"}}, schema));
    }

    SECTION("validate object with required property missing") {
        auto required_schema = std::make_shared<ParameterSchema>();
        required_schema->type = SchemaType::String;
        required_schema->required = true;
        
        auto optional_schema = std::make_shared<ParameterSchema>();
        optional_schema->type = SchemaType::String;
        optional_schema->required = false;
        
        ParameterSchema schema;
        schema.type = SchemaType::Object;
        schema.properties["required_field"] = required_schema;
        schema.properties["optional_field"] = optional_schema;
        
        // Both fields present - valid
        REQUIRE(schema_utils::validate_against_schema(
            nlohmann::json{{"required_field", "value"}, {"optional_field", "value"}}, schema));
        // Required missing - invalid
        REQUIRE_FALSE(schema_utils::validate_against_schema(
            nlohmann::json{{"optional_field", "value"}}, schema));
        // Required present, optional missing - valid
        REQUIRE(schema_utils::validate_against_schema(
            nlohmann::json{{"required_field", "value"}}, schema));
    }

    SECTION("validate object with default value") {
        auto with_default = std::make_shared<ParameterSchema>();
        with_default->type = SchemaType::String;
        with_default->required = true;
        with_default->default_value = "default_value";
        
        ParameterSchema schema;
        schema.type = SchemaType::Object;
        schema.properties["field"] = with_default;
        
        // Missing but has default - valid
        REQUIRE(schema_utils::validate_against_schema(
            nlohmann::json::object(), schema));
    }
}

TEST_CASE("from_json error handling", "[core][llm][tool_schema][error]") {
    SECTION("missing function.name throws") {
        nlohmann::json j = {
            {"type", "function"},
            {"function", {{"description", "No name"}}}
        };
        REQUIRE_THROWS_AS(ToolSchema::from_json(j), std::invalid_argument);
    }

    SECTION("missing name in Anthropic format throws") {
        nlohmann::json j = {
            {"description", "No name"},
            {"input_schema", {{"type", "object"}}}
        };
        REQUIRE_THROWS_AS(ToolSchema::from_json(j), std::invalid_argument);
    }

    SECTION("invalid format throws") {
        nlohmann::json j = {{"description", "No format markers"}};
        REQUIRE_THROWS_AS(ToolSchema::from_json(j), std::invalid_argument);
    }

    SECTION("from_tool_definition returns nullopt for invalid") {
        nlohmann::json j = {{"invalid", true}};
        auto result = ToolSchema::from_tool_definition(j);
        REQUIRE_FALSE(result.has_value());
    }
}
