#pragma once

#include <turbot/core/common/export.hpp>
#include <nlohmann/json.hpp>
#include <cstdint>

namespace turbot::core {

/// Token usage statistics for AI API calls
struct TURBOT_CORE_API TokenUsage {
    int64_t input = 0;       ///< Input tokens (prompt)
    int64_t output = 0;      ///< Output tokens (completion)
    int64_t reasoning = 0;   ///< Reasoning tokens (for o1 models)
    
    /// Cache token usage
    struct CacheUsage {
        int64_t read = 0;    ///< Cache read tokens
        int64_t write = 0;   ///< Cache write tokens
    } cache;

    /// Get total token count
    [[nodiscard]] int64_t total() const noexcept {
        return input + output + reasoning + cache.read + cache.write;
    }

    /// Calculate cost based on pricing
    /// @param pricing JSON with per-token prices: {"input": 0.01, "output": 0.03, ...}
    /// @return Total cost in currency units
    [[nodiscard]] double cost(const nlohmann::json& pricing) const {
        double total_cost = 0.0;
        total_cost += input * pricing.value("input", 0.0);
        total_cost += output * pricing.value("output", 0.0);
        total_cost += reasoning * pricing.value("reasoning", 0.0);
        total_cost += cache.read * pricing.value("cache_read", 0.0);
        total_cost += cache.write * pricing.value("cache_write", 0.0);
        return total_cost;
    }

    /// Add another TokenUsage to this one
    TokenUsage& operator+=(const TokenUsage& other) noexcept {
        input += other.input;
        output += other.output;
        reasoning += other.reasoning;
        cache.read += other.cache.read;
        cache.write += other.cache.write;
        return *this;
    }

    /// Create a combined TokenUsage
    [[nodiscard]] TokenUsage operator+(const TokenUsage& other) const noexcept {
        TokenUsage result = *this;
        result += other;
        return result;
    }

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const {
        return nlohmann::json{
            {"input", input},
            {"output", output},
            {"reasoning", reasoning},
            {"cache", {
                {"read", cache.read},
                {"write", cache.write}
            }}
        };
    }

    /// Deserialize from JSON
    static TokenUsage from_json(const nlohmann::json& j) {
        TokenUsage usage;
        usage.input = j.value("input", int64_t{0});
        usage.output = j.value("output", int64_t{0});
        usage.reasoning = j.value("reasoning", int64_t{0});
        if (j.contains("cache")) {
            usage.cache.read = j["cache"].value("read", int64_t{0});
            usage.cache.write = j["cache"].value("write", int64_t{0});
        }
        return usage;
    }
};

} // namespace turbot::core
