#include "turbot/core/llm/llm.hpp"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>

namespace turbot::core::llm {

// ===== LLMToolDefinition =====

nlohmann::json LLMToolDefinition::to_json() const {
    return {
        {"type", "function"},
        {"function", {
            {"name", name},
            {"description", description},
            {"parameters", parameters}
        }}
    };
}

LLMToolDefinition LLMToolDefinition::from_json(const nlohmann::json& j) {
    LLMToolDefinition tool;
    if (j.contains("function")) {
        const auto& func = j["function"];
        tool.name = func.value("name", std::string{});
        tool.description = func.value("description", std::string{});
        tool.parameters = func.value("parameters", nlohmann::json::object());
    } else {
        tool.name = j.value("name", std::string{});
        tool.description = j.value("description", std::string{});
        tool.parameters = j.value("parameters", nlohmann::json::object());
    }
    return tool;
}

// ===== ToolCallResult =====

nlohmann::json ToolCallResult::to_json() const {
    nlohmann::json j = {
        {"tool_call_id", tool_call_id},
        {"content", content}
    };
    if (is_error) {
        j["is_error"] = true;
    }
    return j;
}

ToolCallResult ToolCallResult::from_json(const nlohmann::json& j) {
    ToolCallResult result;
    result.tool_call_id = j.value("tool_call_id", std::string{});
    result.content = j.value("content", std::string{});
    result.is_error = j.value("is_error", false);
    return result;
}

// ===== LLMMessage =====

nlohmann::json LLMMessage::to_json() const {
    nlohmann::json j = {
        {"role", provider::chat_role_to_string(role)},
        {"content", content}
    };
    if (name.has_value()) {
        j["name"] = *name;
    }
    if (tool_call_id.has_value()) {
        j["tool_call_id"] = *tool_call_id;
    }
    if (!tool_calls.empty()) {
        nlohmann::json tc_array = nlohmann::json::array();
        for (const auto& tc : tool_calls) {
            tc_array.push_back(tc.to_json());
        }
        j["tool_calls"] = tc_array;
    }
    return j;
}

LLMMessage LLMMessage::from_json(const nlohmann::json& j) {
    LLMMessage msg;
    msg.role = provider::chat_role_from_string(j.value("role", "user"));
    msg.content = j.value("content", std::string{});
    if (j.contains("name")) {
        msg.name = j["name"].get<std::string>();
    }
    if (j.contains("tool_call_id")) {
        msg.tool_call_id = j["tool_call_id"].get<std::string>();
    }
    if (j.contains("tool_calls")) {
        for (const auto& tc : j["tool_calls"]) {
            msg.tool_calls.push_back(ToolCallChunk::from_json(tc));
        }
    }
    return msg;
}

LLMMessage LLMMessage::system(const std::string& content) {
    LLMMessage msg;
    msg.role = provider::ChatRole::System;
    msg.content = content;
    return msg;
}

LLMMessage LLMMessage::user(const std::string& content) {
    LLMMessage msg;
    msg.role = provider::ChatRole::User;
    msg.content = content;
    return msg;
}

LLMMessage LLMMessage::assistant(const std::string& content) {
    LLMMessage msg;
    msg.role = provider::ChatRole::Assistant;
    msg.content = content;
    return msg;
}

LLMMessage LLMMessage::assistant_with_tools(
    const std::string& content,
    const std::vector<ToolCallChunk>& tool_calls
) {
    LLMMessage msg;
    msg.role = provider::ChatRole::Assistant;
    msg.content = content;
    msg.tool_calls = tool_calls;
    return msg;
}

LLMMessage LLMMessage::tool_result(
    const std::string& tool_call_id,
    const std::string& content,
    bool is_error
) {
    LLMMessage msg;
    msg.role = provider::ChatRole::Tool;
    msg.tool_call_id = tool_call_id;
    msg.content = content;
    if (is_error) {
        msg.name = "error";
    }
    return msg;
}

// ===== StreamParams =====

nlohmann::json StreamParams::to_json() const {
    nlohmann::json j = {
        {"session_id", session_id},
        {"temperature", temperature}
    };

    if (!messages.empty()) {
        nlohmann::json msg_array = nlohmann::json::array();
        for (const auto& msg : messages) {
            msg_array.push_back(msg.to_json());
        }
        j["messages"] = msg_array;
    }

    if (!tools.empty()) {
        nlohmann::json tool_array = nlohmann::json::array();
        for (const auto& tool : tools) {
            tool_array.push_back(tool.to_json());
        }
        j["tools"] = tool_array;
    }

    if (tool_choice.has_value()) {
        j["tool_choice"] = *tool_choice;
    }
    if (top_p.has_value()) {
        j["top_p"] = *top_p;
    }
    if (max_tokens.has_value()) {
        j["max_tokens"] = *max_tokens;
    }
    if (!stop.empty()) {
        j["stop"] = stop;
    }

    return j;
}

StreamParams StreamParams::from_json(const nlohmann::json& j) {
    StreamParams params;
    params.session_id = j.value("session_id", std::string{});
    params.temperature = j.value("temperature", 1.0);

    if (j.contains("messages")) {
        for (const auto& msg : j["messages"]) {
            params.messages.push_back(LLMMessage::from_json(msg));
        }
    }

    if (j.contains("tools")) {
        for (const auto& tool : j["tools"]) {
            params.tools.push_back(LLMToolDefinition::from_json(tool));
        }
    }

    if (j.contains("tool_choice")) {
        params.tool_choice = j["tool_choice"].get<std::string>();
    }
    if (j.contains("top_p")) {
        params.top_p = j["top_p"].get<double>();
    }
    if (j.contains("max_tokens")) {
        params.max_tokens = j["max_tokens"].get<int>();
    }
    if (j.contains("stop")) {
        for (const auto& s : j["stop"]) {
            params.stop.push_back(s.get<std::string>());
        }
    }

    return params;
}

// ===== StreamingState =====

void StreamingState::add_event(StreamEvent event) {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back(event);
    pending_.push(event);
}

std::optional<StreamEvent> StreamingState::pop_event() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pending_.empty()) {
        return std::nullopt;
    }
    auto event = pending_.front();
    pending_.pop();
    return event;
}

