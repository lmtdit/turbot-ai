#pragma once

#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <cstdlib>
#include <filesystem>

namespace turbot::test {

/// 在所有测试开始前设置必要的环境变量
class EnvironmentSetupListener : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;
    
    void testRunStarting(Catch::TestRunInfo const&) override {
        // 设置 HOME 环境变量（如果未设置）
        if (std::getenv("HOME") == nullptr) {
            setenv("HOME", "/tmp", 1);
        }
        
        // 设置 TURBOT_USER_CONFIG_PATH
        if (std::getenv("TURBOT_USER_CONFIG_PATH") == nullptr) {
            setenv("TURBOT_USER_CONFIG_PATH", "/tmp/turbot-test-config", 1);
        }
        
        // 创建配置目录
        std::filesystem::create_directories("/tmp/turbot-test-config");
    }
};

// 注册监听器
CATCH_REGISTER_LISTENER(EnvironmentSetupListener)

} // namespace turbot::test
