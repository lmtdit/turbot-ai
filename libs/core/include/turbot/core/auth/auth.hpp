#pragma once

#include <turbot/core/common/export.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <map>

namespace turbot::core::auth {

/// Auth type enum
enum class TURBOT_CORE_API AuthType {
    OAuth,      ///< OAuth token authentication
    ApiKey,     ///< API key authentication
    WellKnown   ///< Well-known token authentication
};

/// Convert AuthType to string
[[nodiscard]] TURBOT_CORE_API std::string auth_type_to_string(AuthType type);

/// Convert string to AuthType
[[nodiscard]] TURBOT_CORE_API AuthType string_to_auth_type(const std::string& str);

/// OAuth authentication info
struct TURBOT_CORE_API OAuthInfo {
    std::string refresh_token;          ///< Refresh token
    std::string access_token;           ///< Access token
    int64_t expires_at = 0;             ///< Expiration timestamp
    std::optional<std::string> account_id;///< Account ID
    std::optional<std::string> enterprise_url;///< Enterprise URL
    
    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
    
    /// Deserialize from JSON
    static OAuthInfo from_json(const nlohmann::json& j);
};

/// API Key authentication info
struct TURBOT_CORE_API ApiKeyInfo {
    std::string key;                    ///< API key
    
    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
    
    /// Deserialize from JSON
    static ApiKeyInfo from_json(const nlohmann::json& j);
};

/// Well-known authentication info
struct TURBOT_CORE_API WellKnownInfo {
    std::string key;                    ///< Key identifier
    std::string token;                  ///< Token value
    
    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
    
    /// Deserialize from JSON
    static WellKnownInfo from_json(const nlohmann::json& j);
};

/// Auth info variant
struct TURBOT_CORE_API AuthInfo {
    AuthType type = AuthType::ApiKey;
    std::optional<OAuthInfo> oauth;
    std::optional<ApiKeyInfo> api_key;
    std::optional<WellKnownInfo> well_known;
    
    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
    
    /// Deserialize from JSON
    static AuthInfo from_json(const nlohmann::json& j);
};

/// Auth service for managing authentication credentials
class TURBOT_CORE_API AuthService {
public:
    /// Get the singleton instance
    static AuthService& instance();
    
    /// Get auth info for a provider
    /// @param provider_id Provider ID
    /// @return Auth info or nullopt
    [[nodiscard]] std::optional<AuthInfo> get(const std::string& provider_id) const;
    
    /// Get all auth info
    /// @return Map of provider ID to auth info
    [[nodiscard]] std::map<std::string, AuthInfo> all() const;
    
    /// Set auth info for a provider
    /// @param provider_id Provider ID
    /// @param info Auth info
    /// @return true if successful
    bool set(const std::string& provider_id, const AuthInfo& info);
    
    /// Remove auth info for a provider
    /// @param provider_id Provider ID
    /// @return true if removed
    bool remove(const std::string& provider_id);
    
    /// Check if auth is valid (not expired for OAuth)
    /// @param provider_id Provider ID
    /// @return true if valid
    [[nodiscard]] bool is_valid(const std::string& provider_id) const;
    
    /// Refresh OAuth token if expired
    /// @param provider_id Provider ID
    /// @return true if refreshed
    bool refresh_if_needed(const std::string& provider_id);
    
private:
    AuthService();
    ~AuthService();
    
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace turbot::core::auth