bool StreamingState::has_events() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !pending_.empty();
}

void StreamingState::mark_done(FinishReason reason, const TokenUsage& usage) {
    std::lock_guard<std::mutex> lock(mutex_);
    done_ = true;
    finish_reason_ = reason;
    usage_ = usage;
}

void StreamingState::mark_error(const std::string& message, const std::string& code) {
    std::lock_guard<std::mutex> lock(mutex_);
    done_ = true;
    finish_reason_ = FinishReason::Error;
    error_ = message;
    // Add error event
    StreamEvent error_event = StreamEvent::create_error(message, code.empty() ? std::nullopt : std::optional<std::string>(code));
    events_.push_back(error_event);
    pending_.push(error_event);
}

std::string StreamingState::get_text() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    std::string current_id;

    for (const auto& event : events_) {
        if (event.type == StreamEventType::TextDelta) {
            oss << event.delta;
        }
    }

    return oss.str();
}

std::string StreamingState::get_reasoning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;

    for (const auto& event : events_) {
        if (event.type == StreamEventType::ReasoningDelta) {
            oss << event.delta;
        }
    }

    return oss.str();
}

std::vector<ToolCallChunk> StreamingState::get_tool_calls() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::string, ToolCallChunk> tool_call_map;

    for (const auto& event : events_) {
        if (event.type == StreamEventType::ToolInputStart && event.tool_call.has_value()) {
            tool_call_map[event.id] = *event.tool_call;
        } else if (event.type == StreamEventType::ToolInputDelta) {
            auto it = tool_call_map.find(event.id);
            if (it != tool_call_map.end()) {
                it->second.arguments += event.delta;
            }
        } else if (event.type == StreamEventType::ToolInputEnd) {
            auto it = tool_call_map.find(event.id);
            if (it != tool_call_map.end()) {
                it->second.is_complete = true;
            }
        } else if (event.type == StreamEventType::ToolCall && event.tool_call.has_value()) {
            tool_call_map[event.id] = *event.tool_call;
            tool_call_map[event.id].is_complete = true;
        }
    }

    std::vector<ToolCallChunk> result;
    result.reserve(tool_call_map.size());
    for (auto& [id, chunk] : tool_call_map) {
        result.push_back(std::move(chunk));
    }

    return result;
}

