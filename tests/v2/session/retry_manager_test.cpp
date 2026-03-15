/**
 * @file retry_manager_test.cpp
 * @brief Retry Manager 测试
 */

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/session/retry_manager.hpp>
#include <thread>
#include <chrono>

namespace turbot::test {

using namespace turbot::core::session;

// ============================================================================
// APIError 测试
// ============================================================================

TEST_CASE("Retry.APIError.Construct", "[Retry]") {
    APIError error(429, "Rate limit exceeded", "rate_limit", 60);

    REQUIRE(error.status_code == 429);
    REQUIRE(std::string(error.what()) == "Rate limit exceeded");
    REQUIRE(error.code.has_value());
    REQUIRE(error.code.value() == "rate_limit");
    REQUIRE(error.retry_after_seconds.has_value());
    REQUIRE(error.retry_after_seconds.value() == 60);
}

TEST_CASE("Retry.APIError.FromResponse", "[Retry]") {
    auto error = APIError::from_response(429, "rate_limit exceeded");

    REQUIRE(error.status_code == 429);
    REQUIRE(error.code.has_value());
    REQUIRE(error.code.value() == "rate_limit_exceeded");
}

TEST_CASE("Retry.APIError.FromResponse.ServerOverloaded", "[Retry]") {
    auto error = APIError::from_response(503, "server_overloaded");

    REQUIRE(error.status_code == 503);
    REQUIRE(error.code.has_value());
    REQUIRE(error.code.value() == "server_overloaded");
}

// ============================================================================
// RetryConfig 测试
// ============================================================================

TEST_CASE("Retry.Config.Default", "[Retry]") {
    RetryConfig config;

    REQUIRE(config.max_attempts == 3);
    REQUIRE(config.base_delay_ms == 1000);
    REQUIRE(config.max_delay_ms == 60000);
    REQUIRE(config.jitter_factor == 0.1);
    REQUIRE(config.backoff_multiplier == 2.0);
}

TEST_CASE("Retry.Config.Validate.Valid", "[Retry]") {
    RetryConfig config;
    config.max_attempts = 5;
    config.base_delay_ms = 500;
    config.max_delay_ms = 30000;
    config.jitter_factor = 0.2;
    config.backoff_multiplier = 1.5;

    REQUIRE(config.validate());
}

TEST_CASE("Retry.Config.Validate.InvalidMaxAttempts", "[Retry]") {
    RetryConfig config;
    config.max_attempts = 0;

    REQUIRE_FALSE(config.validate());
}

TEST_CASE("Retry.Config.Validate.InvalidBaseDelay", "[Retry]") {
    RetryConfig config;
    config.base_delay_ms = -100;

    REQUIRE_FALSE(config.validate());
}

TEST_CASE("Retry.Config.Validate.InvalidMaxDelay", "[Retry]") {
    RetryConfig config;
    config.base_delay_ms = 1000;
    config.max_delay_ms = 500;  // 小于 base_delay

    REQUIRE_FALSE(config.validate());
}

TEST_CASE("Retry.Config.Validate.InvalidJitter", "[Retry]") {
    RetryConfig config;
    config.jitter_factor = 1.5;  // > 1

    REQUIRE_FALSE(config.validate());
}

TEST_CASE("Retry.Config.Validate.InvalidBackoff", "[Retry]") {
    RetryConfig config;
    config.backoff_multiplier = 0.5;  // < 1

    REQUIRE_FALSE(config.validate());
}

// ============================================================================
// RetryManager IsRetryable 测试
// ============================================================================

TEST_CASE("Retry.Manager.IsRetryable.RateLimit", "[Retry]") {
    APIError error(429, "Rate limit");
    REQUIRE(RetryManager::is_retryable(error));
}

TEST_CASE("Retry.Manager.IsRetryable.ServerError", "[Retry]") {
    APIError error(500, "Internal server error");
    REQUIRE(RetryManager::is_retryable(error));

    APIError error2(502, "Bad gateway");
    REQUIRE(RetryManager::is_retryable(error2));

    APIError error3(503, "Service unavailable");
    REQUIRE(RetryManager::is_retryable(error3));

    APIError error4(504, "Gateway timeout");
    REQUIRE(RetryManager::is_retryable(error4));
}

TEST_CASE("Retry.Manager.IsRetryable.ClientError", "[Retry]") {
    APIError error(400, "Bad request");
    REQUIRE_FALSE(RetryManager::is_retryable(error));

    APIError error2(401, "Unauthorized");
    REQUIRE_FALSE(RetryManager::is_retryable(error2));

    APIError error3(403, "Forbidden");
    REQUIRE_FALSE(RetryManager::is_retryable(error3));

    APIError error4(404, "Not found");
    REQUIRE_FALSE(RetryManager::is_retryable(error4));
}

TEST_CASE("Retry.Manager.IsRetryable.ErrorCode", "[Retry]") {
    APIError error(500, "Error", "rate_limit_exceeded");
    REQUIRE(RetryManager::is_retryable(error));

    APIError error2(500, "Error", "server_overloaded");
    REQUIRE(RetryManager::is_retryable(error2));

    APIError error3(500, "Error", "timeout");
    REQUIRE(RetryManager::is_retryable(error3));

    APIError error4(500, "Error", "temporary_error");
    REQUIRE(RetryManager::is_retryable(error4));

    APIError error5(500, "Error", "invalid_request");
    REQUIRE_FALSE(RetryManager::is_retryable(error5));
}

// ============================================================================
// RetryManager CalculateDelay 测试
// ============================================================================

TEST_CASE("Retry.Manager.CalculateDelay.Basic", "[Retry]") {
    RetryConfig config;
    config.base_delay_ms = 1000;
    config.max_delay_ms = 60000;
    config.jitter_factor = 0;  // 禁用抖动以便精确测试
    config.backoff_multiplier = 2.0;

    // 第一次尝试: 1000ms
    int delay0 = RetryManager::calculate_delay(0, config, std::nullopt);
    REQUIRE(delay0 == 1000);

    // 第二次尝试: 2000ms
    int delay1 = RetryManager::calculate_delay(1, config, std::nullopt);
    REQUIRE(delay1 == 2000);

    // 第三次尝试: 4000ms
    int delay2 = RetryManager::calculate_delay(2, config, std::nullopt);
    REQUIRE(delay2 == 4000);
}

TEST_CASE("Retry.Manager.CalculateDelay.MaxDelay", "[Retry]") {
    RetryConfig config;
    config.base_delay_ms = 1000;
    config.max_delay_ms = 10000;  // 限制最大延迟
    config.jitter_factor = 0;
    config.backoff_multiplier = 2.0;

    // 高次尝试应该被限制在 max_delay
    int delay10 = RetryManager::calculate_delay(10, config, std::nullopt);
    REQUIRE(delay10 <= 10000);
}

TEST_CASE("Retry.Manager.CalculateDelay.RetryAfter", "[Retry]") {
    RetryConfig config;
    config.base_delay_ms = 1000;
    config.max_delay_ms = 60000;
    config.jitter_factor = 0;
    config.backoff_multiplier = 2.0;

    APIError error(429, "Rate limit", std::nullopt, 30);  // Retry-After: 30 秒

    // 应该使用 Retry-After 值
    int delay = RetryManager::calculate_delay(0, config, error);
    REQUIRE(delay >= 30000);  // 30 秒 = 30000 毫秒
}

TEST_CASE("Retry.Manager.CalculateDelay.Jitter", "[Retry]") {
    RetryConfig config;
    config.base_delay_ms = 1000;
    config.max_delay_ms = 60000;
    config.jitter_factor = 0.5;  // 50% 抖动
    config.backoff_multiplier = 2.0;

    // 多次计算延迟，验证抖动生效
    std::vector<int> delays;
    for (int i = 0; i < 10; ++i) {
        delays.push_back(RetryManager::calculate_delay(0, config, std::nullopt));
    }

    // 检查延迟值有变化（抖动生效）
    bool has_variation = false;
    for (size_t i = 1; i < delays.size(); ++i) {
        if (delays[i] != delays[0]) {
            has_variation = true;
            break;
        }
    }
    REQUIRE(has_variation);
}

// ============================================================================
// RetryManager WithRetry 测试
// ============================================================================

TEST_CASE("Retry.Manager.WithRetry.Success", "[Retry]") {
    RetryConfig config;
    config.max_attempts = 3;
    config.base_delay_ms = 10;  // 短延迟加速测试
    config.jitter_factor = 0;

    int call_count = 0;
    auto result = RetryManager::with_retry(
        [&call_count]() -> int {
            call_count++;
            return 42;  // 成功
        },
        config,
        std::function<void(int, const APIError&, int)>(
            [](int attempt, const APIError& err, int delay_ms) {
                (void)attempt;
                (void)err;
                (void)delay_ms;
            }
        )
    );

    REQUIRE(result == 42);
    REQUIRE(call_count == 1);  // 只调用一次
}

TEST_CASE("Retry.Manager.WithRetry.RetryThenSuccess", "[Retry]") {
    RetryConfig config;
    config.max_attempts = 3;
    config.base_delay_ms = 10;
    config.jitter_factor = 0;

    int call_count = 0;
    auto result = RetryManager::with_retry(
        [&call_count]() -> int {
            call_count++;
            if (call_count < 3) {
                throw APIError(429, "Rate limit");
            }
            return 42;
        },
        config,
        std::function<void(int, const APIError&, int)>(
            [](int attempt, const APIError& err, int delay_ms) {
                (void)attempt;
                (void)err;
                (void)delay_ms;
            }
        )
    );

    REQUIRE(result == 42);
    REQUIRE(call_count == 3);
}

TEST_CASE("Retry.Manager.WithRetry.Exhausted", "[Retry]") {
    RetryConfig config;
    config.max_attempts = 3;
    config.base_delay_ms = 10;
    config.jitter_factor = 0;

    int call_count = 0;
    int retry_callback_count = 0;

    REQUIRE_THROWS_AS(
        RetryManager::with_retry(
            [&call_count]() -> int {
                call_count++;
                throw APIError(429, "Rate limit");
            },
            config,
            std::function<void(int, const APIError&, int)>(
                [&retry_callback_count, &config](int attempt, const APIError& err, int delay_ms) {
                    (void)err;
                    (void)delay_ms;
                    retry_callback_count++;
                    REQUIRE(attempt <= config.max_attempts);
                }
            )
        ),
        APIError
    );

    REQUIRE(call_count == 3);
    REQUIRE(retry_callback_count == 2);  // 重试回调在失败后调用，最后一次失败不触发
}

TEST_CASE("Retry.Manager.WithRetry.NonRetryable", "[Retry]") {
    RetryConfig config;
    config.max_attempts = 3;
    config.base_delay_ms = 10;
    config.jitter_factor = 0;

    int call_count = 0;

    REQUIRE_THROWS_AS(
        RetryManager::with_retry(
            [&call_count]() -> int {
                call_count++;
                throw APIError(400, "Bad request");  // 不可重试
            },
            config,
            std::function<void(int, const APIError&, int)>(
                [](int attempt, const APIError& err, int delay_ms) {
                    (void)attempt;
                    (void)err;
                    (void)delay_ms;
                }
            )
        ),
        APIError
    );

    REQUIRE(call_count == 1);  // 不可重试错误只调用一次
}

// ============================================================================
// RetryManager AbortRetryException 测试
// ============================================================================

TEST_CASE("Retry.Manager.AbortRetry", "[Retry]") {
    RetryConfig config;
    config.max_attempts = 10;
    config.base_delay_ms = 10;
    config.jitter_factor = 0;

    int call_count = 0;

    REQUIRE_THROWS_AS(
        RetryManager::with_retry(
            [&call_count]() -> int {
                call_count++;
                if (call_count == 2) {
                    throw AbortRetryException();
                }
                throw APIError(429, "Rate limit");
            },
            config,
            std::function<void(int, const APIError&, int)>(
                [](int attempt, const APIError& err, int delay_ms) {
                    (void)attempt;
                    (void)err;
                    (void)delay_ms;
                }
            )
        ),
        AbortRetryException
    );

    REQUIRE(call_count == 2);  // 在第二次调用时中止
}

// ============================================================================
// RetryManager Sleep 测试
// ============================================================================

TEST_CASE("Retry.Manager.Sleep", "[Retry]") {
    auto start = std::chrono::steady_clock::now();

    RetryManager::sleep(50);  // 睡眠 50ms

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    REQUIRE(duration.count() >= 50);
}

TEST_CASE("Retry.Manager.Sleep.Zero", "[Retry]") {
    auto start = std::chrono::steady_clock::now();

    RetryManager::sleep(0);

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    REQUIRE(duration.count() < 10);  // 应该几乎立即返回
}

// ============================================================================
// RetryManager 指数退避测试
// ============================================================================

TEST_CASE("Retry.Manager.ExponentialBackoff", "[Retry]") {
    RetryConfig config;
    config.base_delay_ms = 100;
    config.max_delay_ms = 100000;
    config.jitter_factor = 0;
    config.backoff_multiplier = 2.0;

    std::vector<int> delays;
    for (int i = 0; i < 10; ++i) {
        delays.push_back(RetryManager::calculate_delay(i, config, std::nullopt));
    }

    // 验证指数增长
    for (size_t i = 1; i < delays.size(); ++i) {
        REQUIRE(delays[i] == delays[i-1] * 2);
    }
}

TEST_CASE("Retry.Manager.CustomBackoffMultiplier", "[Retry]") {
    RetryConfig config;
    config.base_delay_ms = 100;
    config.max_delay_ms = 100000;
    config.jitter_factor = 0;
    config.backoff_multiplier = 1.5;  // 自定义乘数

    int delay0 = RetryManager::calculate_delay(0, config, std::nullopt);
    int delay1 = RetryManager::calculate_delay(1, config, std::nullopt);
    int delay2 = RetryManager::calculate_delay(2, config, std::nullopt);

    REQUIRE(delay0 == 100);
    REQUIRE(delay1 == 150);
    REQUIRE(delay2 == 225);
}

} // namespace turbot::test
