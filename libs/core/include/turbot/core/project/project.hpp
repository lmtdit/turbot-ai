#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/storage/database.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace turbot::core::project {

/// Project ID type
using ProjectId = std::string;

/// VCS type enum
enum class TURBOT_CORE_API VcsType {
    None,   ///< No version control
    Git,    ///< Git repository
};

/// Convert VcsType to string
[[nodiscard]] TURBOT_CORE_API std::string vcs_type_to_string(VcsType vcs);

/// Convert string to VcsType
[[nodiscard]] TURBOT_CORE_API VcsType string_to_vcs_type(const std::string& str);

/// Project icon information
struct TURBOT_CORE_API ProjectIcon {
    std::optional<std::string> url;       ///< Icon URL or data URI
    std::optional<std::string> color;     ///< Icon color
    std::optional<std::string> override_; ///< Override icon name

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static ProjectIcon from_json(const nlohmann::json& j);

    /// Equality comparison
    bool operator==(const ProjectIcon& other) const noexcept;
};

/// Project commands configuration
struct TURBOT_CORE_API ProjectCommands {
    std::optional<std::string> start; ///< Startup script to run when creating a new workspace

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static ProjectCommands from_json(const nlohmann::json& j);

    /// Equality comparison
    bool operator==(const ProjectCommands& other) const noexcept;
};

/// Project time information
struct TURBOT_CORE_API ProjectTime {
    int64_t created = 0;            ///< Creation timestamp
    int64_t updated = 0;            ///< Last update timestamp
    std::optional<int64_t> initialized; ///< Initialization timestamp

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static ProjectTime from_json(const nlohmann::json& j);

    /// Equality comparison
    bool operator==(const ProjectTime& other) const noexcept;
};

/// Project information structure
/// 
/// Aligned with OpenCode Project.Info:
///   { id, worktree, vcs, name, icon, commands, time, sandboxes }
struct TURBOT_CORE_API ProjectInfo {
    ProjectId id;                           ///< Unique project identifier
    std::string worktree;                   ///< Main working directory
    VcsType vcs = VcsType::None;            ///< Version control system
    std::optional<std::string> name;        ///< Project name
    std::optional<ProjectIcon> icon;        ///< Project icon
    std::optional<ProjectCommands> commands;///< Project commands
    ProjectTime time;                       ///< Time information
    std::vector<std::string> sandboxes;     ///< Sandbox directories

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static ProjectInfo from_json(const nlohmann::json& j);

    /// Equality comparison
    bool operator==(const ProjectInfo& other) const noexcept;
};

/// Parameters for creating a project
struct TURBOT_CORE_API CreateParams {
    std::string directory;              ///< Project directory
    std::optional<std::string> name;    ///< Project name
    std::optional<ProjectIcon> icon;    ///< Project icon
    std::optional<ProjectCommands> commands; ///< Project commands
    bool init_git = false;              ///< Initialize git repository
};

/// Parameters for updating a project
struct TURBOT_CORE_API UpdateParams {
    std::optional<std::string> name;        ///< New name
    std::optional<ProjectIcon> icon;        ///< New icon
    std::optional<ProjectCommands> commands;///< New commands
};

/// Result of loading a project from directory
struct TURBOT_CORE_API LoadResult {
    ProjectInfo project;    ///< Loaded project info
    std::string sandbox;    ///< Sandbox directory
};

/// Project class - manages a project
/// 
/// A project represents a working context with:
/// - A unique identifier (based on git remote or directory path)
/// - A working directory (worktree)
/// - Optional VCS integration
/// - Configuration stored in .turbot/turbot.json
class TURBOT_CORE_API Project {
public:
    /// Special project ID for global context
    static constexpr const char* GLOBAL_ID = "global";

    /// Create a new project
    /// @param params Creation parameters
    /// @return Created project
    [[nodiscard]] static std::optional<Project> create(const CreateParams& params);

    /// Load project from a directory
    /// @param directory Directory path
    /// @return Load result with project and sandbox info
    [[nodiscard]] static LoadResult from_directory(const std::string& directory);

    /// Get a project by ID
    /// @param id Project ID
    /// @return Project or nullopt if not found
    [[nodiscard]] static std::optional<Project> get(const ProjectId& id);

    /// List all projects
    /// @return Vector of projects
    [[nodiscard]] static std::vector<Project> list();

    /// Delete a project by ID
    /// @param id Project ID
    /// @return true if deleted
    static bool remove(const ProjectId& id);

    /// Default constructor
    Project() = default;

    /// Constructor with project info
    explicit Project(ProjectInfo info) : info_(std::move(info)) {}

    /// Update project
    /// @param params Update parameters
    /// @return true if updated
    bool update(const UpdateParams& params);

    /// Set project as initialized
    /// @return true if updated
    bool set_initialized();

    /// Initialize git repository for this project
    /// @return true if git was initialized
    bool init_git();

    /// Get project ID
    [[nodiscard]] const ProjectId& id() const noexcept { return info_.id; }

    /// Get project info
    [[nodiscard]] const ProjectInfo& info() const noexcept { return info_; }

    /// Get worktree path
    [[nodiscard]] const std::string& worktree() const noexcept { return info_.worktree; }

    /// Check if project has VCS
    [[nodiscard]] bool has_vcs() const noexcept { return info_.vcs != VcsType::None; }

    /// Check if project is valid
    [[nodiscard]] bool is_valid() const noexcept { return !info_.id.empty(); }

    /// Check if project is global
    [[nodiscard]] bool is_global() const noexcept { return info_.id == GLOBAL_ID; }

    /// Equality comparison
    bool operator==(const Project& other) const noexcept {
        return info_.id == other.info_.id;
    }

private:
    ProjectInfo info_;

    /// Generate a unique project ID from directory
    [[nodiscard]] static ProjectId generate_id(const std::string& directory);

    /// Get current timestamp
    [[nodiscard]] static int64_t current_timestamp();

    /// Detect VCS type from directory
    [[nodiscard]] static VcsType detect_vcs(const std::string& directory);

    /// Load project config from .turbot/turbot.json
    [[nodiscard]] static std::optional<nlohmann::json> load_config(const std::string& directory);

    /// Save project config to .turbot/turbot.json
    static bool save_config(const std::string& directory, const nlohmann::json& config);

    /// Discover project icon from directory
    [[nodiscard]] static std::optional<ProjectIcon> discover_icon(const std::string& directory);
};

} // namespace turbot::core::project
