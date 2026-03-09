#pragma once

#include <turbot/core/provider/provider.hpp>
#include <turbot/core/common/logger.hpp>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace turbot::test {

/// Error type for mock provider simulation
enum class MockError {
    None,
    RateLimitExceeded,
    NetworkError,
    TimeoutError,
    InvalidApiKey,
    ServerError
};

/// Mock Provider for testing
///
/// This class provides a controllable mock implementation of the Provider interface
/// for unit and integration testing. It allows pre-configuring responses and errors.
class MockProvider : public core::provider::Provider {
public:
    MockProvider() = default;
    explicit MockProvider(const std::string& api_key) : api_key_(api_key) {}

    // ===== Provider interface =====

    [[nodiscard]] std::string id() const override { return "mock"; }
    [[nodiscard]] std::string name() const override { return "Mock Provider"; }
    [[nodiscard]] bool is_ready() const override { return ready_; }

    [[nodiscard]] std::vector<core::provider::ModelInfo> list_models() const override {
        return models_;
    }

    [[nodiscard]] std::optional<core::provider::ModelInfo> get_model(const std::string& model_id) const override {
        for (const auto& m : models_) {
            if (m.id == model_id) return m;
        }
        return std::nullopt;
    }

    [[nodiscard]] bool supports_model(const std::string& model_id) const override {
        return get_model(model_id).has_value();
    }

    [[nodiscard]] core::provider::ChatResponse chat(
        const std::vector<core::provider::ChatMessage>& messages,
        const std::string& model_id,
        const core::provider::ChatOptions& options = {}
    ) override {
        std::lock_guard<std::mutex> lock(mutex_);
        call_count_++;
        last_messages_ = messages;
        last_model_id_ = model_id;
        last_options_ = options;

        // Check for error sequence
        if (!error_sequence_.empty()) {
            auto error = error_sequence_.front();
            error_sequence_.pop();
            if (error != MockError::None) {
                return make_error_response(error, call_count_);
            }
        }

        // Check for configured response
        if (!responses_.empty()) {
            auto response = responses_.front();
            responses_.pop();
            return response;
        }

        // Simulate timeout if configured
        if (timeout_after_ >= 0 && call_count_ > timeout_after_) {
            if (!timeout_then_succeed_) {
                throw std::runtime_error("MockProvider: simulated timeout");
            }
        }

        // Default response
        return make_default_response(model_id, call_count_);
    }

    [[nodiscard]] core::provider::ChatResponse chat_stream(
        const std::vector<core::provider::ChatMessage>& messages,
        const std::string& model_id,
        const core::provider::ChatOptions& options,
        core::provider::StreamCallback callback
    ) override {
        // Step 1: Collect all needed data under lock
        std::vector<std::string> local_chunks;
        core::provider::ChatResponse final_response;
        MockError pending_error = MockError::None;
        int snapshot_call_count = 0;
        bool should_timeout = false;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            call_count_++;
            snapshot_call_count = call_count_;  // Snapshot under lock
            last_messages_ = messages;
            last_model_id_ = model_id;
            last_options_ = options;

            // Check timeout
            if (timeout_after_ >= 0 && call_count_ > timeout_after_ && !timeout_then_succeed_) {
                should_timeout = true;
            }

            // Check for error sequence
            if (!error_sequence_.empty()) {
                pending_error = error_sequence_.front();
                error_sequence_.pop();
            }

            // Copy stream chunks and response while holding lock
            local_chunks = stream_chunks_;

            if (!responses_.empty()) {
                final_response = responses_.front();
                responses_.pop();
            } else {
                final_response = make_default_response(model_id, snapshot_call_count);
            }
        }
        // Lock released

        // Step 2: Handle timeout outside lock
        if (should_timeout) {
            throw std::runtime_error("MockProvider: simulated timeout");
        }

        // Step 3: Handle error outside lock
        if (pending_error != MockError::None) {
            return make_error_response(pending_error, snapshot_call_count);
        }

