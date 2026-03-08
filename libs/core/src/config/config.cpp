#include <turbot/core/config/config.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/json_utils.hpp>
#include <turbot/utils/string_utils.hpp>
#include <turbot/utils/crypto_utils.hpp>
#include <turbot/utils/env_utils.hpp>

#include <fstream>
#include <algorithm>
#include <cctype>
#include <sstream>

// macOS environ workaround
#if defined(__APPLE__)
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
extern char** environ;
#endif

using namespace turbot::utils;

namespace turbot::core {

// 直接使用 utils 命名空间的 env_key_to_config_key 函数

Config& Config::instance() {
    static Config instance;
    return instance;
}

void Config::load(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + file_path);
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    load_from_string(content);

    TURBOT_LOG_INFO("Loaded config from file: {}", file_path);
}

void Config::load_from_string(const std::string& content) {
    try {
        nlohmann::json config = nlohmann::json::parse(content);

        std::lock_guard<std::mutex> lock(mutex_);
        config_ = json::merge(config_, config);

        TURBOT_LOG_INFO("Loaded config from string");
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error("Failed to parse config: " + std::string(e.what()));
    }
}

void Config::load_from_env(const std::string& prefix) {
    std::lock_guard<std::mutex> lock(mutex_);
    int loaded = 0;

    // 单次遍历环境变量
    for (int i = 0; environ[i] != nullptr; ++i) {
        std::string env_var = environ[i];

        size_t equal_pos = env_var.find('=');
        if (equal_pos == std::string::npos) {
            continue;
        }

        std::string key = env_var.substr(0, equal_pos);
        std::string value = env_var.substr(equal_pos + 1);

        // 检查是否匹配前缀
        if (key.size() > prefix.size() &&
            key.substr(0, prefix.size()) == prefix) {
            std::string config_key = utils::env_key_to_config_key(key, prefix);
            if (!config_key.empty()) {
                // 尝试解析值
                nlohmann::json json_value;
                try {
                    json_value = nlohmann::json::parse(value);
                } catch (...) {
                    json_value = value;
                }

                // 设置值
                auto parts = json::split_path(config_key);
                nlohmann::json* target = &config_;

                for (size_t j = 0; j < parts.size() - 1; ++j) {
                    if (!target->contains(parts[j])) {
                        (*target)[parts[j]] = nlohmann::json::object();
                    }
                    target = &(*target)[parts[j]];
                }

                (*target)[parts.back()] = json_value;
                ++loaded;
            }
        }
    }

    if (loaded > 0) {
        TURBOT_LOG_INFO("Loaded {} config values from environment", loaded);
    }
}

void Config::save(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ofstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file for writing: " + file_path);
    }

    file << config_.dump(2);

    TURBOT_LOG_INFO("Saved config to file: {}", file_path);
}

std::string Config::watch(const std::string& key, ChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string watch_id = crypto::generate_uuid();
    watchers_[key].emplace_back(watch_id, callback);

    TURBOT_LOG_DEBUG("Registered watcher for key: {}", key);
    return watch_id;
}

void Config::unwatch(const std::string& key, const std::string& watch_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = watchers_.find(key);
    if (it != watchers_.end()) {
        auto& watchers = it->second;
        watchers.erase(
            std::remove_if(watchers.begin(), watchers.end(),
                          [&](const auto& pair) {
                              return pair.first == watch_id;
                          }),
            watchers.end()
        );

        if (watchers.empty()) {
            watchers_.erase(it);
        }

        TURBOT_LOG_DEBUG("Unregistered watcher for key: {}", key);
    }
}

bool Config::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    nlohmann::json value = config_;
    auto parts = json::split_path(key);

    for (const auto& part : parts) {
        if (value.contains(part)) {
            value = value[part];
        } else {
            return false;
        }
    }

    return true;
}

bool Config::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto parts = json::split_path(key);
    if (parts.empty()) {
        return false;
    }

    nlohmann::json* target = &config_;

    // 导航到倒数第二层
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        if (!target->contains(parts[i])) {
            return false;
        }
        target = &(*target)[parts[i]];
    }

    // 删除最后一层
    if (target->contains(parts.back())) {
        target->erase(parts.back());
        TURBOT_LOG_DEBUG("Removed config key: {}", key);
        return true;
    }

    return false;
}

nlohmann::json Config::get_all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void Config::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.clear();
    watchers_.clear();
    TURBOT_LOG_INFO("Cleared all config");
}

void Config::notify_change(const std::string& key,
                           [[maybe_unused]] const nlohmann::json& old_value,
                           const nlohmann::json& new_value) {
    auto it = watchers_.find(key);
    if (it != watchers_.end()) {
        for (const auto& [watch_id, callback] : it->second) {
            try {
                callback(key, new_value);
            } catch (const std::exception& e) {
                TURBOT_LOG_ERROR("Config change callback error: {}", e.what());
            }
        }
    }
}

} // namespace turbot::core