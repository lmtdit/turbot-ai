#include <turbot/core/message/part.hpp>
#include <turbot/core/logger.hpp>
#include <random>
#include <sstream>
#include <iomanip>

namespace turbot::core {

// ===== PartType conversion =====

std::string_view part_type_to_string(PartType type) noexcept {
    switch (type) {
        case PartType::Text:        return "text";
        case PartType::Tool:        return "tool";
        case PartType::Reasoning:   return "reasoning";
        case PartType::File:        return "file";
        case PartType::Subtask:     return "subtask";
        case PartType::StepStart:   return "step_start";
        case PartType::StepFinish:  return "step_finish";
        case PartType::Snapshot:    return "snapshot";
        case PartType::Patch:       return "patch";
        case PartType::Agent:       return "agent";
        case PartType::Retry:       return "retry";
        case PartType::Compaction:  return "compaction";
        default:                    return "text";
    }
}

PartType part_type_from_string(std::string_view str) {
    if (str == "text")        return PartType::Text;
    if (str == "tool")        return PartType::Tool;
    if (str == "reasoning")   return PartType::Reasoning;
    if (str == "file")        return PartType::File;
    if (str == "subtask")     return PartType::Subtask;
    if (str == "step_start")  return PartType::StepStart;
    if (str == "step_finish") return PartType::StepFinish;
    if (str == "snapshot")    return PartType::Snapshot;
    if (str == "patch")       return PartType::Patch;
    if (str == "agent")       return PartType::Agent;
    if (str == "retry")       return PartType::Retry;
    if (str == "compaction")  return PartType::Compaction;
    return PartType::Text;
}

// ===== Role conversion =====

std::string_view role_to_string(Role role) noexcept {
    switch (role) {
        case Role::User:      return "user";
        case Role::Assistant: return "assistant";
        case Role::System:    return "system";
        default:              return "user";
    }
}

Role role_from_string(std::string_view str) {
    if (str == "user")      return Role::User;
    if (str == "assistant") return Role::Assistant;
    if (str == "system")    return Role::System;
    return Role::User;
}

// ===== Helper functions =====

namespace {

std::string generate_part_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);
    
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";  // UUID v4
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 12; i++) {
        ss << dis(gen);
    }
    return ss.str();
}

int64_t get_current_time_ms() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // anonymous namespace

// ===== Part factory methods =====

Part Part::create_text(const std::string& content) {
    Part part;
    part.id = generate_part_id();
    part.type = PartType::Text;
    part.data = {{"content", content}};
    part.time_created = part.time_updated = get_current_time_ms();
    return part;
}

Part Part::create_tool(
    const std::string& tool_id,
    const std::string& tool_name,
    const nlohmann::json& arguments,
    const std::optional<nlohmann::json>& result
) {
    Part part;
    part.id = generate_part_id();
    part.type = PartType::Tool;
    part.data = {
        {"tool_id", tool_id},
        {"tool_name", tool_name},
        {"arguments", arguments}
    };
    if (result) {
        part.data["result"] = *result;
    }
    part.time_created = part.time_updated = get_current_time_ms();
    return part;
}

Part Part::create_reasoning(const std::string& thought) {
    Part part;
    part.id = generate_part_id();
    part.type = PartType::Reasoning;
    part.data = {{"thought", thought}};
    part.time_created = part.time_updated = get_current_time_ms();
    return part;
}

Part Part::create_file(
    const std::string& path,
    const std::optional<std::string>& content,
    const std::optional<std::string>& mime_type
) {
    Part part;
    part.id = generate_part_id();
    part.type = PartType::File;
    part.data = {{"path", path}};
    if (content) {
        part.data["content"] = *content;
    }
    if (mime_type) {
        part.data["mime_type"] = *mime_type;
    }
    part.time_created = part.time_updated = get_current_time_ms();
    return part;
}

Part Part::create_subtask(
    const std::string& task_id,
    const std::string& agent,
    const std::string& status
) {
    Part part;
    part.id = generate_part_id();
    part.type = PartType::Subtask;
    part.data = {
        {"task_id", task_id},
        {"agent", agent},
        {"status", status}
    };
    part.time_created = part.time_updated = get_current_time_ms();
    return part;
}

Part Part::create_step_start(const std::string& step_id, const std::string& name) {
    Part part;
    part.id = generate_part_id();
    part.type = PartType::StepStart;
    part.data = {
        {"step_id", step_id},
        {"name", name}
    };
    part.time_created = part.time_updated = get_current_time_ms();
    return part;
}

