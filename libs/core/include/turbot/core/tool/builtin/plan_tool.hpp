#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/tool/tool.hpp>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace turbot::core::tool {

/// Plan mode enum
enum class TURBOT_CORE_API PlanMode {
    Normal,   ///< Normal execution mode
    Planning   ///< Planning mode (read-only)
};

/// Convert PlanMode to string
[[nodiscard]] TURBOT_CORE_API std::string plan_mode_to_string(PlanMode mode);

/// Convert string to PlanMode
[[nodiscard]] TURBOT_CORE_API PlanMode string_to_plan_mode(const std::string& str);

/// Plan manager - manages plan mode and plan content per session
class TURBOT_CORE_API PlanManager {
public:
    /// Get the singleton instance
    static PlanManager& instance();

    /// Get plan mode for a session
    /// @param session_id Session ID
    /// @return Current plan mode
    [[nodiscard]] PlanMode get_mode(const std::string& session_id) const;

    /// Set plan mode for a session
    /// @param session_id Session ID
    /// @param mode New plan mode
    void set_mode(const std::string& session_id, PlanMode mode);

    /// Get plan content for a session
    /// @param session_id Session ID
    /// @return Plan content (markdown)
    [[nodiscard]] std::string get_plan(const std::string& session_id) const;

    /// Set plan content for a session
    /// @param session_id Session ID
    /// @param plan Plan content (markdown)
    void set_plan(const std::string& session_id, const std::string& plan);

    /// Get plan file path for a session
    /// @param session_id Session ID
    /// @param working_directory Working directory
    /// @return Plan file path
    [[nodiscard]] std::string get_plan_path(const std::string& session_id, 
                                             const std::string& working_directory) const;

    /// Clear plan data for a session
    /// @param session_id Session ID
    void clear_session(const std::string& session_id);

    /// Clear all data (mainly for testing)
    void clear_all();

private:
    PlanManager() = default;
    ~PlanManager() = default;

    // Non-copyable, non-movable
    PlanManager(const PlanManager&) = delete;
    PlanManager& operator=(const PlanManager&) = delete;
    PlanManager(PlanManager&&) = delete;
    PlanManager& operator=(PlanManager&&) = delete;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, PlanMode> modes_;
    std::unordered_map<std::string, std::string> plans_;
};

/// PlanEnter tool - enters plan mode
class TURBOT_CORE_API PlanEnterTool : public Tool {
public:
    PlanEnterTool() = default;

    [[nodiscard]] std::string name() const override { return "plan_enter"; }
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] nlohmann::json input_schema() const override;
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;
};

/// PlanExit tool - exits plan mode and saves the plan
class TURBOT_CORE_API PlanExitTool : public Tool {
public:
    PlanExitTool() = default;

    [[nodiscard]] std::string name() const override { return "plan_exit"; }
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] nlohmann::json input_schema() const override;
    [[nodiscard]] ToolResult execute(const nlohmann::json& input, ToolContext& ctx) override;
    [[nodiscard]] bool validate_input(const nlohmann::json& input) const override;
};

} // namespace turbot::core::tool
