#pragma once

/**
 * @file env_guard.hpp
 * @brief 环境变量隔离测试夹具
 *
 * 用于测试中安全地修改环境变量，测试结束后自动恢复原始值。
 * 支持：
 * - 设置/修改环境变量
 * - 删除环境变量
 * - 自动恢复原始状态
 * - 批量操作
 */

#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

namespace turbot::test {

/// 环境变量隔离守卫
///
/// RAII 风格的环境变量管理器，在析构时自动恢复原始环境变量状态。
///
/// @example
/// ```cpp
/// TEST_CASE("Environment variable test") {
///     turbot::test::EnvGuard guard;
///
///     guard.set("MY_VAR", "test_value");
///     guard.unset("PATH");  // 临时删除
///
///     // 测试代码...
///
///     // 离开作用域时自动恢复
/// }
/// ```
class EnvGuard {
public:
    EnvGuard() = default;

    /// 析构时自动恢复所有修改的环境变量
    ~EnvGuard() {
        restore_all();
    }

    // 禁止拷贝
    EnvGuard(const EnvGuard&) = delete;
    EnvGuard& operator=(const EnvGuard&) = delete;

    // 允许移动
    EnvGuard(EnvGuard&& other) noexcept
        : saved_vars_(std::move(other.saved_vars_))
        , unset_vars_(std::move(other.unset_vars_)) {
        other.saved_vars_.clear();
        other.unset_vars_.clear();
    }

    EnvGuard& operator=(EnvGuard&& other) noexcept {
        if (this != &other) {
            restore_all();
            saved_vars_ = std::move(other.saved_vars_);
            unset_vars_ = std::move(other.unset_vars_);
            other.saved_vars_.clear();
            other.unset_vars_.clear();
        }
        return *this;
    }

    // === 环境变量操作 ===

    /// 设置环境变量（保存原始值以便恢复）
    /// @param name 环境变量名
    /// @param value 环境变量值
    void set(const std::string& name, const std::string& value) {
        // 保存原始值（如果尚未保存）
        if (saved_vars_.find(name) == saved_vars_.end()) {
            const char* original = std::getenv(name.c_str());
            if (original != nullptr) {
                saved_vars_[name] = original;
            } else {
                // 标记为之前不存在
                saved_vars_[name] = std::nullopt;
            }
        }

        // 设置新值
#ifdef _WIN32
        _putenv_s(name.c_str(), value.c_str());
#else
        setenv(name.c_str(), value.c_str(), 1);
#endif
    }

    /// 删除环境变量（保存原始值以便恢复）
    /// @param name 环境变量名
    void unset(const std::string& name) {
        // 保存原始值（如果尚未保存）
        if (saved_vars_.find(name) == saved_vars_.end()) {
            const char* original = std::getenv(name.c_str());
            if (original != nullptr) {
                saved_vars_[name] = original;
            } else {
                // 标记为之前不存在
                saved_vars_[name] = std::nullopt;
            }
        }

        // 删除环境变量
#ifdef _WIN32
        _putenv_s(name.c_str(), "");
#else
        unsetenv(name.c_str());
#endif

        // 记录为已删除
        unset_vars_.push_back(name);
    }

    /// 获取环境变量值
    /// @param name 环境变量名
    /// @return 环境变量值，如果不存在返回 nullopt
    [[nodiscard]] std::optional<std::string> get(const std::string& name) const {
        const char* value = std::getenv(name.c_str());
        if (value != nullptr) {
            return std::string(value);
        }
        return std::nullopt;
    }

    /// 检查环境变量是否存在
    /// @param name 环境变量名
    [[nodiscard]] bool exists(const std::string& name) const {
        return std::getenv(name.c_str()) != nullptr;
    }

    // === 批量操作 ===

    /// 批量设置环境变量
    /// @param vars 环境变量键值对列表
    void set_many(const std::unordered_map<std::string, std::string>& vars) {
        for (const auto& [name, value] : vars) {
            set(name, value);
        }
    }

    /// 批量删除环境变量
    /// @param names 环境变量名列表
    void unset_many(const std::vector<std::string>& names) {
        for (const auto& name : names) {
            unset(name);
        }
    }

    // === 恢复操作 ===

    /// 恢复单个环境变量
    /// @param name 环境变量名
    void restore(const std::string& name) {
        auto it = saved_vars_.find(name);
        if (it == saved_vars_.end()) {
            return;  // 未被修改过
        }

        const auto& original = it->second;
        if (original.has_value()) {
            // 恢复原始值
#ifdef _WIN32
            _putenv_s(name.c_str(), original->c_str());
#else
            setenv(name.c_str(), original->c_str(), 1);
#endif
        } else {
            // 原本不存在，删除
#ifdef _WIN32
            _putenv_s(name.c_str(), "");
#else
            unsetenv(name.c_str());
#endif
        }

        saved_vars_.erase(it);

        // 从 unset_vars_ 中移除
        auto uit = std::find(unset_vars_.begin(), unset_vars_.end(), name);
        if (uit != unset_vars_.end()) {
            unset_vars_.erase(uit);
        }
    }

    /// 恢复所有被修改的环境变量
    void restore_all() {
        for (const auto& [name, original] : saved_vars_) {
            if (original.has_value()) {
                // 恢复原始值
#ifdef _WIN32
                _putenv_s(name.c_str(), original->c_str());
#else
                setenv(name.c_str(), original->c_str(), 1);
#endif
            } else {
                // 原本不存在，删除
#ifdef _WIN32
                _putenv_s(name.c_str(), "");
#else
                unsetenv(name.c_str());
#endif
            }
        }
        saved_vars_.clear();
        unset_vars_.clear();
    }

    // === 状态查询 ===

    /// 获取被修改的环境变量数量
    [[nodiscard]] size_t modified_count() const {
        return saved_vars_.size();
    }

    /// 检查是否有被修改的环境变量
    [[nodiscard]] bool has_modifications() const {
        return !saved_vars_.empty();
    }

    /// 获取被修改的环境变量名列表
    [[nodiscard]] std::vector<std::string> modified_names() const {
        std::vector<std::string> names;
        names.reserve(saved_vars_.size());
        for (const auto& [name, _] : saved_vars_) {
            names.push_back(name);
        }
        return names;
    }

private:
    /// 保存的原始值（nullopt 表示原本不存在）
    std::unordered_map<std::string, std::optional<std::string>> saved_vars_;

    /// 被删除的环境变量列表
    std::vector<std::string> unset_vars_;
};

/// 环境变量作用域临时修改器
///
/// 更简洁的 RAII 包装，用于单个环境变量的临时修改。
///
/// @example
/// ```cpp
/// TEST_CASE("Single var test") {
///     turbot::test::EnvScope scope("MY_VAR", "temp_value");
///     // MY_VAR = "temp_value"
///     // 离开作用域自动恢复
/// }
/// ```
class EnvScope {
public:
    /// 构造并设置环境变量
    /// @param name 环境变量名
    /// @param value 临时值
    EnvScope(const std::string& name, const std::string& value)
        : name_(name), guard_() {
        guard_.set(name, value);
    }

    /// 析构时自动恢复
    ~EnvScope() = default;

    // 禁止拷贝
    EnvScope(const EnvScope&) = delete;
    EnvScope& operator=(const EnvScope&) = delete;

    // 允许移动
    EnvScope(EnvScope&&) = default;
    EnvScope& operator=(EnvScope&&) = default;

    /// 获取环境变量名
    [[nodiscard]] const std::string& name() const { return name_; }

private:
    std::string name_;
    EnvGuard guard_;
};

} // namespace turbot::test
