#include <turbot/core/config/config.hpp>
#include <turbot/core/common/logger.hpp>
#include "turbot/utils/json_utils.hpp"
#include "turbot/utils/string_utils.hpp"

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

namespace {
    /**
     * @brief 转换环境变量键为配置键
     * @param env_key 环境变量键（如"TURBOT_DATABASE_HOST"）
     * @param prefix 前缀（如"TURBOT_"）
     * @return 配置键（如"database.host"）
     */
    std::string env_key_to_config_key(const std::string& env_key, const std::string& prefix) {
        if (env_key.size() <= prefix.size()) {
            return "";
        }

        std::string key = env_key.substr(prefix.size());
        std::string result;

        for (char c : key) {
            if (c == '_') {
                result += '.';
            } else if (std::isupper(c)) {
                result += static_cast<char>(std::tolower(c));
            } else {
                result += c;
            }
        }

        return result;
    }
}

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
        config_ = merge(config_, config);

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
            std::string config_key = env_key_to_config_key(key, prefix);
            if (!config_key.empty()) {
                // 尝试解析值
                nlohmann::json json_value;
                try {
                    json_value = nlohmann::json::parse(value);
                } catch (...) {
                    json_value = value;
                }

                // 设置值
                auto parts = split_key(config_key);
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

    std::string watch_id = generate_uuid();
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
    auto parts = split_key(key);

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

    auto parts = split_key(key);
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

std::vector<std::string> Config::split_key(const std::string& key) {
    std::vector<std::string> parts;
    std::string current;

    for (char c : key) {
        if (c == '.') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
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

nlohmann::json Config::merge(const nlohmann::json& a, const nlohmann::json& b) {
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

} // namespace turbot::core