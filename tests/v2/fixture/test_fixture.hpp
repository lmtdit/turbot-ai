#pragma once

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace turbot::test {

/// 临时目录管理（类似 OpenCode 的 tmpdir）
/// 使用 RAII 自动清理
class TmpDir {
public:
    /// 构造函数
    /// @param git_init 是否初始化 git 仓库
    /// @param config 可选配置对象
    explicit TmpDir(bool git_init = false, std::optional<nlohmann::json> config = std::nullopt);
    
    /// 析构函数 - 自动清理目录
    ~TmpDir();
    
    // 禁止拷贝
    TmpDir(const TmpDir&) = delete;
    TmpDir& operator=(const TmpDir&) = delete;
    
    // 允许移动
    TmpDir(TmpDir&& other) noexcept;
    TmpDir& operator=(TmpDir&& other) noexcept;
    
    /// 获取临时目录路径
    [[nodiscard]] std::filesystem::path path() const noexcept;
    
    /// 初始化目录内容
    /// @param setup 初始化回调函数
    void init(std::function<void(const std::filesystem::path&)> setup);
    
    /// 创建子目录
    /// @param name 子目录名
    /// @return 子目录路径
    std::filesystem::path create_subdir(const std::string& name);
    
    /// 创建文件
    /// @param name 文件名
    /// @param content 文件内容
    /// @return 文件路径
    std::filesystem::path create_file(const std::string& name, const std::string& content);

private:
    std::filesystem::path path_;
    bool owns_path_ = true;
    
    static std::filesystem::path generate_unique_path();
};

/// 测试上下文（类似 OpenCode 的 Tool.Context）
/// 用于模拟工具执行时的上下文
struct TestContext {
    std::string session_id;
    std::string message_id;
    std::string call_id;
    std::string agent = "build";
    std::vector<nlohmann::json> messages;
    
    /// Mock ask 函数，收集权限请求
    std::vector<nlohmann::json> permission_requests;
    
    /// 请求权限
    /// @param request 权限请求对象
    void ask(const nlohmann::json& request);
    
    /// 清空权限请求
    void clear_requests();
    
    /// 创建默认上下文
    static TestContext create_default();
};

/// Instance 模拟（类似 OpenCode 的 Instance.provide）
/// 管理当前测试的项目目录
class TestInstance {
public:
    /// 获取单例实例
    static TestInstance& instance();
    
    /// 设置当前项目目录
    /// @param directory 项目目录路径
    void provide(const std::filesystem::path& directory);
    
    /// 获取当前项目目录
    [[nodiscard]] std::filesystem::path directory() const noexcept;
    
    /// 检查路径是否在项目内
    /// @param path 要检查的路径
    /// @return 是否在项目内
    [[nodiscard]] bool contains_path(const std::filesystem::path& path) const;
    
    /// 清除当前目录设置
    void clear();

private:
    TestInstance() = default;
    std::filesystem::path directory_;
};

/// 文件系统工具函数
namespace fs_utils {
    /// 规范化路径
    std::filesystem::path canonical_path(const std::filesystem::path& p);
    
    /// 检查路径是否在指定目录内
    bool is_subpath(const std::filesystem::path& base, const std::filesystem::path& path);
}

} // namespace turbot::test
