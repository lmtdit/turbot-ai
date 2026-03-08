#pragma once

#include <turbot/utils/export.hpp>
#include <string>
#include <string_view>
#include <optional>

namespace turbot::utils {

/**
 * @brief 获取环境变量值
 * @param name 环境变量名
 * @return 环境变量值，不存在则返回空字符串
 */
[[nodiscard]] TURBOT_UTILS_API std::string get_env(std::string_view name);

/**
 * @brief 获取环境变量值（带默认值）
 * @param name 环境变量名
 * @param default_value 默认值
 * @return 环境变量值或默认值
 */
[[nodiscard]] TURBOT_UTILS_API std::string get_env_or(std::string_view name, 
                                                       std::string_view default_value);

/**
 * @brief 设置环境变量
 * @param name 环境变量名
 * @param value 环境变量值
 */
TURBOT_UTILS_API void set_env(std::string_view name, std::string_view value);

/**
 * @brief 删除环境变量
 * @param name 环境变量名
 */
TURBOT_UTILS_API void unset_env(std::string_view name);

/**
 * @brief 检查环境变量是否存在
 * @param name 环境变量名
 * @return 是否存在
 */
[[nodiscard]] TURBOT_UTILS_API bool has_env(std::string_view name);

/**
 * @brief 转换环境变量键为配置键
 * @param env_key 环境变量键（如"TURBOT_DATABASE_HOST"）
 * @param prefix 前缀（如"TURBOT_"）
 * @return 配置键（如"database.host"）
 */
[[nodiscard]] TURBOT_UTILS_API std::string env_key_to_config_key(std::string_view env_key,
                                                                  std::string_view prefix);

/**
 * @brief 解析字符串中的环境变量引用
 * @param value 包含 ${VAR} 或 ${VAR:-default} 格式的字符串
 * @return 解析后的字符串
 * @example resolve_env_refs("${HOME}/config") -> "/home/user/config"
 * @example resolve_env_refs("${UNKNOWN:-default}") -> "default"
 */
[[nodiscard]] TURBOT_UTILS_API std::string resolve_env_refs(std::string_view value);

} // namespace turbot::utils
