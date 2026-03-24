// account_cmd.cpp - CLI command to manage accounts
// Aligns with OpenCode `opencode console login/logout/switch` commands

#include <turbot/core/common/logger.hpp>
#include <turbot/core/account/account.hpp>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

namespace turbot::cli {

namespace account = core::account;

/// Format account label for display
static std::string format_account_label(const account::AccountInfo& acc, bool is_active) {
    std::string result = fmt::format("{} ({})", acc.email, acc.url);
    if (is_active) {
        result += " (active)";
    }
    return result;
}

/// Format org line for display
static std::string format_org_line(const account::AccountInfo& acc, 
                                   const account::OrgInfo& org, 
                                   bool is_active) {
    std::string dot = is_active ? "● " : "  ";
    std::string name = is_active ? fmt::format("\033[1m{}\033[0m", org.name) : org.name;
    return fmt::format("  {}{}  {}  {}  {}", dot, name, acc.email, acc.url, org.id);
}

/// Login to a server
int login_account(const std::string& server_url) {
    auto& service = account::AccountService::instance();
    
    // Start login flow
    auto session_opt = service.login(server_url);
    if (!session_opt) {
        fmt::print(stderr, "Failed to start login flow.\n");
        return 1;
    }
    
    const auto& session = *session_opt;
    
    fmt::print("Log in to {}\n\n", server_url);
    fmt::print("Go to: {}\n", session.url);
    fmt::print("Enter code: {}\n\n", session.user_code);
    fmt::print("Waiting for authorization...\n");
    
    // Poll for completion
    int64_t elapsed = 0;
    while (elapsed < session.expiry_seconds) {
        std::this_thread::sleep_for(std::chrono::seconds(session.interval_seconds));
        elapsed += session.interval_seconds;
        
        auto result = service.poll(session);
        
        if (result.is_success()) {
            fmt::print("\nLogged in as {}!\n", result.email.value_or("unknown"));
            return 0;
        }
        
        if (result.type == account::PollResultType::Denied) {
            fmt::print(stderr, "\nAuthorization denied.\n");
            return 1;
        }
        
        if (result.type == account::PollResultType::Expired) {
            fmt::print(stderr, "\nDevice code expired.\n");
            return 1;
        }
        
        if (result.type == account::PollResultType::Error) {
            fmt::print(stderr, "\nError: {}\n", result.error.value_or("unknown"));
            return 1;
        }
        
        // Show progress
        fmt::print(".");
        std::cout.flush();
    }
    
    fmt::print(stderr, "\nLogin timed out.\n");
    return 1;
}

/// Logout from an account
int logout_account(const std::string& email) {
    auto& service = account::AccountService::instance();
    auto accounts = service.list();
    
    if (accounts.empty()) {
        fmt::print("Not logged in.\n");
        return 0;
    }
    
    if (!email.empty()) {
        // Find and remove by email
        for (const auto& acc : accounts) {
            if (acc.email == email) {
                service.remove(acc.id);
                fmt::print("Logged out from {}.\n", email);
                return 0;
            }
        }
        fmt::print(stderr, "Account not found: {}\n", email);
        return 1;
    }
    
    // Show all accounts
    auto active = service.active();
    
    fmt::print("Accounts:\n");
    for (size_t i = 0; i < accounts.size(); ++i) {
        bool is_active = active && active->id == accounts[i].id;
        fmt::print("  {}. {}\n", i + 1, format_account_label(accounts[i], is_active));
    }
    
    fmt::print("\nUse 'turbot-cli account logout <email>' to logout from a specific account.\n");
    return 0;
}

/// List accounts
int list_accounts() {
    auto& service = account::AccountService::instance();
    auto accounts = service.list();
    
    if (accounts.empty()) {
        fmt::print("No accounts logged in.\n");
        fmt::print("Use 'turbot-cli account login <server>' to log in.\n");
        return 0;
    }
    
    auto active = service.active();
    
    fmt::print("Accounts:\n\n");
    fmt::print("{:<30}  {:<30}  {}\n", "Email", "Server", "Status");
    fmt::print("{}\n", std::string(80, '-'));
    
    for (const auto& acc : accounts) {
        bool is_active = active && active->id == acc.id;
        fmt::print("{:<30}  {:<30}  {}\n", 
                   acc.email.substr(0, 29),
                   acc.url.substr(0, 29),
                   is_active ? "active" : "");
    }
    
    fmt::print("\n{} account(s).\n", accounts.size());
    return 0;
}

/// Switch active account
int switch_account(const std::string& email, const std::string& org_id) {
    auto& service = account::AccountService::instance();
    auto accounts = service.list();
    
    if (accounts.empty()) {
        fmt::print("No accounts logged in.\n");
        return 1;
    }
    
    // Find account by email
    for (const auto& acc : accounts) {
        if (acc.email == email) {
            std::optional<std::string> org;
            if (!org_id.empty()) {
                org = org_id;
            }
            
            if (service.use(acc.id, org)) {
                fmt::print("Switched to {} org: {}\n", email, org_id.empty() ? "(default)" : org_id);
                return 0;
            }
        }
    }
    
    fmt::print(stderr, "Account not found: {}\n", email);
    return 1;
}

/// List organizations
int list_orgs() {
    auto& service = account::AccountService::instance();
    auto groups = service.orgs_by_account();
    
    if (groups.empty()) {
        fmt::print("No accounts found.\n");
        return 0;
    }
    
    bool has_orgs = false;
    for (const auto& group : groups) {
        if (!group.orgs.empty()) {
            has_orgs = true;
            break;
        }
    }
    
    if (!has_orgs) {
        fmt::print("No organizations found.\n");
        return 0;
    }
    
    auto active = service.active();
    
    fmt::print("Organizations:\n\n");
    
    for (const auto& group : groups) {
        for (const auto& org : group.orgs) {
            bool is_active = active && 
                            active->id == group.account.id && 
                            active->active_org_id == org.id;
            fmt::print("{}\n", format_org_line(group.account, org, is_active));
        }
    }
    
    return 0;
}

/// Show account status
int show_account_status() {
    auto& service = account::AccountService::instance();
    auto active = service.active();
    
    if (!active) {
        fmt::print("No active account.\n");
        fmt::print("Use 'turbot-cli account login <server>' to log in.\n");
        return 0;
    }
    
    fmt::print("Active Account:\n");
    fmt::print("{}\n", std::string(60, '-'));
    fmt::print("  Email:  {}\n", active->email);
    fmt::print("  Server: {}\n", active->url);
    fmt::print("  ID:     {}\n", active->id);
    
    if (active->active_org_id) {
        auto orgs = service.orgs(active->id);
        for (const auto& org : orgs) {
            if (org.id == *active->active_org_id) {
                fmt::print("  Org:    {} ({})\n", org.name, org.id);
                break;
            }
        }
    }
    
    return 0;
}

} // namespace turbot::cli