Part Part::create_step_finish(
    const std::string& step_id,
    const std::string& status,
    const std::optional<nlohmann::json>& result
) {
    Part part;
    part.id = generate_part_id();
    part.type = PartType::StepFinish;
    part.data = {
        {"step_id", step_id},
        {"status", status}
    };
    if (result) {
        part.data["result"] = *result;
    }
    part.time_created = part.time_updated = get_current_time_ms();
    return part;
}

Part Part::create_snapshot(const nlohmann::json& files) {
    Part part;
    part.id = generate_part_id();
    part.type = PartType::Snapshot;
    part.data = {{"files", files}};
    part.time_created = part.time_updated = get_current_time_ms();
    return part;
}

Part Part::create_patch(const std::string& file_path, const nlohmann::json& diff) {
    Part part;
    part.id = generate_part_id();
    part.type = PartType::Patch;
    part.data = {
        {"file_path", file_path},
        {"diff", diff}
    };
    part.time_created = part.time_updated = get_current_time_ms();
    return part;
}

Part Part::create_agent(
    const std::string& agent_id,
    const std::string& agent_name,
    const std::optional<std::string>& model
) {
    Part part;
    part.id = generate_part_id();
    part.type = PartType::Agent;
    part.data = {
        {"agent_id", agent_id},
        {"agent_name", agent_name}
    };
    if (model) {
        part.data["model"] = *model;
    }
    part.time_created = part.time_updated = get_current_time_ms();
    return part;
}

Part Part::create_retry(int attempt, const std::string& reason, int max_attempts) {
    Part part;
    part.id = generate_part_id();
    part.type = PartType::Retry;
    part.data = {
        {"attempt", attempt},
        {"reason", reason},
        {"max_attempts", max_attempts}
    };
    part.time_created = part.time_updated = get_current_time_ms();
    return part;
}

Part Part::create_compaction(
    int64_t original_tokens,
    int64_t compacted_tokens,
    const nlohmann::json& summary
) {
    Part part;
    part.id = generate_part_id();
    part.type = PartType::Compaction;
    part.data = {
        {"original_tokens", original_tokens},
        {"compacted_tokens", compacted_tokens},
        {"summary", summary}
    };
    part.time_created = part.time_updated = get_current_time_ms();
    return part;
}

// ===== Part data accessors =====

std::string Part::get_text() const {
    if (type == PartType::Text) {
        return data.value("content", std::string{});
    }
    return "";
}

nlohmann::json Part::get_tool() const {
    if (type == PartType::Tool) {
        return data;
    }
    return nlohmann::json::object();
}

std::string Part::get_reasoning() const {
    if (type == PartType::Reasoning) {
        return data.value("thought", std::string{});
    }
    return "";
}

nlohmann::json Part::get_file() const {
    if (type == PartType::File) {
        return data;
    }
    return nlohmann::json::object();
}

nlohmann::json Part::get_subtask() const {
    if (type == PartType::Subtask) {
        return data;
    }
    return nlohmann::json::object();
}

nlohmann::json Part::get_step() const {
    if (type == PartType::StepStart || type == PartType::StepFinish) {
        return data;
    }
    return nlohmann::json::object();
}

nlohmann::json Part::get_snapshot() const {
    if (type == PartType::Snapshot) {
        return data.value("files", nlohmann::json::object());
    }
    return nlohmann::json::object();
}

nlohmann::json Part::get_patch() const {
    if (type == PartType::Patch) {
        return data;
    }
    return nlohmann::json::object();
}

nlohmann::json Part::get_agent() const {
    if (type == PartType::Agent) {
        return data;
    }
    return nlohmann::json::object();
}

nlohmann::json Part::get_retry() const {
    if (type == PartType::Retry) {
        return data;
    }
    return nlohmann::json::object();
}

nlohmann::json Part::get_compaction() const {
    if (type == PartType::Compaction) {
        return data;
    }
    return nlohmann::json::object();
}

// ===== Part serialization =====

nlohmann::json Part::to_json() const {
    return nlohmann::json{
        {"id", id},
        {"message_id", message_id},
        {"session_id", session_id},
        {"type", std::string(part_type_to_string(type))},
        {"data", data},
        {"time_created", time_created},
        {"time_updated", time_updated}
    };
}

Part Part::from_json(const nlohmann::json& j) {
    Part part;
    part.id = j.value("id", std::string{});
    part.message_id = j.value("message_id", std::string{});
    part.session_id = j.value("session_id", std::string{});
    part.type = part_type_from_string(j.value("type", std::string{"text"}));
    part.data = j.value("data", nlohmann::json::object());
    part.time_created = j.value("time_created", int64_t{0});
    part.time_updated = j.value("time_updated", int64_t{0});
    return part;
}

} // namespace turbot::core