        // Step 4: Call callback outside lock to avoid potential deadlock
        if (callback) {
            // Send text delta events (if any explicit stream chunks configured)
            for (const auto& chunk : local_chunks) {
                core::provider::ChatStreamEvent event;
                event.type = core::provider::StreamEventType::TextDelta;
                event.content = chunk;
                if (!callback(event)) {
                    // Stream aborted
                    break;
                }
            }

            // Emit any tool calls from the pre-configured response so that
            // LLM::stream's StreamingState can pick them up via ToolCall events.
            for (const auto& choice : final_response.choices) {
                if (choice.tool_calls.has_value()) {
                    for (const auto& tc : *choice.tool_calls) {
                        core::provider::ChatStreamEvent tc_event;
                        tc_event.type = core::provider::StreamEventType::ToolCall;
                        tc_event.tool_call = tc;
                        if (!callback(tc_event)) {
                            break;
                        }
                    }
                }
            }

            // Emit text content from choices (if no explicit stream chunks)
            if (local_chunks.empty()) {
                for (const auto& choice : final_response.choices) {
                    if (!choice.content.empty() &&
                        (!choice.tool_calls.has_value() || choice.tool_calls->empty())) {
                        core::provider::ChatStreamEvent text_event;
                        text_event.type = core::provider::StreamEventType::TextDelta;
                        text_event.content = choice.content;
                        if (!callback(text_event)) {
                            break;
                        }
                    }
                }
            }

            // Send finish event
            core::provider::ChatStreamEvent finish_event;
            finish_event.type = core::provider::StreamEventType::Finish;
            finish_event.finish_reason = final_response.finish_reason.empty()
                                            ? "stop" : final_response.finish_reason;
            finish_event.usage = final_response.usage;
            callback(finish_event);
        }

        return final_response;
    }

    [[nodiscard]] int64_t count_tokens(
        const std::vector<core::provider::ChatMessage>& messages,
        [[maybe_unused]] const std::string& model_id
    ) const override {
        int64_t total = 0;
        for (const auto& msg : messages) {
            total += static_cast<int64_t>(msg.content.size() / 4);  // Rough estimate
        }
        return total;
    }

    [[nodiscard]] bool validate() override {
        return !api_key_.empty();
    }

    // ===== Mock configuration methods =====

    /// Set the ready state
    void set_ready(bool ready) { ready_ = ready; }

    /// Set API key
    void set_api_key(const std::string& key) { api_key_ = key; }

    /// Add a model to the available models list
    void add_model(const core::provider::ModelInfo& model) {
        models_.push_back(model);
    }

    /// Set the next response to return (thread-safe)
    void set_next_response(const core::provider::ChatResponse& response) {
        std::lock_guard<std::mutex> lock(mutex_);
        responses_.push(response);
    }

    /// Set a sequence of responses (thread-safe)
    void set_response_sequence(const std::vector<core::provider::ChatResponse>& responses) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& r : responses) {
            responses_.push(r);
        }
    }

    /// Set error sequence for retry testing (thread-safe)
    void set_error_sequence(const std::vector<MockError>& errors) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& e : errors) {
            error_sequence_.push(e);
        }
    }

    /// Set stream chunks for streaming simulation (thread-safe)
    void set_stream_chunks(const std::vector<std::string>& chunks) {
        std::lock_guard<std::mutex> lock(mutex_);
        stream_chunks_ = chunks;
    }

    /// Set timeout behavior (thread-safe)
    void set_timeout_behavior(int timeout_after, bool then_succeed) {
        std::lock_guard<std::mutex> lock(mutex_);
        timeout_after_ = timeout_after;
        timeout_then_succeed_ = then_succeed;
    }

    /// Get call count (thread-safe)
    [[nodiscard]] int call_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return call_count_;
    }

    /// Get last messages (thread-safe)
    [[nodiscard]] std::vector<core::provider::ChatMessage> last_messages() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_messages_;
    }

    /// Get last model ID (thread-safe)
    [[nodiscard]] std::string last_model_id() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_model_id_;
    }

    /// Get last options (thread-safe)
    [[nodiscard]] core::provider::ChatOptions last_options() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_options_;
    }

    /// Reset all state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        call_count_ = 0;
        last_messages_.clear();
        last_model_id_.clear();
        last_options_ = {};
        responses_ = {};
        error_sequence_ = {};
        stream_chunks_.clear();
        models_.clear();
        ready_ = true;
    }

