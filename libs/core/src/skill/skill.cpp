#include "turbot/core/skill/skill.hpp"
#include "turbot/core/config/config_manager.hpp"
#include <turbot/core/common/logger.hpp>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_set>

namespace turbot::core {

// ===== Thread-safe cached path utilities =====
namespace {
    std::string cached_home_dir;
    std::string cached_xdg_config_dir;
    std::once_flag home_dir_flag;

    void init_home_directories() {
        const char* home = std::getenv("HOME");
        if (!home) {
            home = std::getenv("USERPROFILE");  // Windows
        }
        if (home) {
            cached_home_dir = home;

            const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
            if (xdg_config) {
                cached_xdg_config_dir = xdg_config;
            } else {
                cached_xdg_config_dir = cached_home_dir + "/.config";
            }
        }
    }

    const std::string& get_home_dir() {
        std::call_once(home_dir_flag, init_home_directories);
        return cached_home_dir;
    }

    const std::string& get_xdg_config_dir() {
        std::call_once(home_dir_flag, init_home_directories);
        return cached_xdg_config_dir;
    }
}

// ===== SkillSource conversion =====

std::string_view skill_source_to_string(SkillSource source) noexcept {
    switch (source) {
        case SkillSource::Project:  return "project";
        case SkillSource::Global:   return "global";
        case SkillSource::External: return "external";
        case SkillSource::Remote:   return "remote";
    }
    return "unknown";
}

SkillSource skill_source_from_string(std::string_view str) {
    if (str == "project")  return SkillSource::Project;
    if (str == "global")   return SkillSource::Global;
    if (str == "external") return SkillSource::External;
    if (str == "remote")   return SkillSource::Remote;
    return SkillSource::Project;
}

// ===== Skill name validation =====

bool validate_skill_name(const std::string& name) noexcept {
    if (name.empty() || name.size() > skill_constants::max_name_length) {
        return false;
    }

    // Pattern: lowercase alphanumeric with single hyphens
    // ^[a-z0-9]+(-[a-z0-9]+)*$
    static const std::regex pattern("^[a-z0-9]+(-[a-z0-9]+)*$");
    return std::regex_match(name, pattern);
}

// ===== Skill implementation =====

std::optional<Skill> Skill::parse(const std::string& file_path) {
    namespace fs = std::filesystem;

    try {
        if (!fs::exists(file_path)) {
            TURBOT_LOG_ERROR("Skill file not found: {}", file_path);
            return std::nullopt;
        }

        // Use config_manager's parse_markdown_config for consistency
        auto config = ConfigManager::instance().parse_markdown_config(file_path);

        if (config.frontmatter.is_null() || config.frontmatter.empty()) {
            TURBOT_LOG_ERROR("No frontmatter found in skill file: {}", file_path);
            return std::nullopt;
        }

        // Extract required fields
        std::string name;
        if (config.frontmatter.contains("name") && !config.frontmatter["name"].is_null()) {
            name = config.frontmatter["name"].get<std::string>();
        }

        std::string description;
        if (config.frontmatter.contains("description") && !config.frontmatter["description"].is_null()) {
            description = config.frontmatter["description"].get<std::string>();
        }

        // Validate required fields
        if (name.empty()) {
            TURBOT_LOG_ERROR("Skill missing required field 'name': {}", file_path);
            return std::nullopt;
        }

        if (description.empty()) {
            TURBOT_LOG_ERROR("Skill missing required field 'description': {}", file_path);
            return std::nullopt;
        }

        // Validate name format
        if (!validate_skill_name(name)) {
            TURBOT_LOG_ERROR("Invalid skill name '{}': must be 1-64 lowercase alphanumeric with single hyphens", name);
            return std::nullopt;
        }

        Skill skill;
        skill.name = name;
        skill.description = description;
        skill.instructions = config.content;
        skill.skill_file_path = file_path;
        skill.path = fs::path(file_path).parent_path().string();

        // Extract optional fields
        if (config.frontmatter.contains("license") && !config.frontmatter["license"].is_null()) {
            skill.license = config.frontmatter["license"].get<std::string>();
        }

        if (config.frontmatter.contains("compatibility") && !config.frontmatter["compatibility"].is_null()) {
            skill.compatibility = config.frontmatter["compatibility"].get<std::string>();
        }

        if (config.frontmatter.contains("version") && !config.frontmatter["version"].is_null()) {
            skill.version = config.frontmatter["version"].get<std::string>();
        }

        if (config.frontmatter.contains("dependencies") && config.frontmatter["dependencies"].is_array()) {
            std::vector<std::string> deps;
            for (const auto& dep : config.frontmatter["dependencies"]) {
                if (dep.is_string()) {
                    deps.push_back(dep.get<std::string>());
                }
            }
            if (!deps.empty()) {
                skill.dependencies = std::move(deps);
            }
        }

        // Extract any additional metadata using unordered_set for O(1) lookup
        static const std::unordered_set<std::string> known_fields = {
            "name", "description", "license", "compatibility", "version", "dependencies"
        };

        std::map<std::string, std::string> meta_map;
        for (auto it = config.frontmatter.begin(); it != config.frontmatter.end(); ++it) {
            if (known_fields.find(it.key()) == known_fields.end() && it.value().is_string()) {
                meta_map[it.key()] = it.value().get<std::string>();
            }
        }
        if (!meta_map.empty()) {
            skill.metadata = std::move(meta_map);
        }

        // Set timestamp
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        skill.time_loaded = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

        return skill;

    } catch (const fs::filesystem_error& e) {
        TURBOT_LOG_ERROR("Filesystem error parsing skill '{}': {}", file_path, e.what());
        return std::nullopt;
    } catch (const nlohmann::json::exception& e) {
        TURBOT_LOG_ERROR("JSON error parsing skill '{}': {}", file_path, e.what());
        return std::nullopt;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("Error parsing skill '{}': {}", file_path, e.what());
        return std::nullopt;
    }
}

Skill Skill::create(
    const std::string& name,
    const std::string& description,
    const std::string& instructions,
    const std::string& path
) {
    Skill skill;
    skill.name = name;
    skill.description = description;
    skill.instructions = instructions;
    skill.path = path;
    skill.source = SkillSource::Project;

    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    skill.time_loaded = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    return skill;
}

bool Skill::is_valid() const noexcept {
    return !name.empty() &&
           !description.empty() &&
           validate_skill_name(name) &&
           name.size() <= skill_constants::max_name_length &&
           description.size() <= skill_constants::max_description_length;
}

std::vector<std::string> Skill::get_validation_errors() const {
    std::vector<std::string> errors;

    if (name.empty()) {
        errors.push_back("name is required");
    } else {
        if (name.size() > skill_constants::max_name_length) {
            errors.push_back("name exceeds maximum length of " +
                           std::to_string(skill_constants::max_name_length));
        }
        if (!validate_skill_name(name)) {
            errors.push_back("name must be lowercase alphanumeric with single hyphens");
        }
    }

    if (description.empty()) {
        errors.push_back("description is required");
    } else if (description.size() > skill_constants::max_description_length) {
        errors.push_back("description exceeds maximum length of " +
                        std::to_string(skill_constants::max_description_length));
    }

    if (compatibility.has_value() && compatibility->size() > skill_constants::max_compatibility_length) {
        errors.push_back("compatibility exceeds maximum length of " +
                        std::to_string(skill_constants::max_compatibility_length));
    }

    return errors;
}

nlohmann::json Skill::to_json() const {
    nlohmann::json j = {
        {"name", name},
        {"description", description},
        {"instructions", instructions},
        {"path", path},
        {"skill_file_path", skill_file_path},
        {"source", std::string(skill_source_to_string(source))},
        {"time_loaded", time_loaded}
    };

    if (license.has_value()) {
        j["license"] = *license;
    }
    if (compatibility.has_value()) {
        j["compatibility"] = *compatibility;
    }
    if (version.has_value()) {
        j["version"] = *version;
    }
    if (dependencies.has_value()) {
        j["dependencies"] = *dependencies;
    }
    if (metadata.has_value()) {
        j["metadata"] = *metadata;
    }

    return j;
}

Skill Skill::from_json(const nlohmann::json& j) {
    Skill skill;
    skill.name = j.value("name", std::string{});
    skill.description = j.value("description", std::string{});
    skill.instructions = j.value("instructions", std::string{});
    skill.path = j.value("path", std::string{});
    skill.skill_file_path = j.value("skill_file_path", std::string{});
    skill.source = skill_source_from_string(j.value("source", "project"));
    skill.time_loaded = j.value("time_loaded", int64_t{0});

    // Validate name format
    if (!skill.name.empty() && !validate_skill_name(skill.name)) {
        TURBOT_LOG_WARN("Invalid skill name '{}' in JSON deserialization", skill.name);
    }

    if (j.contains("license") && !j["license"].is_null()) {
        skill.license = j["license"].get<std::string>();
    }
    if (j.contains("compatibility") && !j["compatibility"].is_null()) {
        skill.compatibility = j["compatibility"].get<std::string>();
    }
    if (j.contains("version") && !j["version"].is_null()) {
        skill.version = j["version"].get<std::string>();
    }
    if (j.contains("dependencies") && j["dependencies"].is_array()) {
        std::vector<std::string> deps;
        for (const auto& dep : j["dependencies"]) {
            if (dep.is_string()) {
                deps.push_back(dep.get<std::string>());
            }
        }
        if (!deps.empty()) {
            skill.dependencies = std::move(deps);
        }
    }
    if (j.contains("metadata") && j["metadata"].is_object()) {
        std::map<std::string, std::string> meta;
        for (auto it = j["metadata"].begin(); it != j["metadata"].end(); ++it) {
            if (it.value().is_string()) {
                meta[it.key()] = it.value().get<std::string>();
            }
        }
        if (!meta.empty()) {
            skill.metadata = std::move(meta);
        }
    }

    return skill;
}

// ===== SkillRegistry implementation =====

SkillRegistry& SkillRegistry::instance() {
    static SkillRegistry instance;
    return instance;
}

size_t SkillRegistry::scan_directory(const std::string& dir_path, SkillSource source) {
    namespace fs = std::filesystem;

    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        TURBOT_LOG_DEBUG("Skill directory not found: {}", dir_path);
        return 0;
    }

