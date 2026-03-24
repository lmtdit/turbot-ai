// init.cpp - CLI command to initialize a project
// Aligns with OpenCode `opencode init` command capability

#include <turbot/core/common/logger.hpp>
#include <turbot/core/project/project.hpp>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace turbot::cli {

namespace fs = std::filesystem;

/// Initialize a new project in the current directory
int init_project(const std::string& name, bool init_git) {
    std::string directory = fs::current_path().string();
    
    // Check if already initialized
    fs::path config_path = fs::path(directory) / ".turbot" / "turbot.json";
    if (fs::exists(config_path)) {
        fmt::print("Project already initialized in this directory.\n");
        fmt::print("Config file: {}\n", config_path.string());
        
        // Load and display current project info
        std::ifstream f(config_path);
        if (f.is_open()) {
            try {
                nlohmann::json config = nlohmann::json::parse(f);
                if (config.contains("project")) {
                    auto& proj = config["project"];
                    fmt::print("\nProject Info:\n");
                    fmt::print("  ID:       {}\n", proj.value("id", "unknown"));
                    fmt::print("  Name:     {}\n", proj.value("name", "unnamed"));
                    fmt::print("  VCS:      {}\n", proj.value("vcs", "none"));
                    fmt::print("  Worktree: {}\n", proj.value("worktree", directory));
                }
            } catch (const std::exception& e) {
                fmt::print(stderr, "Failed to parse config: {}\n", e.what());
            }
        }
        return 0;
    }
    
    // Create project
    core::project::CreateParams params;
    params.directory = directory;
    params.name = name.empty() ? fs::path(directory).filename().string() : name;
    params.init_git = init_git;
    
    auto project = core::project::Project::create(params);
    
    if (!project) {
        fmt::print(stderr, "Failed to initialize project.\n");
        return 1;
    }
    
    const auto& info = project->info();
    
    fmt::print("Project initialized successfully!\n\n");
    fmt::print("Project Info:\n");
    fmt::print("  ID:       {}\n", info.id);
    fmt::print("  Name:     {}\n", info.name.value_or("unnamed"));
    fmt::print("  Directory: {}\n", info.worktree);
    fmt::print("  VCS:      {}\n", core::project::vcs_type_to_string(info.vcs));
    fmt::print("  Config:   {}\n", config_path.string());
    
    // Show next steps
    fmt::print("\nNext steps:\n");
    fmt::print("  1. Configure a provider:\n");
    fmt::print("     turbot-cli providers add myprovider --type bailian --api-key <key>\n");
    fmt::print("  2. Start a session:\n");
    fmt::print("     turbot-cli run\n");
    
    // Mark as initialized
    project->set_initialized();
    
    return 0;
}

/// Show project status
int show_project_status() {
    std::string directory = fs::current_path().string();
    
    // Try to load project from current directory
    auto result = core::project::Project::from_directory(directory);
    
    const auto& info = result.project;
    
    if (info.id == core::project::Project::GLOBAL_ID || info.worktree.empty()) {
        fmt::print("No project found in current directory.\n");
        fmt::print("Run 'turbot-cli init' to initialize a project.\n");
        return 0;
    }
    
    fmt::print("Project: {}\n", info.name.value_or(info.id));
    fmt::print("{}\n", std::string(60, '-'));
    fmt::print("  ID:         {}\n", info.id);
    fmt::print("  Directory:  {}\n", info.worktree);
    fmt::print("  VCS:        {}\n", core::project::vcs_type_to_string(info.vcs));
    
    if (info.time.created > 0) {
        std::time_t t = static_cast<std::time_t>(info.time.created);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&t));
        fmt::print("  Created:    {}\n", buf);
    }
    
    if (info.time.initialized) {
        std::time_t t = static_cast<std::time_t>(*info.time.initialized);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&t));
        fmt::print("  Initialized: {}\n", buf);
    }
    
    if (info.icon) {
        fmt::print("  Icon:       {} (color: {})\n", 
                   info.icon->url ? "configured" : "none",
                   info.icon->color.value_or("default"));
    }
    
    if (info.commands && info.commands->start) {
        fmt::print("  Start cmd:  {}\n", *info.commands->start);
    }
    
    if (!info.sandboxes.empty()) {
        fmt::print("\nSandboxes:\n");
        for (const auto& sandbox : info.sandboxes) {
            fmt::print("  - {}\n", sandbox);
        }
    }
    
    return 0;
}

/// Set project name
int set_project_name(const std::string& name) {
    std::string directory = fs::current_path().string();
    
    auto result = core::project::Project::from_directory(directory);
    core::project::Project project(result.project);
    
    core::project::UpdateParams params;
    params.name = name;
    
    if (project.update(params)) {
        fmt::print("Project name updated to: {}\n", name);
        return 0;
    } else {
        fmt::print(stderr, "Failed to update project name.\n");
        return 1;
    }
}

/// Initialize git repository for project
int init_project_git() {
    std::string directory = fs::current_path().string();
    
    auto result = core::project::Project::from_directory(directory);
    core::project::Project project(result.project);
    
    if (project.init_git()) {
        fmt::print("Git repository initialized for project: {}\n", project.id());
        return 0;
    } else {
        fmt::print("Git already initialized or initialization failed.\n");
        return 1;
    }
}

} // namespace turbot::cli
