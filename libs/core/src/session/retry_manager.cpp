#include <turbot/core/session/retry_manager.hpp>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <mutex>
#include <chrono>

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
    // FreeUsageLimitError: not retryable — mirrors opencode retry.ts retryable() L66-68.
    // When the provider returns this error, the user must add credits; retrying is pointless.
    // We check status_code == 0 (streamed body error) or any status if message mentions it.
    if (std::string(error.what()).find("FreeUsageLimitError") != std::string::npos) {
        return false;
    }

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
    // Mirrors opencode SessionRetry.delay() in retry.ts:
    //
    //   if (error && headers) {
    //     1. retry-after-ms header (ms level) → return directly, no cap
    //     2. retry-after header (seconds) → convert to ms, no cap
    //     3. retry-after as HTTP Date → Date.parse - Date.now(), no cap
    //     fallback → exponential, no cap (RETRY_MAX_DELAY only = 2^31-1)
    //   }
    //   no headers → Math.min(exponential, RETRY_MAX_DELAY_NO_HEADERS)

    const double exponential_delay =
        static_cast<double>(config.base_delay_ms) *
        std::pow(config.backoff_multiplier, attempt);

    // ── Has Retry-After headers (any of ms / seconds / date) ──────────────
    // When any Retry-After header is present, opencode does NOT apply the
    // RETRY_MAX_DELAY_NO_HEADERS cap (only the theoretical 2^31-1 ceiling).
    // turbot mirrors this: skip max_delay_ms cap when headers are present.

    if (error) {
        // Priority 1: retry-after-ms (milliseconds, direct)
        // Mirrors: const retryAfterMs = headers["retry-after-ms"]
        if (error->retry_after_ms && *error->retry_after_ms > 0) {
            const double ms = *error->retry_after_ms;
            const int raw_delay = static_cast<int>(std::ceil(ms));
            // No RETRY_MAX_DELAY_NO_HEADERS cap when headers present (opencode behavior).
            // Apply jitter only if the caller configured it (default 0.1).
            if (config.jitter_factor > 0) {
                return apply_jitter(raw_delay, config.jitter_factor);
            }
            return raw_delay;
        }

        // Priority 2 & 3: retry-after in seconds or as HTTP Date
        // Mirrors: const retryAfter = headers["retry-after"]
        if (error->retry_after_seconds) {
            int retry_after_ms_val = 0;
            if (*error->retry_after_seconds > 0) {
                // Numeric seconds → convert to ms (mirrors Math.ceil(parsedSeconds * 1000))
                retry_after_ms_val = (*error->retry_after_seconds) * 1000;
            }
            // If retry_after_seconds == 0 it might represent an HTTP Date that was
            // pre-calculated by the HTTP layer and stored as remaining ms; treat
            // exponential as fallback in that case.
            if (retry_after_ms_val > 0) {
                // Use max(exponential, retry_after) to never wait less than back-off
                const double base = std::max(exponential_delay,
                                             static_cast<double>(retry_after_ms_val));
                // No RETRY_MAX_DELAY_NO_HEADERS cap (headers present — opencode behavior).
                const int raw_delay = static_cast<int>(base);
                if (config.jitter_factor > 0) {
                    return apply_jitter(raw_delay, config.jitter_factor);
                }
                return raw_delay;
            }
        }

        // Fallback with headers present: exponential, no RETRY_MAX_DELAY_NO_HEADERS cap.
        // Mirrors opencode: return RETRY_INITIAL_DELAY * Math.pow(RETRY_BACKOFF_FACTOR, attempt - 1)
        // (inside the `if (headers)` branch).
        if (error->retry_after_seconds.has_value() || error->retry_after_ms.has_value()) {
            const int raw_delay = static_cast<int>(exponential_delay);
            if (config.jitter_factor > 0) {
                return apply_jitter(raw_delay, config.jitter_factor);
            }
            return raw_delay;
        }
    }

    // ── No headers: apply RETRY_MAX_DELAY_NO_HEADERS cap ──────────────────
    // Mirrors: return Math.min(RETRY_INITIAL_DELAY * Math.pow(...), RETRY_MAX_DELAY_NO_HEADERS)
    int delay = static_cast<int>(
        std::min(exponential_delay, static_cast<double>(config.max_delay_ms)));

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
