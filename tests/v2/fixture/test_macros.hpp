#pragma once

#include "test_fixture.hpp"

/// 设置测试项目目录
/// 用法: TURBOT_TEST_PROVIDE(tmp.path())
#define TURBOT_TEST_PROVIDE(dir) \
    turbot::test::TestInstance::instance().provide(dir)

/// 创建临时目录
/// 用法: TURBOT_TEST_TMPDIR(tmp, true)  // true = 初始化 git
#define TURBOT_TEST_TMPDIR(name, git_init) \
    turbot::test::TmpDir name(git_init)

/// 创建带配置的临时目录
/// 用法: TURBOT_TEST_TMPDIR_WITH_CONFIG(tmp, true, R"({"key": "value"})")
#define TURBOT_TEST_TMPDIR_WITH_CONFIG(name, git_init, config_json) \
    turbot::test::TmpDir name(git_init, nlohmann::json::parse(config_json))

/// 创建默认测试上下文
#define TURBOT_TEST_CONTEXT() \
    turbot::test::TestContext::create_default()

/// 断言权限请求被调用
#define TURBOT_ASSERT_PERMISSION_ASKED(ctx, perm_type) \
    do { \
        bool found = false; \
        for (const auto& req : (ctx).permission_requests) { \
            if (req.contains("permission") && req["permission"] == (perm_type)) { \
                found = true; \
                break; \
            } \
        } \
        REQUIRE(found); \
    } while(0)

/// 断言路径在项目内
#define TURBOT_ASSERT_PATH_CONTAINS(base, path) \
    REQUIRE(turbot::test::TestInstance::instance().contains_path(path))

/// 断言路径不在项目内
#define TURBOT_ASSERT_PATH_NOT_CONTAINS(base, path) \
    REQUIRE_FALSE(turbot::test::TestInstance::instance().contains_path(path))
