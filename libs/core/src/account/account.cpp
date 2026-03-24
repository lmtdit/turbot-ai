// account.cpp - Account management implementation
// Aligns with OpenCode Account module capability

#include <turbot/core/account/account.hpp>
#include <turbot/core/common/logger.hpp>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

namespace turbot::core::account {

namespace fs = std::filesystem;

// ============================================================================
// AccountInfo
// ============================================================================

nlohmann::json AccountInfo::to_json() const {
    nlohmann::json j = nlohmann::json::object();
    j["id"] = id;
    j["email"] = email;
    j["url"] = url;
    if (active_org_id) j["active_org_id"] = *active_org_id;
    return j;
}

AccountInfo AccountInfo::from_json(const nlohmann::json& j) {
    AccountInfo info;
    info.id = j.value("id", std::string{});
    info.email = j.value("email", std::string{});
    info.url = j.value("url", std::string{});
    if (j.contains("active_org_id") && !j["active_org_id"].is_null()) {
        info.active_org_id = j["active_org_id"].get<std::string>();
    }
    return info;
}

bool AccountInfo::operator==(const AccountInfo& other) const noexcept {
    return id == other.id && email == other.email && 
           url == other.url && active_org_id == other.active_org_id;
}

// ============================================================================
// OrgInfo
// ============================================================================

nlohmann::json OrgInfo::to_json() const {
    return {{"id", id}, {"name", name}};
}

OrgInfo OrgInfo::from_json(const nlohmann::json& j) {
    OrgInfo org;
    org.id = j.value("id", std::string{});
    org.name = j.value("name", std::string{});
    return org;
}

bool OrgInfo::operator==(const OrgInfo& other) const noexcept {
    return id == other.id && name == other.name;
}

// ============================================================================
// LoginSession
// ============================================================================

nlohmann::json LoginSession::to_json() const {
    return {
        {"device_code", device_code},
        {"user_code", user_code},
        {"url", url},
        {"server", server},
        {"expiry_seconds", expiry_seconds},
        {"interval_seconds", interval_seconds}
    };
}

LoginSession LoginSession::from_json(const nlohmann::json& j) {
    LoginSession session;
    session.device_code = j.value("device_code", std::string{});
    session.user_code = j.value("user_code", std::string{});
    session.url = j.value("url", std::string{});
    session.server = j.value("server", std::string{});
    session.expiry_seconds = j.value("expiry_seconds", int64_t{0});
    session.interval_seconds = j.value("interval_seconds", int64_t{5});
    return session;
}

// ============================================================================
// AccountOrgGroup
// ============================================================================

nlohmann::json AccountOrgGroup::to_json() const {
    nlohmann::json orgs_json = nlohmann::json::array();
    for (const auto& org : orgs) {
        orgs_json.push_back(org.to_json());
    }
    return {
        {"account", account.to_json()},
        {"orgs", orgs_json}
    };
}

// ============================================================================
// AccountService::Impl
// ============================================================================

struct AccountService::Impl {
    std::string config_path;
    std::vector<AccountInfo> accounts;
    std::optional<AccountId> active_account_id;
    std::map<AccountId, std::vector<OrgInfo>> account_orgs;
    std::map<AccountId, AccessToken> access_tokens;
    std::map<AccountId, RefreshToken> refresh_tokens;
    
    Impl() {
        // Get config path from environment or default
        const char* home = std::getenv("HOME");
        if (home) {
            config_path = fmt::format("{}/.turbot/accounts.json", home);
        } else {
            config_path = ".turbot/accounts.json";
        }
        load_accounts();
    }
    
    void load_accounts() {
        if (!fs::exists(config_path)) {
            return;
        }
        
        std::ifstream f(config_path);
        if (!f.is_open()) {
            return;
        }
        
        try {
            nlohmann::json config = nlohmann::json::parse(f);
            
            if (config.contains("accounts")) {
                for (const auto& j : config["accounts"]) {
                    accounts.push_back(AccountInfo::from_json(j));
                }
            }
            
            if (config.contains("active_account")) {
                active_account_id = config["active_account"].get<std::string>();
            }
            
            if (config.contains("orgs")) {
                for (const auto& [account_id, orgs_json] : config["orgs"].items()) {
                    std::vector<OrgInfo> org_list;
                    for (const auto& org_j : orgs_json) {
                        org_list.push_back(OrgInfo::from_json(org_j));
                    }
                    account_orgs[account_id] = std::move(org_list);
                }
            }
            
            if (config.contains("tokens")) {
                for (const auto& [account_id, token_j] : config["tokens"].items()) {
                    if (token_j.contains("access")) {
                        access_tokens[account_id] = token_j["access"].get<std::string>();
                    }
                    if (token_j.contains("refresh")) {
                        refresh_tokens[account_id] = token_j["refresh"].get<std::string>();
                    }
                }
            }
        } catch (const std::exception& e) {
            TURBOT_LOG_WARN("Failed to load accounts: {}", e.what());
        }
    }
    
