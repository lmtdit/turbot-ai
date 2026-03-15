#include <turbot/core/tool/external_directory.hpp>
#include <turbot/core/common/logger.hpp>
#include <filesystem>
#include <sstream>

namespace turbot::core::tool {

bool is_inside_project(const std::string& target, const std::string& project_dir) {
    if (target.empty() || project_dir.empty()) {
        return true;  // Empty target is considered inside
    }
    
    try {
        std::filesystem::path target_path(target);
        std::filesystem::path project_path(project_dir);
        
        // Make absolute if relative
        if (target_path.is_relative()) {
            target_path = std::filesystem::absolute(project_path / target_path);
        }
        if (project_path.is_relative()) {
            project_path = std::filesystem::absolute(project_path);
        }
        
        // Canonicalize both paths
        std::error_code ec;
        auto canonical_target = std::filesystem::canonical(target_path, ec);
        if (ec) {
            // Target doesn't exist yet, use weakly_canonical
            canonical_target = std::filesystem::weakly_canonical(target_path);
        }
        auto canonical_project = std::filesystem::canonical(project_path, ec);
        if (ec) {
            return true;  // Can't determine, assume inside
        }
        
        // Check if target starts with project path
        auto project_str = canonical_project.string();
        auto target_str = canonical_target.string();
        
        // Ensure consistent trailing slashes for comparison
        if (project_str.back() != '/') {
            project_str += '/';
        }
        
        // Target is inside if it starts with project path
        if (target_str.length() >= project_str.length() - 1) {
            // Check exact match or starts with project/
            if (target_str == canonical_project.string()) {
                return true;
            }
            if (target_str.substr(0, project_str.length() - 1) == canonical_project.string()) {
                return true;
            }
            if (target_str.length() > project_str.length() && 
                target_str.substr(0, project_str.length()) == project_str) {
                return true;
            }
        }
        
        return false;
    } catch (const std::exception& e) {
        TURBOT_LOG_WARN("Error checking path containment: {}", e.what());
        return true;  // Assume inside on error
    }
}

std::string get_parent_directory(const std::string& path, ExternalPathKind kind) {
    std::filesystem::path p(path);
    
    if (kind == ExternalPathKind::Directory) {
        return p.string();
    }
    
    return p.parent_path().string();
}

bool assert_external_directory(
    const std::string& target,
    const std::string& project_dir,
    ToolContext& ctx,
    const ExternalDirectoryOptions& options
) {
    // Skip if no target
    if (target.empty()) {
        return true;
    }
    
    // Skip if bypass is set
    if (options.bypass) {
        return true;
    }
    
    // Check if target is inside project
    if (is_inside_project(target, project_dir)) {
        return true;
    }
    
    // Target is outside project - need to request permission
    TURBOT_LOG_INFO("External directory access requested: {} (project: {})", target, project_dir);
    
    // Get parent directory for the permission pattern
    std::string parent_dir = get_parent_directory(target, options.kind);
    
    // Build glob pattern for the parent directory
    std::filesystem::path parent_path(parent_dir);
    std::string glob_pattern = (parent_path / "*").string();
    
    // Normalize the pattern
    std::replace(glob_pattern.begin(), glob_pattern.end(), '\\', '/');
    
    // Check if permission is already granted via ruleset
    if (ctx.ask_permission) {
        // Create permission request
        permission::PermissionRequest request;
        request.id = "ext_dir_" + std::to_string(std::hash<std::string>{}(target));
        request.permission = "external_directory";
        request.patterns = {glob_pattern};
        request.metadata = {
            {"filepath", target},
            {"parentDir", parent_dir}
        };
        
        // Request permission
        auto reply = ctx.ask_permission(request);
        
        switch (reply.type) {
            case permission::PermissionReply::Type::Always:
                // User granted always - the caller should add to ruleset
                TURBOT_LOG_INFO("External directory access granted (always): {}", glob_pattern);
                return true;
            case permission::PermissionReply::Type::Once:
                TURBOT_LOG_INFO("External directory access granted (once): {}", target);
                return true;
            case permission::PermissionReply::Type::Reject:
                TURBOT_LOG_WARN("External directory access denied: {}", target);
                return false;
        }
    }
    
    // No permission callback - deny by default for safety
    TURBOT_LOG_WARN("External directory access denied (no permission handler): {}", target);
    return false;
}

} // namespace turbot::core::tool
