#include <turbot/core/permission/permission.hpp>
#include <stdexcept>
#include <string>

namespace turbot::core::permission {

// ============================================================================
// PermissionAction helpers
// ============================================================================

std::string_view permission_action_to_string(PermissionAction action) noexcept {
    switch (action) {
        case PermissionAction::Allow: return "allow";
        case PermissionAction::Deny:  return "deny";
        case PermissionAction::Ask:   return "ask";
    }
    std::unreachable();
}

PermissionAction permission_action_from_string(std::string_view str) {
    if (str == "allow") return PermissionAction::Allow;
    if (str == "deny")  return PermissionAction::Deny;
    if (str == "ask")   return PermissionAction::Ask;
    throw std::invalid_argument("Unknown PermissionAction: " + std::string(str));
}

// ============================================================================
// PermissionRule
// ============================================================================

nlohmann::json PermissionRule::to_json() const {
    return {
        {"permission", permission},
        {"pattern",    pattern},
        {"action",     std::string(permission_action_to_string(action))}
    };
}

PermissionRule PermissionRule::from_json(const nlohmann::json& j) {
    PermissionRule rule;
    rule.permission = j.at("permission").get<std::string>();
    rule.pattern    = j.at("pattern").get<std::string>();
    rule.action     = permission_action_from_string(j.at("action").get<std::string>());
    return rule;
}

// ============================================================================
// Ruleset helpers
// ============================================================================

nlohmann::json ruleset_to_json(const Ruleset& ruleset) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& rule : ruleset) {
        arr.push_back(rule.to_json());
    }
    return arr;
}

Ruleset ruleset_from_json(const nlohmann::json& j) {
    Ruleset ruleset;
    for (const auto& item : j) {
        ruleset.push_back(PermissionRule::from_json(item));
    }
    return ruleset;
}

// ============================================================================
// PermissionRequest
// ============================================================================

nlohmann::json PermissionRequest::to_json() const {
    nlohmann::json j = {
        {"id",         id},
        {"permission", permission},
        {"patterns",   patterns},
        {"metadata",   metadata}
    };
    if (tool.has_value()) {
        j["tool"] = *tool;
    }
    return j;
}

PermissionRequest PermissionRequest::from_json(const nlohmann::json& j) {
    PermissionRequest req;
    req.id         = j.at("id").get<std::string>();
    req.permission = j.at("permission").get<std::string>();
    req.patterns   = j.at("patterns").get<std::vector<std::string>>();
    if (j.contains("metadata")) {
        req.metadata = j["metadata"];
    }
    if (j.contains("tool") && !j["tool"].is_null()) {
        req.tool = j["tool"].get<std::string>();
    }
    return req;
}

// ============================================================================
// PermissionReply::Type helpers
// ============================================================================

std::string_view permission_reply_type_to_string(PermissionReply::Type type) noexcept {
    switch (type) {
        case PermissionReply::Type::Once:   return "once";
        case PermissionReply::Type::Always: return "always";
        case PermissionReply::Type::Reject: return "reject";
    }
    std::unreachable();
}

PermissionReply::Type permission_reply_type_from_string(std::string_view str) {
    if (str == "once")   return PermissionReply::Type::Once;
    if (str == "always") return PermissionReply::Type::Always;
    if (str == "reject") return PermissionReply::Type::Reject;
    throw std::invalid_argument("Unknown PermissionReply::Type: " + std::string(str));
}

// ============================================================================
// PermissionReply
// ============================================================================

nlohmann::json PermissionReply::to_json() const {
    nlohmann::json j = {
        {"type", std::string(permission_reply_type_to_string(type))}
    };
    if (message.has_value()) {
        j["message"] = *message;
    }
    return j;
}

PermissionReply PermissionReply::from_json(const nlohmann::json& j) {
    PermissionReply reply;
    reply.type = permission_reply_type_from_string(j.at("type").get<std::string>());
    if (j.contains("message") && !j["message"].is_null()) {
        reply.message = j["message"].get<std::string>();
    }
    return reply;
}

PermissionReply PermissionReply::once(const std::optional<std::string>& msg) {
    return {Type::Once, msg};
}

PermissionReply PermissionReply::always(const std::optional<std::string>& msg) {
    return {Type::Always, msg};
}

PermissionReply PermissionReply::reject(const std::optional<std::string>& msg) {
    return {Type::Reject, msg};
}

// ============================================================================
// PermissionSystem
// ============================================================================

PermissionAction PermissionSystem::evaluate(
    const std::string& permission,
    const std::string& pattern,
    const Ruleset& ruleset
) noexcept {
    // Default is Ask if no rules match (OpenCode behavior)
    // This allows the system to request user permission for unknown operations
    PermissionAction result = PermissionAction::Ask;

    for (const auto& rule : ruleset) {
        // Check both permission type and resource pattern match
        if (wildcard_match(rule.permission, permission) &&
            wildcard_match(rule.pattern, pattern)) {
            result = rule.action;  // Last matching rule wins
        }
    }

    return result;
}

Ruleset PermissionSystem::merge(const std::vector<Ruleset>& rulesets) {
    size_t total = 0;
    for (const auto& rs : rulesets) { total += rs.size(); }
    Ruleset merged;
    merged.reserve(total);
    for (const auto& ruleset : rulesets) {
        merged.insert(merged.end(), ruleset.begin(), ruleset.end());
    }
    return merged;
}

bool PermissionSystem::wildcard_match(
    const std::string& pattern,
    const std::string& text
) noexcept {
    // Handle exact '*' wildcard - matches everything
    if (pattern == "*") {
        return true;
    }

    const size_t n = text.size();
    const size_t m = pattern.size();

    // DP approach to handle multiple '*' correctly
    // dp[i][j] = true if text[0..i-1] matches pattern[0..j-1]
    // For efficiency, use two rolling arrays

    size_t pi = 0;  // pattern index
    size_t ti = 0;  // text index
    size_t star_pi = std::string::npos;  // last '*' position in pattern
    size_t match_ti = 0;  // text position when '*' was encountered

    while (ti < n) {
        if (pi < m && (pattern[pi] == text[ti] || pattern[pi] == '?')) {
            ++pi;
            ++ti;
        } else if (pi < m && pattern[pi] == '*') {
            star_pi = pi;
            match_ti = ti;
            ++pi;
        } else if (star_pi != std::string::npos) {
            // Backtrack: '*' matches one more character
            pi = star_pi + 1;
            ++match_ti;
            ti = match_ti;
        } else {
            return false;
        }
    }

    // Skip trailing '*' in pattern
    while (pi < m && pattern[pi] == '*') {
        ++pi;
    }

    return pi == m;
}

bool PermissionSystem::is_allowed(
    const std::string& permission,
    const std::string& pattern,
    const Ruleset& ruleset
) noexcept {
    return evaluate(permission, pattern, ruleset) == PermissionAction::Allow;
}

bool PermissionSystem::should_ask(
    const std::string& permission,
    const std::string& pattern,
    const Ruleset& ruleset
) noexcept {
    return evaluate(permission, pattern, ruleset) == PermissionAction::Ask;
}

} // namespace turbot::core::permission
