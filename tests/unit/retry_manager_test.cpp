#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <turbot/core/session/retry_manager.hpp>

using namespace turbot::core::session;

TEST_CASE("APIError construction", "[core][session][retry]") {
    SECTION("basic error") {
        APIError error(429, "Rate limit exceeded");
        REQUIRE(error.status_code == 429);
        REQUIRE(std::string(error.what()) == "Rate limit exceeded");
        REQUIRE_FALSE(error.code.has_value());
        REQUIRE_FALSE(error.retry_after_seconds.has_value());
    }

    SECTION("error with code and retry-after") {
        APIError error(503, "Service unavailable", "server_overloaded", 30);
        REQUIRE(error.status_code == 503);
        REQUIRE(error.code == "server_overloaded");
        REQUIRE(error.retry_after_seconds == 30);
    }

    SECTION("from_response") {
        auto error = APIError::from_response(429, "rate_limit exceeded");
        REQUIRE(error.status_code == 429);
        REQUIRE(error.code == "rate_limit_exceeded");
    }
}

TEST_CASE("RetryConfig validation", "[core][session][retry]") {
    SECTION("default config is valid") {
        RetryConfig config;
        REQUIRE(config.validate());
    }

    SECTION("invalid max_attempts") {
        RetryConfig config;
        config.max_attempts = 0;
        REQUIRE_FALSE(config.validate());
    }

    SECTION("invalid delay values") {
        RetryConfig config;
        config.base_delay_ms = -1;
        REQUIRE_FALSE(config.validate());
    }

    SECTION("max_delay less than base_delay") {
        RetryConfig config;
        config.base_delay_ms = 1000;
        config.max_delay_ms = 500;
        REQUIRE_FALSE(config.validate());
    }

    SECTION("invalid jitter factor") {
        RetryConfig config;
        config.jitter_factor = 1.5;
        REQUIRE_FALSE(config.validate());
    }

    SECTION("invalid backoff multiplier") {
        RetryConfig config;
        config.backoff_multiplier = 0.5;
        REQUIRE_FALSE(config.validate());
    }
}

TEST_CASE("RetryManager::is_retryable", "[core][session][retry]") {
    SECTION("retryable errors") {
        REQUIRE(RetryManager::is_retryable({429, "Rate limit"}));
        REQUIRE(RetryManager::is_retryable({500, "Internal error"}));
        REQUIRE(RetryManager::is_retryable({502, "Bad gateway"}));
        REQUIRE(RetryManager::is_retryable({503, "Service unavailable"}));
        REQUIRE(RetryManager::is_retryable({504, "Gateway timeout"}));
    }

    SECTION("non-retryable errors") {
        REQUIRE_FALSE(RetryManager::is_retryable({400, "Bad request"}));
        REQUIRE_FALSE(RetryManager::is_retryable({401, "Unauthorized"}));
        REQUIRE_FALSE(RetryManager::is_retryable({403, "Forbidden"}));
        REQUIRE_FALSE(RetryManager::is_retryable({404, "Not found"}));
        REQUIRE_FALSE(RetryManager::is_retryable({422, "Unprocessable entity"}));
    }

    SECTION("retryable by error code") {
        APIError error1(400, "Temporary issue", "timeout");
        REQUIRE(RetryManager::is_retryable(error1));

        APIError error2(400, "Overloaded", "server_overloaded");
        REQUIRE(RetryManager::is_retryable(error2));

        APIError error3(400, "Rate issue", "rate_limit_exceeded");
        REQUIRE(RetryManager::is_retryable(error3));
    }
}

TEST_CASE("RetryManager::calculate_delay", "[core][session][retry]") {
    RetryConfig config;
    config.base_delay_ms = 1000;
    config.max_delay_ms = 60000;
    config.jitter_factor = 0;  // Disable jitter for predictable tests
    config.backoff_multiplier = 2.0;

    SECTION("exponential backoff") {
        // Without jitter: 1000 * 2^attempt
        int d0 = RetryManager::calculate_delay(0, config);
        REQUIRE(d0 == 1000);

        int d1 = RetryManager::calculate_delay(1, config);
        REQUIRE(d1 == 2000);

        int d2 = RetryManager::calculate_delay(2, config);
        REQUIRE(d2 == 4000);

        int d3 = RetryManager::calculate_delay(3, config);
        REQUIRE(d3 == 8000);
    }

    SECTION("max delay cap") {
        config.max_delay_ms = 5000;

        int d = RetryManager::calculate_delay(10, config);
        REQUIRE(d <= config.max_delay_ms);
    }

    SECTION("retry-after header") {
        APIError error(429, "Rate limit", std::nullopt, 10);  // 10 seconds

        int delay = RetryManager::calculate_delay(0, config, error);
        REQUIRE(delay >= 10000);  // At least 10 seconds
    }

    SECTION("retry-after overrides exponential") {
        config.base_delay_ms = 100;
        APIError error(429, "Rate limit", std::nullopt, 60);  // 60 seconds

        int delay = RetryManager::calculate_delay(0, config, error);
        REQUIRE(delay >= 60000);
    }
}

