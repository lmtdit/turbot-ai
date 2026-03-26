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
    std::optional<int>    retry_after_seconds;  ///< Retry-After header value (seconds)
    std::optional<double> retry_after_ms;       ///< Retry-After-Ms header value (milliseconds, mirrors opencode retry.ts)

    APIError(int status, const std::string& message,
             std::optional<std::string> error_code = std::nullopt,
             std::optional<int> retry_after = std::nullopt,
             std::optional<double> retry_after_ms_val = std::nullopt)
        : std::runtime_error(message), status_code(status),
          code(std::move(error_code)), retry_after_seconds(retry_after),
          retry_after_ms(retry_after_ms_val) {}

    /// Create from HTTP response
    static APIError from_response(int status, const std::string& body);
};

/// 专用中止信号：由 operation 或 on_retry 回调抛出，干净地中止重试序列。
/// 不经过 is_retryable 过滤，直接穿透 with_retry 传播给调用者。
struct TURBOT_CORE_API AbortRetryException : public std::exception {
    const char* what() const noexcept override { return "Retry aborted by caller"; }
};

/// 重试配置
struct TURBOT_CORE_API RetryConfig {
    int max_attempts = 5;           ///< 最大重试次数
    int base_delay_ms = 2000;       ///< 基础延迟（毫秒）mirrors opencode RETRY_INITIAL_DELAY = 2000
    int max_delay_ms = 30000;       ///< 无 headers 时最大延迟（毫秒）mirrors opencode RETRY_MAX_DELAY_NO_HEADERS = 30_000
    double jitter_factor = 0.1;     ///< 抖动因子（0-1）
    double backoff_multiplier = 2.0;///< 退避倍数 mirrors opencode RETRY_BACKOFF_FACTOR = 2

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
        // State extracted from catch block to use after it closes
        bool should_retry = false;
        int  retry_delay  = 0;

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

            // 提取重试信息（在 catch 块内），稍后在块外使用
            retry_delay  = calculate_delay(attempt, config, e);
            should_retry = true;
        }
        // ↑ catch 块在此关闭。此后 on_retry 抛出的任何异常都不会被本层重捕获，
        //   会干净地向上传播（包括 AbortRetryException）。

        if (should_retry) {
            // 调用重试通知回调：可安全抛出 AbortRetryException 或其他异常
            if (on_retry) {
                on_retry(attempt, last_error, retry_delay);
            }
            // 等待后重试
            sleep(retry_delay);
        }
    }

    throw last_error;
}

} // namespace turbot::core::session
