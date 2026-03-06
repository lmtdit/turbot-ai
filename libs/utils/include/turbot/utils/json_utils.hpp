#pragma once

#include <turbot/utils/export.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace turbot::utils::json {

/**
 * @brief JSON Schema验证
 * @param data 要验证的JSON数据
 * @param schema JSON Schema
 * @return 验证是否通过
 */
TURBOT_UTILS_EXPORT bool validate_schema(const nlohmann::json& data,
                                         const nlohmann::json& schema);

/**
 * @brief 深度合并两个JSON对象
 * @param a 第一个JSON对象
 * @param b 第二个JSON对象（会覆盖a中的重复键）
 * @return 合并后的JSON对象
 */
TURBOT_UTILS_EXPORT nlohmann::json merge(const nlohmann::json& a,
                                         const nlohmann::json& b);

/**
 * @brief JSON路径查询（支持 "path.to.key[0]" 格式）
 * @param data JSON数据
 * @param path 路径字符串
 * @return 查询结果，如果路径不存在则返回nullopt
 */
TURBOT_UTILS_EXPORT std::optional<nlohmann::json> query(const nlohmann::json& data,
                                                         const std::string& path);

/**
 * @brief 安全获取JSON值
 * @param data JSON数据
 * @param key 键名
 * @return 值，如果键不存在则返回nullopt
 */
template<typename T>
std::optional<T> get_safe(const nlohmann::json& data, const std::string& key) {
    if (!data.contains(key)) {
        return std::nullopt;
    }
    try {
        return data[key].get<T>();
    } catch (...) {
        return std::nullopt;
    }
}

/**
 * @brief 获取JSON值或返回默认值
 * @param data JSON数据
 * @param key 键名
 * @param default_value 默认值
 * @return 值或默认值
 */
template<typename T>
T get_or(const nlohmann::json& data, const std::string& key, const T& default_value) {
    auto result = get_safe<T>(data, key);
    return result ? *result : default_value;
}

/**
 * @brief 分割路径为多个部分
 * @param path 路径字符串（如 "path.to.key[0]"）
 * @return 分割后的路径部分
 */
TURBOT_UTILS_EXPORT std::vector<std::string> split_path(const std::string& path);

/**
 * @brief 检查JSON是否包含特定路径
 * @param data JSON数据
 * @param path 路径字符串
 * @return 是否包含该路径
 */
TURBOT_UTILS_EXPORT bool has_path(const nlohmann::json& data, const std::string& path);

/**
 * @brief 深度克隆JSON对象
 * @param data 要克隆的JSON数据
 * @return 克隆后的JSON对象
 */
TURBOT_UTILS_EXPORT nlohmann::json clone(const nlohmann::json& data);

/**
 * @brief 比较两个JSON对象是否相等
 * @param a 第一个JSON对象
 * @param b 第二个JSON对象
 * @return 是否相等
 */
TURBOT_UTILS_EXPORT bool equals(const nlohmann::json& a, const nlohmann::json& b);

/**
 * @brief 从JSON对象中删除指定路径
 * @param data JSON数据
 * @param path 要删除的路径
 * @return 是否删除成功
 */
TURBOT_UTILS_EXPORT bool remove_path(nlohmann::json& data, const std::string& path);

/**
 * @brief 格式化JSON字符串（美化输出）
 * @param data JSON数据
 * @param indent 缩进空格数
 * @return 格式化后的JSON字符串
 */
TURBOT_UTILS_EXPORT std::string pretty_print(const nlohmann::json& data, int indent = 2);

} // namespace turbot::utils::json