StreamResult StreamingState::to_result() const {
    std::lock_guard<std::mutex> lock(mutex_);
    StreamResult result;
    result.id = response_id_;
    result.model = model_;
    result.events = events_;
    result.usage = usage_;
    result.finish_reason = finish_reason_;
    result.error = error_;
    return result;
}

// ===== LLMStreamResult =====

LLMStreamResult::LLMStreamResult(std::shared_ptr<StreamingState> state)
    : state_(std::move(state)) {}

std::optional<StreamEvent> LLMStreamResult::next() {
    if (!state_) return std::nullopt;

    // Wait for events or completion
    while (!state_->is_done() || state_->has_events()) {
        auto event = state_->pop_event();
        if (event.has_value()) {
            return event;
        }
        // Small sleep to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return std::nullopt;
}

std::vector<StreamEvent> LLMStreamResult::collect() {
    std::vector<StreamEvent> events;

    while (auto event = next()) {
        events.push_back(*event);
    }

    return events;
}

bool LLMStreamResult::is_done() const {
    return !state_ || state_->is_done();
}

std::string LLMStreamResult::final_text() const {
    return state_ ? state_->get_text() : "";
}

std::string LLMStreamResult::final_reasoning() const {
    return state_ ? state_->get_reasoning() : "";
}

std::vector<ToolCallChunk> LLMStreamResult::tool_calls() const {
    return state_ ? state_->get_tool_calls() : std::vector<ToolCallChunk>{};
}

TokenUsage LLMStreamResult::usage() const {
    return state_ ? state_->usage() : TokenUsage{};
}

FinishReason LLMStreamResult::finish_reason() const {
    return state_ ? state_->finish_reason() : FinishReason::Stop;
}

bool LLMStreamResult::has_error() const {
    return state_ && state_->error().has_value();
}

std::optional<std::string> LLMStreamResult::error() const {
    return state_ ? state_->error() : std::nullopt;
}

// ===== LLM =====

provider::ChatMessage LLM::to_provider_message(const LLMMessage& msg) {
    provider::ChatMessage pmsg;
    pmsg.role = msg.role;
    pmsg.content = msg.content;
    pmsg.name = msg.name;
    pmsg.tool_call_id = msg.tool_call_id;

    if (!msg.tool_calls.empty()) {
        pmsg.tool_calls = std::vector<provider::ToolCall>{};
        for (const auto& tc : msg.tool_calls) {
            provider::ToolCall ptc;
            ptc.id = tc.id;
            ptc.name = tc.name;
            try {
                ptc.arguments = nlohmann::json::parse(tc.arguments);
            } catch (...) {
                ptc.arguments = tc.arguments;
            }
            pmsg.tool_calls->push_back(std::move(ptc));
        }
    }

    return pmsg;
}

provider::ToolDefinition LLM::to_provider_tool(const LLMToolDefinition& tool) {
    provider::ToolDefinition ptool;
    ptool.type = "function";
    ptool.name = tool.name;
    ptool.description = tool.description;
    ptool.parameters = tool.parameters;
    return ptool;
}

provider::ChatOptions LLM::to_chat_options(const StreamParams& params) {
    provider::ChatOptions options;
    options.temperature = params.temperature;
    options.top_p = params.top_p.value_or(1.0);
    options.max_tokens = params.max_tokens.value_or(4096);
    options.stop = params.stop;
    options.stream = true;

    for (const auto& tool : params.tools) {
        options.tools.push_back(to_provider_tool(tool));
    }

    // Transparently forward tool_choice to the provider via extra so that all
    // OpenAI-compatible backends (including LiteLLM, Bailian, etc.) receive it.
    if (params.tool_choice.has_value()) {
        options.extra["tool_choice"] = *params.tool_choice;
    }

    return options;
}

LLMStreamResult LLM::stream(
    provider::Provider& provider,
    const std::string& model_id,
    const StreamParams& params,
    StreamEventHandler handler
) {
    // Convert messages
    std::vector<provider::ChatMessage> messages;
    messages.reserve(params.messages.size());
    for (const auto& msg : params.messages) {
        messages.push_back(to_provider_message(msg));
    }

    // Convert options
    auto options = to_chat_options(params);
    options.stream = true;

    // Create streaming state
    auto state = std::make_shared<StreamingState>();

    // Create stream callback
    auto callback = [&state, &params, handler](const provider::ChatStreamEvent& event) -> bool {
        // Check abort
        if (params.is_aborted && params.is_aborted()) {
            state->mark_error("Stream aborted", "aborted");
            return false;
        }

        // Convert provider event to StreamEvent
        switch (event.type) {
            case provider::StreamEventType::TextDelta: {
                // Generate text ID if not exists
                std::string text_id = "text-0";
                auto stream_event = StreamEvent::create_text_delta(text_id, event.content);
                state->add_event(stream_event);
                if (handler) handler(stream_event);
                break;
            }
            case provider::StreamEventType::ToolCall: {
                if (event.tool_call.has_value()) {
                    ToolCallChunk chunk;
                    chunk.id = event.tool_call->id;
                    chunk.name = event.tool_call->name;
                    chunk.arguments = event.tool_call->arguments.dump();
                    chunk.is_complete = true;

                    auto stream_event = StreamEvent::create_tool_call(chunk);
                    state->add_event(stream_event);
                    if (handler) handler(stream_event);
                }
                break;
            }
            case provider::StreamEventType::Reasoning: {
                std::string reasoning_id = "reasoning-0";
                auto stream_event = StreamEvent::create_reasoning_delta(reasoning_id, event.content);
                state->add_event(stream_event);
                if (handler) handler(stream_event);
                break;
            }
            case provider::StreamEventType::Finish: {
                FinishReason reason = FinishReason::Stop;
                if (event.finish_reason.has_value()) {
                    reason = finish_reason_from_string(*event.finish_reason);
                }
                state->mark_done(reason, event.usage);

                auto stream_event = StreamEvent::create_finish(reason, event.usage);
                state->add_event(stream_event);
                if (handler) handler(stream_event);
                return false;  // Stop streaming
            }
            case provider::StreamEventType::Error: {
                std::string message = "Unknown error";
                std::string code;
                if (event.error.has_value()) {
                    if (event.error->contains("message")) {
                        message = (*event.error)["message"].get<std::string>();
                    }
                    if (event.error->contains("code")) {
                        code = (*event.error)["code"].get<std::string>();
                    }
                }
                state->mark_error(message, code);
                return false;  // Stop streaming
            }
        }

        return true;  // Continue streaming
    };

    // Perform streaming call
    try {
        auto response = provider.chat_stream(messages, model_id, options, callback);
        state->set_response_id(response.id);
        state->set_model(response.model);

        if (!state->is_done()) {
            // If not marked done by callback, mark it now
            FinishReason reason = finish_reason_from_string(response.finish_reason);
            state->mark_done(reason, response.usage);
        }
    } catch (const nlohmann::json::exception& e) {
        // JSON parsing/serialization error
        state->mark_error(e.what(), "json_error");
    } catch (const std::runtime_error& e) {
        // Runtime error from provider
        state->mark_error(e.what(), "runtime_error");
    } catch (const std::exception& e) {
        // Other standard exceptions
        state->mark_error(e.what(), "exception");
    } catch (...) {
        // Unknown exception
        state->mark_error("Unknown error occurred", "unknown_error");
    }

    return LLMStreamResult(state);
}

std::string LLM::complete(
    provider::Provider& provider,
    const std::string& model_id,
    const StreamParams& params
) {
    // Convert messages
    std::vector<provider::ChatMessage> messages;
    messages.reserve(params.messages.size());
    for (const auto& msg : params.messages) {
        messages.push_back(to_provider_message(msg));
    }

    // Convert options
    auto options = to_chat_options(params);
    options.stream = false;

    // Perform non-streaming call (let exceptions propagate to caller)
    auto response = provider.chat(messages, model_id, options);
    if (response.is_error()) {
        if (response.error.has_value() && response.error->contains("message")) {
            throw std::runtime_error((*response.error)["message"].get<std::string>());
        }
        throw std::runtime_error("LLM call failed");
    }
    return response.get_text();
}

LLMStreamResult LLM::stream_with_provider_format(
    provider::Provider& provider,
    const std::string& model_id,
    const std::vector<provider::ChatMessage>& messages,
    const provider::ChatOptions& options,
    StreamEventHandler handler
) {
    // Create streaming state
    auto state = std::make_shared<StreamingState>();

    // Create abort callback wrapper
    std::function<bool()> is_aborted = nullptr;

    // Create stream callback
    auto callback = [&state, handler, is_aborted](const provider::ChatStreamEvent& event) -> bool {
        // Check abort
        if (is_aborted && is_aborted()) {
            state->mark_error("Stream aborted", "aborted");
            return false;
        }

        // Convert provider event to StreamEvent
        switch (event.type) {
            case provider::StreamEventType::TextDelta: {
                std::string text_id = "text-0";
                auto stream_event = StreamEvent::create_text_delta(text_id, event.content);
                state->add_event(stream_event);
                if (handler) handler(stream_event);
                break;
            }
            case provider::StreamEventType::ToolCall: {
                if (event.tool_call.has_value()) {
                    ToolCallChunk chunk;
                    chunk.id = event.tool_call->id;
                    chunk.name = event.tool_call->name;
                    chunk.arguments = event.tool_call->arguments.dump();
                    chunk.is_complete = true;

                    auto stream_event = StreamEvent::create_tool_call(chunk);
                    state->add_event(stream_event);
                    if (handler) handler(stream_event);
                }
                break;
            }
            case provider::StreamEventType::Reasoning: {
                std::string reasoning_id = "reasoning-0";
                auto stream_event = StreamEvent::create_reasoning_delta(reasoning_id, event.content);
                state->add_event(stream_event);
                if (handler) handler(stream_event);
                break;
            }
            case provider::StreamEventType::Finish: {
                FinishReason reason = FinishReason::Stop;
                if (event.finish_reason.has_value()) {
                    reason = finish_reason_from_string(*event.finish_reason);
                }
                state->mark_done(reason, event.usage);

                auto stream_event = StreamEvent::create_finish(reason, event.usage);
                state->add_event(stream_event);
                if (handler) handler(stream_event);
                return false;
            }
            case provider::StreamEventType::Error: {
                std::string message = "Unknown error";
                std::string code;
                if (event.error.has_value()) {
                    if (event.error->contains("message")) {
                        message = (*event.error)["message"].get<std::string>();
                    }
                    if (event.error->contains("code")) {
                        code = (*event.error)["code"].get<std::string>();
                    }
                }
                state->mark_error(message, code);
                return false;
            }
        }

        return true;
    };

    // Perform streaming call
    try {
        auto mutable_options = options;
        mutable_options.stream = true;

        auto response = provider.chat_stream(messages, model_id, mutable_options, callback);
        state->set_response_id(response.id);
        state->set_model(response.model);

        if (!state->is_done()) {
            FinishReason reason = finish_reason_from_string(response.finish_reason);
            state->mark_done(reason, response.usage);
        }
    } catch (const nlohmann::json::exception& e) {
        // JSON parsing/serialization error
        state->mark_error(e.what(), "json_error");
    } catch (const std::runtime_error& e) {
        // Runtime error from provider
        state->mark_error(e.what(), "runtime_error");
    } catch (const std::exception& e) {
        // Other standard exceptions
        state->mark_error(e.what(), "exception");
    } catch (...) {
        // Unknown exception
        state->mark_error("Unknown error occurred", "unknown_error");
    }

    return LLMStreamResult(state);
}

} // namespace turbot::core::llm
