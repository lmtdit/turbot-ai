#include <turbot/core/session/retry_manager.hpp>
#include <algorithm>
#include <cmath>
#include <mutex>

namespace turbot::core::session {

// ============================================================================
// APIError Implementation
// ============================================================================

APIError APIError::from_response(int status, const std::string& body) {
    // Parse error from response body (simplified)
    std::optional<std::string> code;
    std::optional<int> retry_after;

    // Try to extract retry-after from body if present
    // In a real implementation, this would parse JSON
    if (body.find("rate_limit") != std::string::npos) {
        code = "rate_limit_exceeded";
    } else if (body.find("overloaded") != std::string::npos) {
        code = "server_overloaded";
    }

    return APIError(status, body, code, retry_after);
}

// ============================================================================
// RetryConfig Implementation
// ============================================================================

bool RetryConfig::validate() const noexcept {
    if (max_attempts < 1) return false;
    if (base_delay_ms < 0) return false;
    if (max_delay_ms < base_delay_ms) return false;
    if (jitter_factor < 0 || jitter_factor > 1) return false;
    if (backoff_multiplier < 1.0) return false;
    return true;
}

// ============================================================================
// RetryManager Implementation
// ============================================================================

std::mt19937& RetryManager::get_rng() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return gen;
}

static std::mutex& get_rng_mutex() {
    static std::mutex m;
    return m;
}

bool RetryManager::is_retryable(const APIError& error) {
    // Rate limit errors are retryable
    if (error.status_code == 429) {
        return true;
    }

    // Server errors (5xx) are retryable
    if (error.status_code >= 500 && error.status_code < 600) {
        return true;
    }

    // Specific error codes that are retryable
    if (error.code) {
        const std::string& code = *error.code;
        if (code == "rate_limit_exceeded" ||
            code == "server_overloaded" ||
            code == "timeout" ||
            code == "temporary_error") {
            return true;
        }
    }

    return false;
}

int RetryManager::calculate_delay(
    int attempt,
    const RetryConfig& config,
    const std::optional<APIError>& error
) {
    // Base exponential backoff: base * multiplier^attempt
    double base_delay = static_cast<double>(config.base_delay_ms) *
                        std::pow(config.backoff_multiplier, attempt);

    // Check for Retry-After header
    if (error && error->retry_after_seconds) {
        int retry_after_ms = (*error->retry_after_seconds) * 1000;
        base_delay = std::max(base_delay, static_cast<double>(retry_after_ms));
    }

    // Apply maximum delay cap
    int delay = static_cast<int>(std::min(base_delay, static_cast<double>(config.max_delay_ms)));

    // Apply jitter
    if (config.jitter_factor > 0) {
        delay = apply_jitter(delay, config.jitter_factor);
    }

    return delay;
}

int RetryManager::apply_jitter(int base_delay, double factor) {
    if (factor <= 0) return base_delay;

    // Calculate jitter range
    int jitter_range = static_cast<int>(base_delay * factor);

    // Generate random jitter (serialised: mt19937 is not thread-safe)
    std::lock_guard<std::mutex> lock(get_rng_mutex());
    std::uniform_int_distribution<> dist(-jitter_range, jitter_range);
    int jitter = dist(get_rng());

    // Apply jitter and ensure non-negative
    return std::max(1, base_delay + jitter);
}

void RetryManager::sleep(int milliseconds) {
    if (milliseconds > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }
}

} // namespace turbot::core::session
