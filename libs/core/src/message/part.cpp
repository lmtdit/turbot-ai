#include <turbot/core/message/part.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/crypto_utils.hpp>
#include <fmt/format.h>
#include <chrono>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <unordered_map>

namespace turbot::core {

// ===== PartType conversion =====

std::string_view part_type_to_string(PartType type) noexcept {
    static const std::unordered_map<PartType, std::string_view> type_to_str = {
        {PartType::Text, "text"},
        {PartType::Tool, "tool"},
        {PartType::Reasoning, "reasoning"},
        {PartType::File, "file"},
        {PartType::Image, "image"},
        {PartType::Error, "error"},
        {PartType::Subtask, "subtask"},
        {PartType::StepStart, "step_start"},
        {PartType::StepFinish, "step_finish"},
        {PartType::Snapshot, "snapshot"},
        {PartType::Patch, "patch"},
        {PartType::Agent, "agent"},
        {PartType::Retry, "retry"},
        {PartType::Compaction, "compaction"},
        {PartType::Source, "source"}
    };
    auto it = type_to_str.find(type);
    return it != type_to_str.end() ? it->second : "text";
}

PartType part_type_from_string(std::string_view str) {
    static const std::unordered_map<std::string_view, PartType> str_to_type = {
        {"text", PartType::Text},
        {"tool", PartType::Tool},
        {"reasoning", PartType::Reasoning},
        {"file", PartType::File},
        {"image", PartType::Image},
        {"error", PartType::Error},
        {"subtask", PartType::Subtask},
        {"step_start", PartType::StepStart},
        {"step_finish", PartType::StepFinish},
        {"snapshot", PartType::Snapshot},
        {"patch", PartType::Patch},
        {"agent", PartType::Agent},
        {"retry", PartType::Retry},
        {"compaction", PartType::Compaction},
        {"source", PartType::Source}
    };
    auto it = str_to_type.find(str);
    if (it == str_to_type.end()) {
        TURBOT_LOG_WARN("part_type_from_string: unknown type '{}', defaulting to Text", str);
        return PartType::Text;
    }
    return it->second;
}

// ===== Role conversion =====

std::string_view role_to_string(Role role) noexcept {
    static const std::unordered_map<Role, std::string_view> role_to_str = {
        {Role::User, "user"},
        {Role::Assistant, "assistant"},
        {Role::System, "system"},
        {Role::Tool, "tool"}
    };
    auto it = role_to_str.find(role);
    return it != role_to_str.end() ? it->second : "user";
}

Role role_from_string(std::string_view str, bool strict) {
    static const std::unordered_map<std::string_view, Role> str_to_role = {
        {"user", Role::User},
        {"assistant", Role::Assistant},
        {"system", Role::System},
        {"tool", Role::Tool}
    };
    auto it = str_to_role.find(str);
    if (it == str_to_role.end()) {
        if (strict) {
            throw std::invalid_argument(
                fmt::format("Unknown role string: '{}'. Valid: user, assistant, system, tool", str));
        }
        TURBOT_LOG_WARN("role_from_string: unknown role '{}', defaulting to User", str);
        return Role::User;
    }
    return it->second;
}

// ===== Helper functions =====

namespace {

// 使用 crypto::generate_uuid() 确保线程安全
std::string generate_part_id() {
    return turbot::utils::crypto::generate_uuid();
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

Part Part::create_image(
    const std::string& url,
    const std::optional<std::string>& alt_text,
    const std::optional<std::string>& mime_type
) {
    Part part;
    part.id = generate_part_id();
    part.type = PartType::Image;
    part.data = {{"url", url}};
    if (alt_text) {
        part.data["alt_text"] = *alt_text;
    }
    if (mime_type) {
        part.data["mime_type"] = *mime_type;
    }
    part.time_created = part.time_updated = get_current_time_ms();
    return part;
}

Part Part::create_image_base64(
    const std::string& base64_data,
    const std::string& mime_type,
    const std::optional<std::string>& alt_text
) {
    Part part;
    part.id = generate_part_id();
    part.type = PartType::Image;
    part.data = {
        {"url", "data:" + mime_type + ";base64," + base64_data},
        {"mime_type", mime_type}
    };
    if (alt_text) {
        part.data["alt_text"] = *alt_text;
    }
    part.time_created = part.time_updated = get_current_time_ms();
    return part;
}

Part Part::create_error(
    const std::string& message,
    const std::optional<std::string>& code,
    const std::optional<nlohmann::json>& details
) {
    Part part;
    part.id = generate_part_id();
    part.type = PartType::Error;
    part.data = {{"message", message}};
    if (code) {
        part.data["code"] = *code;
    }
    if (details) {
        part.data["details"] = *details;
    }
    part.time_created = part.time_updated = get_current_time_ms();
    return part;
}

Part Part::create_source(
    const std::string& source_id,
    const std::string& source_type,
    const std::optional<std::string>& title,
    const std::optional<std::string>& url
) {
    Part part;
    part.id = generate_part_id();
    part.type = PartType::Source;
    part.data = {
        {"source_id", source_id},
        {"source_type", source_type}
    };
    if (title) {
        part.data["title"] = *title;
    }
    if (url) {
        part.data["url"] = *url;
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

nlohmann::json Part::get_image() const {
    if (type == PartType::Image) {
        return data;
    }
    return nlohmann::json::object();
}

nlohmann::json Part::get_error() const {
    if (type == PartType::Error) {
        return data;
    }
    return nlohmann::json::object();
}

nlohmann::json Part::get_source() const {
    if (type == PartType::Source) {
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
