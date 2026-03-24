// account.cpp - Account management implementation
// Aligns with OpenCode Account module capability

#include <turbot/core/account/account.hpp>
#include <turbot/core/account/account_store.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/network/http_client.hpp>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <chrono>
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
    // G07: POST {server}/auth/device/code with {"client_id":"opencode"}
    // Aligned with OpenCode Account.login(server) device code flow.
    TURBOT_LOG_INFO("Starting login flow for: {}", server_url);

    try {
        turbot::network::HttpClient http;
        nlohmann::json req_body = {{"client_id", "opencode"}};
        const std::string endpoint = server_url + "/auth/device/code";

        auto resp = http.post_json(endpoint, req_body.dump());
        if (!resp.is_success()) {
            TURBOT_LOG_ERROR("login(): POST {} failed with status {}",
                             endpoint, resp.status_code);
            return std::nullopt;
        }

        auto j = nlohmann::json::parse(resp.body);

        LoginSession session;
        session.device_code = j.value("device_code", std::string{});
        session.user_code   = j.value("user_code",   std::string{});
        // OpenCode returns verification_uri_complete (full URL); fall back to verification_uri.
        std::string verify_uri = j.contains("verification_uri_complete")
            ? j["verification_uri_complete"].get<std::string>()
            : j.value("verification_uri", std::string{});
        session.url = verify_uri;
        session.server          = server_url;
        session.expiry_seconds  = j.value("expires_in", int64_t{300});
        session.interval_seconds= j.value("interval",   int64_t{5});

        return session;
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("login() exception: {}", e.what());
        return std::nullopt;
    }
}

