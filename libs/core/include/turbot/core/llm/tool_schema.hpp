#pragma once

#include <turbot/core/common/export.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace turbot::core::llm {

// Forward declaration
struct ParameterSchema;

/// JSON Schema types for tool parameters
enum class TURBOT_CORE_API SchemaType {
    String,    ///< String type
    Number,    ///< Number type (double)
    Integer,   ///< Integer type
    Boolean,   ///< Boolean type
    Object,    ///< Object type (nested)
    Array,     ///< Array type
    Null       ///< Null type
};

/// Convert SchemaType to string
[[nodiscard]] TURBOT_CORE_API std::string schema_type_to_string(SchemaType type);

/// Convert string to SchemaType
[[nodiscard]] TURBOT_CORE_API SchemaType string_to_schema_type(const std::string& str);

/// Parameter schema definition
struct TURBOT_CORE_API ParameterSchema {
    SchemaType type = SchemaType::String;      ///< Parameter type
    std::optional<std::string> description;    ///< Parameter description
    bool required = false;                      ///< Whether parameter is required
    std::optional<nlohmann::json> default_value; ///< Default value
    std::optional<nlohmann::json> enum_values;   ///< Enum values (array)
    std::optional<double> minimum;               ///< Minimum value (for numbers)
    std::optional<double> maximum;               ///< Maximum value (for numbers)
    std::optional<int> min_length;               ///< Minimum string length
    std::optional<int> max_length;               ///< Maximum string length
    std::optional<std::string> pattern;          ///< Regex pattern for strings
    std::shared_ptr<ParameterSchema> items;      ///< Item schema for arrays
    std::map<std::string, std::shared_ptr<ParameterSchema>> properties; ///< Properties for objects

    /// Convert to JSON Schema format
    [[nodiscard]] nlohmann::json to_json_schema() const;

    /// Create from JSON Schema
    static ParameterSchema from_json_schema(const nlohmann::json& j);
};

/// Tool Schema builder for constructing LLM tool definitions
class TURBOT_CORE_API ToolSchema {
public:
    /// Create an empty tool schema
    ToolSchema() = default;

    /// Create a tool schema with name and description
    explicit ToolSchema(std::string name, std::string description = "");

    /// Set the tool name
    ToolSchema& set_name(std::string name);

    /// Set the tool description
    ToolSchema& set_description(std::string description);

    /// Add a string parameter
    ToolSchema& add_string_param(
        const std::string& name,
        const std::string& description,
        bool required = true,
        std::optional<std::string> default_value = std::nullopt
    );

    /// Add a number parameter
    ToolSchema& add_number_param(
        const std::string& name,
        const std::string& description,
        bool required = true,
        std::optional<double> default_value = std::nullopt,
        std::optional<double> minimum = std::nullopt,
        std::optional<double> maximum = std::nullopt
    );

    /// Add an integer parameter
    ToolSchema& add_integer_param(
        const std::string& name,
        const std::string& description,
        bool required = true,
        std::optional<int> default_value = std::nullopt,
        std::optional<int> minimum = std::nullopt,
        std::optional<int> maximum = std::nullopt
    );

    /// Add a boolean parameter
    ToolSchema& add_boolean_param(
        const std::string& name,
        const std::string& description,
        bool required = true,
        std::optional<bool> default_value = std::nullopt
    );

    /// Add an enum parameter
    ToolSchema& add_enum_param(
        const std::string& name,
        const std::string& description,
        const std::vector<std::string>& values,
        bool required = true
    );

    /// Add an array parameter
    ToolSchema& add_array_param(
        const std::string& name,
        const std::string& description,
        const ParameterSchema& item_schema,
        bool required = true
    );

    /// Add an object parameter (nested)
    ToolSchema& add_object_param(
        const std::string& name,
        const std::string& description,
        const std::map<std::string, ParameterSchema>& properties,
        bool required = true
    );

    /// Add a custom parameter schema
    ToolSchema& add_param(
        const std::string& name,
        const ParameterSchema& schema,
        bool required = true
    );

    /// Get the tool name
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    /// Get the tool description
    [[nodiscard]] const std::string& description() const noexcept { return description_; }

    /// Get the parameters schema
    [[nodiscard]] const std::map<std::string, ParameterSchema>& parameters() const noexcept { 
        return parameters_; 
    }

    /// Get the required parameters
    [[nodiscard]] const std::vector<std::string>& required_params() const noexcept { 
        return required_params_; 
    }

    /// Convert to OpenAI-compatible tool definition format
    /// @return JSON object with type, function.name, function.description, function.parameters
    [[nodiscard]] nlohmann::json to_openai_tool() const;

    /// Convert to Anthropic-compatible tool definition format
    /// @return JSON object with name, description, input_schema
    [[nodiscard]] nlohmann::json to_anthropic_tool() const;

    /// Convert to common tool definition format (OpenAI style)
    [[nodiscard]] nlohmann::json to_json() const;

    /// Create from JSON
    static ToolSchema from_json(const nlohmann::json& j);

    /// Create from existing tool definition (auto-detect format)
    static std::optional<ToolSchema> from_tool_definition(const nlohmann::json& j);

private:
    std::string name_;
    std::string description_;
    std::map<std::string, ParameterSchema> parameters_;
    std::vector<std::string> required_params_;
};

/// Utility functions for schema operations
namespace schema_utils {

/// Merge multiple tool schemas into a single parameters object
[[nodiscard]] TURBOT_CORE_API nlohmann::json merge_schemas(
    const std::vector<std::reference_wrapper<const ToolSchema>>& schemas
);

/// Validate JSON against a parameter schema
[[nodiscard]] TURBOT_CORE_API bool validate_against_schema(
    const nlohmann::json& value,
    const ParameterSchema& schema
);

/// Get validation error message
[[nodiscard]] TURBOT_CORE_API std::string get_validation_error(
    const nlohmann::json& value,
    const ParameterSchema& schema
);

} // namespace schema_utils

} // namespace turbot::core::llm
