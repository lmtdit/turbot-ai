#include <turbot/core/message/message.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/core/id/id.hpp>
#include <turbot/storage/database.hpp>
#include <turbot/utils/crypto_utils.hpp>
#include <chrono>
#include <sstream>

namespace turbot::core {

// ===== Utility functions =====

namespace {

// Generate a message ID using the central ID module.
// Mirrors OpenCode's MessageID.ascending() → "msg_<12hex><14base62>"
std::string generate_message_id() {
    return turbot::core::id::message_id();
}

int64_t get_current_time_ms() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // anonymous namespace

std::string generate_uuid() {
    return turbot::utils::crypto::generate_uuid();
}

int64_t current_timestamp_ms() {
    return get_current_time_ms();
}

// ===== MessageInfo serialization =====

nlohmann::json MessageInfo::to_json() const {
    nlohmann::json j = {
        {"id", id},
        {"session_id", session_id},
        {"role", std::string(role_to_string(role))},
        {"time_created", time_created},
        {"time_updated", time_updated},
        {"agent", agent},
        {"model_id", model_id},
        {"provider_id", provider_id},
        {"cost", cost},
        {"tokens", tokens.to_json()}
    };
    
    if (parent_id) j["parent_id"] = *parent_id;
    if (system) j["system"] = *system;
    if (tools) j["tools"] = *tools;
    if (variant) j["variant"] = *variant;
    if (error) j["error"] = *error;
    if (finish) j["finish"] = *finish;
    if (summary) j["summary"] = *summary;
    if (structured) j["structured"] = *structured;
    if (format) j["format"] = *format;    // G71
    if (path) j["path"] = path->to_json(); // G72
    
    return j;
}

MessageInfo MessageInfo::from_json(const nlohmann::json& j) {
    MessageInfo info;
    info.id = j.value("id", std::string{});
    info.session_id = j.value("session_id", std::string{});
    info.role = role_from_string(j.value("role", std::string{"user"}));
    info.time_created = j.value("time_created", int64_t{0});
    info.time_updated = j.value("time_updated", int64_t{0});
    info.agent = j.value("agent", std::string{});
    info.model_id = j.value("model_id", std::string{});
    info.provider_id = j.value("provider_id", std::string{});
    info.cost = j.value("cost", 0.0);
    
    if (j.contains("tokens")) {
        info.tokens = TokenUsage::from_json(j["tokens"]);
    }
    if (j.contains("parent_id")) info.parent_id = j["parent_id"].get<std::string>();
    if (j.contains("system")) info.system = j["system"].get<std::string>();
    if (j.contains("tools")) info.tools = j["tools"];
    if (j.contains("variant")) info.variant = j["variant"].get<std::string>();
    if (j.contains("error")) info.error = j["error"];
    if (j.contains("finish")) info.finish = j["finish"].get<std::string>();
    if (j.contains("summary")) info.summary = j["summary"].get<bool>();
    if (j.contains("structured")) info.structured = j["structured"];
    if (j.contains("format")) info.format = j["format"];  // G71
    if (j.contains("path") && j["path"].is_object()) {    // G72
        info.path = MessageInfo::PathInfo::from_json(j["path"]);
    }
    
    return info;
}

// ===== Message implementation =====

Message::Message(
    const std::string& session_id,
    Role role,
    const std::string& agent,
    const std::string& model_id,
    const std::string& provider_id
) : info_{} {
    generate_id();
    info_.session_id = session_id;
    info_.role = role;
    info_.agent = agent;
    info_.model_id = model_id;
    info_.provider_id = provider_id;
    update_timestamp();
}

Message::Message(
    const MessageInfo& info,
    std::vector<Part> parts,
    std::shared_ptr<turbot::storage::Database> db
) : info_(info), parts_(std::move(parts)), db_(std::move(db)) {
}

void Message::generate_id() {
    info_.id = generate_message_id();
}

void Message::update_timestamp() {
    auto now = get_current_time_ms();
    if (info_.time_created == 0) {
        info_.time_created = now;
    }
    info_.time_updated = now;
}

// ===== Part management =====

void Message::add_part(const Part& part) {
    Part p = part;
    p.message_id = info_.id;
    p.session_id = info_.session_id;
    parts_.push_back(p);
    update_timestamp();
}

void Message::add_text(const std::string& content) {
    add_part(Part::create_text(content));
}

void Message::add_tool(
    const std::string& tool_id,
    const std::string& tool_name,
    const nlohmann::json& arguments,
    const std::optional<nlohmann::json>& result
) {
    add_part(Part::create_tool(tool_id, tool_name, arguments, result));
}

void Message::add_reasoning(const std::string& thought) {
    add_part(Part::create_reasoning(thought));
}

std::vector<Part> Message::get_parts(PartType type) const {
    std::vector<Part> result;
    for (const auto& part : parts_) {
        if (part.type == type) {
            result.push_back(part);
        }
    }
    return result;
}

std::string Message::get_text() const {
    std::string text;
    for (const auto& part : parts_) {
        if (part.is_text()) {
            text += part.get_text();
        }
    }
    return text;
}

std::vector<nlohmann::json> Message::get_tool_calls() const {
    std::vector<nlohmann::json> calls;
    for (const auto& part : parts_) {
        if (part.is_tool()) {
            calls.push_back(part.get_tool());
        }
    }
    return calls;
}

bool Message::has_tool_calls() const {
    for (const auto& part : parts_) {
        if (part.is_tool()) {
            return true;
        }
    }
    return false;
}

std::string Message::get_full_text() const {
    std::string text;
    for (const auto& part : parts_) {
        if (part.is_text()) {
            text += part.get_text();
        } else if (part.is_reasoning()) {
            text += part.get_reasoning();
        }
    }
    return text;
}

// ===== Status management =====

void Message::set_error(const nlohmann::json& error) {
    info_.error = error;
    update_timestamp();
}

void Message::set_finish(const std::string& finish_reason) {
    info_.finish = finish_reason;
    update_timestamp();
}

void Message::update_tokens(const TokenUsage& tokens, const nlohmann::json& pricing) {
    info_.tokens += tokens;
    info_.cost += tokens.cost(pricing);
    update_timestamp();
}

// ===== Persistence =====

void Message::save() {
    if (!db_) {
        TURBOT_LOG_ERROR("No database set for message persistence");
        return;
    }
    
    MessageDao dao(db_);
    dao.create_message(info_);
    
    for (const auto& part : parts_) {
        Part p = part;
        p.message_id = info_.id;
        p.session_id = info_.session_id;
        dao.create_part(p);
    }
}

void Message::update() {
    if (!db_) {
        TURBOT_LOG_ERROR("No database set for message persistence");
        return;
    }
    
    MessageDao dao(db_);
    dao.update_message(info_);
}

void Message::remove() {
    if (!db_) {
        TURBOT_LOG_ERROR("No database set for message persistence");
        return;
    }
    
    MessageDao dao(db_);
    dao.delete_parts(info_.id);
    dao.delete_message(info_.id);
}

// ===== Static queries =====

std::optional<Message> Message::get(
    const std::string& id,
    std::shared_ptr<turbot::storage::Database> db
) {
    if (!db) {
        return std::nullopt;
    }
    
    MessageDao dao(db);
    auto info = dao.get_message(id);
    if (!info) {
        return std::nullopt;
    }
    
    auto parts = dao.list_parts(id);
    return Message(*info, parts, db);
}

std::vector<Message> Message::list_by_session(
    const std::string& session_id,
    std::shared_ptr<turbot::storage::Database> db,
    int limit,
    int offset
) {
    if (!db) {
        return {};
    }
    
    MessageDao dao(db);
    auto infos = dao.list_messages_by_session(session_id, limit, offset);
    
    std::vector<Message> messages;
    messages.reserve(infos.size());
    
    for (const auto& info : infos) {
        auto parts = dao.list_parts(info.id);
        messages.emplace_back(info, parts, db);
    }
    
    return messages;
}

std::vector<Message> Message::query(
    const std::string& session_id,
    std::shared_ptr<turbot::storage::Database> db,
    [[maybe_unused]] const nlohmann::json& filter
) {
    // TODO: Implement query with filter
    return list_by_session(session_id, db);
}

void Message::remove_by_session(
    const std::string& session_id,
    std::shared_ptr<turbot::storage::Database> db
) {
    if (!db) {
        return;
    }
    
    MessageDao dao(db);
    dao.delete_messages_by_session(session_id);
}

// ===== Serialization =====

nlohmann::json Message::to_json() const {
    nlohmann::json j = info_.to_json();
    j["parts"] = nlohmann::json::array();
    for (const auto& part : parts_) {
        j["parts"].push_back(part.to_json());
    }
    return j;
}

Message Message::from_json(const nlohmann::json& j, std::shared_ptr<turbot::storage::Database> db) {
    MessageInfo info = MessageInfo::from_json(j);
    
    std::vector<Part> parts;
    if (j.contains("parts") && j["parts"].is_array()) {
        for (const auto& part_json : j["parts"]) {
            parts.push_back(Part::from_json(part_json));
        }
    }
    
    return Message(info, parts, db);
}

// ===== MessageDao implementation =====

MessageDao::MessageDao(std::shared_ptr<turbot::storage::Database> db)
    : db_(std::move(db)) {
}

void MessageDao::create_message(const MessageInfo& info) {
    std::string sql = R"(
        INSERT INTO messages (
            id, session_id, role, time_created, time_updated,
            parent_id, agent, model_id, provider_id, system,
            tools, variant, error, finish, cost, tokens,
            summary, structured
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    
    std::vector<nlohmann::json> params = {
        info.id,
        info.session_id,
        std::string(role_to_string(info.role)),
        info.time_created,
        info.time_updated,
        info.parent_id.value_or(""),
        info.agent,
        info.model_id,
        info.provider_id,
        info.system.value_or(""),
        info.tools.value_or(nlohmann::json::object()),
        info.variant.value_or(""),
        info.error.value_or(nlohmann::json::object()),
        info.finish.value_or(""),
        info.cost,
        info.tokens.to_json(),
        info.summary.value_or(false),
        info.structured.value_or(nlohmann::json::object())
    };
    
    db_->execute(sql, params);
}

std::optional<MessageInfo> MessageDao::get_message(const std::string& id) {
    std::string sql = "SELECT * FROM messages WHERE id = ?";
    
    auto result = db_->execute_one(sql, {id});
    if (!result) {
        return std::nullopt;
    }
    
    MessageInfo info;
    info.id = result->value("id", std::string{});
    info.session_id = result->value("session_id", std::string{});
    info.role = role_from_string(result->value("role", std::string{"user"}), /*strict=*/true);
    info.time_created = result->value("time_created", int64_t{0});
    info.time_updated = result->value("time_updated", int64_t{0});
    info.agent = result->value("agent", std::string{});
    info.model_id = result->value("model_id", std::string{});
    info.provider_id = result->value("provider_id", std::string{});
    info.cost = result->value("cost", 0.0);
    
    // Optional fields - need to parse JSON strings stored in SQLite
    if (result->contains("parent_id")) {
        auto val = (*result)["parent_id"];
        if (val.is_string() && !val.get<std::string>().empty()) {
            info.parent_id = val.get<std::string>();
        }
    }
    if (result->contains("system")) {
        auto val = (*result)["system"];
        if (val.is_string() && !val.get<std::string>().empty()) {
            info.system = val.get<std::string>();
        }
    }
    if (result->contains("tokens")) {
        auto val = (*result)["tokens"];
        if (val.is_string()) {
            try {
                info.tokens = TokenUsage::from_json(nlohmann::json::parse(val.get<std::string>()));
            } catch (const nlohmann::json::parse_error& e) {
                TURBOT_LOG_ERROR("Failed to parse tokens JSON: {}", e.what());
            }
        } else if (val.is_object()) {
            info.tokens = TokenUsage::from_json(val);
        }
    }
    if (result->contains("tools")) {
        auto val = (*result)["tools"];
        if (val.is_string()) {
            try {
                auto parsed = nlohmann::json::parse(val.get<std::string>());
                if (!parsed.empty()) info.tools = parsed;
            } catch (const nlohmann::json::parse_error& e) {
                TURBOT_LOG_ERROR("Failed to parse tools JSON: {}", e.what());
            }
        } else if (val.is_object() || val.is_array()) {
            if (!val.empty()) info.tools = val;
        }
    }
    if (result->contains("variant")) {
        auto val = (*result)["variant"];
        if (val.is_string() && !val.get<std::string>().empty()) {
            info.variant = val.get<std::string>();
        }
    }
    if (result->contains("error")) {
        auto val = (*result)["error"];
        if (val.is_string()) {
            try {
                auto parsed = nlohmann::json::parse(val.get<std::string>());
                if (!parsed.empty()) info.error = parsed;
            } catch (const nlohmann::json::parse_error& e) {
                TURBOT_LOG_ERROR("Failed to parse error JSON: {}", e.what());
            }
        } else if (val.is_object()) {
            if (!val.empty()) info.error = val;
        }
    }
    if (result->contains("finish")) {
        auto val = (*result)["finish"];
        if (val.is_string() && !val.get<std::string>().empty()) {
            info.finish = val.get<std::string>();
        }
    }
    if (result->contains("summary")) {
        auto val = (*result)["summary"];
        if (val.is_boolean()) {
            info.summary = val.get<bool>();
        } else if (val.is_number_integer()) {
            info.summary = val.get<int>() != 0;
        }
    }
    if (result->contains("structured")) {
        auto val = (*result)["structured"];
        if (val.is_string()) {
            try {
                auto parsed = nlohmann::json::parse(val.get<std::string>());
                if (!parsed.empty()) info.structured = parsed;
            } catch (const nlohmann::json::parse_error& e) {
                TURBOT_LOG_ERROR("Failed to parse structured JSON: {}", e.what());
            }
        } else if (val.is_object() || val.is_array()) {
            if (!val.empty()) info.structured = val;
        }
    }
    
    return info;
}

void MessageDao::update_message(const MessageInfo& info) {
    std::string sql = R"(
        UPDATE messages SET
            time_updated = ?,
            error = ?,
            finish = ?,
            cost = ?,
            tokens = ?,
            summary = ?,
            structured = ?
        WHERE id = ?
    )";
    
    std::vector<nlohmann::json> params = {
        info.time_updated,
        info.error.value_or(nlohmann::json::object()),
        info.finish.value_or(""),
        info.cost,
        info.tokens.to_json(),
        info.summary.value_or(false),
        info.structured.value_or(nlohmann::json::object()),
        info.id
    };
    
    db_->execute(sql, params);
}

void MessageDao::delete_message(const std::string& id) {
    db_->execute("DELETE FROM messages WHERE id = ?", {id});
}

std::vector<MessageInfo> MessageDao::list_messages_by_session(
    const std::string& session_id,
    int limit,
    int offset
) {
    std::string sql = "SELECT * FROM messages WHERE session_id = ? ORDER BY time_created ASC LIMIT ? OFFSET ?";
    
    auto result = db_->execute(sql, {session_id, limit, offset});
    
    std::vector<MessageInfo> messages;
    for (const auto& row : result.rows) {
        MessageInfo info;
        info.id = row.value("id", std::string{});
        info.session_id = row.value("session_id", std::string{});
        info.role = role_from_string(row.value("role", std::string{"user"}), /*strict=*/true);
        info.time_created = row.value("time_created", int64_t{0});
        info.time_updated = row.value("time_updated", int64_t{0});
        info.agent = row.value("agent", std::string{});
        info.model_id = row.value("model_id", std::string{});
        info.provider_id = row.value("provider_id", std::string{});
        info.cost = row.value("cost", 0.0);
        
        // Handle JSON strings stored in SQLite
        if (row.contains("tokens")) {
            auto val = row["tokens"];
            if (val.is_string()) {
                try {
                    info.tokens = TokenUsage::from_json(nlohmann::json::parse(val.get<std::string>()));
                } catch (const nlohmann::json::parse_error& e) {
                    TURBOT_LOG_ERROR("Failed to parse tokens JSON: {}", e.what());
                }
            } else if (val.is_object()) {
                info.tokens = TokenUsage::from_json(val);
            }
        }
        if (row.contains("parent_id")) {
            auto val = row["parent_id"];
            if (val.is_string() && !val.get<std::string>().empty()) {
                info.parent_id = val.get<std::string>();
            }
        }
        if (row.contains("system")) {
            auto val = row["system"];
            if (val.is_string() && !val.get<std::string>().empty()) {
                info.system = val.get<std::string>();
            }
        }
        if (row.contains("finish")) {
            auto val = row["finish"];
            if (val.is_string() && !val.get<std::string>().empty()) {
                info.finish = val.get<std::string>();
            }
        }
        
        messages.push_back(info);
    }
    
    return messages;
}

void MessageDao::create_part(const Part& part) {
    std::string sql = R"(
        INSERT INTO parts (
            id, message_id, session_id, type, data,
            time_created, time_updated
        ) VALUES (?, ?, ?, ?, ?, ?, ?)
    )";
    