PollResult AccountService::poll(const LoginSession& session) {
    // G08: POST {server}/auth/device/token to check authorization status.
    // Aligned with OpenCode Account.poll(loginSession) device token flow.
    TURBOT_LOG_DEBUG("Polling for login completion: {}", session.device_code);

    try {
        turbot::network::HttpClient http;
        nlohmann::json req_body = {
            {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"},
            {"device_code", session.device_code},
            {"client_id",   "opencode"}
        };
        const std::string endpoint = session.server + "/auth/device/token";

        auto resp = http.post_json(endpoint, req_body.dump());

        // Both success and pending responses may arrive as 200 or 4xx depending
        // on server implementation; always try to parse the body for the status.
        auto j = nlohmann::json::parse(resp.body);

        if (j.contains("access_token") && !j["access_token"].is_null()) {
            // ── Success path ──────────────────────────────────────────────
            PollResult result;
            result.type = PollResultType::Success;

            std::string at = j.value("access_token",  std::string{});
            std::string rt = j.value("refresh_token", std::string{});
            int64_t expires_in = j.value("expires_in", int64_t{3600});

            // Compute expiry as ms since epoch
            const int64_t expiry_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count() + expires_in * 1000LL;

            // Persist to SQLite account_state
            auto& store = AccountStore::instance();
            if (store.is_initialized()) {
                AccountState state;
                state.account_id    = session.server;  // placeholder until fetchUser()
                state.access_token  = at;
                state.refresh_token = rt;
                state.token_expiry  = expiry_ms;
                if (!store.save_state(state)) {
                    TURBOT_LOG_WARN("poll(): failed to persist token state to SQLite");
                }
            }

            // Also update in-memory token maps for backward compat.
            impl_->access_tokens[session.server]  = at;
            impl_->refresh_tokens[session.server] = rt;
            impl_->save_accounts();

            return result;
        }

        // ── Error / pending path ──────────────────────────────────────────
        const std::string error = j.value("error", std::string{});
        if (error == "authorization_pending") return PollResult{.type = PollResultType::Pending};
        if (error == "slow_down")             return PollResult{.type = PollResultType::Slow};
        if (error == "expired_token")         return PollResult{.type = PollResultType::Expired};
        if (error == "access_denied")         return PollResult{.type = PollResultType::Denied};

        // Unknown error
        TURBOT_LOG_WARN("poll(): unknown error from server: {}", error);
        return PollResult{.type = PollResultType::Error};

    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("poll() exception: {}", e.what());
        return PollResult{.type = PollResultType::Error};
    }
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
    // G09 (sub-04): Implement actual OAuth2 token refresh via HTTP.
    //
    // The account_id doubles as the server URL in this implementation
    // (set during poll() as session.server placeholder).
    // Endpoint: POST {server}/auth/device/token
    // Body:     { "grant_type": "refresh_token",
    //             "refresh_token": "<token>",
    //             "client_id": "opencode" }

    // ── Step 1: resolve the refresh token ────────────────────────────────
    std::string rt_value;

    // Try in-memory map first (backward compat with JSON-loaded accounts)
    auto it = impl_->refresh_tokens.find(account_id);
    if (it != impl_->refresh_tokens.end()) {
        rt_value = it->second;
    } else {
        // Fall back to SQLite account_state singleton
        auto& store = AccountStore::instance();
        if (!store.is_initialized()) {
            TURBOT_LOG_WARN("refresh_token: no store available for {}", account_id);
            return false;
        }
        auto state = store.load_state();
        if (!state || !state->refresh_token) {
            TURBOT_LOG_WARN("refresh_token: no refresh_token found for {}", account_id);
            return false;
        }
        rt_value = *state->refresh_token;
    }

    // ── Step 2: POST to the refresh endpoint ─────────────────────────────
    const std::string endpoint = account_id + "/auth/device/token";
    TURBOT_LOG_INFO("refresh_token: posting to {} for account {}", endpoint, account_id);

    try {
        turbot::network::HttpClient http;
        nlohmann::json req_body = {
            {"grant_type",    "refresh_token"},
            {"refresh_token", rt_value},
            {"client_id",     "opencode"}
        };

        auto resp = http.post_json(endpoint, req_body.dump());
        if (!resp.is_success()) {
            TURBOT_LOG_ERROR("refresh_token: POST {} returned status {}",
                             endpoint, resp.status_code);
            return false;
        }

        auto j = nlohmann::json::parse(resp.body);

        if (!j.contains("access_token") || j["access_token"].is_null()) {
            const std::string err = j.value("error", std::string{});
            TURBOT_LOG_ERROR("refresh_token: no access_token in response (error={})", err);
            return false;
        }

        // ── Step 3: persist new tokens ───────────────────────────────────
        const std::string new_at = j.value("access_token",  std::string{});
        const std::string new_rt = j.value("refresh_token", rt_value); // keep old if not rotated
        const int64_t expires_in = j.value("expires_in", int64_t{3600});

        const int64_t expiry_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count() + expires_in * 1000LL;

        // Update in-memory token maps
        impl_->access_tokens[account_id]  = new_at;
        impl_->refresh_tokens[account_id] = new_rt;
        impl_->save_accounts();

        // Update SQLite account_state
        auto& store = AccountStore::instance();
        if (store.is_initialized()) {
            AccountState state;
            state.account_id    = account_id;
            state.access_token  = new_at;
            state.refresh_token = new_rt;
            state.token_expiry  = expiry_ms;
            if (!store.save_state(state)) {
                TURBOT_LOG_WARN("refresh_token: failed to persist new tokens to SQLite");
            }
        }

        TURBOT_LOG_INFO("refresh_token: successfully refreshed token for {}", account_id);
        return true;

    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("refresh_token: exception: {}", e.what());
        return false;
    }
}

std::optional<AccessToken> AccountService::get_access_token(const AccountId& account_id) const {
    auto it = impl_->access_tokens.find(account_id);
    if (it != impl_->access_tokens.end()) {
        return it->second;
    }
    // Fallback: check SQLite account_state
    auto& store = AccountStore::instance();
    if (store.is_initialized()) {
        auto state = store.load_state();
        if (state && state->account_id == account_id && state->access_token)
            return *state->access_token;
    }
    return std::nullopt;
}

// ─── resolve_token (G09) ──────────────────────────────────────────────────────

std::optional<AccessToken> AccountService::resolve_token(const AccountId& account_id) {
    // Aligned with OpenCode Account.resolveToken(accountID):
    //   if (token_expiry <= now + 60s) → refresh first
    //   else → return access_token directly

    auto& store = AccountStore::instance();
    if (store.is_initialized()) {
        auto state = store.load_state();
        if (state && state->access_token) {
            const int64_t now_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count();
            const int64_t grace_ms = 60LL * 1000LL;  // 60-second safety buffer

            if (state->token_expiry && *state->token_expiry <= now_ms + grace_ms) {
                // Token expired or about to expire — attempt refresh
                TURBOT_LOG_INFO("resolve_token: token near expiry; refreshing for {}", account_id);
                if (refresh_token(account_id)) {
                    // Re-read after refresh
                    state = store.load_state();
                } else {
                    TURBOT_LOG_WARN("resolve_token: refresh failed for {}", account_id);
                    return std::nullopt;
                }
            }
            if (state && state->access_token)
                return *state->access_token;
        }
    }

    // Fallback: in-memory map (used when store is not initialised)
    return get_access_token(account_id);
}

// ─── init_store (G06) ─────────────────────────────────────────────────────────

void AccountService::init_store(std::shared_ptr<turbot::storage::Database> db) {
    AccountStore::instance().init(std::move(db));

    // G06: Migrate existing in-memory accounts to SQLite on first init.
    auto& store = AccountStore::instance();
    for (const auto& acc : impl_->accounts) {
        if (!store.save_account(acc.id, acc.email, acc.url)) {
            TURBOT_LOG_WARN("init_store: failed to migrate account {} to SQLite", acc.id);
        }
    }
    // Migrate active token to account_state if available
    if (impl_->active_account_id) {
        AccountState state;
        state.account_id = impl_->active_account_id;
        auto at_it = impl_->access_tokens.find(*impl_->active_account_id);
        if (at_it != impl_->access_tokens.end())
            state.access_token = at_it->second;
        auto rt_it = impl_->refresh_tokens.find(*impl_->active_account_id);
        if (rt_it != impl_->refresh_tokens.end())
            state.refresh_token = rt_it->second;
        if (state.access_token || state.refresh_token) {
            if (!store.save_state(state)) {
                TURBOT_LOG_WARN("init_store: failed to migrate token state to SQLite");
            }
        }
    }
}

} // namespace turbot::core::account
