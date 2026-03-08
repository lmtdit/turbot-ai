#include "turbot/core/llm/stream_event.hpp"
#include <chrono>
#include <functional>
#include <map>
#include <unordered_map>

namespace turbot::core {

// ===== StreamEventType conversion =====

std::string_view stream_event_type_to_string(StreamEventType type) noexcept {
    switch (type) {
        case StreamEventType::Start:          return "start";
        case StreamEventType::Finish:         return "finish";
        case StreamEventType::Error:          return "error";
        case StreamEventType::TextStart:      return "text-start";
        case StreamEventType::TextDelta:      return "text-delta";
        case StreamEventType::TextEnd:        return "text-end";
        case StreamEventType::ReasoningStart: return "reasoning-start";
        case StreamEventType::ReasoningDelta: return "reasoning-delta";
        case StreamEventType::ReasoningEnd:   return "reasoning-end";
        case StreamEventType::ToolInputStart: return "tool-input-start";
        case StreamEventType::ToolInputDelta: return "tool-input-delta";
        case StreamEventType::ToolInputEnd:   return "tool-input-end";
        case StreamEventType::ToolCall:       return "tool-call";
        case StreamEventType::StepStart:      return "step-start";
        case StreamEventType::StepFinish:     return "step-finish";
        case StreamEventType::SourceStart:    return "source-start";
        case StreamEventType::SourceEnd:      return "source-end";
    }
    return "unknown";
}

StreamEventType stream_event_type_from_string(std::string_view str) {
    // Use static hash map for O(1) lookup
    static const std::unordered_map<std::string_view, StreamEventType> type_map = {
        {"start", StreamEventType::Start},
        {"finish", StreamEventType::Finish},
        {"error", StreamEventType::Error},
        {"text-start", StreamEventType::TextStart},
        {"text-delta", StreamEventType::TextDelta},
        {"text-end", StreamEventType::TextEnd},
        {"reasoning-start", StreamEventType::ReasoningStart},
        {"reasoning-delta", StreamEventType::ReasoningDelta},
        {"reasoning-end", StreamEventType::ReasoningEnd},
        {"tool-input-start", StreamEventType::ToolInputStart},
        {"tool-input-delta", StreamEventType::ToolInputDelta},
        {"tool-input-end", StreamEventType::ToolInputEnd},
        {"tool-call", StreamEventType::ToolCall},
        {"step-start", StreamEventType::StepStart},
        {"step-finish", StreamEventType::StepFinish},
        {"source-start", StreamEventType::SourceStart},
        {"source-end", StreamEventType::SourceEnd}
    };

    auto it = type_map.find(str);
    return it != type_map.end() ? it->second : StreamEventType::TextDelta;
}

// ===== FinishReason conversion =====

std::string_view finish_reason_to_string(FinishReason reason) noexcept {
    switch (reason) {
        case FinishReason::Stop:          return "stop";
        case FinishReason::Length:        return "length";
        case FinishReason::ToolCall:      return "tool-call";
        case FinishReason::ContentFilter: return "content-filter";
        case FinishReason::Error:         return "error";
        case FinishReason::Other:         return "other";
    }
    return "unknown";
}

FinishReason finish_reason_from_string(std::string_view str) {
    // Use static hash map for O(1) lookup
    static const std::unordered_map<std::string_view, FinishReason> reason_map = {
        {"stop", FinishReason::Stop},
        {"length", FinishReason::Length},
        {"tool-call", FinishReason::ToolCall},
        {"content-filter", FinishReason::ContentFilter},
        {"error", FinishReason::Error},
        {"other", FinishReason::Other}
    };

    auto it = reason_map.find(str);
    return it != reason_map.end() ? it->second : FinishReason::Other;
}

// ===== ToolCallChunk =====

nlohmann::json ToolCallChunk::to_json() const {
    nlohmann::json j = {
        {"id", id},
        {"name", name},
        {"arguments", arguments},
        {"is_complete", is_complete}
    };
    return j;
}

ToolCallChunk ToolCallChunk::from_json(const nlohmann::json& j) {
    ToolCallChunk chunk;
    chunk.id = j.value("id", std::string{});
    chunk.name = j.value("name", std::string{});
    chunk.arguments = j.value("arguments", std::string{});
    chunk.is_complete = j.value("is_complete", false);
    return chunk;
}

// ===== ReasoningMetadata =====

nlohmann::json ReasoningMetadata::to_json() const {
    nlohmann::json j = nlohmann::json::object();
    if (encrypted_content.has_value()) {
        j["encrypted_content"] = *encrypted_content;
    }
    if (budget_tokens.has_value()) {
        j["budget_tokens"] = *budget_tokens;
    }
    if (used_tokens.has_value()) {
        j["used_tokens"] = *used_tokens;
    }
    return j;
}

ReasoningMetadata ReasoningMetadata::from_json(const nlohmann::json& j) {
    ReasoningMetadata meta;
    if (j.contains("encrypted_content") && !j["encrypted_content"].is_null()) {
        meta.encrypted_content = j["encrypted_content"].get<std::string>();
    }
    if (j.contains("budget_tokens") && !j["budget_tokens"].is_null()) {
        meta.budget_tokens = j["budget_tokens"].get<int64_t>();
    }
    if (j.contains("used_tokens") && !j["used_tokens"].is_null()) {
        meta.used_tokens = j["used_tokens"].get<int64_t>();
    }
    return meta;
}

// ===== SourceInfo =====

nlohmann::json SourceInfo::to_json() const {
    nlohmann::json j = {
        {"id", id},
        {"type", type}
    };
    if (title.has_value()) {
        j["title"] = *title;
    }
    if (url.has_value()) {
        j["url"] = *url;
    }
    if (filename.has_value()) {
        j["filename"] = *filename;
    }
    return j;
}

SourceInfo SourceInfo::from_json(const nlohmann::json& j) {
    SourceInfo info;
    info.id = j.value("id", std::string{});
    info.type = j.value("type", std::string{});
    if (j.contains("title") && !j["title"].is_null()) {
        info.title = j["title"].get<std::string>();
    }
    if (j.contains("url") && !j["url"].is_null()) {
        info.url = j["url"].get<std::string>();
    }
    if (j.contains("filename") && !j["filename"].is_null()) {
        info.filename = j["filename"].get<std::string>();
    }
    return info;
}

// ===== StreamEvent =====

int64_t StreamEvent::get_current_timestamp() noexcept {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

StreamEvent StreamEvent::create_start() {
    StreamEvent event;
    event.type = StreamEventType::Start;
    event.timestamp = get_current_timestamp();
    return event;
}

StreamEvent StreamEvent::create_finish(FinishReason reason, const TokenUsage& usage) {
    StreamEvent event;
    event.type = StreamEventType::Finish;
    event.finish_reason = reason;
    event.usage = usage;
    event.timestamp = get_current_timestamp();
    return event;
}

StreamEvent StreamEvent::create_error(const std::string& message, const std::optional<std::string>& code) {
    StreamEvent event;
    event.type = StreamEventType::Error;
    event.error_message = message;
    event.error_code = code;
    event.timestamp = get_current_timestamp();
    return event;
}

StreamEvent StreamEvent::create_text_start(const std::string& id) {
    StreamEvent event;
    event.type = StreamEventType::TextStart;
    event.id = id;
    event.timestamp = get_current_timestamp();
    return event;
}

StreamEvent StreamEvent::create_text_delta(const std::string& id, const std::string& delta) {
    StreamEvent event;
    event.type = StreamEventType::TextDelta;
    event.id = id;
    event.delta = delta;
    event.timestamp = get_current_timestamp();
    return event;
}

StreamEvent StreamEvent::create_text_end(const std::string& id) {
    StreamEvent event;
    event.type = StreamEventType::TextEnd;
    event.id = id;
    event.timestamp = get_current_timestamp();
    return event;
}

StreamEvent StreamEvent::create_reasoning_start(const std::string& id) {
    StreamEvent event;
    event.type = StreamEventType::ReasoningStart;
    event.id = id;
    event.timestamp = get_current_timestamp();
    return event;
}

StreamEvent StreamEvent::create_reasoning_delta(const std::string& id, const std::string& delta) {
    StreamEvent event;
    event.type = StreamEventType::ReasoningDelta;
    event.id = id;
    event.delta = delta;
    event.timestamp = get_current_timestamp();
    return event;
}

StreamEvent StreamEvent::create_reasoning_end(const std::string& id) {
    StreamEvent event;
    event.type = StreamEventType::ReasoningEnd;
    event.id = id;
    event.timestamp = get_current_timestamp();
    return event;
}

StreamEvent StreamEvent::create_tool_input_start(const std::string& id, const std::string& tool_name) {
    StreamEvent event;
    event.type = StreamEventType::ToolInputStart;
    event.id = id;
    event.tool_call = ToolCallChunk{id, tool_name, "", false};
    event.timestamp = get_current_timestamp();
    return event;
}

StreamEvent StreamEvent::create_tool_input_delta(const std::string& id, const std::string& delta) {
    StreamEvent event;
    event.type = StreamEventType::ToolInputDelta;
    event.id = id;
    event.delta = delta;
    event.timestamp = get_current_timestamp();
    return event;
}

StreamEvent StreamEvent::create_tool_input_end(const std::string& id) {
    StreamEvent event;
    event.type = StreamEventType::ToolInputEnd;
    event.id = id;
    event.timestamp = get_current_timestamp();
    return event;
}

StreamEvent StreamEvent::create_tool_call(const ToolCallChunk& tool_call) {
    StreamEvent event;
    event.type = StreamEventType::ToolCall;
    event.id = tool_call.id;
    event.tool_call = tool_call;
    event.timestamp = get_current_timestamp();
    return event;
}

StreamEvent StreamEvent::create_step_start(const std::string& step_id, const std::string& name) {
    StreamEvent event;
    event.type = StreamEventType::StepStart;
    event.id = step_id;
    event.delta = name;
    event.timestamp = get_current_timestamp();
    return event;
}

StreamEvent StreamEvent::create_step_finish(const std::string& step_id, const std::optional<nlohmann::json>& result) {
    StreamEvent event;
    event.type = StreamEventType::StepFinish;
    event.id = step_id;
    if (result.has_value()) {
        event.provider_metadata = *result;
    }
    event.timestamp = get_current_timestamp();
    return event;
}

nlohmann::json StreamEvent::to_json() const {
    nlohmann::json j = {
        {"type", std::string(stream_event_type_to_string(type))},
        {"id", id}
    };

    if (!delta.empty()) {
        j["delta"] = delta;
    }

    if (tool_call.has_value()) {
        j["tool_call"] = tool_call->to_json();
    }

    if (reasoning.has_value()) {
        j["reasoning"] = reasoning->to_json();
    }

    if (source.has_value()) {
        j["source"] = source->to_json();
    }

    if (finish_reason.has_value()) {
        j["finish_reason"] = std::string(finish_reason_to_string(*finish_reason));
    }

    if (usage.has_value()) {
        j["usage"] = usage->to_json();
    }

    if (error_message.has_value()) {
        j["error_message"] = *error_message;
    }

    if (error_code.has_value()) {
        j["error_code"] = *error_code;
    }

    if (provider_metadata.has_value()) {
        j["provider_metadata"] = *provider_metadata;
    }

    j["timestamp"] = timestamp;

    return j;
}

StreamEvent StreamEvent::from_json(const nlohmann::json& j) {
    StreamEvent event;
    event.type = stream_event_type_from_string(j.value("type", "text-delta"));
    event.id = j.value("id", std::string{});
    event.delta = j.value("delta", std::string{});

    if (j.contains("tool_call")) {
        event.tool_call = ToolCallChunk::from_json(j["tool_call"]);
    }

    if (j.contains("reasoning")) {
        event.reasoning = ReasoningMetadata::from_json(j["reasoning"]);
    }

    if (j.contains("source")) {
        event.source = SourceInfo::from_json(j["source"]);
    }

    if (j.contains("finish_reason")) {
        event.finish_reason = finish_reason_from_string(j["finish_reason"].get<std::string>());
    }

    if (j.contains("usage")) {
        event.usage = TokenUsage::from_json(j["usage"]);
    }

    if (j.contains("error_message") && !j["error_message"].is_null()) {
        event.error_message = j["error_message"].get<std::string>();
    }
    if (j.contains("error_code") && !j["error_code"].is_null()) {
        event.error_code = j["error_code"].get<std::string>();
    }

    if (j.contains("provider_metadata")) {
        event.provider_metadata = j["provider_metadata"];
    }

    event.timestamp = j.value("timestamp", int64_t{0});

    return event;
}

// ===== StreamResult =====

std::string StreamResult::get_text() const {
    std::string result;
    for (const auto& event : events) {
        if (event.type == StreamEventType::TextDelta) {
            result += event.delta;
        }
    }
    return result;
}

std::string StreamResult::get_reasoning() const {
    std::string result;
    for (const auto& event : events) {
        if (event.type == StreamEventType::ReasoningDelta) {
            result += event.delta;
        }
    }
    return result;
}

std::vector<ToolCallChunk> StreamResult::get_tool_calls() const {
    std::vector<ToolCallChunk> result;
    std::map<std::string, ToolCallChunk> tool_call_map;

    for (const auto& event : events) {
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

    for (auto& [tool_id, chunk] : tool_call_map) {
        result.push_back(std::move(chunk));
    }

    return result;
}

nlohmann::json StreamResult::to_json() const {
    nlohmann::json j = {
        {"id", id},
        {"model", model},
        {"finish_reason", std::string(finish_reason_to_string(finish_reason))},
        {"usage", usage.to_json()}
    };

    j["events"] = nlohmann::json::array();
    for (const auto& event : events) {
        j["events"].push_back(event.to_json());
    }

    if (error.has_value()) {
        j["error"] = *error;
    }

    return j;
}

StreamResult StreamResult::from_json(const nlohmann::json& j) {
    StreamResult result;
    result.id = j.value("id", std::string{});
    result.model = j.value("model", std::string{});
    result.finish_reason = finish_reason_from_string(j.value("finish_reason", "stop"));

    if (j.contains("usage")) {
        result.usage = TokenUsage::from_json(j["usage"]);
    }

    if (j.contains("events")) {
        for (const auto& event_json : j["events"]) {
            result.events.push_back(StreamEvent::from_json(event_json));
        }
    }

    if (j.contains("error") && !j["error"].is_null()) {
        result.error = j["error"].get<std::string>();
    }

    return result;
}

} // namespace turbot::core
