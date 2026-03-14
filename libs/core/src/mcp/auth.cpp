#include <turbot/core/mcp/auth.hpp>
#include <turbot/core/common/logger.hpp>

#include <nlohmann/json.hpp>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <optional>
#include <pwd.h>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace turbot::core::mcp {

namespace {

/// Current Unix timestamp (seconds)
[[nodiscard]] int64_t now_seconds() noexcept {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

/// Get home directory (prefer $HOME, fallback to getpwuid)
[[nodiscard]] std::string home_dir() {
    const char* home = ::getenv("HOME");
    if (home && home[0] != '\0') return home;
    struct passwd* pw = ::getpwuid(::getuid());
    if (pw && pw->pw_dir) return pw->pw_dir;
    throw std::runtime_error("McpAuth: cannot determine home directory");
}

}  // namespace

// ─── Tokens JSON ──────────────────────────────────────────────────────────────

nlohmann::json Tokens::to_json() const {
    nlohmann::json j;
    j["accessToken"] = access_token;
    if (refresh_token) j["refreshToken"] = *refresh_token;
    if (expires_at)   j["expiresAt"]   = *expires_at;
    if (scope)        j["scope"]       = *scope;
    return j;
}

Tokens Tokens::from_json(const nlohmann::json& j) {
    Tokens t;
    t.access_token = j.value("accessToken", std::string{});
    if (j.contains("refreshToken") && !j["refreshToken"].is_null())
        t.refresh_token = j["refreshToken"].get<std::string>();
    if (j.contains("expiresAt") && j["expiresAt"].is_number())
        t.expires_at = j["expiresAt"].get<int64_t>();
    if (j.contains("scope") && !j["scope"].is_null())
        t.scope = j["scope"].get<std::string>();
    return t;
}

// ─── ClientInfo JSON ──────────────────────────────────────────────────────────

nlohmann::json ClientInfo::to_json() const {
    nlohmann::json j;
    j["clientId"] = client_id;
    if (client_secret)             j["clientSecret"]            = *client_secret;
    if (client_id_issued_at)       j["clientIdIssuedAt"]        = *client_id_issued_at;
    if (client_secret_expires_at)  j["clientSecretExpiresAt"]   = *client_secret_expires_at;
    return j;
}

ClientInfo ClientInfo::from_json(const nlohmann::json& j) {
    ClientInfo c;
    c.client_id = j.value("clientId", std::string{});
    if (j.contains("clientSecret") && !j["clientSecret"].is_null())
        c.client_secret = j["clientSecret"].get<std::string>();
    if (j.contains("clientIdIssuedAt") && j["clientIdIssuedAt"].is_number())
        c.client_id_issued_at = j["clientIdIssuedAt"].get<int64_t>();
    if (j.contains("clientSecretExpiresAt") && j["clientSecretExpiresAt"].is_number())
        c.client_secret_expires_at = j["clientSecretExpiresAt"].get<int64_t>();
    return c;
}

// ─── AuthEntry JSON ───────────────────────────────────────────────────────────

nlohmann::json AuthEntry::to_json() const {
    nlohmann::json j = nlohmann::json::object();
    if (tokens)       j["tokens"]      = tokens->to_json();
    if (client_info)  j["clientInfo"]  = client_info->to_json();
    if (code_verifier) j["codeVerifier"] = *code_verifier;
    if (oauth_state)  j["oauthState"]  = *oauth_state;
    if (server_url)   j["serverUrl"]   = *server_url;
    return j;
}

AuthEntry AuthEntry::from_json(const nlohmann::json& j) {
    AuthEntry e;
    if (j.contains("tokens") && j["tokens"].is_object())
        e.tokens = Tokens::from_json(j["tokens"]);
    if (j.contains("clientInfo") && j["clientInfo"].is_object())
        e.client_info = ClientInfo::from_json(j["clientInfo"]);
    if (j.contains("codeVerifier") && !j["codeVerifier"].is_null())
        e.code_verifier = j["codeVerifier"].get<std::string>();
    if (j.contains("oauthState") && !j["oauthState"].is_null())
        e.oauth_state = j["oauthState"].get<std::string>();
    if (j.contains("serverUrl") && !j["serverUrl"].is_null())
        e.server_url = j["serverUrl"].get<std::string>();
    return e;
}

// ─── McpAuth private helpers ──────────────────────────────────────────────────

std::string McpAuth::auth_file_path() {
    return home_dir() + "/.turbot/mcp-auth.json";
}

nlohmann::json McpAuth::read_all() {
    const std::string path = auth_file_path();
    std::ifstream f(path);
    if (!f.is_open()) return nlohmann::json::object();
    try {
        nlohmann::json data;
        f >> data;
        if (data.is_object()) return data;
    } catch (...) {
        // Corrupt file: treat as empty
        TURBOT_LOG_WARN("McpAuth: corrupt auth file, treating as empty: {}", path);
    }
    return nlohmann::json::object();
}

void McpAuth::write_all(const nlohmann::json& data) {
    const std::string path = auth_file_path();

    // Ensure parent directory exists
    const std::string dir = path.substr(0, path.rfind('/'));
    ::mkdir(dir.c_str(), 0700);

    // Write to temp file then rename (atomic write)
    const std::string tmp_path = path + ".tmp";
    {
        std::ofstream f(tmp_path, std::ios::out | std::ios::trunc);
        if (!f.is_open()) {
            throw std::runtime_error("McpAuth: cannot write to " + tmp_path + ": " + strerror(errno));
        }
        f << data.dump(2);
        if (!f) {
            ::remove(tmp_path.c_str());
            throw std::runtime_error("McpAuth: write failed for " + tmp_path);
        }
    }  // flush + close

    // chmod 0600 on temp file before rename
    if (::chmod(tmp_path.c_str(), 0600) != 0) {
        TURBOT_LOG_WARN("McpAuth: chmod 0600 failed for {}: {}", tmp_path, strerror(errno));
    }

    // Atomic rename
    if (::rename(tmp_path.c_str(), path.c_str()) != 0) {
        ::remove(tmp_path.c_str());  // cleanup on rename failure
        throw std::runtime_error("McpAuth: rename failed " + tmp_path + " -> " + path + ": " + strerror(errno));
    }
}

std::string McpAuth::redact_token(const std::string& token) {
    if (token.size() <= 4) return "****";
    return token.substr(0, 4) + "****";
}

// ─── McpAuth public API ───────────────────────────────────────────────────────

std::optional<AuthEntry> McpAuth::get(const std::string& mcp_name) {
    const auto data = read_all();
    if (!data.contains(mcp_name) || !data[mcp_name].is_object()) return std::nullopt;
    return AuthEntry::from_json(data[mcp_name]);
}

std::optional<AuthEntry> McpAuth::get_for_url(
    const std::string& mcp_name,
    const std::string& server_url
) {
    const auto entry = get(mcp_name);
    if (!entry) return std::nullopt;

    // No serverUrl stored → old version credentials, consider invalid
    if (!entry->server_url) return std::nullopt;

    // URL changed → credentials are invalid
    if (*entry->server_url != server_url) return std::nullopt;

    return entry;
}

void McpAuth::set(
    const std::string& mcp_name,
    const AuthEntry& entry,
    const std::optional<std::string>& server_url
) {
    auto data = read_all();
    AuthEntry e = entry;
    if (server_url) e.server_url = server_url;
    data[mcp_name] = e.to_json();
    write_all(data);
}

void McpAuth::remove(const std::string& mcp_name) {
    auto data = read_all();
    data.erase(mcp_name);
    write_all(data);
}

void McpAuth::update_tokens(
    const std::string& mcp_name,
    const Tokens& tokens,
    const std::optional<std::string>& server_url
) {
    auto entry = get(mcp_name).value_or(AuthEntry{});
    entry.tokens = tokens;
    set(mcp_name, entry, server_url);

    TURBOT_LOG_DEBUG("McpAuth: updated tokens for {} (access={})", mcp_name, redact_token(tokens.access_token));
}

void McpAuth::update_client_info(
    const std::string& mcp_name,
    const ClientInfo& info,
    const std::optional<std::string>& server_url
) {
    auto entry = get(mcp_name).value_or(AuthEntry{});
    entry.client_info = info;
    set(mcp_name, entry, server_url);

    TURBOT_LOG_DEBUG("McpAuth: updated client_info for {} (client_id={})", mcp_name, info.client_id);
}

void McpAuth::update_code_verifier(const std::string& mcp_name, const std::string& verifier) {
    auto entry = get(mcp_name).value_or(AuthEntry{});
    entry.code_verifier = verifier;
    set(mcp_name, entry);
}

void McpAuth::update_oauth_state(const std::string& mcp_name, const std::string& state) {
    auto entry = get(mcp_name).value_or(AuthEntry{});
    entry.oauth_state = state;
    set(mcp_name, entry);
}

void McpAuth::clear_oauth_state(const std::string& mcp_name) {
    auto entry = get(mcp_name);
    if (entry) {
        entry->oauth_state = std::nullopt;
        set(mcp_name, *entry);
    }
}

void McpAuth::clear_code_verifier(const std::string& mcp_name) {
    auto entry = get(mcp_name);
    if (entry) {
        entry->code_verifier = std::nullopt;
        set(mcp_name, *entry);
    }
}

std::optional<bool> McpAuth::is_token_expired(const std::string& mcp_name) {
    const auto entry = get(mcp_name);
    if (!entry || !entry->tokens) return std::nullopt;           // no token
    if (!entry->tokens->expires_at) return false;               // no expiry → not expired
    return *entry->tokens->expires_at < now_seconds();          // true if expired
}

std::optional<std::string> McpAuth::get_oauth_state(const std::string& mcp_name) {
    const auto entry = get(mcp_name);
    if (!entry) return std::nullopt;
    return entry->oauth_state;
}

McpAuth::AuthStatus McpAuth::get_auth_status(const std::string& mcp_name) {
    const auto expired = is_token_expired(mcp_name);
    if (!expired.has_value()) return AuthStatus::NotAuthenticated;
    if (*expired)             return AuthStatus::Expired;
    return AuthStatus::Authenticated;
}

}  // namespace turbot::core::mcp
