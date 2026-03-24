// project.cpp - Project management implementation
// Aligns with OpenCode Project module capability

#include <turbot/core/project/project.hpp>
#include <turbot/core/project/project_store.hpp>
#include <turbot/core/common/logger.hpp>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdio>

namespace turbot::core::project {

namespace fs = std::filesystem;

// ============================================================================
// VcsType utilities
// ============================================================================

std::string vcs_type_to_string(VcsType vcs) {
    switch (vcs) {
        case VcsType::Git: return "git";
        case VcsType::None:
        default: return "none";
    }
}

VcsType string_to_vcs_type(const std::string& str) {
    if (str == "git") return VcsType::Git;
    return VcsType::None;
}

// ============================================================================
// ProjectIcon
// ============================================================================

nlohmann::json ProjectIcon::to_json() const {
    nlohmann::json j = nlohmann::json::object();
    if (url) j["url"] = *url;
    if (color) j["color"] = *color;
    if (override_) j["override"] = *override_;
    return j;
}

ProjectIcon ProjectIcon::from_json(const nlohmann::json& j) {
    ProjectIcon icon;
    if (j.contains("url")) icon.url = j["url"].get<std::string>();
    if (j.contains("color")) icon.color = j["color"].get<std::string>();
    if (j.contains("override")) icon.override_ = j["override"].get<std::string>();
    return icon;
}

bool ProjectIcon::operator==(const ProjectIcon& other) const noexcept {
    return url == other.url && color == other.color && override_ == other.override_;
}

// ============================================================================
// ProjectCommands
// ============================================================================

nlohmann::json ProjectCommands::to_json() const {
    nlohmann::json j = nlohmann::json::object();
    if (start) j["start"] = *start;
    return j;
}

ProjectCommands ProjectCommands::from_json(const nlohmann::json& j) {
    ProjectCommands cmd;
    if (j.contains("start")) cmd.start = j["start"].get<std::string>();
    return cmd;
}

bool ProjectCommands::operator==(const ProjectCommands& other) const noexcept {
    return start == other.start;
}

// ============================================================================
// ProjectTime
// ============================================================================

nlohmann::json ProjectTime::to_json() const {
    nlohmann::json j = nlohmann::json::object();
    j["created"] = created;
    j["updated"] = updated;
    if (initialized) j["initialized"] = *initialized;
    return j;
}

ProjectTime ProjectTime::from_json(const nlohmann::json& j) {
    ProjectTime time;
    time.created = j.value("created", 0);
    time.updated = j.value("updated", 0);
    if (j.contains("initialized")) time.initialized = j["initialized"].get<int64_t>();
    return time;
}

bool ProjectTime::operator==(const ProjectTime& other) const noexcept {
    return created == other.created && 
           updated == other.updated && 
           initialized == other.initialized;
}

// ============================================================================
// ProjectInfo
// ============================================================================

nlohmann::json ProjectInfo::to_json() const {
    nlohmann::json j = nlohmann::json::object();
    j["id"] = id;
    j["worktree"] = worktree;
    j["vcs"] = vcs_type_to_string(vcs);
    if (name) j["name"] = *name;
    if (icon) j["icon"] = icon->to_json();
    if (commands) j["commands"] = commands->to_json();
    j["time"] = time.to_json();
    j["sandboxes"] = sandboxes;
    return j;
}

ProjectInfo ProjectInfo::from_json(const nlohmann::json& j) {
    ProjectInfo info;
    info.id = j.value("id", std::string{});
    info.worktree = j.value("worktree", std::string{});
    info.vcs = string_to_vcs_type(j.value("vcs", "none"));
    if (j.contains("name") && !j["name"].is_null())
        info.name = j["name"].get<std::string>();

    // icon: nested object (wire) or flat columns (DB)
    if (j.contains("icon") && !j["icon"].is_null()) {
        info.icon = ProjectIcon::from_json(j["icon"]);
    } else {
        // Flat DB columns: icon_url / icon_color
        bool has_icon = false;
        ProjectIcon icon;
        if (j.contains("icon_url") && !j["icon_url"].is_null()) {
            icon.url = j["icon_url"].get<std::string>();
            has_icon = true;
        }
        if (j.contains("icon_color") && !j["icon_color"].is_null()) {
            icon.color = j["icon_color"].get<std::string>();
            has_icon = true;
        }
        if (has_icon) info.icon = std::move(icon);
    }

    // commands: nested object (wire) or flat JSON string (DB)
    if (j.contains("commands") && !j["commands"].is_null()) {
        const auto& cmd = j["commands"];
        if (cmd.is_object()) {
            info.commands = ProjectCommands::from_json(cmd);
        } else if (cmd.is_string()) {
            try {
                info.commands = ProjectCommands::from_json(
                    nlohmann::json::parse(cmd.get<std::string>()));
            } catch (const std::exception&) { /* ignore malformed */ }
        }
    }

    // time: nested object (wire) or flat columns (DB)
    if (j.contains("time") && j["time"].is_object()) {
        info.time = ProjectTime::from_json(j["time"]);
    } else {
        info.time.created = j.value("time_created", int64_t{0});
        info.time.updated = j.value("time_updated", int64_t{0});
        if (j.contains("time_initialized") && !j["time_initialized"].is_null())
            info.time.initialized = j["time_initialized"].get<int64_t>();
    }

    // sandboxes: JSON array (wire) or JSON string (DB)
    if (j.contains("sandboxes") && !j["sandboxes"].is_null()) {
        const auto& sb = j["sandboxes"];
        if (sb.is_array()) {
            info.sandboxes = sb.get<std::vector<std::string>>();
        } else if (sb.is_string()) {
            try {
                auto parsed = nlohmann::json::parse(sb.get<std::string>());
                if (parsed.is_array())
                    info.sandboxes = parsed.get<std::vector<std::string>>();
            } catch (const std::exception&) { /* ignore malformed */ }
        }
    }

    return info;
}

bool ProjectInfo::operator==(const ProjectInfo& other) const noexcept {
    return id == other.id && worktree == other.worktree && vcs == other.vcs &&
           name == other.name && icon == other.icon && commands == other.commands &&
           time == other.time && sandboxes == other.sandboxes;
}

// ============================================================================
// Project
// ============================================================================

int64_t Project::current_timestamp() {
    // Return milliseconds since epoch — aligns with OpenCode's Date.now() convention.
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

/// Run a git command in `cwd` and return trimmed stdout, or empty on error.
static std::string run_git(const std::string& args, const std::string& cwd) {
    // Build command: git <args> run from cwd, stderr discarded.
    // popen is used for portability (no external library dependency).
    std::string cmd = "git -C " + cwd + " " + args + " 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};
    std::string output;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) {
        output += buf;
    }
    pclose(pipe);
    // Trim trailing whitespace/newlines.
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r' || output.back() == ' '))
        output.pop_back();
    return output;
}

/// Read cached project ID from `<git_dir>/opencode` file.
static std::string read_cached_id(const fs::path& git_dir) {
    fs::path cache_file = git_dir / "opencode";
    if (!fs::exists(cache_file)) return {};
    std::ifstream f(cache_file);
    if (!f.is_open()) return {};
    std::string id;
    std::getline(f, id);
    // Trim whitespace.
    while (!id.empty() && (id.back() == '\n' || id.back() == '\r' || id.back() == ' '))
        id.pop_back();
    return id;
}

/// Write project ID to `<git_dir>/opencode` cache file (best-effort).
static void write_cached_id(const fs::path& git_dir, const std::string& id) {
    fs::path cache_file = git_dir / "opencode";
    std::ofstream f(cache_file);
    if (f.is_open()) f << id << "\n";
}

std::string Project::generate_id(const std::string& directory) {
    // Aligns with OpenCode's Project.fromDirectory() ID algorithm:
    //
    //   1. Locate .git directory (file or folder) by walking up from `directory`.
    //   2. Resolve the common git dir (handles git worktrees where .git is a file).
    //   3. Read cached ID from <common_git_dir>/opencode if present.
    //   4. Otherwise, run `git rev-list --max-parents=0 HEAD` to get root commit(s).
    //      Sort them and take the first one — stable across clones of the same repo.
    //   5. Write the ID to <common_git_dir>/opencode for future lookups.
    //   6. Fall back to GLOBAL_ID if no git root or no commits yet.

    // Step 1: Find .git entry by walking up.
    fs::path sandbox = fs::path(directory);
    fs::path dot_git;
    for (fs::path p = sandbox; !p.empty(); p = p.parent_path()) {
        fs::path candidate = p / ".git";
        std::error_code ec;
        if (fs::exists(candidate, ec)) {
            dot_git = candidate;
            sandbox = p;
            break;
        }
        if (p == p.parent_path()) break;  // reached filesystem root
    }

    if (dot_git.empty()) {
        // No git repository found.
        return GLOBAL_ID;
    }

    // Step 2: Resolve common git dir (handles git worktrees).
    // For a standard repo, .git is a directory — common dir is .git itself.
    // For a worktree, .git is a file pointing to .git/worktrees/<name>;
    // `git rev-parse --git-common-dir` returns the common dir.
    fs::path git_common_dir;
    if (fs::is_directory(dot_git)) {
        git_common_dir = dot_git;
    } else {
        // Worktree: .git is a file; ask git for the common dir.
        std::string common = run_git("rev-parse --git-common-dir", sandbox.string());
        if (common.empty()) {
            git_common_dir = dot_git;
        } else {
            git_common_dir = fs::path(common).is_absolute() ? fs::path(common)
                                                             : (sandbox / common).lexically_normal();
        }
    }

    // Step 3: Try to read cached ID.
    std::string id = read_cached_id(git_common_dir);
    if (!id.empty()) {
        return id;
    }

    // Step 4: Generate from root commits.
    std::string roots_raw = run_git("rev-list --max-parents=0 HEAD", sandbox.string());
    if (roots_raw.empty()) {
        // No commits yet (empty repo) — fall back to global.
        return GLOBAL_ID;
    }

    // Split by newlines, strip empties, sort.
    std::vector<std::string> roots;
    std::istringstream ss(roots_raw);
    std::string line;
    while (std::getline(ss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (!line.empty()) roots.push_back(line);
    }
    if (roots.empty()) return GLOBAL_ID;
    std::sort(roots.begin(), roots.end());
    id = roots[0];

    // Step 5: Cache the ID for next time.
    write_cached_id(git_common_dir, id);

    return id;
}

VcsType Project::detect_vcs(const std::string& directory) {
    fs::path git_dir = fs::path(directory) / ".git";
    if (fs::exists(git_dir)) {
        return VcsType::Git;
    }
    return VcsType::None;
}

std::optional<nlohmann::json> Project::load_config(const std::string& directory) {
    fs::path config_path = fs::path(directory) / ".turbot" / "turbot.json";
    
    if (!fs::exists(config_path)) {
        return std::nullopt;
    }
    
    std::ifstream f(config_path);
    if (!f.is_open()) {
        return std::nullopt;
    }
    
    try {
        return nlohmann::json::parse(f);
    } catch (const std::exception& e) {
        TURBOT_LOG_WARN("Failed to parse project config: {}", e.what());
        return std::nullopt;
    }
}

bool Project::save_config(const std::string& directory, const nlohmann::json& config) {
    fs::path config_dir = fs::path(directory) / ".turbot";
    fs::path config_path = config_dir / "turbot.json";
    
    // Create directory if needed
    std::error_code ec;
    if (!fs::exists(config_dir)) {
        if (!fs::create_directories(config_dir, ec)) {
            TURBOT_LOG_ERROR("Failed to create config directory: {}", ec.message());
            return false;
        }
    }
    
    std::ofstream f(config_path);
    if (!f.is_open()) {
        TURBOT_LOG_ERROR("Failed to open config file for writing");
        return false;
    }
    
    f << config.dump(2) << "\n";
    return true;
}

std::optional<ProjectIcon> Project::discover_icon(const std::string& directory) {
    // Look for common favicon files
    const std::vector<std::string> icon_names = {
        "favicon.ico", "favicon.png", "favicon.svg",
        "icon.png", "icon.svg", "logo.png", "logo.svg"
    };
    
    for (const auto& name : icon_names) {
        fs::path icon_path = fs::path(directory) / name;
        if (fs::exists(icon_path)) {
            // Read file and convert to data URI
            std::ifstream f(icon_path, std::ios::binary);
            if (f.is_open()) {
                std::stringstream buffer;
                buffer << f.rdbuf();
                std::string content = buffer.str();
                
                // Determine MIME type
                std::string mime = "image/png";
                if (name.find(".svg") != std::string::npos) {
                    mime = "image/svg+xml";
                } else if (name.find(".ico") != std::string::npos) {
                    mime = "image/x-icon";
                }
                
                // Convert to base64
                static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                std::string base64;
                int i = 0;
                int j = 0;
                unsigned char arr3[3];
                unsigned char arr4[4];
                size_t len = content.size();
                const unsigned char* data = reinterpret_cast<const unsigned char*>(content.c_str());
                
                while (len--) {
                    arr3[i++] = *(data++);
                    if (i == 3) {
                        arr4[0] = (arr3[0] & 0xfc) >> 2;
                        arr4[1] = ((arr3[0] & 0x03) << 4) + ((arr3[1] & 0xf0) >> 4);
                        arr4[2] = ((arr3[1] & 0x0f) << 2) + ((arr3[2] & 0xc0) >> 6);
                        arr4[3] = arr3[2] & 0x3f;
                        for (i = 0; i < 4; i++) base64 += chars[arr4[i]];
                        i = 0;
                    }
                }
                if (i) {
                    for (j = i; j < 3; j++) arr3[j] = '\0';
                    arr4[0] = (arr3[0] & 0xfc) >> 2;
                    arr4[1] = ((arr3[0] & 0x03) << 4) + ((arr3[1] & 0xf0) >> 4);
                    arr4[2] = ((arr3[1] & 0x0f) << 2) + ((arr3[2] & 0xc0) >> 6);
                    for (j = 0; j < i + 1; j++) base64 += chars[arr4[j]];
                    while (i++ < 3) base64 += '=';
                }
                
                ProjectIcon icon;
                icon.url = fmt::format("data:{};base64,{}", mime, base64);
                return icon;
            }
        }
    }
    
    return std::nullopt;
}

LoadResult Project::from_directory(const std::string& directory) {
    LoadResult result;
    
    // Resolve to absolute path
    fs::path abs_dir = fs::absolute(directory);
    std::string dir_str = abs_dir.string();
    
    // Detect VCS
    VcsType vcs = detect_vcs(dir_str);
    
    // Generate project ID
    ProjectId id = generate_id(dir_str);
    
    int64_t now = current_timestamp();

    auto& store = ProjectStore::instance();

    // Try to load from SQLite first; fall back to JSON config for migration.
    if (store.is_initialized()) {
        auto row = store.find_by_id(id);
        if (row) {
            try {
                result.project = ProjectInfo::from_json(*row);
                result.project.vcs     = vcs;
                result.project.time.updated = now;
                store.save(result.project);
                result.sandbox = result.project.worktree;
                return result;
            } catch (const std::exception& e) {
                TURBOT_LOG_WARN("Project::from_directory: corrupt DB row for {}: {}", id, e.what());
            }
        }
    }

    // Not in DB — load from JSON config if available (legacy), or create new.
    auto config_opt = load_config(dir_str);
    
    if (config_opt && config_opt->contains("project")) {
        // Migrate from JSON config
        result.project = ProjectInfo::from_json((*config_opt)["project"]);
        result.project.vcs = vcs;
        result.project.time.updated = now;
    } else {
        // Create new project info
        result.project.id       = id;
        result.project.worktree = dir_str;
        result.project.vcs      = vcs;
        result.project.time.created = now;
        result.project.time.updated = now;
        
        // Try to discover icon
        auto icon = discover_icon(dir_str);
        if (icon) result.project.icon = icon;
    }
    
    // Set sandbox to worktree
    result.sandbox = result.project.worktree;
    
    // Persist to SQLite (UPSERT)
    if (store.is_initialized()) {
        store.save(result.project);

        // G05: Migrate sessions that were assigned to 'global' project
        // but belong to this worktree — aligned with OpenCode's fromDirectory()
        // migration logic.
        int migrated = store.migrate_sessions(result.project.id, dir_str);
        if (migrated > 0) {
            TURBOT_LOG_INFO("Project::from_directory: migrated {} sessions from "
                            "'global' to project {} (worktree={})",
                            migrated, result.project.id, dir_str);
        }
    }

    // Also keep JSON config in sync (backward compatibility).
    nlohmann::json config;
    if (config_opt) config = *config_opt;
    config["project"] = result.project.to_json();
    save_config(dir_str, config);
    
    return result;
}

std::optional<Project> Project::create(const CreateParams& params) {
    // Resolve directory
    fs::path abs_dir = fs::absolute(params.directory);
    std::string dir_str = abs_dir.string();
    
    // Create directory if needed
    std::error_code ec;
    if (!fs::exists(abs_dir)) {
        if (!fs::create_directories(abs_dir, ec)) {
            TURBOT_LOG_ERROR("Failed to create project directory: {}", ec.message());
            return std::nullopt;
        }
    }
    
    // Initialize git if requested
    if (params.init_git && !fs::exists(abs_dir / ".git")) {
        // Note: In production, this would call git init
        TURBOT_LOG_INFO("Git initialization requested for: {}", dir_str);
    }
    
    // Create project from directory
    LoadResult result = from_directory(dir_str);
    
    // Apply overrides
    if (params.name) {
        result.project.name = params.name;
    }
    if (params.icon) {
        result.project.icon = params.icon;
    }
    if (params.commands) {
        result.project.commands = params.commands;
    }
    
    // Save config
    nlohmann::json config;
    auto config_opt = load_config(dir_str);
    if (config_opt) {
        config = *config_opt;
    }
    config["project"] = result.project.to_json();
    save_config(dir_str, config);
    
    return Project(result.project);
}

std::optional<Project> Project::get(const ProjectId& id) {
    auto& store = ProjectStore::instance();
    if (store.is_initialized()) {
        auto row = store.find_by_id(id);
        if (row) {
            try {
                return Project(ProjectInfo::from_json(*row));
            } catch (const std::exception& e) {
                TURBOT_LOG_WARN("Project::get: corrupt DB row for {}: {}", id, e.what());
            }
        }
        return std::nullopt;
    }
    // Fallback: linear search (store not initialised)
    auto projects = list();
    for (const auto& project : projects) {
        if (project.id() == id) return project;
    }
    return std::nullopt;
}

std::vector<Project> Project::list() {
    auto& store = ProjectStore::instance();
    if (!store.is_initialized()) return {};

    auto rows = store.find_all();
    std::vector<Project> projects;
    projects.reserve(rows.size());
    for (const auto& row : rows) {
        try {
            projects.emplace_back(ProjectInfo::from_json(row));
        } catch (const std::exception&) {
            // Skip corrupt rows
        }
    }
    return projects;
}

bool Project::remove(const ProjectId& id) {
    auto& store = ProjectStore::instance();
    if (!store.is_initialized()) {
        TURBOT_LOG_INFO("Project removal requested: {}", id);
        return true;
    }
    return store.remove(id);
}

bool Project::update(const UpdateParams& params) {
    bool changed = false;
    
    if (params.name) {
        info_.name = params.name;
        changed = true;
    }
    if (params.icon) {
        info_.icon = params.icon;
        changed = true;
    }
    if (params.commands) {
        info_.commands = params.commands;
        changed = true;
    }
    
    if (changed) {
        info_.time.updated = current_timestamp();
        
        // Persist to SQLite
        auto& store = ProjectStore::instance();
        if (store.is_initialized()) {
            store.save(info_);
        }
        // Also keep JSON config in sync (backward compatibility).
        auto config_opt = load_config(info_.worktree);
        nlohmann::json config;
        if (config_opt) config = *config_opt;
        config["project"] = info_.to_json();
        save_config(info_.worktree, config);
    }
    
    return changed;
}

bool Project::set_initialized() {
    if (info_.time.initialized) {
        return false; // Already initialized
    }
    
    info_.time.initialized = current_timestamp();
    info_.time.updated = *info_.time.initialized;
    
    // Persist to SQLite
    auto& store = ProjectStore::instance();
    if (store.is_initialized()) {
        store.save(info_);
    }
    // Also keep JSON config in sync (backward compatibility).
    auto config_opt = load_config(info_.worktree);
    nlohmann::json config;
    if (config_opt) config = *config_opt;
    config["project"] = info_.to_json();
    save_config(info_.worktree, config);
    
    return true;
}

bool Project::init_git() {
    if (info_.vcs == VcsType::Git) {
        return false; // Already has git
    }
    
    // In production, this would call git init
    TURBOT_LOG_INFO("Initializing git for project: {}", info_.id);
    
    info_.vcs = VcsType::Git;
    info_.time.updated = current_timestamp();
    
    // Regenerate ID based on git remote
    info_.id = generate_id(info_.worktree);
    
    // Persist to SQLite
    auto& store = ProjectStore::instance();
    if (store.is_initialized()) {
        store.save(info_);
    }
    // Also keep JSON config in sync (backward compatibility).
    auto config_opt = load_config(info_.worktree);
    nlohmann::json config;
    if (config_opt) config = *config_opt;
    config["project"] = info_.to_json();
    save_config(info_.worktree, config);
    
    return true;
}

} // namespace turbot::core::project