    void save_accounts() {
        fs::path p(config_path);
        if (p.has_parent_path() && !fs::exists(p.parent_path())) {
            std::error_code ec;
            fs::create_directories(p.parent_path(), ec);
        }
        
        nlohmann::json config = nlohmann::json::object();
        
        nlohmann::json accounts_json = nlohmann::json::array();
        for (const auto& acc : accounts) {
            accounts_json.push_back(acc.to_json());
        }
        config["accounts"] = accounts_json;
        
        if (active_account_id) {
            config["active_account"] = *active_account_id;
        }
        
        nlohmann::json orgs_json = nlohmann::json::object();
        for (const auto& [account_id, orgs] : account_orgs) {
            nlohmann::json org_list = nlohmann::json::array();
            for (const auto& org : orgs) {
                org_list.push_back(org.to_json());
            }
            orgs_json[account_id] = org_list;
        }
        config["orgs"] = orgs_json;
        
        nlohmann::json tokens_json = nlohmann::json::object();
        for (const auto& [account_id, token] : access_tokens) {
            tokens_json[account_id]["access"] = token;
        }
        for (const auto& [account_id, token] : refresh_tokens) {
            tokens_json[account_id]["refresh"] = token;
        }
        config["tokens"] = tokens_json;
        
        std::ofstream f(config_path);
        if (f.is_open()) {
            f << config.dump(2) << "\n";
        }
    }
    
    std::string generate_id() {
        static const char* chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 35);
        
        std::string id;
        for (int i = 0; i < 16; ++i) {
            id += chars[dis(gen)];
        }
        return id;
    }
};

// ============================================================================
// AccountService
// ============================================================================

AccountService::AccountService() : impl_(std::make_unique<Impl>()) {}

AccountService::~AccountService() = default;

AccountService& AccountService::instance() {
    static AccountService instance;
    return instance;
}

std::optional<LoginSession> AccountService::login(const std::string& server_url) {
    // In production, this would call the OAuth device code endpoint
    // For now, return a mock session
    TURBOT_LOG_INFO("Starting login flow for: {}", server_url);
    
    LoginSession session;
    session.device_code = impl_->generate_id();
    session.user_code = impl_->generate_id();
    session.url = fmt::format("{}/login?code={}", server_url, session.user_code);
    session.server = server_url;
    session.expiry_seconds = 300;
    session.interval_seconds = 5;
    
    return session;
}

PollResult AccountService::poll(const LoginSession& session) {
    // In production, this would poll the OAuth token endpoint
    // For now, return pending
    TURBOT_LOG_DEBUG("Polling for login completion: {}", session.device_code);
    
    PollResult result;
    result.type = PollResultType::Pending;
    return result;
}

std::vector<AccountInfo> AccountService::list() const {
    return impl_->accounts;
}

std::optional<AccountInfo> AccountService::active() const {
    if (!impl_->active_account_id) {
        return std::nullopt;
    }
    
    for (const auto& acc : impl_->accounts) {
        if (acc.id == *impl_->active_account_id) {
            return acc;
        }
    }
    
    return std::nullopt;
}

bool AccountService::use(const AccountId& account_id, const std::optional<OrgId>& org_id) {
    bool found = false;
    for (auto& acc : impl_->accounts) {
        if (acc.id == account_id) {
            acc.active_org_id = org_id;
            found = true;
            break;
        }
    }
    
    if (found) {
        impl_->active_account_id = account_id;
        impl_->save_accounts();
        TURBOT_LOG_INFO("Switched to account: {} org: {}", 
                       account_id, org_id.value_or("(default)"));
    }
    
    return found;
}

bool AccountService::remove(const AccountId& account_id) {
    auto it = std::remove_if(impl_->accounts.begin(), impl_->accounts.end(),
        [&](const AccountInfo& acc) { return acc.id == account_id; });
    
    if (it != impl_->accounts.end()) {
        impl_->accounts.erase(it, impl_->accounts.end());
        impl_->account_orgs.erase(account_id);
        impl_->access_tokens.erase(account_id);
        impl_->refresh_tokens.erase(account_id);
        
        if (impl_->active_account_id == account_id) {
            impl_->active_account_id = std::nullopt;
        }
        
        impl_->save_accounts();
        TURBOT_LOG_INFO("Removed account: {}", account_id);
        return true;
    }
    
    return false;
}

std::vector<OrgInfo> AccountService::orgs(const AccountId& account_id) const {
    auto it = impl_->account_orgs.find(account_id);
    if (it != impl_->account_orgs.end()) {
        return it->second;
    }
    return {};
}

std::vector<AccountOrgGroup> AccountService::orgs_by_account() const {
    std::vector<AccountOrgGroup> result;
    
    for (const auto& acc : impl_->accounts) {
        AccountOrgGroup group;
        group.account = acc;
        
        auto it = impl_->account_orgs.find(acc.id);
        if (it != impl_->account_orgs.end()) {
            group.orgs = it->second;
        }
        
        result.push_back(std::move(group));
    }
    
    return result;
}

bool AccountService::refresh_token(const AccountId& account_id) {
    auto it = impl_->refresh_tokens.find(account_id);
    if (it == impl_->refresh_tokens.end()) {
        return false;
    }
    
    // In production, this would call the OAuth refresh endpoint
    TURBOT_LOG_INFO("Refreshing token for account: {}", account_id);
    return true;
}

std::optional<AccessToken> AccountService::get_access_token(const AccountId& account_id) const {
    auto it = impl_->access_tokens.find(account_id);
    if (it != impl_->access_tokens.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace turbot::core::account
