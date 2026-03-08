#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/permission/permission.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace turbot::core::agent {

/// Agent mode determines how the agent is used
enum class TURBOT_CORE_API AgentMode {
    Primary,   ///< Main agent that can execute tools
    Subagent,  ///< Sub-agent spawned by primary agent
    All        ///< Special value for listing all agents
};

/// Convert AgentMode to string
[[nodiscard]] TURBOT_CORE_API std::string agent_mode_to_string(AgentMode mode);

/// Convert string to AgentMode
[[nodiscard]] TURBOT_CORE_API AgentMode string_to_agent_mode(const std::string& str);

/// Model reference for an agent
struct TURBOT_CORE_API ModelRef {
    std::string model_id;      ///< Model identifier (e.g., "claude-3-opus")
    std::string provider_id;   ///< Provider identifier (e.g., "anthropic")

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static ModelRef from_json(const nlohmann::json& j);

    /// Equality comparison
    bool operator==(const ModelRef& other) const noexcept {
        return model_id == other.model_id && provider_id == other.provider_id;
    }
};

/// Agent information structure
struct TURBOT_CORE_API AgentInfo {
    // === Basic fields (v1.0) ===
    std::string name;                      ///< Agent identifier
    std::optional<std::string> description; ///< Human-readable description
    AgentMode mode = AgentMode::Primary;   ///< Agent mode
    bool native = false;                   ///< Whether this is a native (built-in) agent
    bool hidden = false;                   ///< Whether this agent should be hidden from UI
    permission::Ruleset permission;        ///< Permission rules for this agent
    std::optional<ModelRef> model;         ///< Preferred model (model_id + provider_id)
    nlohmann::json options;                ///< Additional agent options (excluded from equality comparison)

    // === Extended fields (v2.0) ===
    std::optional<std::string> prompt;     ///< Agent-specific system prompt template
    std::optional<double> temperature;     ///< Generation temperature (0.0-2.0, validated)
    std::optional<double> top_p;           ///< Top-p sampling parameter (0.0-1.0, validated)
    std::optional<int> steps;              ///< Maximum execution steps (>= 1, validated)
    std::optional<std::string> color;      ///< UI color identifier (hex format: #RRGGBB or name)
    std::optional<std::string> variant;    ///< Model variant identifier

    /// Validate field values
    /// @return true if all fields are valid
    [[nodiscard]] bool validate() const noexcept;

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    /// @throws std::out_of_range if temperature/top_p/steps are out of valid range
    /// @throws nlohmann::json::out_of_range if required fields are missing
    static AgentInfo from_json(const nlohmann::json& j);

    /// Equality comparison (excludes options field)
    bool operator==(const AgentInfo& other) const noexcept;
};

/// Parameters for agent execution
struct TURBOT_CORE_API ExecuteParams {
    std::string session_id;       ///< Session ID for this execution
    std::string prompt;           ///< User prompt/input
    nlohmann::json context;       ///< Additional context data
    std::optional<std::string> model_override; ///< Override model for this execution
};

/// Result of agent execution
struct TURBOT_CORE_API ExecuteResult {
    std::string output;           ///< Agent output text
    nlohmann::json metadata;      ///< Structured metadata
    bool is_success = true;       ///< Whether execution succeeded
    std::optional<std::string> error_message; ///< Error message if failed

    /// Create a success result
    [[nodiscard]] static ExecuteResult ok(
        const std::string& output,
        const nlohmann::json& metadata = nlohmann::json::object()
    );

    /// Create an error result
    [[nodiscard]] static ExecuteResult error(const std::string& error_msg);
};

/// Abstract base class for all agents
class TURBOT_CORE_API Agent {
public:
    virtual ~Agent() = default;

    // Non-copyable
    Agent(const Agent&) = delete;
    Agent& operator=(const Agent&) = delete;

    // Movable
    Agent(Agent&&) = default;
    Agent& operator=(Agent&&) = default;

    /// Get the agent name (unique identifier)
    [[nodiscard]] virtual std::string name() const = 0;

    /// Get the agent description
    [[nodiscard]] virtual std::string description() const = 0;

    /// Execute the agent with given parameters
    /// @param params Execution parameters
    /// @return Execution result
    [[nodiscard]] virtual ExecuteResult execute(const ExecuteParams& params) = 0;

    /// Get the agent info
    [[nodiscard]] const AgentInfo& info() const noexcept { return info_; }

    // ===== Convenience accessors for common properties =====

    /// Get the agent's system prompt (if configured)
    [[nodiscard]] std::optional<std::string> prompt() const noexcept { return info_.prompt; }

    /// Get the preferred model reference (if configured)
    [[nodiscard]] std::optional<ModelRef> model() const noexcept { return info_.model; }

    /// Get the preferred model ID (if configured)
    [[nodiscard]] std::optional<std::string> model_id() const noexcept {
        return info_.model.has_value() ? std::optional<std::string>(info_.model->model_id) : std::nullopt;
    }

