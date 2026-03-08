#pragma once

#include <turbot/core/common/export.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace turbot::core {

/// Skill validation constants
namespace skill_constants {
    constexpr size_t max_name_length = 64;
    constexpr size_t max_description_length = 1024;
    constexpr size_t max_compatibility_length = 500;
    constexpr const char* skill_file_name = "SKILL.md";
}

/// Skill source location type
enum class SkillSource : uint8_t {
    Project,    ///< Project-level skill (.opencode/skills/)
    Global,     ///< Global skill (~/.config/opencode/skills/)
    External,   ///< External directory (.claude/skills/, .agents/skills/)
    Remote      ///< Downloaded from remote URL
};

/// Convert SkillSource to string
[[nodiscard]] TURBOT_CORE_API std::string_view skill_source_to_string(SkillSource source) noexcept;

/// Parse SkillSource from string
[[nodiscard]] TURBOT_CORE_API SkillSource skill_source_from_string(std::string_view str);

/// Validate skill name format
/// Name must be 1-64 characters, lowercase alphanumeric with single hyphens
[[nodiscard]] TURBOT_CORE_API bool validate_skill_name(const std::string& name) noexcept;

/// Skill definition - represents a parsed SKILL.md file
struct TURBOT_CORE_API Skill {
    std::string name;                      ///< Skill name (required, unique identifier)
    std::string description;               ///< Skill description (required)
    std::string instructions;              ///< Skill instructions (markdown content)
    std::string path;                      ///< Directory path containing SKILL.md
    std::string skill_file_path;           ///< Full path to SKILL.md file

    // Optional metadata
    std::optional<std::string> license;    ///< License identifier (e.g., "MIT")
    std::optional<std::string> compatibility; ///< Compatibility info (e.g., "opencode>=1.0")
    std::optional<std::string> version;    ///< Skill version
    std::optional<std::vector<std::string>> dependencies; ///< Required skill names
    std::optional<std::map<std::string, std::string>> metadata; ///< Additional metadata

    // Source information
    SkillSource source = SkillSource::Project;
    int64_t time_loaded = 0;

    // ===== Factory methods =====

    /// Parse a SKILL.md file
    [[nodiscard]] static std::optional<Skill> parse(const std::string& file_path);

    /// Create a skill from frontmatter and content
    [[nodiscard]] static Skill create(
        const std::string& name,
        const std::string& description,
        const std::string& instructions,
        const std::string& path = ""
    );

    // ===== Validation =====

    /// Validate skill fields
    [[nodiscard]] bool is_valid() const noexcept;

    /// Get validation errors
    [[nodiscard]] std::vector<std::string> get_validation_errors() const;

    // ===== Serialization =====

    [[nodiscard]] nlohmann::json to_json() const;
    static Skill from_json(const nlohmann::json& j);
};

/// Skill loading result
struct TURBOT_CORE_API SkillLoadResult {
    bool success = false;
    std::string path;
    std::string name;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/// Skill registry - manages skill discovery and loading
class TURBOT_CORE_API SkillRegistry {
public:
    /// Get singleton instance
    static SkillRegistry& instance();

    // Disable copy and move
    SkillRegistry(const SkillRegistry&) = delete;
    SkillRegistry& operator=(const SkillRegistry&) = delete;

    // ===== Directory scanning =====

    /// Scan a directory for skills (looks for SKILL.md in subdirectories)
    /// @param dir_path Directory to scan
    /// @param source Source type for loaded skills
    /// @return Number of skills loaded
    size_t scan_directory(const std::string& dir_path, SkillSource source = SkillSource::Project);

    /// Scan project-level skill directories
    /// @param project_root Project root directory
    /// @return Number of skills loaded
    size_t scan_project(const std::string& project_root);

    /// Scan global skill directories
    /// @return Number of skills loaded
    size_t scan_global();

    /// Scan external directories (.claude/skills, .agents/skills)
    /// @param project_root Project root directory
    /// @return Number of skills loaded
    size_t scan_external(const std::string& project_root);

    /// Scan all configured directories
    /// @param project_root Project root directory
    /// @return Total number of skills loaded
    size_t scan_all(const std::string& project_root);

    // ===== Skill management =====

    /// Register a skill
    /// @param skill Skill to register
    /// @return true if registered successfully
    bool register_skill(const Skill& skill);

    /// Unregister a skill by name
    /// @param name Skill name
    /// @return true if skill was removed
    bool unregister(const std::string& name);

    /// Get a skill by name
    [[nodiscard]] std::optional<Skill> get(const std::string& name) const;

    /// Check if a skill exists
    [[nodiscard]] bool has(const std::string& name) const;

    /// Get all registered skills
    [[nodiscard]] std::vector<Skill> all() const;

    /// Get all skill names
    [[nodiscard]] std::vector<std::string> names() const;

