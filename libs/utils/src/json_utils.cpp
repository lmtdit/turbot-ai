#include "turbot/utils/json_utils.hpp"
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace turbot::utils::json {

bool validate_schema(const nlohmann::json& data, const nlohmann::json& schema) {
    // 简化的Schema验证，实际项目中可以使用json-schema-validator库
    if (!schema.is_object()) {
        return false;
    }

    // 检查type
    if (schema.contains("type")) {
        // JSON Schema allows "type" to be a string OR an array of strings.
        // Guard against type_error when the value is not a plain string.
        if (!schema["type"].is_string() && !schema["type"].is_array()) {
            return false;  // malformed schema
        }

        auto check_single_type = [&](const std::string& type) -> bool {
            if (type == "string")  return data.is_string();
            if (type == "number")  return data.is_number_integer() || data.is_number_float();
            if (type == "integer") return data.is_number_integer();
            if (type == "boolean") return data.is_boolean();
            if (type == "array")   return data.is_array();
            if (type == "object")  return data.is_object();
            if (type == "null")    return data.is_null();
            return false;
        };

        bool type_valid = false;
        if (schema["type"].is_string()) {
            type_valid = check_single_type(schema["type"].get<std::string>());
        } else {
            // array of types: valid if data matches any
            for (const auto& t : schema["type"]) {
                if (t.is_string() && check_single_type(t.get<std::string>())) {
                    type_valid = true;
                    break;
                }
            }
        }

        if (!type_valid) {
            return false;
        }
    }

    // 检查required字段
    if (schema.contains("required") && data.is_object()) {
        auto required = schema["required"];
        if (required.is_array()) {
            for (const auto& field : required) {
                if (!data.contains(field)) {
                    return false;
                }
            }
        }
    }

    // 检查properties
    if (schema.contains("properties") && data.is_object()) {
        auto properties = schema["properties"];
        for (auto& [key, value] : data.items()) {
            if (properties.contains(key)) {
                if (!validate_schema(value, properties[key])) {
                    return false;
                }
            }
        }
    }

    // 检查enum
    if (schema.contains("enum")) {
        auto enum_values = schema["enum"];
        bool found = false;
        for (const auto& value : enum_values) {
            if (value == data) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    return true;
}

nlohmann::json merge(const nlohmann::json& a, const nlohmann::json& b) {
    if (a.is_object() && b.is_object()) {
        nlohmann::json result = a;
        for (auto& [key, value] : b.items()) {
            if (result.contains(key) && result[key].is_object() && value.is_object()) {
                result[key] = merge(result[key], value);
            } else {
                result[key] = value;
            }
        }
        return result;
    } else if (a.is_array() && b.is_array()) {
        nlohmann::json result = a;
        result.insert(result.end(), b.begin(), b.end());
        return result;
    } else {
        return b;
    }
}

std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> parts;
    std::string current;

    for (size_t i = 0; i < path.size(); ++i) {
        char c = path[i];

        if (c == '.') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else if (c == '[') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
            // 查找匹配的 ]
            size_t close = path.find(']', i);
            if (close != std::string::npos) {
                parts.push_back(path.substr(i, close - i + 1));
                i = close;
            }
        } else {
            current += c;
        }
    }

    if (!current.empty()) {
        parts.push_back(current);
    }

    return parts;
}

std::optional<nlohmann::json> query(const nlohmann::json& data, const std::string& path) {
    if (path.empty()) {
        return data;
    }

    nlohmann::json current = data;
    auto parts = split_path(path);

    for (const auto& part : parts) {
        if (part.empty()) {
            continue;
        }

        if (part[0] == '[') {
            // 数组索引
            size_t close = part.find(']');
            if (close == std::string::npos || close == 1) {
                return std::nullopt;
            }

            if (!current.is_array()) {
                return std::nullopt;
            }

            std::string index_str = part.substr(1, close - 1);
            try {
                size_t index = std::stoul(index_str);
                if (index >= current.size()) {
                    return std::nullopt;
                }
                current = current[index];
            } catch (...) {
                return std::nullopt;
            }
        } else {
            // 对象键
            if (!current.is_object()) {
                return std::nullopt;
            }

            if (!current.contains(part)) {
                return std::nullopt;
            }

            current = current[part];
        }
    }

    return current;
}

bool has_path(const nlohmann::json& data, const std::string& path) {
    return query(data, path).has_value();
}

nlohmann::json clone(const nlohmann::json& data) {
    return nlohmann::json(data);
}

bool equals(const nlohmann::json& a, const nlohmann::json& b) {
    return a == b;
}

bool remove_path(nlohmann::json& data, const std::string& path) {
    if (path.empty()) {
        return false;
    }

    auto parts = split_path(path);
    if (parts.empty()) {
        return false;
    }

    nlohmann::json* current = &data;

    // 导航到倒数第二层
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        const auto& part = parts[i];

        if (part.empty()) {
            return false;
        }

        if (part[0] == '[') {
            // 数组索引
            size_t close = part.find(']');
            if (close == std::string::npos || close == 1) {
                return false;
            }

            if (!current->is_array()) {
                return false;
            }

            std::string index_str = part.substr(1, close - 1);
            try {
                size_t index = std::stoul(index_str);
                if (index >= current->size()) {
                    return false;
                }
                current = &(*current)[index];
            } catch (...) {
                return false;
            }
        } else {
            // 对象键
            if (!current->is_object()) {
                return false;
            }

            if (!current->contains(part)) {
                return false;
            }

            current = &(*current)[part];
        }
    }

    // 删除最后一层
    const auto& last_part = parts.back();
    if (last_part.empty()) {
        return false;
    }

    if (last_part[0] == '[') {
        // 数组索引
        size_t close = last_part.find(']');
        if (close == std::string::npos || close == 1) {
            return false;
        }

        if (!current->is_array()) {
            return false;
        }

        std::string index_str = last_part.substr(1, close - 1);
        try {
            size_t index = std::stoul(index_str);
            if (index >= current->size()) {
                return false;
            }
            current->erase(index);
            return true;
        } catch (...) {
            return false;
        }
    } else {
        // 对象键
        if (!current->is_object()) {
            return false;
        }

        if (!current->contains(last_part)) {
            return false;
        }

        current->erase(last_part);
        return true;
    }
}

std::string pretty_print(const nlohmann::json& data, int indent) {
    return data.dump(indent);
}

} // namespace turbot::utils::json