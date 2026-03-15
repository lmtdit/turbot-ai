#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/tool/builtin/apply_patch_tool.hpp>
#include <turbot/core/tool/builtin/bash_tool.hpp>
#include <turbot/core/tool/builtin/batch_tool.hpp>
#include <turbot/core/tool/builtin/codesearch_tool.hpp>
#include <turbot/core/tool/builtin/edit_tool.hpp>
#include <turbot/core/tool/builtin/glob_tool.hpp>
#include <turbot/core/tool/builtin/grep_tool.hpp>
#include <turbot/core/tool/builtin/invalid_tool.hpp>
#include <turbot/core/tool/builtin/list_tool.hpp>
#include <turbot/core/tool/builtin/lsp_tool.hpp>
#include <turbot/core/tool/builtin/multiedit_tool.hpp>
#include <turbot/core/tool/builtin/question_tool.hpp>
#include <turbot/core/tool/builtin/read_file_tool.hpp>
#include <turbot/core/tool/builtin/webfetch_tool.hpp>
#include <turbot/core/tool/builtin/websearch_tool.hpp>
#include <turbot/core/tool/builtin/write_file_tool.hpp>
#include <turbot/core/tool/builtin/task_tool.hpp>
#include <turbot/core/tool/builtin/todo_tool.hpp>
#include <turbot/core/tool/builtin/plan_tool.hpp>
#include <turbot/core/tool/skill_tool.hpp>
#include <turbot/core/tool/external_command_tool.hpp>
#include <turbot/core/common/logger.hpp>
#include <stdexcept>
#include <filesystem>

namespace turbot::core::tool {

ToolRegistry& ToolRegistry::instance() {
    static ToolRegistry registry;
    return registry;
}

void ToolRegistry::register_tool(std::unique_ptr<Tool> tool) {
    if (!tool) {
        throw std::invalid_argument("Cannot register null tool");
    }
    const std::string tool_name = tool->name();
    std::unique_lock lock(mutex_);
    tools_[tool_name] = std::move(tool);
}

ToolPtr ToolRegistry::get(const std::string& name) const {
    std::shared_lock lock(mutex_);
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        return nullptr;
    }
    return it->second;
}

bool ToolRegistry::has(const std::string& name) const {
    std::shared_lock lock(mutex_);
    return tools_.contains(name);
}

std::vector<ToolPtr> ToolRegistry::list_locked() const {
    std::vector<ToolPtr> result;
    result.reserve(tools_.size());
    for (const auto& [name, tool] : tools_) {
        result.push_back(tool);
    }
    return result;
}

std::vector<ToolPtr> ToolRegistry::list() const {
    std::shared_lock lock(mutex_);
    return list_locked();
}

std::vector<std::string> ToolRegistry::names() const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> result;
    result.reserve(tools_.size());
    for (const auto& [name, _] : tools_) {
        result.push_back(name);
    }
    return result;
}

std::vector<ToolPtr> ToolRegistry::filter(const std::vector<std::string>& allowed_names) const {
    std::shared_lock lock(mutex_);
    if (allowed_names.empty()) {
        return list_locked();  // Reuse internal impl, lock already held
    }
    std::vector<ToolPtr> result;
    result.reserve(allowed_names.size());
    for (const auto& name : allowed_names) {
        if (auto it = tools_.find(name); it != tools_.end()) {
            result.push_back(it->second);
        }
    }
    return result;
}

bool ToolRegistry::remove(const std::string& name) {
    std::unique_lock lock(mutex_);
    return tools_.erase(name) > 0;
}

void ToolRegistry::clear() {
    std::unique_lock lock(mutex_);
    tools_.clear();
}

size_t ToolRegistry::size() const {
    std::shared_lock lock(mutex_);
    return tools_.size();
}

nlohmann::json ToolRegistry::to_tool_definitions() const {
    std::shared_lock lock(mutex_);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& [name, tool] : tools_) {
        arr.push_back(tool->to_tool_definition());
    }
    return arr;
}

