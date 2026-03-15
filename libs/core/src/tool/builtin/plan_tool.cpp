#include <turbot/core/tool/builtin/plan_tool.hpp>
#include <turbot/core/common/logger.hpp>
#include <fmt/format.h>
#include <chrono>
#include <fstream>
#include <filesystem>

namespace turbot::core::tool {

// ============================================================================
// PlanMode helpers
// ============================================================================

std::string plan_mode_to_string(PlanMode mode) {
    switch (mode) {
        case PlanMode::Planning: return "planning";
        case PlanMode::Normal:
        default:                 return "normal";
    }
}

PlanMode string_to_plan_mode(const std::string& str) {
    if (str == "planning") return PlanMode::Planning;
    return PlanMode::Normal;
}

// ============================================================================
// PlanManager
// ============================================================================

PlanManager& PlanManager::instance() {
    static PlanManager instance;
    return instance;
}

PlanMode PlanManager::get_mode(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = modes_.find(session_id);
    if (it == modes_.end()) {
        return PlanMode::Normal;
    }
    return it->second;
}

void PlanManager::set_mode(const std::string& session_id, PlanMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    modes_[session_id] = mode;
    TURBOT_LOG_DEBUG("PlanManager: set mode to {} for session {}", 
                     plan_mode_to_string(mode), session_id);
}

std::string PlanManager::get_plan(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plans_.find(session_id);
    if (it == plans_.end()) {
        return "";
    }
    return it->second;
}

void PlanManager::set_plan(const std::string& session_id, const std::string& plan) {
    std::lock_guard<std::mutex> lock(mutex_);
    plans_[session_id] = plan;
}

std::string PlanManager::get_plan_path(const std::string& session_id,
                                        const std::string& working_directory) const {
    // Generate plan file path: .opencode/plans/<timestamp>-<session-slug>.md
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
    
    // Create a slug from session_id (take first 8 characters)
    std::string slug = session_id.substr(0, std::min(size_t(8), session_id.length()));
    
    return fmt::format("{}/.opencode/plans/{}-{}.md", working_directory, timestamp, slug);
}

void PlanManager::clear_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    modes_.erase(session_id);
    plans_.erase(session_id);
}

void PlanManager::clear_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    modes_.clear();
    plans_.clear();
}

// ============================================================================
// PlanEnterTool
// ============================================================================

std::string PlanEnterTool::description() const {
    return "Enter plan mode. In plan mode, you can research and create a plan "
           "but cannot make changes to files. Use this to analyze requirements "
           "and create a structured plan before implementation.";
}

nlohmann::json PlanEnterTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", nlohmann::json::object()},
        {"required", nlohmann::json::array()}
    };
}

ToolResult PlanEnterTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    // Check abort flag
    if (ctx.should_abort()) {
        return ToolResult::error("plan_enter", "Operation aborted", {{"aborted", true}});
    }

    // Set plan mode
    PlanManager::instance().set_mode(ctx.session_id, PlanMode::Planning);
    
    // Get plan path
    std::string plan_path = PlanManager::instance().get_plan_path(
        ctx.session_id, ctx.working_directory);
    
    TURBOT_LOG_INFO("PlanEnterTool: entered plan mode for session {}", ctx.session_id);
    
    return ToolResult::success(
        "Entered plan mode",
        fmt::format("You are now in plan mode. You can research and analyze, "
                    "but cannot make changes to files. The plan will be saved to: {}\n\n"
                    "When ready, use plan_exit to save your plan and return to normal mode.",
                    plan_path),
        {
            {"mode", "planning"},
            {"plan_path", plan_path}
        }
    );
}

// ============================================================================
// PlanExitTool
// ============================================================================

std::string PlanExitTool::description() const {
    return "Exit plan mode and save the plan. Provide the plan content as markdown. "
           "The plan will be saved to .opencode/plans/ directory.";
}

nlohmann::json PlanExitTool::input_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"plan", {
                {"type", "string"},
                {"description", "The plan content in markdown format"}
            }}
        }},
        {"required", {"plan"}}
    };
}

bool PlanExitTool::validate_input(const nlohmann::json& input) const {
    if (!input.is_object()) return false;
    if (!input.contains("plan")) return false;
    if (!input["plan"].is_string()) return false;
    return true;
}

ToolResult PlanExitTool::execute(const nlohmann::json& input, ToolContext& ctx) {
    // Check abort flag
    if (ctx.should_abort()) {
        return ToolResult::error("plan_exit", "Operation aborted", {{"aborted", true}});
    }

    std::string plan = input["plan"];
    
    // Get plan path
    std::string plan_path = PlanManager::instance().get_plan_path(
        ctx.session_id, ctx.working_directory);
    
    // Create directory if needed
    std::filesystem::path dir_path = std::filesystem::path(plan_path).parent_path();
    if (!std::filesystem::exists(dir_path)) {
        std::error_code ec;
        if (!std::filesystem::create_directories(dir_path, ec)) {
            TURBOT_LOG_ERROR("PlanExitTool: failed to create directory: {}", ec.message());
            return ToolResult::error("plan_exit", 
                fmt::format("Failed to create plan directory: {}", ec.message()));
        }
    }
    
    // Write plan to file
    std::ofstream file(plan_path);
    if (!file.is_open()) {
        TURBOT_LOG_ERROR("PlanExitTool: failed to open file: {}", plan_path);
        return ToolResult::error("plan_exit", 
            fmt::format("Failed to create plan file: {}", plan_path));
    }
    
    file << plan;
    file.close();
    
    // Store plan in memory
    PlanManager::instance().set_plan(ctx.session_id, plan);
    
    // Exit plan mode
    PlanManager::instance().set_mode(ctx.session_id, PlanMode::Normal);
    
    TURBOT_LOG_INFO("PlanExitTool: saved plan to {} for session {}", plan_path, ctx.session_id);
    
    return ToolResult::success(
        "Plan saved",
        fmt::format("Plan saved to: {}\n\nYou are now in normal mode and can "
                    "start implementing the plan.", plan_path),
        {
            {"mode", "normal"},
            {"plan_path", plan_path},
            {"plan_length", plan.length()}
        }
    );
}

} // namespace turbot::core::tool