TEST_CASE("RetryManager::apply_jitter", "[core][session][retry]") {
    SECTION("jitter is applied within range") {
        int base = 1000;
        double factor = 0.1;  // 10%

        // Run multiple times to check range
        for (int i = 0; i < 100; i++) {
            int result = RetryManager::apply_jitter(base, factor);
            REQUIRE(result >= 900);   // 1000 - 100
            REQUIRE(result <= 1100);  // 1000 + 100
        }
    }

    SECTION("zero factor returns base") {
        int result = RetryManager::apply_jitter(1000, 0);
        REQUIRE(result == 1000);
    }

    SECTION("result is always positive") {
        int result = RetryManager::apply_jitter(1, 0.5);
        REQUIRE(result >= 1);
    }
}

TEST_CASE("RetryManager::with_retry success", "[core][session][retry]") {
    SECTION("succeeds on first try") {
        int call_count = 0;

        auto operation = [&call_count]() -> int {
            call_count++;
            return 42;
        };

        RetryConfig config;
        int result = RetryManager::with_retry(operation, config);

        REQUIRE(result == 42);
        REQUIRE(call_count == 1);
    }

    SECTION("succeeds after retries") {
        int call_count = 0;

        auto operation = [&call_count]() -> int {
            call_count++;
            if (call_count < 3) {
                throw APIError(429, "Rate limit");
            }
            return 42;
        };

        RetryConfig config;
        config.base_delay_ms = 1;  // Fast for testing

        int result = RetryManager::with_retry(operation, config);

        REQUIRE(result == 42);
        REQUIRE(call_count == 3);
    }

    SECTION("retry callback is called") {
        int call_count = 0;
        int callback_count = 0;

        auto operation = [&call_count]() -> int {
            call_count++;
            if (call_count < 2) {
                throw APIError(429, "Rate limit");
            }
            return 42;
        };

        auto on_retry = [&callback_count](int attempt, const APIError& error, int delay_ms) {
            callback_count++;
            REQUIRE(attempt == 0);
            REQUIRE(error.status_code == 429);
        };

        RetryConfig config;
        config.base_delay_ms = 1;

        int result = RetryManager::with_retry(operation, config, on_retry);

        REQUIRE(result == 42);
        REQUIRE(callback_count == 1);
    }
}

TEST_CASE("RetryManager::with_retry failure", "[core][session][retry]") {
    SECTION("non-retryable error throws immediately") {
        int call_count = 0;

        auto operation = [&call_count]() -> int {
            call_count++;
            throw APIError(400, "Bad request");
        };

        RetryConfig config;

        REQUIRE_THROWS_AS(RetryManager::with_retry(operation, config), APIError);
        REQUIRE(call_count == 1);
    }

    SECTION("exhausts retries") {
        int call_count = 0;

        auto operation = [&call_count]() -> int {
            call_count++;
            throw APIError(429, "Rate limit");
        };

        RetryConfig config;
        config.max_attempts = 3;
        config.base_delay_ms = 1;

        REQUIRE_THROWS_AS(RetryManager::with_retry(operation, config), APIError);
        REQUIRE(call_count == 3);
    }

    SECTION("throws last error on exhaustion") {
        auto operation = []() -> int {
            throw APIError(503, "Service unavailable", "server_overloaded");
        };

        RetryConfig config;
        config.max_attempts = 2;
        config.base_delay_ms = 1;

        try {
            RetryManager::with_retry(operation, config);
            FAIL("Should have thrown");
        } catch (const APIError& e) {
            REQUIRE(e.status_code == 503);
            REQUIRE(e.code == "server_overloaded");
        }
    }
}
