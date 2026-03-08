#include "turbot/core/session/usage_tracker.hpp"
#include <algorithm>

namespace turbot::core::session {

// ===== CostInfo =====

nlohmann::json CostInfo::to_json() const {
    return nlohmann::json{
        {"input_cost", input_cost},
        {"output_cost", output_cost},
        {"cache_read_cost", cache_read_cost},
        {"cache_write_cost", cache_write_cost},
        {"reasoning_cost", reasoning_cost},
        {"total_cost", total_cost}
    };
}

CostInfo CostInfo::from_json(const nlohmann::json& j) {
    CostInfo info;
    info.input_cost = j.value("input_cost", 0.0);
    info.output_cost = j.value("output_cost", 0.0);
    info.cache_read_cost = j.value("cache_read_cost", 0.0);
    info.cache_write_cost = j.value("cache_write_cost", 0.0);
    info.reasoning_cost = j.value("reasoning_cost", 0.0);
    info.total_cost = j.value("total_cost", 0.0);
    return info;
}

CostInfo& CostInfo::operator+=(const CostInfo& other) noexcept {
    input_cost += other.input_cost;
    output_cost += other.output_cost;
    cache_read_cost += other.cache_read_cost;
    cache_write_cost += other.cache_write_cost;
    reasoning_cost += other.reasoning_cost;
    total_cost += other.total_cost;
    return *this;
}

CostInfo& CostInfo::operator-=(const CostInfo& other) noexcept {
    input_cost -= other.input_cost;
    output_cost -= other.output_cost;
    cache_read_cost -= other.cache_read_cost;
    cache_write_cost -= other.cache_write_cost;
    reasoning_cost -= other.reasoning_cost;
    total_cost -= other.total_cost;
    return *this;
}

// ===== SessionUsage =====

nlohmann::json SessionUsage::to_json() const {
    return nlohmann::json{
        {"tokens", tokens.to_json()},
        {"cost", cost.to_json()},
        {"message_count", message_count},
        {"tool_calls", tool_calls},
        {"request_count", request_count}
    };
}

SessionUsage SessionUsage::from_json(const nlohmann::json& j) {
    SessionUsage usage;
    if (j.contains("tokens")) {
        usage.tokens = TokenUsage::from_json(j["tokens"]);
    }
    if (j.contains("cost")) {
        usage.cost = CostInfo::from_json(j["cost"]);
    }
    usage.message_count = j.value("message_count", int64_t{0});
    usage.tool_calls = j.value("tool_calls", int64_t{0});
    usage.request_count = j.value("request_count", int64_t{0});
    return usage;
}

SessionUsage& SessionUsage::operator+=(const SessionUsage& other) noexcept {
    tokens += other.tokens;
    cost += other.cost;
    message_count += other.message_count;
    tool_calls += other.tool_calls;
    request_count += other.request_count;
    return *this;
}

// ===== UsageTracker Static Methods =====

TokenUsage UsageTracker::calculate_usage(
    const nlohmann::json& raw_usage,
    const std::optional<nlohmann::json>& metadata
) {
    TokenUsage usage;

    // Handle different API response formats
    // OpenAI format: {"prompt_tokens": 100, "completion_tokens": 50}
    // Anthropic format: {"input_tokens": 100, "output_tokens": 50}

    // Input tokens
    if (raw_usage.contains("prompt_tokens")) {
        usage.input = raw_usage["prompt_tokens"].get<int64_t>();
    } else if (raw_usage.contains("input_tokens")) {
        usage.input = raw_usage["input_tokens"].get<int64_t>();
    } else if (raw_usage.contains("promptTokens")) {
        usage.input = raw_usage["promptTokens"].get<int64_t>();
    }

    // Output tokens
    if (raw_usage.contains("completion_tokens")) {
        usage.output = raw_usage["completion_tokens"].get<int64_t>();
    } else if (raw_usage.contains("output_tokens")) {
        usage.output = raw_usage["output_tokens"].get<int64_t>();
    } else if (raw_usage.contains("completionTokens")) {
        usage.output = raw_usage["completionTokens"].get<int64_t>();
    }

    // Reasoning tokens (from metadata, for o1 models)
    if (metadata.has_value()) {
        if (metadata->contains("reasoning_tokens")) {
            usage.reasoning = (*metadata)["reasoning_tokens"].get<int64_t>();
        } else if (metadata->contains("reasoningTokens")) {
            usage.reasoning = (*metadata)["reasoningTokens"].get<int64_t>();
        }

        // Cache tokens
        if (metadata->contains("cache_read_tokens")) {
            usage.cache.read = (*metadata)["cache_read_tokens"].get<int64_t>();
        } else if (metadata->contains("cacheReadTokens")) {
            usage.cache.read = (*metadata)["cacheReadTokens"].get<int64_t>();
        }

        if (metadata->contains("cache_write_tokens")) {
            usage.cache.write = (*metadata)["cache_write_tokens"].get<int64_t>();
        } else if (metadata->contains("cacheWriteTokens")) {
            usage.cache.write = (*metadata)["cacheWriteTokens"].get<int64_t>();
        }
    }

    // Cache tokens from raw_usage (Anthropic format)
    if (raw_usage.contains("cache_read_input_tokens")) {
        usage.cache.read = raw_usage["cache_read_input_tokens"].get<int64_t>();
    }
    if (raw_usage.contains("cache_creation_input_tokens")) {
        usage.cache.write = raw_usage["cache_creation_input_tokens"].get<int64_t>();
    }

    return usage;
}

CostInfo UsageTracker::calculate_cost(
    const provider::ModelInfo& model,
    const TokenUsage& usage
) {
    return calculate_cost(model.pricing, usage);
}

CostInfo UsageTracker::calculate_cost(
    const nlohmann::json& pricing,
    const TokenUsage& usage
) {
    CostInfo cost;

    // Pricing is typically per 1K tokens, convert to per-token
    // Pricing format: {"input": 0.01, "output": 0.03, "cache_read": 0.003, ...}
    // Values are typically in dollars per 1K tokens

    double input_price = pricing.value("input", 0.0);
    double output_price = pricing.value("output", 0.0);
    double cache_read_price = pricing.value("cache_read", pricing.value("cache_read_input", 0.0));
    double cache_write_price = pricing.value("cache_write", pricing.value("cache_creation", 0.0));
    double reasoning_price = pricing.value("reasoning", 0.0);

    // Calculate costs (prices are per 1K tokens)
    cost.input_cost = (usage.input / 1000.0) * input_price;
    cost.output_cost = (usage.output / 1000.0) * output_price;
    cost.cache_read_cost = (usage.cache.read / 1000.0) * cache_read_price;
    cost.cache_write_cost = (usage.cache.write / 1000.0) * cache_write_price;
    cost.reasoning_cost = (usage.reasoning / 1000.0) * reasoning_price;

    cost.total_cost = cost.input_cost + cost.output_cost +
                      cost.cache_read_cost + cost.cache_write_cost +
                      cost.reasoning_cost;

    return cost;
}

// ===== UsageTracker Instance Methods =====

SessionUsage& UsageTracker::get_or_create_session(const std::string& session_id) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        it = sessions_.emplace(session_id, SessionUsage{}).first;
    }
    return it->second;
}