void ToolRegistry::register_builtin_tools() {
    // Core file operations
    register_tool(std::make_unique<builtin::ReadFileTool>());
    register_tool(std::make_unique<builtin::WriteFileTool>());
    register_tool(std::make_unique<builtin::EditTool>());
    register_tool(std::make_unique<builtin::MultiEditTool>());
    register_tool(std::make_unique<builtin::ApplyPatchTool>());
    register_tool(std::make_unique<builtin::BatchTool>());
    register_tool(std::make_unique<builtin::InvalidTool>());
    
    // Experimental tools (require environment variable to enable)
    if (builtin::LSPTool::is_enabled()) {
        register_tool(std::make_unique<builtin::LSPTool>());
    }
    
    // Shell execution
    register_tool(std::make_unique<builtin::BashTool>());
    
    // Search and discovery
    register_tool(std::make_unique<builtin::GlobTool>());
    register_tool(std::make_unique<builtin::GrepTool>());
    register_tool(std::make_unique<builtin::ListTool>());
    register_tool(std::make_unique<builtin::CodeSearchTool>());
    
    // Web access
    register_tool(std::make_unique<builtin::WebFetchTool>());
    register_tool(std::make_unique<builtin::WebSearchTool>());
    
    // Task management
    register_tool(std::make_unique<TaskTool>());
    register_tool(std::make_unique<TodoReadTool>());
    register_tool(std::make_unique<TodoWriteTool>());
    register_tool(std::make_unique<PlanEnterTool>());
    register_tool(std::make_unique<PlanExitTool>());
    register_tool(std::make_unique<SkillTool>());

    // Interactive question tool — only registered when explicitly enabled
    // (e.g. for CLI/app/desktop clients that support interactive UI).
    // Call ToolRegistry::enable_question_tool() before register_builtin_tools().
    if (question_tool_enabled_) {
        register_tool(std::make_unique<builtin::QuestionTool>());
    }
}

void ToolRegistry::enable_question_tool(bool enable) {
    question_tool_enabled_ = enable;
}

size_t ToolRegistry::discover_custom_tools(const std::string& project_dir) {
    std::string tools_dir = project_dir + "/.turbot/tools";
    
    if (!std::filesystem::exists(tools_dir)) {
        TURBOT_LOG_INFO("No custom tools directory found at: {}", tools_dir);
        return 0;
    }
    
    size_t count = 0;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(tools_dir)) {
            if (!entry.is_regular_file()) continue;
            
            std::string path = entry.path().string();
            std::string ext = entry.path().extension().string();
            
            // Only process .json files
            if (ext != ".json") continue;
            
            auto config = CustomToolConfig::from_file(path);
            if (!config) {
                TURBOT_LOG_WARN("Failed to load custom tool config: {}", path);
                continue;
            }
            
            if (!config->enabled) {
                TURBOT_LOG_INFO("Custom tool '{}' is disabled, skipping", config->name);
                continue;
            }
            
            // Check for name conflict with builtin tools
            if (has(config->name)) {
                TURBOT_LOG_WARN("Custom tool '{}' conflicts with existing tool, replacing", config->name);
            }
            
            // Create and register the external command tool
            auto tool = std::make_unique<ExternalCommandTool>(*config);
            register_tool(std::move(tool));
            
            // Track custom tool name
            {
                std::unique_lock lock(mutex_);
                custom_tool_names_.push_back(config->name);
            }
            
            TURBOT_LOG_INFO("Registered custom tool '{}' from {}", config->name, path);
            count++;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        TURBOT_LOG_ERROR("Error scanning custom tools directory: {}", e.what());
    }
    
    return count;
}

size_t ToolRegistry::discover_custom_tools(const std::vector<std::string>& directories) {
    size_t total = 0;
    for (const auto& dir : directories) {
        total += discover_custom_tools(dir);
    }
    return total;
}

std::vector<std::string> ToolRegistry::custom_tool_names() const {
    std::shared_lock lock(mutex_);
    return custom_tool_names_;
}

} // namespace turbot::core::tool