    /// Get the preferred provider ID (if configured)
    [[nodiscard]] std::optional<std::string> provider_id() const noexcept {
        return info_.model.has_value() ? std::optional<std::string>(info_.model->provider_id) : std::nullopt;
    }

    /// Get the generation temperature (if configured)
    [[nodiscard]] std::optional<double> temperature() const noexcept { return info_.temperature; }

    /// Get the maximum execution steps (if configured)
    [[nodiscard]] std::optional<int> steps() const noexcept { return info_.steps; }

    /// Get the agent mode
    [[nodiscard]] AgentMode mode() const noexcept { return info_.mode; }

    /// Check if agent is hidden
    [[nodiscard]] bool is_hidden() const noexcept { return info_.hidden; }

    /// Convert agent to tool definition format for LLM
    [[nodiscard]] nlohmann::json to_tool_definition() const;

protected:
    AgentInfo info_;
    Agent() = default;
};

/// Smart pointer for Agent
using AgentPtr = std::shared_ptr<Agent>;

/// Agent registry - singleton that manages all registered agents
class TURBOT_CORE_API AgentRegistry {
public:
    /// Get the singleton instance
    static AgentRegistry& instance();

    /// Register an agent
    /// @param agent Agent to register
    /// @return true if registration succeeded, false if agent with same name exists
    bool register_agent(AgentPtr agent);

    /// Unregister an agent by name
    /// @param name Agent name to unregister
    /// @return true if agent was removed, false if not found
    bool unregister_agent(const std::string& name);

    /// Get an agent by name
    /// @param name Agent name
    /// @return Agent pointer or nullptr if not found
    [[nodiscard]] AgentPtr get(const std::string& name) const;

    /// Check if an agent exists
    /// @param name Agent name
    /// @return true if agent exists
    [[nodiscard]] bool has(const std::string& name) const;

    /// List all registered agents
    /// @return Vector of agent pointers
    [[nodiscard]] std::vector<AgentPtr> list() const;

    /// List agents by mode
    /// @param mode Agent mode filter
    /// @return Vector of agent pointers
    [[nodiscard]] std::vector<AgentPtr> list_by_mode(AgentMode mode) const;

    /// List visible agents (non-hidden)
    /// @return Vector of agent pointers
    [[nodiscard]] std::vector<AgentPtr> list_visible() const;

    /// List primary agents (mode == Primary and not hidden)
    /// @return Vector of agent pointers
    [[nodiscard]] std::vector<AgentPtr> list_primary() const;

    /// Get all agent names
    /// @return Vector of agent names
    [[nodiscard]] std::vector<std::string> names() const;

    /// Get the default agent name
    /// Returns the first visible primary agent, or "build" if available
    /// @return Default agent name, or empty string if none found
    [[nodiscard]] std::string default_agent() const;

    /// Clear all agents (mainly for testing)
    void clear();

    /// Get the number of registered agents
    [[nodiscard]] size_t size() const;

private:
    AgentRegistry() = default;
    ~AgentRegistry() = default;

    // Non-copyable, non-movable
    AgentRegistry(const AgentRegistry&) = delete;
    AgentRegistry& operator=(const AgentRegistry&) = delete;
    AgentRegistry(AgentRegistry&&) = delete;
    AgentRegistry& operator=(AgentRegistry&&) = delete;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, AgentPtr> agents_;

    // Internal unlocked version for use within locked methods
    [[nodiscard]] std::vector<AgentPtr> list_locked() const;
};

// ============================================================================
// Agent Loader - Initialize and load agents
// ============================================================================

/// Agent loader - handles initialization and loading of agents
namespace agent_loader {

/// Initialize built-in agents (build, plan, explore, etc.)
/// @return Number of agents initialized
TURBOT_CORE_API size_t initialize_builtin_agents();

/// Load agents from configuration
/// @param config_json Configuration JSON with agent definitions
/// @return Number of agents loaded
TURBOT_CORE_API size_t load_from_config(const nlohmann::json& config_json);

/// Reload all agents (clear and reinitialize)
/// @return Number of agents loaded
TURBOT_CORE_API size_t reload();

} // namespace agent_loader

// ============================================================================
// Agent Generator - Generate new agent configurations
// ============================================================================

/// Agent generation result
struct TURBOT_CORE_API AgentGenerateResult {
    std::string identifier;      ///< Agent identifier/name
    std::string when_to_use;     ///< Description of when to use this agent
    std::string system_prompt;   ///< System prompt for the agent
    
    /// Convert to AgentInfo
    [[nodiscard]] AgentInfo to_agent_info() const;
    
    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Agent generator - creates new agent configurations using LLM
namespace agent_generator {

/// Parameters for agent generation
struct GenerateParams {
    std::string description;     ///< Description of what the agent should do
    std::optional<ModelRef> model; ///< Optional model to use for generation
};

/// Generate a new agent configuration
/// @param params Generation parameters
/// @return Generated agent result
/// @throws std::runtime_error if generation fails
[[nodiscard]] TURBOT_CORE_API AgentGenerateResult generate(const GenerateParams& params);

} // namespace agent_generator

} // namespace turbot::core::agent