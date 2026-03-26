#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/message/token_usage.hpp>
#include <turbot/core/provider/provider.hpp>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <mutex>
#include <optional>

namespace turbot::core::session {

/// Cost breakdown information
struct TURBOT_CORE_API CostInfo {
    double input_cost = 0.0;       ///< Cost for input tokens
    double output_cost = 0.0;      ///< Cost for output tokens
    double cache_read_cost = 0.0;  ///< Cost for cache read tokens
    double cache_write_cost = 0.0; ///< Cost for cache write tokens
    double reasoning_cost = 0.0;   ///< Cost for reasoning tokens
    double total_cost = 0.0;       ///< Total cost

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static CostInfo from_json(const nlohmann::json& j);

    /// Add another CostInfo
    CostInfo& operator+=(const CostInfo& other) noexcept;

    /// Subtract another CostInfo
    CostInfo& operator-=(const CostInfo& other) noexcept;
};

/// Session usage statistics
struct TURBOT_CORE_API SessionUsage {
    TokenUsage tokens;    ///< Token usage
    CostInfo cost;                 ///< Cost information
    int64_t message_count = 0;     ///< Number of messages
    int64_t tool_calls = 0;        ///< Number of tool calls
    int64_t request_count = 0;     ///< Number of API requests

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static SessionUsage from_json(const nlohmann::json& j);

    /// Add another SessionUsage
    SessionUsage& operator+=(const SessionUsage& other) noexcept;
};

/// Usage tracker for sessions
class TURBOT_CORE_API UsageTracker {
public:
    UsageTracker() = default;

    // ===== Static Utilities =====

    /// Calculate token usage from raw API response
    /// @param raw_usage Raw usage data from API
    /// @param metadata Optional metadata (e.g., reasoning tokens)
    /// @return TokenUsage structure
    [[nodiscard]] static TokenUsage calculate_usage(
        const nlohmann::json& raw_usage,
        const std::optional<nlohmann::json>& metadata = std::nullopt
    );

    /// G64: Calculate token usage with provider-aware adjustedInput logic.
    ///
    /// Mirrors OpenCode Session.getUsage():
    ///   - Anthropic / Bedrock: inputTokens does NOT include cached tokens
    ///     → adjustedInput = inputTokens (no subtraction)
    ///   - Other providers: inputTokens INCLUDES cached tokens
    ///     → adjustedInput = inputTokens - cacheRead - cacheWrite
    ///
    /// @param raw_usage   Raw usage JSON from AI SDK
    /// @param metadata    Provider metadata (anthropic/bedrock key presence controls logic)
    /// @param provider_id Provider ID — "anthropic" / "amazon-bedrock" → excludesCachedTokens
    /// @return Adjusted TokenUsage with corrected input token count
    [[nodiscard]] static TokenUsage get_usage(
        const nlohmann::json& raw_usage,
        const std::optional<nlohmann::json>& metadata,
        const std::string& provider_id
    );

    /// Calculate cost based on model pricing
    /// @param model Model information with pricing
    /// @param usage Token usage
    /// @return Cost breakdown
    [[nodiscard]] static CostInfo calculate_cost(
        const provider::ModelInfo& model,
        const TokenUsage& usage
    );

    /// G65: Calculate cost with over-200K token pricing support.
    ///
    /// Mirrors OpenCode Session.getUsage() costInfo selection:
    ///   if (cost?.experimentalOver200K && tokens.input + tokens.cache.read > 200_000)
    ///       use experimentalOver200K pricing
    ///
    /// @param model  Model information (pricing.experimentalOver200K checked)
    /// @param usage  Token usage (input + cache.read determines tier)
    /// @return Cost breakdown using correct pricing tier
    [[nodiscard]] static CostInfo calculate_cost_tiered(
        const provider::ModelInfo& model,
        const TokenUsage& usage
    );

    /// Calculate cost using explicit pricing
    /// @param pricing Pricing JSON {"input": 0.01, "output": 0.03, ...}
    /// @param usage Token usage
    /// @return Cost breakdown
    [[nodiscard]] static CostInfo calculate_cost(
        const nlohmann::json& pricing,
        const TokenUsage& usage
    );

    // ===== Instance Methods =====

    /// Add usage for a session
    /// @param session_id Session identifier
    /// @param usage Token usage to add
    /// @param cost Cost information (optional, will be calculated if not provided)
    void add_usage(
        const std::string& session_id,
        const TokenUsage& usage,
        const std::optional<CostInfo>& cost = std::nullopt
    );

    /// Add usage with model pricing
    /// @param session_id Session identifier
    /// @param usage Token usage to add
    /// @param model Model for cost calculation
    void add_usage(
        const std::string& session_id,
        const TokenUsage& usage,
        const provider::ModelInfo& model
    );

    /// Increment message count for a session
    void increment_message_count(const std::string& session_id);

    /// Increment tool call count for a session
    void increment_tool_calls(const std::string& session_id);

    /// Increment request count for a session
    void increment_request_count(const std::string& session_id);

    /// Get usage for a specific session
    [[nodiscard]] SessionUsage get_session_usage(const std::string& session_id) const;

    /// Get total usage across all sessions
    [[nodiscard]] SessionUsage get_total_usage() const;

    /// Check if a session exists
    [[nodiscard]] bool has_session(const std::string& session_id) const;

    /// Get all session IDs
    [[nodiscard]] std::vector<std::string> get_session_ids() const;

    /// Reset usage for a specific session
    void reset_session(const std::string& session_id);

    /// Reset all sessions
    void reset_all();

    /// Get number of sessions
    [[nodiscard]] size_t session_count() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, SessionUsage> sessions_;
    SessionUsage total_;

    /// Get or create session usage
    SessionUsage& get_or_create_session(const std::string& session_id);
};

} // namespace turbot::core::session
