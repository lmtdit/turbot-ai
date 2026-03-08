#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/json_utils.hpp>
#include <nlohmann/json.hpp>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <filesystem>

namespace turbot::core {

/**
 * @brief 配置层级枚举
 */
enum class ConfigLevel {
    Default,    ///< 内置默认配置
    User,       ///< 用户级配置 (~/.turbot/)
    Project     ///< 项目级配置 (./.turbot/)
};

/**
 * @brief 扩展模块类型枚举
 */
enum class ExtensionType {
    Agent,      ///< Agent 扩展
    Skill,      ///< 技能扩展
    Rule,       ///< 规则扩展
    Event,      ///< 事件扩展
    Extension   ///< 插件扩展
};

/**
 * @brief 配置加载结果
 */
struct LoadResult {
    bool success = false;                           ///< 是否成功
    ConfigLevel level = ConfigLevel::Default;       ///< 配置层级
    std::string path;                               ///< 配置路径
    std::vector<std::string> warnings;              ///< 警告信息
    std::vector<std::string> errors;                ///< 错误信息
};

/**
 * @brief 配置错误类型
 */
enum class ConfigError {
    FileNotFound,       ///< 文件不存在
    ParseError,         ///< 解析错误
    ValidationError,    ///< 验证错误
    PermissionDenied,   ///< 权限拒绝
    InvalidPath,        ///< 无效路径
    CircularReference   ///< 循环引用
};

/**
 * @brief 配置异常类
 */
class ConfigException : public std::runtime_error {
public:
    ConfigException(ConfigError error, const std::string& message)
        : std::runtime_error(message), error_(error) {}

    ConfigError error() const { return error_; }

private:
    ConfigError error_;
};

/**
 * @brief Markdown 配置解析结果
 */
struct MarkdownConfig {
    nlohmann::json frontmatter;     ///< YAML Frontmatter 解析结果
    std::string content;            ///< Markdown 正文内容
};

/**
 * @brief 配置管理器
 *
 * 提供分级配置管理功能，支持内置默认、用户级、项目级三级配置加载，
 * 以及环境变量覆盖和 YAML Frontmatter 解析。
 */
class TURBOT_CORE_API ConfigManager {
public:
    /// 配置变更回调类型
    using ConfigChangeCallback = std::function<void(ConfigLevel, const std::string&, const nlohmann::json&)>;

    /// 获取单例实例
    static ConfigManager& instance();

    // ========== 初始化与加载 ==========

    /**
     * @brief 初始化配置系统（加载所有层级配置）
     * @return 各层级加载结果
     */
    std::vector<LoadResult> initialize();

    /**
     * @brief 加载指定层级配置
     * @param level 配置层级
     * @return 加载结果
     */
    LoadResult load_config(ConfigLevel level);

    /**
     * @brief 加载扩展模块
     * @param level 配置层级
     * @param type 扩展类型
     * @return 各扩展加载结果
     */
    std::vector<LoadResult> load_extensions(ConfigLevel level, ExtensionType type);

    /**
     * @brief 重新加载所有配置
     */
    void reload();

    // ========== 路径管理 ==========

    /**
     * @brief 获取配置路径
     * @param level 配置层级
     * @return 配置文件路径
     */
    std::string get_config_path(ConfigLevel level) const;

    /**
     * @brief 获取扩展目录路径
     * @param level 配置层级
     * @param type 扩展类型
     * @return 扩展目录路径
     */
    std::string get_extension_path(ConfigLevel level, ExtensionType type) const;

    /**
     * @brief 获取日志目录路径
     * @return 日志目录路径
     */
    std::string get_log_path() const;

    // ========== 保存与持久化 ==========

    /**
     * @brief 保存配置到指定层级
     * @param level 配置层级
     * @return 是否成功
     */
    bool save_config(ConfigLevel level);

    // ========== 配置访问 ==========

    /**
     * @brief 获取配置值
     * @tparam T 值类型
     * @param key 配置键（支持点号分隔）
     * @return 配置值，不存在则返回 nullopt
     */
    template<typename T>
    std::optional<T> get(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);

        nlohmann::json value = merged_config_;
        auto parts = utils::json::split_path(key);