    size_t loaded = 0;

    try {
        for (const auto& entry : fs::directory_iterator(dir_path)) {
            if (!entry.is_directory()) {
                continue;
            }

            std::string skill_file = entry.path().string() + "/" + skill_constants::skill_file_name;
            if (fs::exists(skill_file)) {
                auto result = load_skill(skill_file, source);
                if (result.success) {
                    loaded++;
                    TURBOT_LOG_INFO("Loaded skill '{}' from {}", result.name, skill_file);
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        TURBOT_LOG_ERROR("Error scanning skill directory {}: {}", dir_path, e.what());
    }

    return loaded;
}

size_t SkillRegistry::scan_project(const std::string& project_root) {
    namespace fs = std::filesystem;
    size_t total = 0;

    // Scan .opencode/skills/
    std::string opencode_skills = project_root + "/.opencode/skills";
    if (fs::exists(opencode_skills)) {
        total += scan_directory(opencode_skills, SkillSource::Project);
    }

    // Scan .opencode/skill/ (alternative singular form)
    std::string opencode_skill = project_root + "/.opencode/skill";
    if (fs::exists(opencode_skill)) {
        total += scan_directory(opencode_skill, SkillSource::Project);
    }

    return total;
}

std::string SkillRegistry::get_global_skill_directory() {
    const std::string& xdg_config = get_xdg_config_dir();
    if (!xdg_config.empty()) {
        return xdg_config + "/opencode/skills";
    }
    return "";
}

size_t SkillRegistry::scan_global() {
    size_t total = 0;

    std::string global_dir = get_global_skill_directory();
    if (!global_dir.empty()) {
        namespace fs = std::filesystem;
        if (fs::exists(global_dir)) {
            total += scan_directory(global_dir, SkillSource::Global);
        }
    }

    // Also scan ~/.claude/skills/ for compatibility
    const std::string& home = get_home_dir();
    if (!home.empty()) {
        std::string claude_global = home + "/.claude/skills";
        namespace fs = std::filesystem;
        if (fs::exists(claude_global)) {
            total += scan_directory(claude_global, SkillSource::Global);
        }
    }

    return total;
}

size_t SkillRegistry::scan_external(const std::string& project_root) {
    namespace fs = std::filesystem;
    size_t total = 0;

    // Scan .claude/skills/
    std::string claude_skills = project_root + "/.claude/skills";
    if (fs::exists(claude_skills)) {
        total += scan_directory(claude_skills, SkillSource::External);
    }

    // Scan .agents/skills/
    std::string agents_skills = project_root + "/.agents/skills";
    if (fs::exists(agents_skills)) {
        total += scan_directory(agents_skills, SkillSource::External);
    }

    return total;
}

size_t SkillRegistry::scan_all(const std::string& project_root) {
    size_t total = 0;

    // Priority order (later overrides earlier):
    // 1. Global skills
    // 2. External skills (.claude, .agents)
    // 3. Project skills (.opencode)

    total += scan_global();
    total += scan_external(project_root);
    total += scan_project(project_root);

    // Scan custom directories
    for (const auto& dir : custom_directories_) {
        total += scan_directory(dir, SkillSource::Project);
    }

    TURBOT_LOG_INFO("Loaded {} skills total", total);
    return total;
}

bool SkillRegistry::register_skill(const Skill& skill) {
    if (!skill.is_valid()) {
        TURBOT_LOG_ERROR("Cannot register invalid skill: {}", skill.name);
        return false;
    }

    // Check for existing skill
    if (skills_.contains(skill.name)) {
        TURBOT_LOG_DEBUG("Overriding existing skill '{}' with new version", skill.name);
    }

    skills_[skill.name] = skill;
    return true;
}

bool SkillRegistry::unregister(const std::string& name) {
    auto it = skills_.find(name);
    if (it != skills_.end()) {
        skills_.erase(it);
        return true;
    }
    return false;
}

std::optional<Skill> SkillRegistry::get(const std::string& name) const {
    auto it = skills_.find(name);
    if (it != skills_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool SkillRegistry::has(const std::string& name) const {
    return skills_.contains(name);
}

std::vector<Skill> SkillRegistry::all() const {
    std::vector<Skill> result;
    result.reserve(skills_.size());
    for (const auto& [name, skill] : skills_) {
        result.push_back(skill);
    }
    return result;
}

std::vector<std::string> SkillRegistry::names() const {
    std::vector<std::string> result;
    result.reserve(skills_.size());
    for (const auto& [name, skill] : skills_) {
        result.push_back(name);
    }
    return result;
}

std::vector<Skill> SkillRegistry::get_by_source(SkillSource source) const {
    std::vector<Skill> result;
    for (const auto& [name, skill] : skills_) {
        if (skill.source == source) {
            result.push_back(skill);
        }
    }
    return result;
}

void SkillRegistry::clear() {
    skills_.clear();
}

size_t SkillRegistry::size() const noexcept {
    return skills_.size();
}

SkillLoadResult SkillRegistry::load_skill(const std::string& file_path, SkillSource source) {
    SkillLoadResult result;
    result.path = file_path;

    auto skill_opt = Skill::parse(file_path);

    if (!skill_opt.has_value()) {
        result.errors.push_back("Failed to parse skill file");
        return result;
    }

    auto& skill = *skill_opt;
    skill.source = source;

    auto errors = skill.get_validation_errors();
    if (!errors.empty()) {
        result.errors = std::move(errors);
        return result;
    }

    result.name = skill.name;
    result.success = register_skill(skill);

    return result;
}

size_t SkillRegistry::reload(const std::string& project_root) {
    clear();
    return scan_all(project_root);
}

void SkillRegistry::add_skill_directory(const std::string& path) {
    custom_directories_.push_back(path);
}

std::vector<std::string> SkillRegistry::get_skill_directories() const {
    std::vector<std::string> dirs;

    // Add global directories
    std::string global_dir = get_global_skill_directory();
    if (!global_dir.empty()) {
        dirs.push_back(global_dir);
    }

    const std::string& home = get_home_dir();
    if (!home.empty()) {
        dirs.push_back(home + "/.claude/skills");
    }

    // Add custom directories
    for (const auto& dir : custom_directories_) {
        dirs.push_back(dir);
    }

    return dirs;
}

// ===== skill_discovery namespace =====

namespace skill_discovery {

SearchPaths get_search_paths(const std::string& project_root) {
    SearchPaths paths;

    paths.project_opencode = project_root + "/.opencode/skills";
    paths.project_claude = project_root + "/.claude/skills";
    paths.project_agents = project_root + "/.agents/skills";

    const std::string& home = get_home_dir();
    if (!home.empty()) {
        const std::string& xdg_config = get_xdg_config_dir();
        if (!xdg_config.empty()) {
            paths.global_opencode = xdg_config + "/opencode/skills";
        }
        paths.global_claude = home + "/.claude/skills";
    }

    return paths;
}

std::vector<std::string> SearchPaths::to_vector() const {
    std::vector<std::string> result;

    if (!project_opencode.empty()) result.push_back(project_opencode);
    if (!project_claude.empty()) result.push_back(project_claude);
    if (!project_agents.empty()) result.push_back(project_agents);
    if (!global_opencode.empty()) result.push_back(global_opencode);
    if (!global_claude.empty()) result.push_back(global_claude);

    return result;
}

std::vector<std::string> find_skill_files(const std::string& dir_path) {
    namespace fs = std::filesystem;
    std::vector<std::string> result;

    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        return result;
    }

    try {
        for (const auto& entry : fs::directory_iterator(dir_path)) {
            if (!entry.is_directory()) {
                continue;
            }

            std::string skill_file = entry.path().string() + "/" + skill_constants::skill_file_name;
            if (fs::exists(skill_file)) {
                result.push_back(skill_file);
            }
        }
    } catch (const fs::filesystem_error&) {
        // Ignore errors during directory traversal
    }

    return result;
}

bool is_skill_directory(const std::string& dir_path) {
    namespace fs = std::filesystem;

    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        return false;
    }

    std::string skill_file = dir_path + "/" + skill_constants::skill_file_name;
    return fs::exists(skill_file);
}

} // namespace skill_discovery

} // namespace turbot::core
