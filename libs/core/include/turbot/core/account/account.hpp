#pragma once

#include <turbot/core/common/export.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace turbot::core::account {

/// Account ID type
using AccountId = std::string;

/// Organization ID type
using OrgId = std::string;

/// Access token type
using AccessToken = std::string;

/// Refresh token type
using RefreshToken = std::string;

/// Device code type
using DeviceCode = std::string;

/// User code type
using UserCode = std::string;

/// Account information structure
struct TURBOT_CORE_API AccountInfo {
    AccountId id;                      ///< Unique account identifier
    std::string email;                 ///< Account email
    std::string url;                   ///< Server URL
    std::optional<OrgId> active_org_id;///< Active organization ID
    
    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
    
    /// Deserialize from JSON
    static AccountInfo from_json(const nlohmann::json& j);
    
    /// Equality comparison
    bool operator==(const AccountInfo& other) const noexcept;
};

/// Organization information
struct TURBOT_CORE_API OrgInfo {
    OrgId id;          ///< Organization ID
    std::string name;  ///< Organization name
    
    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
    
    /// Deserialize from JSON
    static OrgInfo from_json(const nlohmann::json& j);
    
    /// Equality comparison
    bool operator==(const OrgInfo& other) const noexcept;
};

/// Login session information
struct TURBOT_CORE_API LoginSession {
    DeviceCode device_code;           ///< Device code for polling
    UserCode user_code;               ///< User code to display
    std::string url;                  ///< URL for user to visit
    std::string server;               ///< Server URL
    int64_t expiry_seconds = 0;       ///< Expiry duration in seconds
    int64_t interval_seconds = 5;     ///< Poll interval in seconds
    
    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
    
    /// Deserialize from JSON
    static LoginSession from_json(const nlohmann::json& j);
};

/// Poll result types
enum class TURBOT_CORE_API PollResultType {
    Success,   ///< Login successful
    Pending,   ///< Still waiting
    Slow,      ///< Slow down polling
    Expired,   ///< Device code expired
    Denied,    ///< Authorization denied
    Error      ///< Error occurred
};

/// Poll result
struct TURBOT_CORE_API PollResult {
    PollResultType type = PollResultType::Pending;
    std::optional<std::string> email;    ///< Email on success
    std::optional<std::string> error;    ///< Error message
    
    /// Check if successful
    [[nodiscard]] bool is_success() const noexcept { return type == PollResultType::Success; }
    
    /// Check if still pending
    [[nodiscard]] bool is_pending() const noexcept { return type == PollResultType::Pending; }
};

/// Organization grouped by account
struct TURBOT_CORE_API AccountOrgGroup {
    AccountInfo account;         ///< Account info
    std::vector<OrgInfo> orgs;   ///< Organizations for this account
    
    /// Serialize to JSON
    [[nodiscard]] nlohmann::json to_json() const;
};

/// Account service interface
/// 
/// Provides account management capabilities including:
/// - Device code flow authentication
/// - Account and organization management
/// - Token storage and refresh
class TURBOT_CORE_API AccountService {
public:
    /// Get the singleton instance
    static AccountService& instance();
    
    /// Start device code login flow
    /// @param server_url Server URL to authenticate with
    /// @return Login session with device code
    [[nodiscard]] std::optional<LoginSession> login(const std::string& server_url);
    
    /// Poll for login completion
    /// @param session Login session from login()
    /// @return Poll result
    [[nodiscard]] PollResult poll(const LoginSession& session);
    
    /// List all accounts
    /// @return Vector of account info
    [[nodiscard]] std::vector<AccountInfo> list() const;
    
    /// Get active account
    /// @return Active account or nullopt
    [[nodiscard]] std::optional<AccountInfo> active() const;
    
    /// Set active account
    /// @param account_id Account ID
    /// @param org_id Optional organization ID
    /// @return true if successful
    bool use(const AccountId& account_id, const std::optional<OrgId>& org_id = std::nullopt);
    
    /// Remove an account
    /// @param account_id Account ID
    /// @return true if removed
    bool remove(const AccountId& account_id);
    
    /// Get organizations for an account
    /// @param account_id Account ID
    /// @return Vector of organizations
    [[nodiscard]] std::vector<OrgInfo> orgs(const AccountId& account_id) const;
    
    /// Get organizations grouped by account
    /// @return Vector of account-org groups
    [[nodiscard]] std::vector<AccountOrgGroup> orgs_by_account() const;
    
    /// Refresh access token
    /// @param account_id Account ID
    /// @return true if refreshed
    bool refresh_token(const AccountId& account_id);
    
    /// Get access token for an account
    /// @param account_id Account ID
    /// @return Access token or nullopt
    [[nodiscard]] std::optional<AccessToken> get_access_token(const AccountId& account_id) const;
    
private:
    AccountService();
    ~AccountService();
    
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace turbot::core::account
