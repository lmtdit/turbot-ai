#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/json_utils.hpp>
#include <turbot/utils/crypto_utils.hpp>
#include <nlohmann/json.hpp>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace turbot::core {

/**
 * @brief 配置管理系统
 *
 * 提供统一的配置管理接口，支持从文件加载配置、环境变量覆盖、配置监听等功能。
 */
class TURBOT_CORE_API Config {
public:
    /**
     * @brief 配置变化回调函数类型
     */
    using ChangeCallback = std::function<void(const std::string&, const nlohmann::json&)>;

    /**
     * @brief 获取Config单例实例
     * @return Config实例引用
     */
    static Config& instance();

    /**
     * @brief 加载配置文件
     * @param file_path 配置文件路径（JSON格式）
     * @throws std::runtime_error 如果文件不存在或格式错误
     */
    void load(const std::string& file_path);

    /**
     * @brief 从字符串加载配置
     * @param content JSON格式的配置字符串
     * @throws std::runtime_error 如果格式错误
     */
    void load_from_string(const std::string& content);

    /**
     * @brief 从环境变量加载配置
     * @param prefix 环境变量前缀（默认为"TURBOT_"）
     *
     * 环境变量格式：TURBOT_DATABASE_HOST=localhost
     * 对应配置路径：database.host
     */
    void load_from_env(const std::string& prefix = "TURBOT_");

    /**
     * @brief 保存配置到文件
     * @param file_path 文件路径
     * @throws std::runtime_error 如果保存失败
     */
    void save(const std::string& file_path);

    /**
     * @brief 获取配置值
     * @tparam T 值类型
     * @param key 配置键（支持点号分隔，如"database.host"）
     * @return 配置值，如果不存在则返回nullopt
     */
    template<typename T>
    std::optional<T> get(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);

        nlohmann::json value = config_;
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
     * @brief 获取配置值或返回默认值
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
     * @brief 设置配置值
     * @tparam T 值类型
     * @param key 配置键（支持点号分隔）
     * @param value 配置值
     */
    template<typename T>
    void set(const std::string& key, const T& value) {
        // 收集需要通知的回调（在锁外调用以避免死锁）
        std::vector<std::pair<std::string, ChangeCallback>> callbacks_to_notify;
        nlohmann::json new_value;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            nlohmann::json current = config_;
            auto parts = utils::json::split_path(key);

            nlohmann::json* target = &current;
            for (size_t i = 0; i < parts.size() - 1; ++i) {
                if (!target->contains(parts[i])) {
                    (*target)[parts[i]] = nlohmann::json::object();
                }
                target = &(*target)[parts[i]];
            }

            (*target)[parts.back()] = value;
            new_value = value;
            config_ = current;

            // 在锁内收集回调
            auto it = watchers_.find(key);
            if (it != watchers_.end()) {
                callbacks_to_notify = it->second;
            }
        }  // 释放锁后再调用回调

        // 锁外调用回调，避免死锁
        for (const auto& [watch_id, callback] : callbacks_to_notify) {
            try {
                callback(key, new_value);
            } catch (const std::exception& e) {
                // 记录错误但不抛出异常，避免影响其他回调
                TURBOT_LOG_ERROR("Config change callback error for key '{}': {}", key, e.what());
            }
        }
    }

    /**
     * @brief 监听配置变化
     * @param key 配置键
     * @param callback 回调函数
     * @return 监听器ID
     */
    std::string watch(const std::string& key, ChangeCallback callback);

    /**
     * @brief 取消监听
     * @param key 配置键
     * @param watch_id 监听器ID
     */
    void unwatch(const std::string& key, const std::string& watch_id);

    /**
     * @brief 检查配置键是否存在
     * @param key 配置键
     * @return 是否存在
     */
    bool has(const std::string& key) const;

    /**
     * @brief 删除配置键
     * @param key 配置键
     * @return 是否删除成功
     */
    bool remove(const std::string& key);

    /**
     * @brief 获取所有配置
     * @return 配置JSON对象
     */
    nlohmann::json get_all() const;

    /**
     * @brief 清空所有配置
     */
    void clear();

    // Delete copy and move
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(Config&&) = delete;

private:
    Config() = default;
    ~Config() = default;

    /**
     * @brief 通知配置变化
     * @param key 配置键
     * @param old_value 旧值
     * @param new_value 新值
     */
    void notify_change(const std::string& key,
                       const nlohmann::json& old_value,
                       const nlohmann::json& new_value);

    mutable std::mutex mutex_;
    nlohmann::json config_;

    // 监听器：key -> [(watch_id, callback)]
    std::unordered_map<std::string,
                       std::vector<std::pair<std::string, ChangeCallback>>> watchers_;
};

} // namespace turbot::core