private:
    std::string api_key_;
    bool ready_ = true;
    std::vector<core::provider::ModelInfo> models_;
    std::queue<core::provider::ChatResponse> responses_;
    std::queue<MockError> error_sequence_;
    std::vector<std::string> stream_chunks_;
    int timeout_after_ = -1;  // -1 = disabled; N = throw after N-th call
    bool timeout_then_succeed_ = false;

    mutable std::mutex mutex_;
    int call_count_ = 0;
    std::vector<core::provider::ChatMessage> last_messages_;
    std::string last_model_id_;
    core::provider::ChatOptions last_options_;

    /// Create an error response
    [[nodiscard]] core::provider::ChatResponse make_error_response(
        MockError error, int call_count_snapshot) const {
        core::provider::ChatResponse response;
        response.id = "mock-error-" + std::to_string(call_count_snapshot);

        switch (error) {
            case MockError::RateLimitExceeded:
                response.error = nlohmann::json{
                    {"type", "rate_limit_exceeded"},
                    {"message", "Rate limit exceeded. Please retry after 1 second."}
                };
                break;
            case MockError::NetworkError:
                response.error = nlohmann::json{
                    {"type", "network_error"},
                    {"message", "Network connection failed."}
                };
                break;
            case MockError::TimeoutError:
                response.error = nlohmann::json{
                    {"type", "timeout"},
                    {"message", "Request timed out."}
                };
                break;
            case MockError::InvalidApiKey:
                response.error = nlohmann::json{
                    {"type", "invalid_api_key"},
                    {"message", "Invalid API key provided."}
                };
                break;
            case MockError::ServerError:
                response.error = nlohmann::json{
                    {"type", "server_error"},
                    {"message", "Internal server error."}
                };
                break;
            default:
                break;
        }

        return response;
    }

    /// Create a default response
    [[nodiscard]] core::provider::ChatResponse make_default_response(
        const std::string& model_id, int call_count_snapshot) const {
        core::provider::ChatResponse response;
        response.id = "mock-" + std::to_string(call_count_snapshot);
        response.model = model_id;
        response.finish_reason = "stop";
        response.usage = core::TokenUsage(100, 50, 150);

        core::provider::ChatMessage msg = core::provider::ChatMessage::assistant(
            "This is a mock response from the MockProvider."
        );
        response.choices.push_back(msg);

        return response;
    }
};

/// Builder for creating MockProvider instances with specific configurations
class MockProviderBuilder {
public:
    MockProviderBuilder() = default;

    MockProviderBuilder& with_api_key(const std::string& key) {
        api_key_ = key;
        return *this;
    }

    MockProviderBuilder& with_ready(bool ready) {
        ready_ = ready;
        return *this;
    }

    MockProviderBuilder& with_model(const core::provider::ModelInfo& model) {
        models_.push_back(model);
        return *this;
    }

    MockProviderBuilder& with_response(const core::provider::ChatResponse& response) {
        responses_.push_back(response);
        return *this;
    }

    MockProviderBuilder& with_text_response(const std::string& text) {
        core::provider::ChatResponse response;
        response.id = "mock-" + std::to_string(responses_.size());
        response.model = "mock-model";
        response.finish_reason = "stop";
        response.choices.push_back(core::provider::ChatMessage::assistant(text));
        responses_.push_back(response);
        return *this;
    }

    MockProviderBuilder& with_tool_call_response(
        const std::string& tool_name,
        const std::string& tool_id,
        const nlohmann::json& arguments
    ) {
        core::provider::ChatResponse response;
        response.id = "mock-tool-" + std::to_string(responses_.size());
        response.model = "mock-model";
        response.finish_reason = "tool_calls";

        core::provider::ToolCall tc;
        tc.id = tool_id;
        tc.name = tool_name;
        tc.arguments = arguments;

        core::provider::ChatMessage msg = core::provider::ChatMessage::assistant_with_tools("", {tc});
        response.choices.push_back(msg);
        responses_.push_back(response);
        return *this;
    }

    MockProviderBuilder& with_stream_chunks(const std::vector<std::string>& chunks) {
        stream_chunks_ = chunks;
        return *this;
    }

    MockProviderBuilder& with_error_sequence(const std::vector<MockError>& errors) {
        error_sequence_ = errors;
        return *this;
    }

    std::shared_ptr<MockProvider> build() {
        auto provider = std::make_shared<MockProvider>();
        if (!api_key_.empty()) provider->set_api_key(api_key_);
        provider->set_ready(ready_);
        for (const auto& m : models_) provider->add_model(m);
        for (const auto& r : responses_) provider->set_next_response(r);
        if (!stream_chunks_.empty()) provider->set_stream_chunks(stream_chunks_);
        if (!error_sequence_.empty()) provider->set_error_sequence(error_sequence_);
        return provider;
    }

private:
    std::string api_key_;
    bool ready_ = true;
    std::vector<core::provider::ModelInfo> models_;
    std::vector<core::provider::ChatResponse> responses_;
    std::vector<std::string> stream_chunks_;
    std::vector<MockError> error_sequence_;
};

} // namespace turbot::test
