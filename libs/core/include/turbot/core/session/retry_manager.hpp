#pragma once

#include <turbot/core/common/export.hpp>
#include <chrono>
#include <functional>
#include <optional>
#include <random>
#include <thread>
#include <stdexcept>

namespace turbot::core::session {

/// API 错误信息
struct TURBOT_CORE_API APIError : public std::runtime_error {
    int status_code = 0;
    std::optional<std::string> code;
    std::optional<int> retry_after_seconds;

    APIError(int status, const std::string& message,
             std::optional<std::string> error_code = std::nullopt,
             std::optional<int> retry_after = std::nullopt)
        : std::runtime_error(message), status_code(status),
          code(std::move(error_code)), retry_after_seconds(retry_after) {}

    /// Create from HTTP response
    static APIError from_response(int status, const std::string& body);
};

/// 重试配置
struct TURBOT_CORE_API RetryConfig {
    int max_attempts = 5;           ///< 最大重试次数
    int base_delay_ms = 1000;       ///< 基础延迟（毫秒）
    int max_delay_ms = 60000;       ///< 最大延迟（毫秒）
    double jitter_factor = 0.1;     ///< 抖动因子（0-1）
    double backoff_multiplier = 2.0;///< 退避倍数

    /// Validate configuration
    [[nodiscard]] bool validate() const noexcept;
};

/// 重试统计
struct TURBOT_CORE_API RetryStats {
    int total_attempts = 0;
    int successful_retry = 0;
    int total_delay_ms = 0;
    std::string last_error_message;
};

/// 重试管理器
class TURBOT_CORE_API RetryManager {
public:
    /// 检查错误是否可重试
    /// @param error API 错误
    /// @return true 如果可以重试
    [[nodiscard]] static bool is_retryable(const APIError& error);

    /// 计算重试延迟（毫秒）
    /// @param attempt 当前尝试次数（从0开始）
    /// @param config 重试配置
    /// @param error 可选的错误信息（用于 Retry-After）
    /// @return 延迟毫秒数
    [[nodiscard]] static int calculate_delay(
        int attempt,
        const RetryConfig& config = {},
        const std::optional<APIError>& error = std::nullopt
    );

    /// 执行带重试的操作（同步版本）
    /// @param operation 要执行的操作
    /// @param config 重试配置
    /// @param on_retry 重试时的回调（可选）
    /// @return 操作结果
    /// @throws APIError 如果所有重试都失败
    template<typename Func>
    static auto with_retry(
        Func&& operation,
        const RetryConfig& config = {},
        std::function<void(int attempt, const APIError& error, int delay_ms)> on_retry = nullptr
    ) -> decltype(operation());

    /// 等待指定时间
    /// @param milliseconds 等待毫秒数
    static void sleep(int milliseconds);

    /// 获取随机抖动值
    /// @param base_delay 基础延迟
    /// @param factor 抖动因子
    /// @return 应用抖动后的延迟
    [[nodiscard]] static int apply_jitter(int base_delay, double factor);

private:
    static std::mt19937& get_rng();
};

// ============================================================================
// 模板实现
// ============================================================================

template<typename Func>
auto RetryManager::with_retry(
    Func&& operation,
    const RetryConfig& config,
    std::function<void(int attempt, const APIError& error, int delay_ms)> on_retry
) -> decltype(operation()) {
    using ResultType = decltype(operation());

    APIError last_error{0, "Unknown error"};

    for (int attempt = 0; attempt < config.max_attempts; attempt++) {
        try {
            return operation();
        } catch (const APIError& e) {
            last_error = e;

            // 检查是否可重试
            if (!is_retryable(e)) {
                throw;
            }

            // 检查是否已达到最大尝试次数
            if (attempt == config.max_attempts - 1) {
                throw;
            }

            // 计算延迟
            int delay = calculate_delay(attempt, config, e);

            // 调用回调
            if (on_retry) {
                on_retry(attempt, e, delay);
            }

            // 等待
            sleep(delay);
        }
    }

    throw last_error;
}

} // namespace turbot::core::session