    /// Get skills by source
    [[nodiscard]] std::vector<Skill> get_by_source(SkillSource source) const;

    /// Clear all registered skills
    void clear();

    /// Get number of registered skills
    [[nodiscard]] size_t size() const noexcept;

    // ===== Loading helpers =====

    /// Load a single skill file
    [[nodiscard]] SkillLoadResult load_skill(const std::string& file_path, SkillSource source);

    /// Reload all skills (clear and rescan)
    /// @param project_root Project root directory
    /// @return Number of skills loaded
    size_t reload(const std::string& project_root);

    // ===== Configuration =====

    /// Add custom skill directory to scan
    void add_skill_directory(const std::string& path);

    /// Get configured skill directories
    [[nodiscard]] std::vector<std::string> get_skill_directories() const;

private:
    SkillRegistry() = default;
    ~SkillRegistry() = default;

    std::map<std::string, Skill> skills_;
    std::vector<std::string> custom_directories_;

    // Helper to get global skill directory
    [[nodiscard]] static std::string get_global_skill_directory();
};

/// Skill discovery utilities for finding skills across multiple locations
namespace skill_discovery {

/// Skill search paths
struct TURBOT_CORE_API SearchPaths {
    std::string project_opencode;    ///< .opencode/skills/ or .opencode/skill/
    std::string project_claude;      ///< .claude/skills/
    std::string project_agents;      ///< .agents/skills/
    std::string global_opencode;     ///< ~/.config/opencode/skills/
    std::string global_claude;       ///< ~/.claude/skills/

    /// Get all paths as a vector
    [[nodiscard]] std::vector<std::string> to_vector() const;
};

/// Get default search paths for a project
[[nodiscard]] TURBOT_CORE_API SearchPaths get_search_paths(const std::string& project_root);

/// Find all SKILL.md files in a directory (non-recursive, checks subdirectories)
[[nodiscard]] TURBOT_CORE_API std::vector<std::string> find_skill_files(const std::string& dir_path);

/// Check if a path contains a valid skill directory (has SKILL.md)
[[nodiscard]] TURBOT_CORE_API bool is_skill_directory(const std::string& dir_path);

// ============================================================================
// Remote Skill Discovery (pull skills from URL)
// ============================================================================

/// Remote skill entry in index.json
struct TURBOT_CORE_API RemoteSkillEntry {
    std::string name;                   ///< Skill name
    std::string description;            ///< Skill description
    std::vector<std::string> files;     ///< List of files to download (relative paths)

    /// Parse from JSON
    [[nodiscard]] static std::optional<RemoteSkillEntry> from_json(const nlohmann::json& j);

    /// Convert to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Remote skill index (index.json format)
struct TURBOT_CORE_API RemoteSkillIndex {
    std::vector<RemoteSkillEntry> skills;  ///< List of available skills

    /// Parse from JSON
    [[nodiscard]] static std::optional<RemoteSkillIndex> from_json(const nlohmann::json& j);

    /// Convert to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Result of a pull operation
struct TURBOT_CORE_API PullResult {
    bool success = false;               ///< Whether the pull was successful
    std::vector<std::string> dirs;      ///< Directories where skills were downloaded
    std::vector<std::string> errors;    ///< Error messages
    int skills_downloaded = 0;          ///< Number of skills downloaded
    int files_downloaded = 0;           ///< Number of files downloaded
    bool from_cache = false;            ///< Whether results came from cache

    /// Convert to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Get the cache directory for remote skills
/// Returns ~/.cache/turbot/skills/ (or platform equivalent)
[[nodiscard]] TURBOT_CORE_API std::string get_cache_dir();

/// Pull skills from a remote URL
/// @param base_url Base URL containing index.json (e.g., "https://example.com/skills/")
/// @return PullResult with downloaded skill directories
/// @note The URL should point to a directory containing index.json
/// @note Files are cached in get_cache_dir() and not re-downloaded
[[nodiscard]] TURBOT_CORE_API PullResult pull(const std::string& base_url);

/// Pull skills from multiple URLs
/// @param urls List of base URLs to pull from
/// @return Combined PullResult from all URLs
[[nodiscard]] TURBOT_CORE_API PullResult pull_all(const std::vector<std::string>& urls);

/// Download a single file from URL to destination
/// @param url URL to download from
/// @param dest_path Local file path to save to
/// @return true if download succeeded (or file already exists)
[[nodiscard]] TURBOT_CORE_API bool download_file(const std::string& url, const std::string& dest_path);

/// Clear the skill cache directory
/// @return true if cache was cleared successfully
TURBOT_CORE_API bool clear_cache();

/// Check if a URL is a valid skill index URL
/// @param url URL to check
/// @return true if URL appears to be a valid skill index
[[nodiscard]] TURBOT_CORE_API bool is_valid_skill_url(const std::string& url);

} // namespace skill_discovery

} // namespace turbot::core
