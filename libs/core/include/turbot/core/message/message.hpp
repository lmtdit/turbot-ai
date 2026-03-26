#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/message/part.hpp>
#include <turbot/core/message/token_usage.hpp>
#include <nlohmann/json.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Forward declaration to avoid circular dependency
namespace turbot::storage {
class Database;
}

namespace turbot::core {

/// Message metadata
struct TURBOT_CORE_API MessageInfo {
    std::string id;
    std::string session_id;
    Role role = Role::User;
    int64_t time_created = 0;
    int64_t time_updated = 0;
    std::optional<std::string> parent_id;
    std::string agent;
    std::string model_id;
    std::string provider_id;
    std::optional<std::string> system;
    std::optional<nlohmann::json> tools;
    std::optional<std::string> variant;
    std::optional<nlohmann::json> error;
    std::optional<std::string> finish;
    double cost = 0.0;
    TokenUsage tokens;
    std::optional<bool> summary;
    std::optional<nlohmann::json> structured;

    /// G71: Output format for structured output requests.
    /// Mirrors opencode MessageV2.User.format (OutputFormat: {type:"text"} | {type:"json_schema",...})
    std::optional<nlohmann::json> format;

    /// G72: Working directory paths recorded at AI run time.
    /// Mirrors opencode MessageV2.Assistant.path: {cwd: string, root: string}
    struct PathInfo {
        std::string cwd;
        std::string root;
        [[nodiscard]] nlohmann::json to_json() const { return {{"cwd", cwd}, {"root", root}}; }
        static PathInfo from_json(const nlohmann::json& j) {
            return PathInfo{j.value("cwd", std::string{}), j.value("root", std::string{})};
        }
    };
    std::optional<PathInfo> path;

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static MessageInfo from_json(const nlohmann::json& j);
};

/// Message - A single message in a conversation
class TURBOT_CORE_API Message {
public:
    /// Create a new message
    /// @param session_id Session this message belongs to
    /// @param role Message role
    /// @param agent Agent name
    /// @param model_id Model identifier
    /// @param provider_id Provider identifier
    Message(
        const std::string& session_id,
        Role role,
        const std::string& agent,
        const std::string& model_id,
        const std::string& provider_id
    );

    /// Create message from database
    Message(
        const MessageInfo& info,
        std::vector<Part> parts,
        std::shared_ptr<turbot::storage::Database> db
    );

    // ===== Getters =====

    [[nodiscard]] const std::string& id() const noexcept { return info_.id; }
    [[nodiscard]] const std::string& session_id() const noexcept { return info_.session_id; }
    [[nodiscard]] Role role() const noexcept { return info_.role; }
    [[nodiscard]] const MessageInfo& info() const noexcept { return info_; }
    [[nodiscard]] const std::vector<Part>& parts() const noexcept { return parts_; }
    [[nodiscard]] const TokenUsage& tokens() const noexcept { return info_.tokens; }
    [[nodiscard]] double cost() const noexcept { return info_.cost; }

    /// Non-const access to a specific part (for in-place mutation, e.g. prune compaction).
    /// @throws std::out_of_range if idx >= parts_.size()
    [[nodiscard]] Part& mutable_part(size_t idx) { return parts_.at(idx); }

    // ===== Part management =====

    /// Add a part to this message
    void add_part(const Part& part);

    /// Add a text part
    void add_text(const std::string& content);

    /// Add a tool call part
    void add_tool(
        const std::string& tool_id,
        const std::string& tool_name,
        const nlohmann::json& arguments,
        const std::optional<nlohmann::json>& result = std::nullopt
    );

    /// Add a reasoning part
    void add_reasoning(const std::string& thought);

    /// Get parts of a specific type
    [[nodiscard]] std::vector<Part> get_parts(PartType type) const;

    /// Get all text content from text parts
    [[nodiscard]] std::string get_text() const;

    /// Get all tool calls
    [[nodiscard]] std::vector<nlohmann::json> get_tool_calls() const;

    /// Check if message has tool calls
    [[nodiscard]] bool has_tool_calls() const;

    /// Get full text including reasoning
    [[nodiscard]] std::string get_full_text() const;

    // ===== Status management =====

    /// Set error information
    void set_error(const nlohmann::json& error);

    /// Set finish reason
    void set_finish(const std::string& finish_reason);

    /// Update token usage and cost
    void update_tokens(const TokenUsage& tokens, const nlohmann::json& pricing);

    /// Set database for persistence
    void set_database(std::shared_ptr<turbot::storage::Database> db) { db_ = std::move(db); }

    // ===== Persistence =====

    /// Save message to database
    void save();

    /// Update existing message in database
    void update();

    /// Delete message from database
    void remove();

    // ===== Static queries =====

    /// Get a message by ID
    [[nodiscard]] static std::optional<Message> get(
        const std::string& id,
        std::shared_ptr<turbot::storage::Database> db
    );

    /// Get messages for a session
    [[nodiscard]] static std::vector<Message> list_by_session(
        const std::string& session_id,
        std::shared_ptr<turbot::storage::Database> db,
        int limit = 50,
        int offset = 0
    );

    /// Query messages with filter
    [[nodiscard]] static std::vector<Message> query(
        const std::string& session_id,
        std::shared_ptr<turbot::storage::Database> db,
        const nlohmann::json& filter
    );

    /// Delete all messages for a session
    static void remove_by_session(
        const std::string& session_id,
        std::shared_ptr<turbot::storage::Database> db
    );

    // ===== Serialization =====

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static Message from_json(const nlohmann::json& j, std::shared_ptr<turbot::storage::Database> db = nullptr);

private:
    MessageInfo info_;
    std::vector<Part> parts_;
    std::shared_ptr<turbot::storage::Database> db_;

    void generate_id();
    void update_timestamp();
};

/// Message DAO (Data Access Object)
class TURBOT_CORE_API MessageDao {
public:
    explicit MessageDao(std::shared_ptr<turbot::storage::Database> db);

    // Message operations
    void create_message(const MessageInfo& info);
    [[nodiscard]] std::optional<MessageInfo> get_message(const std::string& id);
    void update_message(const MessageInfo& info);
    void delete_message(const std::string& id);
    [[nodiscard]] std::vector<MessageInfo> list_messages_by_session(
        const std::string& session_id,
        int limit = 50,
        int offset = 0
    );

    // Part operations
    void create_part(const Part& part);
    [[nodiscard]] std::vector<Part> list_parts(const std::string& message_id);
    void delete_parts(const std::string& message_id);

    // Bulk operations
    void delete_messages_by_session(const std::string& session_id);

private:
    std::shared_ptr<turbot::storage::Database> db_;
};

// Utility functions

/// Generate a UUID v4
[[nodiscard]] TURBOT_CORE_API std::string generate_uuid();

/// Get current timestamp in milliseconds since epoch
[[nodiscard]] TURBOT_CORE_API int64_t current_timestamp_ms();

} // namespace turbot::core