    std::vector<nlohmann::json> params = {
        part.id,
        part.message_id,
        part.session_id,
        std::string(part_type_to_string(part.type)),
        part.data,
        part.time_created,
        part.time_updated
    };
    
    db_->execute(sql, params);
}

std::vector<Part> MessageDao::list_parts(const std::string& message_id) {
    std::string sql = "SELECT * FROM parts WHERE message_id = ? ORDER BY time_created ASC";
    
    auto result = db_->execute(sql, {message_id});
    
    std::vector<Part> parts;
    for (const auto& row : result.rows) {
        Part part;
        part.id = row.value("id", std::string{});
        part.message_id = row.value("message_id", std::string{});
        part.session_id = row.value("session_id", std::string{});
        part.type = part_type_from_string(row.value("type", std::string{"text"}));
        
        // Handle JSON string stored in SQLite
        auto data_val = row["data"];
        if (data_val.is_string()) {
            try {
                part.data = nlohmann::json::parse(data_val.get<std::string>());
            } catch (const nlohmann::json::parse_error& e) {
                TURBOT_LOG_ERROR("Failed to parse part data JSON: {}", e.what());
                part.data = nullptr;
            }
        } else {
            part.data = data_val;
        }
        
        part.time_created = row.value("time_created", int64_t{0});
        part.time_updated = row.value("time_updated", int64_t{0});
        parts.push_back(part);
    }
    
    return parts;
}

void MessageDao::delete_parts(const std::string& message_id) {
    db_->execute("DELETE FROM parts WHERE message_id = ?", {message_id});
}

void MessageDao::delete_messages_by_session(const std::string& session_id) {
    db_->execute("DELETE FROM parts WHERE session_id = ?", {session_id});
    db_->execute("DELETE FROM messages WHERE session_id = ?", {session_id});
}

} // namespace turbot::core
