#include <turbot/core/llm/tool_schema.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <unordered_map>

namespace turbot::core::llm {

// SchemaType conversion functions
std::string schema_type_to_string(SchemaType type) {
    static const std::unordered_map<SchemaType, std::string_view> type_to_str = {
        {SchemaType::String, "string"},
        {SchemaType::Number, "number"},
        {SchemaType::Integer, "integer"},
        {SchemaType::Boolean, "boolean"},
        {SchemaType::Object, "object"},
        {SchemaType::Array, "array"},
        {SchemaType::Null, "null"}
    };
    auto it = type_to_str.find(type);
    if (it != type_to_str.end()) {
        return std::string(it->second);
    }
    throw std::invalid_argument(
        fmt::format("Invalid SchemaType value: {}", static_cast<int>(type)));
}

SchemaType string_to_schema_type(const std::string& str) {
    static const std::unordered_map<std::string_view, SchemaType> str_to_type = {
        {"string", SchemaType::String},
        {"number", SchemaType::Number},
        {"integer", SchemaType::Integer},
        {"boolean", SchemaType::Boolean},
        {"object", SchemaType::Object},
        {"array", SchemaType::Array},
        {"null", SchemaType::Null}
    };
    auto it = str_to_type.find(str);
    if (it != str_to_type.end()) {
        return it->second;
    }
    throw std::invalid_argument(fmt::format("Invalid schema type: {}", str));
}

// ParameterSchema implementation
nlohmann::json ParameterSchema::to_json_schema() const {
    nlohmann::json j;
    j["type"] = schema_type_to_string(type);
    
    if (description) {
        j["description"] = *description;
    }
    
    if (default_value) {
        j["default"] = *default_value;
    }
    
    if (enum_values) {
        j["enum"] = *enum_values;
    }
    
    // Number/Integer constraints
    if (minimum) {
        j["minimum"] = *minimum;
    }
    if (maximum) {
        j["maximum"] = *maximum;
    }
    
    // String constraints
    if (min_length) {
        j["minLength"] = *min_length;
    }
    if (max_length) {
        j["maxLength"] = *max_length;
    }
    if (pattern) {
        j["pattern"] = *pattern;
    }
    
    // Array items
    if (items && type == SchemaType::Array) {
        j["items"] = items->to_json_schema();
    }
    
    // Object properties
    if (!properties.empty() && type == SchemaType::Object) {
        nlohmann::json props = nlohmann::json::object();
        for (const auto& [name, schema] : properties) {
            props[name] = schema->to_json_schema();
        }
        j["properties"] = props;
    }
    
    return j;
}

ParameterSchema ParameterSchema::from_json_schema(const nlohmann::json& j) {
    ParameterSchema schema;
    
    if (j.contains("type")) {
        schema.type = string_to_schema_type(j["type"].get<std::string>());
    }
    
    if (j.contains("description")) {
        schema.description = j["description"].get<std::string>();
    }
    
    if (j.contains("default")) {
        schema.default_value = j["default"];
    }
    
    if (j.contains("enum")) {
        schema.enum_values = j["enum"];
    }
    
    if (j.contains("minimum")) {
        schema.minimum = j["minimum"].get<double>();
    }
    if (j.contains("maximum")) {
        schema.maximum = j["maximum"].get<double>();
    }
    
    if (j.contains("minLength")) {
        schema.min_length = j["minLength"].get<int>();
    }
    if (j.contains("maxLength")) {
        schema.max_length = j["maxLength"].get<int>();
    }
    if (j.contains("pattern")) {
        schema.pattern = j["pattern"].get<std::string>();
    }
    
    if (j.contains("items") && schema.type == SchemaType::Array) {
        schema.items = std::make_shared<ParameterSchema>(
            from_json_schema(j["items"])
        );
    }
    
    if (j.contains("properties") && schema.type == SchemaType::Object) {
        for (const auto& [name, prop] : j["properties"].items()) {
            schema.properties[name] = std::make_shared<ParameterSchema>(
                from_json_schema(prop)
            );
        }
    }
    
    return schema;
}

// ToolSchema implementation
ToolSchema::ToolSchema(std::string name, std::string description)
    : name_(std::move(name)), description_(std::move(description)) {}

ToolSchema& ToolSchema::set_name(std::string name) {
    name_ = std::move(name);
    return *this;
}

ToolSchema& ToolSchema::set_description(std::string description) {
    description_ = std::move(description);
    return *this;
}

ToolSchema& ToolSchema::add_string_param(
    const std::string& name,
    const std::string& description,
    bool required,
    std::optional<std::string> default_value
) {
    ParameterSchema schema;
    schema.type = SchemaType::String;
    schema.description = description;
    if (default_value) {
        schema.default_value = *default_value;
    }
    return add_param(name, schema, required);
}

ToolSchema& ToolSchema::add_number_param(
    const std::string& name,
    const std::string& description,
    bool required,
    std::optional<double> default_value,
    std::optional<double> minimum,
    std::optional<double> maximum
) {
    ParameterSchema schema;
    schema.type = SchemaType::Number;
    schema.description = description;
    if (default_value) {
        schema.default_value = *default_value;
    }
    schema.minimum = minimum;
    schema.maximum = maximum;
    return add_param(name, schema, required);
}

ToolSchema& ToolSchema::add_integer_param(
    const std::string& name,
    const std::string& description,
    bool required,
    std::optional<int> default_value,
    std::optional<int> minimum,
    std::optional<int> maximum
) {
    ParameterSchema schema;
    schema.type = SchemaType::Integer;
    schema.description = description;
    if (default_value) {
        schema.default_value = *default_value;
    }
    if (minimum) {
        schema.minimum = static_cast<double>(*minimum);
    }
    if (maximum) {
        schema.maximum = static_cast<double>(*maximum);
    }
    return add_param(name, schema, required);
}

ToolSchema& ToolSchema::add_boolean_param(
    const std::string& name,
    const std::string& description,
    bool required,
    std::optional<bool> default_value
) {
    ParameterSchema schema;
    schema.type = SchemaType::Boolean;
    schema.description = description;
    if (default_value) {
        schema.default_value = *default_value;
    }
    return add_param(name, schema, required);
}

ToolSchema& ToolSchema::add_enum_param(
    const std::string& name,
    const std::string& description,
    const std::vector<std::string>& values,
    bool required
) {
    ParameterSchema schema;
    schema.type = SchemaType::String;
    schema.description = description;
    nlohmann::json enum_arr = nlohmann::json::array();
    for (const auto& v : values) {
        enum_arr.push_back(v);
    }
    schema.enum_values = enum_arr;
    return add_param(name, schema, required);
}

ToolSchema& ToolSchema::add_array_param(
    const std::string& name,
    const std::string& description,
    const ParameterSchema& item_schema,
    bool required
) {
    ParameterSchema schema;
    schema.type = SchemaType::Array;
    schema.description = description;
    schema.items = std::make_shared<ParameterSchema>(item_schema);
    return add_param(name, schema, required);
}

ToolSchema& ToolSchema::add_object_param(
    const std::string& name,
    const std::string& description,
    const std::map<std::string, ParameterSchema>& properties,
    bool required
) {
    ParameterSchema schema;
    schema.type = SchemaType::Object;
    schema.description = description;
    for (const auto& [prop_name, prop_schema] : properties) {
        schema.properties[prop_name] = std::make_shared<ParameterSchema>(prop_schema);
    }
    return add_param(name, schema, required);
}

ToolSchema& ToolSchema::add_param(
    const std::string& name,
    const ParameterSchema& schema,
    bool required
) {
    parameters_[name] = schema;
    if (required) {
        required_params_.push_back(name);
    }
    return *this;
}

nlohmann::json ToolSchema::to_openai_tool() const {
    nlohmann::json params;
    params["type"] = "object";
    
    nlohmann::json props = nlohmann::json::object();
    for (const auto& [name, schema] : parameters_) {
        props[name] = schema.to_json_schema();
    }
    params["properties"] = props;
    
    if (!required_params_.empty()) {
        params["required"] = required_params_;
    }
    
    return {
        {"type", "function"},
        {"function", {
            {"name", name_},
            {"description", description_},
            {"parameters", params}
        }}
    };
}

nlohmann::json ToolSchema::to_anthropic_tool() const {
    nlohmann::json params;
    params["type"] = "object";
    
    nlohmann::json props = nlohmann::json::object();
    for (const auto& [name, schema] : parameters_) {
        props[name] = schema.to_json_schema();
    }
    params["properties"] = props;
    
    if (!required_params_.empty()) {
        params["required"] = required_params_;
    }
    
    return {
        {"name", name_},
        {"description", description_},
        {"input_schema", params}
    };
}

nlohmann::json ToolSchema::to_json() const {
    return to_openai_tool();
}

ToolSchema ToolSchema::from_json(const nlohmann::json& j) {
    ToolSchema schema;
    
    // Try OpenAI format first
    if (j.contains("function") && j["function"].is_object()) {
        const auto& func = j["function"];
        if (!func.contains("name") || func["name"].is_null()) {
            throw std::invalid_argument("Missing required field: function.name");
        }
        schema.name_ = func["name"].get<std::string>();
        if (func.contains("description") && !func["description"].is_null()) {
            schema.description_ = func["description"].get<std::string>();
        }
        if (func.contains("parameters") && func["parameters"].is_object()) {
            const auto& params = func["parameters"];
            if (params.contains("properties") && params["properties"].is_object()) {
                for (const auto& [name, prop] : params["properties"].items()) {
                    schema.parameters_[name] = ParameterSchema::from_json_schema(prop);
                }
            }
            if (params.contains("required") && params["required"].is_array()) {
                for (const auto& req : params["required"]) {
                    schema.required_params_.push_back(req.get<std::string>());
                }
            }
        }
    }
    // Try Anthropic format
    else if (j.contains("input_schema") && j["input_schema"].is_object()) {
        if (!j.contains("name") || j["name"].is_null()) {
            throw std::invalid_argument("Missing required field: name");
        }
        schema.name_ = j["name"].get<std::string>();
        if (j.contains("description") && !j["description"].is_null()) {
            schema.description_ = j["description"].get<std::string>();
        }
        const auto& params = j["input_schema"];
        if (params.contains("properties") && params["properties"].is_object()) {
            for (const auto& [name, prop] : params["properties"].items()) {
                schema.parameters_[name] = ParameterSchema::from_json_schema(prop);
            }
        }
        if (params.contains("required") && params["required"].is_array()) {
            for (const auto& req : params["required"]) {
                schema.required_params_.push_back(req.get<std::string>());
            }
        }
    }
    else {
        throw std::invalid_argument("Invalid tool definition format: missing 'function' or 'input_schema'");
    }
    
    return schema;
}

std::optional<ToolSchema> ToolSchema::from_tool_definition(const nlohmann::json& j) {
    try {
        return from_json(j);
    } catch (const std::exception&) {
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

// schema_utils implementation
namespace schema_utils {

nlohmann::json merge_schemas(
    const std::vector<std::reference_wrapper<const ToolSchema>>& schemas
) {
    nlohmann::json result;
    result["type"] = "object";
    result["properties"] = nlohmann::json::object();
    
    for (const auto& schema_ref : schemas) {
        const auto& schema = schema_ref.get();
        for (const auto& [name, param] : schema.parameters()) {
            result["properties"][name] = param.to_json_schema();
        }
    }
    
    return result;
}

std::string get_validation_error(
    const nlohmann::json& value,
    const ParameterSchema& schema
);

namespace {

// Helper function to build detailed validation errors
std::string build_validation_error(
    const nlohmann::json& value,
    const ParameterSchema& schema,
    const std::string& reason
) {
    return fmt::format(
        "Validation failed: {}. Expected type '{}', got '{}'. Value: {}",
        reason,
        schema_type_to_string(schema.type),
        value.is_string() ? "string" :
        value.is_number() ? "number" :
        value.is_boolean() ? "boolean" :
        value.is_object() ? "object" :
        value.is_array() ? "array" :
        value.is_null() ? "null" : "unknown",
        value.dump().substr(0, 100)  // Limit output length
    );
}

} // anonymous namespace

bool validate_against_schema(
    const nlohmann::json& value,
    const ParameterSchema& schema
) {
    // Basic type validation
    switch (schema.type) {
        case SchemaType::String:
            if (!value.is_string()) return false;
            break;
        case SchemaType::Number:
            if (!value.is_number()) return false;
            break;
        case SchemaType::Integer:
            if (!value.is_number_integer()) return false;
            break;
        case SchemaType::Boolean:
            if (!value.is_boolean()) return false;
            break;
        case SchemaType::Object:
            if (!value.is_object()) return false;
            // Recursively validate properties
            if (!schema.properties.empty()) {
                for (const auto& [name, prop_schema] : schema.properties) {
                    if (value.contains(name)) {
                        if (!validate_against_schema(value[name], *prop_schema)) {
                            return false;
                        }
                    } else if (prop_schema->required && !prop_schema->default_value) {
                        // Required property missing and no default value
                        return false;
                    }
                }
            }
            break;
        case SchemaType::Array:
            if (!value.is_array()) return false;
            // Recursively validate items
            if (schema.items) {
                for (const auto& item : value) {
                    if (!validate_against_schema(item, *schema.items)) {
                        return false;
                    }
                }
            }
            break;
        case SchemaType::Null:
            if (!value.is_null()) return false;
            break;
    }
    
    // Enum validation - use unordered_set for O(1) lookup
    if (schema.enum_values) {
        for (const auto& v : *schema.enum_values) {
            if (value == v) {
                return true;  // Found in enum
            }
        }
        return false;  // Not found in enum values
    }
    
    // Range validation
    if (schema.minimum && value.is_number()) {
        if (value.get<double>() < *schema.minimum) return false;
    }
    if (schema.maximum && value.is_number()) {
        if (value.get<double>() > *schema.maximum) return false;
    }
    
    // String length validation
    if (value.is_string()) {
        auto str = value.get<std::string>();
        if (schema.min_length && str.length() < static_cast<size_t>(*schema.min_length)) {
            return false;
        }
        if (schema.max_length && str.length() > static_cast<size_t>(*schema.max_length)) {
            return false;
        }
    }
    
    return true;
}

std::string get_validation_error(
    const nlohmann::json& value,
    const ParameterSchema& schema
) {
    // Type mismatch
    if (!validate_against_schema(value, schema)) {
        return build_validation_error(value, schema, "Value does not match schema");
    }
    return "";
}

} // namespace schema_utils

} // namespace turbot::core::llm