void UsageTracker::add_usage(
    const std::string& session_id,
    const TokenUsage& usage,
    const std::optional<CostInfo>& cost
) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto& session = get_or_create_session(session_id);
    session.tokens += usage;

    if (cost.has_value()) {
        session.cost += *cost;
    }

    total_.tokens += usage;
    if (cost.has_value()) {
        total_.cost += *cost;
    }
}

void UsageTracker::add_usage(
    const std::string& session_id,
    const TokenUsage& usage,
    const provider::ModelInfo& model
) {
    auto cost = calculate_cost(model, usage);
    add_usage(session_id, usage, cost);
}

void UsageTracker::increment_message_count(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& session = get_or_create_session(session_id);
    session.message_count++;
    total_.message_count++;
}

void UsageTracker::increment_tool_calls(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& session = get_or_create_session(session_id);
    session.tool_calls++;
    total_.tool_calls++;
}

void UsageTracker::increment_request_count(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& session = get_or_create_session(session_id);
    session.request_count++;
    total_.request_count++;
}

SessionUsage UsageTracker::get_session_usage(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return SessionUsage{};
    }
    return it->second;
}

SessionUsage UsageTracker::get_total_usage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_;
}

bool UsageTracker::has_session(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.find(session_id) != sessions_.end();
}

std::vector<std::string> UsageTracker::get_session_ids() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> ids;
    ids.reserve(sessions_.size());
    for (const auto& [id, _] : sessions_) {
        ids.push_back(id);
    }
    return ids;
}

void UsageTracker::reset_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        // Subtract from total
        total_.tokens -= it->second.tokens;
        total_.cost -= it->second.cost;
        total_.message_count -= it->second.message_count;
        total_.tool_calls -= it->second.tool_calls;
        total_.request_count -= it->second.request_count;

        sessions_.erase(it);
    }
}

void UsageTracker::reset_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.clear();
    total_ = SessionUsage{};
}

size_t UsageTracker::session_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

} // namespace turbot::core::session
