#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/message/token_usage.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace turbot::core {

/// Part type enumeration
enum class PartType {
    Text,        ///< Text content
    Tool,        ///< Tool call
    Reasoning,   ///< Reasoning process
    File,        ///< File reference
    Subtask,     ///< Subtask reference
    StepStart,   ///< Step start marker
    StepFinish,  ///< Step finish marker
    Snapshot,    ///< File snapshot
    Patch,       ///< Code patch/diff
    Agent,       ///< Agent info
    Retry,       ///< Retry information
    Compaction   ///< Context compaction
};

/// Convert PartType to string
[[nodiscard]] TURBOT_CORE_API std::string_view part_type_to_string(PartType type) noexcept;

/// Parse PartType from string
[[nodiscard]] TURBOT_CORE_API PartType part_type_from_string(std::string_view str);

/// Part - A component of a message
struct TURBOT_CORE_API Part {
    std::string id;
    std::string message_id;
    std::string session_id;
    PartType type = PartType::Text;
    nlohmann::json data;
    int64_t time_created = 0;
    int64_t time_updated = 0;

    // ===== Factory methods =====

    /// Create a text part
    [[nodiscard]] static Part create_text(const std::string& content);

    /// Create a tool call part
    [[nodiscard]] static Part create_tool(
        const std::string& tool_id,
        const std::string& tool_name,
        const nlohmann::json& arguments,
        const std::optional<nlohmann::json>& result = std::nullopt
    );

    /// Create a reasoning part
    [[nodiscard]] static Part create_reasoning(const std::string& thought);

    /// Create a file reference part
    [[nodiscard]] static Part create_file(
        const std::string& path,
        const std::optional<std::string>& content = std::nullopt,
        const std::optional<std::string>& mime_type = std::nullopt
    );

    /// Create a subtask reference part
    [[nodiscard]] static Part create_subtask(
        const std::string& task_id,
        const std::string& agent,
        const std::string& status
    );

    /// Create a step start part
    [[nodiscard]] static Part create_step_start(const std::string& step_id, const std::string& name = "");

    /// Create a step finish part
    [[nodiscard]] static Part create_step_finish(
        const std::string& step_id,
        const std::string& status,
        const std::optional<nlohmann::json>& result = std::nullopt
    );

    /// Create a snapshot part
    [[nodiscard]] static Part create_snapshot(const nlohmann::json& files);

    /// Create a patch part
    [[nodiscard]] static Part create_patch(
        const std::string& file_path,
        const nlohmann::json& diff
    );

    /// Create an agent part
    [[nodiscard]] static Part create_agent(
        const std::string& agent_id,
        const std::string& agent_name,
        const std::optional<std::string>& model = std::nullopt
    );

    /// Create a retry part
    [[nodiscard]] static Part create_retry(
        int attempt,
        const std::string& reason,
        int max_attempts = 3
    );

    /// Create a compaction part
    [[nodiscard]] static Part create_compaction(
        int64_t original_tokens,
        int64_t compacted_tokens,
        const nlohmann::json& summary
    );

    // ===== Type checking =====

    [[nodiscard]] bool is_text() const noexcept { return type == PartType::Text; }
    [[nodiscard]] bool is_tool() const noexcept { return type == PartType::Tool; }
    [[nodiscard]] bool is_reasoning() const noexcept { return type == PartType::Reasoning; }
    [[nodiscard]] bool is_file() const noexcept { return type == PartType::File; }
    [[nodiscard]] bool is_subtask() const noexcept { return type == PartType::Subtask; }
    [[nodiscard]] bool is_step_start() const noexcept { return type == PartType::StepStart; }
    [[nodiscard]] bool is_step_finish() const noexcept { return type == PartType::StepFinish; }
    [[nodiscard]] bool is_snapshot() const noexcept { return type == PartType::Snapshot; }
    [[nodiscard]] bool is_patch() const noexcept { return type == PartType::Patch; }
    [[nodiscard]] bool is_agent() const noexcept { return type == PartType::Agent; }
    [[nodiscard]] bool is_retry() const noexcept { return type == PartType::Retry; }
    [[nodiscard]] bool is_compaction() const noexcept { return type == PartType::Compaction; }

    // ===== Data accessors =====

    /// Get text content (for TextPart)
    [[nodiscard]] std::string get_text() const;

    /// Get tool call info (for ToolPart)
    [[nodiscard]] nlohmann::json get_tool() const;

    /// Get reasoning text (for ReasoningPart)
    [[nodiscard]] std::string get_reasoning() const;

    /// Get file info (for FilePart)
    [[nodiscard]] nlohmann::json get_file() const;

    /// Get subtask info (for SubtaskPart)
    [[nodiscard]] nlohmann::json get_subtask() const;

    /// Get step info (for StepStart/StepFinish)
    [[nodiscard]] nlohmann::json get_step() const;

    /// Get snapshot files (for SnapshotPart)
    [[nodiscard]] nlohmann::json get_snapshot() const;

    /// Get patch diff (for PatchPart)
    [[nodiscard]] nlohmann::json get_patch() const;

    /// Get agent info (for AgentPart)
    [[nodiscard]] nlohmann::json get_agent() const;

    /// Get retry info (for RetryPart)
    [[nodiscard]] nlohmann::json get_retry() const;

    /// Get compaction info (for CompactionPart)
    [[nodiscard]] nlohmann::json get_compaction() const;

    // ===== Serialization =====

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static Part from_json(const nlohmann::json& j);
};

/// Role in conversation
enum class Role {
    User,      ///< User message
    Assistant, ///< AI assistant message
    System     ///< System message
};

/// Convert Role to string
[[nodiscard]] TURBOT_CORE_API std::string_view role_to_string(Role role) noexcept;

/// Parse Role from string
[[nodiscard]] TURBOT_CORE_API Role role_from_string(std::string_view str);

} // namespace turbot::core