        for (const auto& part : parts) {
            if (value.contains(part)) {
                value = value[part];
            } else {
                return std::nullopt;
            }
        }

        try {
            return value.get<T>();
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief 获取配置值或默认值
     * @tparam T 值类型
     * @param key 配置键
     * @param default_value 默认值
     * @return 配置值或默认值
     */
    template<typename T>
    T get_or(const std::string& key, const T& default_value) const {
        auto result = get<T>(key);
        return result ? *result : default_value;
    }

    /**
     * @brief 检查配置键是否存在
     * @param key 配置键
     * @return 是否存在
     */
    bool has(const std::string& key) const;

    /**
     * @brief 获取合并后的完整配置
     * @return 合并后的配置 JSON
     */
    nlohmann::json get_all() const;

    // ========== 配置变更 ==========

    /**
     * @brief 设置配置值（运行时）
     * @tparam T 值类型
     * @param key 配置键
     * @param value 配置值
     */
    template<typename T>
    void set(const std::string& key, const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);

        nlohmann::json current = merged_config_;
        auto parts = utils::json::split_path(key);

        nlohmann::json* target = &current;
        for (size_t i = 0; i < parts.size() - 1; ++i) {
            if (!target->contains(parts[i])) {
                (*target)[parts[i]] = nlohmann::json::object();
            }
            target = &(*target)[parts[i]];
        }

        (*target)[parts.back()] = value;
        merged_config_ = current;
    }

    /**
     * @brief 注册配置变更回调
     * @param callback 回调函数
     */
    void on_config_change(ConfigChangeCallback callback);

    // ========== 验证 ==========

    /**
     * @brief 验证配置
     * @param config 配置 JSON
     * @return 是否有效
     */
    bool validate_config(const nlohmann::json& config) const;

    // ========== YAML/Markdown 解析 ==========

    /**
     * @brief 解析 Markdown + YAML Frontmatter 格式配置
     * @param file_path 文件路径
     * @return 解析结果
     */
    MarkdownConfig parse_markdown_config(const std::string& file_path) const;

    /**
     * @brief 解析 YAML 字符串为 JSON
     * @param yaml_content YAML 内容
     * @return JSON 对象
     */
    nlohmann::json parse_yaml(const std::string& yaml_content) const;

    // ========== 环境变量解析 ==========

    /**
     * @brief 解析环境变量引用
     * @param value 可能包含环境变量引用的字符串
     * @return 解析后的字符串
     */
    std::string resolve_env_vars(const std::string& value) const;

    // Delete copy and move
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    ConfigManager(ConfigManager&&) = delete;
    ConfigManager& operator=(ConfigManager&&) = delete;

private:
    ConfigManager() = default;
    ~ConfigManager() = default;

    /// 获取内置默认配置
    nlohmann::json get_default_config() const;

    /// 加载 JSON 配置文件
    LoadResult load_json_file(const std::string& file_path);

    /// 合并配置到当前配置
    void merge_config(const nlohmann::json& config, ConfigLevel level);

    /// 深度合并两个 JSON 对象（支持 _append 后缀的数组追加）
    static nlohmann::json merge_config_with_append(const nlohmann::json& base, const nlohmann::json& override);

    /// 获取扩展类型目录名
    static std::string extension_type_to_dir(ExtensionType type);

    /// 内部配置加载
    LoadResult load_config_internal(ConfigLevel level);

    /// 加载 JSON 文件内容
    nlohmann::json load_json_content(const std::string& file_path);

    /// 加载环境变量覆盖
    void load_env_overrides();

    /// 按路径设置配置值
    void set_by_path(nlohmann::json& config, const std::string& path, const nlohmann::json& value);

    /// 环境变量键转配置键
    std::string env_key_to_config_key(const std::string& env_key);

    mutable std::mutex mutex_;
    nlohmann::json merged_config_;                              ///< 合并后的配置
    nlohmann::json default_config_;                              ///< 默认配置
    nlohmann::json user_config_;                                 ///< 用户级配置
    nlohmann::json project_config_;                              ///< 项目级配置
    std::vector<ConfigChangeCallback> change_callbacks_;         ///< 变更回调
    bool initialized_ = false;                                   ///< 是否已初始化
};

} // namespace turbot::core
