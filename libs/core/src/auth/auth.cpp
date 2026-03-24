// auth.cpp - Authentication management implementation
// Aligns with OpenCode Auth module capability

#include <turbot/core/auth/auth.hpp>
#include <turbot/core/common/logger.hpp>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <fstream>

namespace turbot::core::auth {

namespace fs = std::filesystem;

// ============================================================================
// Type utilities
// ============================================================================

std::string auth_type_to_string(AuthType type) {
    switch (type) {
        case AuthType::OAuth: return "oauth";
        case AuthType::ApiKey: return "api";
        case AuthType::WellKnown: return "wellknown";
        default: return "unknown";
    }
}

AuthType string_to_auth_type(const std::string& str) {
    if (str == "oauth") return AuthType::OAuth;
    if (str == "wellknown") return AuthType::WellKnown;
    return AuthType::ApiKey;
}

// ============================================================================
// OAuthInfo
// ============================================================================

nlohmann::json OAuthInfo::to_json() const {
    nlohmann::json j = nlohmann::json::object();
    j["refresh"] = refresh_token;
    j["access"] = access_token;
    j["expires"] = expires_at;
    if (account_id) j["accountId"] = *account_id;
    if (enterprise_url) j["enterpriseUrl"] = *enterprise_url;
    return j;
}

OAuthInfo OAuthInfo::from_json(const nlohmann::json& j) {
    OAuthInfo info;
    info.refresh_token = j.value("refresh", std::string{});
    info.access_token = j.value("access", std::string{});
    info.expires_at = j.value("expires", int64_t{0});
    if (j.contains("accountId")) info.account_id = j["accountId"].get<std::string>();
    if (j.contains("enterpriseUrl")) info.enterprise_url = j["enterpriseUrl"].get<std::string>();
    return info;
}

// ============================================================================
// ApiKeyInfo
// ============================================================================

nlohmann::json ApiKeyInfo::to_json() const {
    return {{"key", key}};
}

ApiKeyInfo ApiKeyInfo::from_json(const nlohmann::json& j) {
    ApiKeyInfo info;
    info.key = j.value("key", std::string{});
    return info;
}

// ============================================================================
// WellKnownInfo
// ============================================================================

nlohmann::json WellKnownInfo::to_json() const {
    return {{"key", key}, {"token", token}};
}

WellKnownInfo WellKnownInfo::from_json(const nlohmann::json& j) {
    WellKnownInfo info;
    info.key = j.value("key", std::string{});
    info.token = j.value("token", std::string{});
    return info;
}

// ============================================================================
// AuthInfo
// ============================================================================

nlohmann::json AuthInfo::to_json() const {
    nlohmann::json j = nlohmann::json::object();
    j["type"] = auth_type_to_string(type);
    
    switch (type) {
        case AuthType::OAuth:
            if (oauth) j.update(oauth->to_json());
            break;
        case AuthType::ApiKey:
            if (api_key) j.update(api_key->to_json());
            break;
        case AuthType::WellKnown:
            if (well_known) j.update(well_known->to_json());
            break;
    }
    
    return j;
}

AuthInfo AuthInfo::from_json(const nlohmann::json& j) {
    AuthInfo info;
    info.type = string_to_auth_type(j.value("type", "api"));
    
    switch (info.type) {
        case AuthType::OAuth:
            info.oauth = OAuthInfo::from_json(j);
            break;
        case AuthType::ApiKey:
            info.api_key = ApiKeyInfo::from_json(j);
            break;
        case AuthType::WellKnown:
            info.well_known = WellKnownInfo::from_json(j);
            break;
    }
    
    return info;
}

// ============================================================================
// AuthService::Impl
// ============================================================================

struct AuthService::Impl {
    std::string config_path;
    std::map<std::string, AuthInfo> auth_data;
    
    Impl() {
        // Get config path
        const char* home = std::getenv("HOME");
        if (home) {
            config_path = fmt::format("{}/.turbot/auth.json", home);
        } else {
            config_path = ".turbot/auth.json";
        }
        load();
    }
    
    void load() {
        if (!fs::exists(config_path)) {
            return;
        }
        
        std::ifstream f(config_path);
        if (!f.is_open()) {
            return;
        }
        
        try {
            nlohmann::json data = nlohmann::json::parse(f);
            for (auto it = data.begin(); it != data.end(); ++it) {
                auth_data[it.key()] = AuthInfo::from_json(it.value());
            }
        } catch (const std::exception& e) {
            TURBOT_LOG_WARN("Failed to load auth data: {}", e.what());
        }
    }
    
    void save() {
        fs::path p(config_path);
        if (p.has_parent_path() && !fs::exists(p.parent_path())) {
            std::error_code ec;
            fs::create_directories(p.parent_path(), ec);
        }
        
        nlohmann::json data = nlohmann::json::object();
        for (const auto& [key, info] : auth_data) {
            data[key] = info.to_json();
        }
        
        std::ofstream f(config_path);
        if (f.is_open()) {
            f << data.dump(2) << "\n";
        }
    }
    
    int64_t current_timestamp() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
};

// ============================================================================
// AuthService
// ============================================================================

AuthService::AuthService() : impl_(std::make_unique<Impl>()) {}

AuthService::~AuthService() = default;

AuthService& AuthService::instance() {
    static AuthService instance;
    return instance;
}

std::optional<AuthInfo> AuthService::get(const std::string& provider_id) const {
    auto it = impl_->auth_data.find(provider_id);
    if (it != impl_->auth_data.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::map<std::string, AuthInfo> AuthService::all() const {
    return impl_->auth_data;
}

bool AuthService::set(const std::string& provider_id, const AuthInfo& info) {
    std::string normalized = provider_id;
    // Remove trailing slash
    while (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }
    
    impl_->auth_data[normalized] = info;
    impl_->save();
    
    TURBOT_LOG_INFO("Auth info saved for: {}", normalized);
    return true;
}

bool AuthService::remove(const std::string& provider_id) {
    std::string normalized = provider_id;
    while (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }
    
    auto it = impl_->auth_data.find(normalized);
    if (it != impl_->auth_data.end()) {
        impl_->auth_data.erase(it);
        impl_->save();
        TURBOT_LOG_INFO("Auth info removed for: {}", normalized);
        return true;
    }
    
    return false;
}

bool AuthService::is_valid(const std::string& provider_id) const {
    auto info = get(provider_id);
    if (!info) return false;
    
    if (info->type == AuthType::OAuth && info->oauth) {
        return info->oauth->expires_at > impl_->current_timestamp();
    }
    
    return true; // API keys don't expire
}

bool AuthService::refresh_if_needed(const std::string& provider_id) {
    auto info = get(provider_id);
    if (!info || info->type != AuthType::OAuth || !info->oauth) {
        return false;
    }
    
    // Check if token needs refresh (5 minute buffer)
    int64_t buffer = 300;
    if (info->oauth->expires_at > impl_->current_timestamp() + buffer) {
        return false; // Not expired
    }
    
    // In production, this would call the OAuth refresh endpoint
    TURBOT_LOG_INFO("Refreshing OAuth token for: {}", provider_id);
    return true;
}

} // namespace turbot::core::auth
