#pragma once

#include <turbot/core/common/export.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace turbot::core::permission {

/// Permission action result
enum class PermissionAction {
    Allow,  ///< Permission is allowed
    Deny,   ///< Permission is denied
    Ask     ///< Permission needs to be requested from user
};

/// Convert PermissionAction to string
[[nodiscard]] TURBOT_CORE_API std::string_view permission_action_to_string(PermissionAction action) noexcept;

/// Parse PermissionAction from string
[[nodiscard]] TURBOT_CORE_API PermissionAction permission_action_from_string(std::string_view str);

/// A single permission rule
struct TURBOT_CORE_API PermissionRule {
    std::string permission;   ///< Permission type (e.g. "read", "write", "execute")
    std::string pattern;      ///< Pattern to match (supports * and ? wildcards)
    PermissionAction action;  ///< Action to take when rule matches

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static PermissionRule from_json(const nlohmann::json& j);

    bool operator==(const PermissionRule& other) const noexcept {
        return permission == other.permission && pattern == other.pattern && action == other.action;
    }
};

/// Collection of permission rules
using Ruleset = std::vector<PermissionRule>;

/// Serialize a Ruleset to JSON
[[nodiscard]] TURBOT_CORE_API nlohmann::json ruleset_to_json(const Ruleset& ruleset);

/// Deserialize a Ruleset from JSON
[[nodiscard]] TURBOT_CORE_API Ruleset ruleset_from_json(const nlohmann::json& j);

/// Permission request - sent to ask the user for permission
struct TURBOT_CORE_API PermissionRequest {
    std::string id;                          ///< Unique request ID
    std::string permission;                  ///< Permission type being requested
    std::vector<std::string> patterns;       ///< Patterns to be permitted
    std::optional<std::string> tool;         ///< Tool that is requesting permission
    nlohmann::json metadata;                 ///< Additional metadata

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static PermissionRequest from_json(const nlohmann::json& j);
};

/// Permission reply - user's response to a permission request
struct TURBOT_CORE_API PermissionReply {
    /// Reply type
    enum class Type {
        Once,   ///< Allow this one time only
        Always, ///< Always allow (add to ruleset)
        Reject  ///< Deny permission
    };

    Type type;
    std::optional<std::string> message;  ///< Optional message from user

    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;

    /// Deserialize from JSON
    static PermissionReply from_json(const nlohmann::json& j);

    /// Create a once reply
    [[nodiscard]] static PermissionReply once(const std::optional<std::string>& msg = std::nullopt);

    /// Create an always reply
    [[nodiscard]] static PermissionReply always(const std::optional<std::string>& msg = std::nullopt);

    /// Create a reject reply
    [[nodiscard]] static PermissionReply reject(const std::optional<std::string>& msg = std::nullopt);
};

/// Convert PermissionReply::Type to string
[[nodiscard]] TURBOT_CORE_API std::string_view permission_reply_type_to_string(PermissionReply::Type type) noexcept;

/// Parse PermissionReply::Type from string
[[nodiscard]] TURBOT_CORE_API PermissionReply::Type permission_reply_type_from_string(std::string_view str);

/// The core permission evaluation system
class TURBOT_CORE_API PermissionSystem {
public:
    PermissionSystem() = default;
    ~PermissionSystem() = default;

    // Non-copyable, movable
    PermissionSystem(const PermissionSystem&) = delete;
    PermissionSystem& operator=(const PermissionSystem&) = delete;
    PermissionSystem(PermissionSystem&&) = default;
    PermissionSystem& operator=(PermissionSystem&&) = default;

    /// Evaluate a permission against a ruleset
    /// Rules are evaluated in order; last matching rule wins.
    /// Default result (no rules match) is Deny.
    /// @param permission The permission type (e.g. "read", "write")
    /// @param pattern The resource pattern to check (e.g. "/path/to/file.txt")
    /// @param ruleset The set of rules to evaluate
    /// @return The evaluated permission action
    [[nodiscard]] static PermissionAction evaluate(
        const std::string& permission,
        const std::string& pattern,
        const Ruleset& ruleset
    ) noexcept;

    /// Merge multiple rulesets into one (last ruleset wins on conflict)
    [[nodiscard]] static Ruleset merge(const std::vector<Ruleset>& rulesets);

    /// Wildcard pattern matching
    /// Supports '*' (any sequence of characters) and '?' (any single character)
    /// @param pattern The pattern to match against
    /// @param text The text to match
    /// @return true if the text matches the pattern
    [[nodiscard]] static bool wildcard_match(
        const std::string& pattern,
        const std::string& text
    ) noexcept;

    /// Check if a permission request matches any Allow rule in the ruleset
    [[nodiscard]] static bool is_allowed(
        const std::string& permission,
        const std::string& pattern,
        const Ruleset& ruleset
    ) noexcept;

    /// Check if a permission request should ask for user input
    [[nodiscard]] static bool should_ask(
        const std::string& permission,
        const std::string& pattern,
        const Ruleset& ruleset
    ) noexcept;
};

} // namespace turbot::core::permission
