#pragma once

#include <turbot/core/common/export.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace turbot::core::mcp {

// ─── Token/ClientInfo/AuthEntry 数据结构 ───────────────────────────────────────

struct TURBOT_CORE_API Tokens {
    std::string access_token;
    std::optional<std::string> refresh_token;
    std::optional<int64_t> expires_at;   // Unix 时间戳（秒）
    std::optional<std::string> scope;

    [[nodiscard]] nlohmann::json to_json() const;
    static Tokens from_json(const nlohmann::json& j);
};

struct TURBOT_CORE_API ClientInfo {
    std::string client_id;
    std::optional<std::string> client_secret;
    std::optional<int64_t> client_id_issued_at;
    std::optional<int64_t> client_secret_expires_at;

    [[nodiscard]] nlohmann::json to_json() const;
    static ClientInfo from_json(const nlohmann::json& j);
};

struct TURBOT_CORE_API AuthEntry {
    std::optional<Tokens>     tokens;
    std::optional<ClientInfo> client_info;
    std::optional<std::string> code_verifier;   // PKCE code verifier
    std::optional<std::string> oauth_state;     // CSRF state parameter
    std::optional<std::string> server_url;      // URL 绑定（凭据与 URL 绑定）

    [[nodiscard]] nlohmann::json to_json() const;
    static AuthEntry from_json(const nlohmann::json& j);
};

// ─── McpAuth ──────────────────────────────────────────────────────────────────

/// OAuth 认证信息持久化存储（~/.turbot/mcp-auth.json，权限 0600）
/// 对齐 OpenCode McpAuth namespace
class TURBOT_CORE_API McpAuth {
public:
    /// 获取认证条目
    [[nodiscard]] static std::optional<AuthEntry> get(const std::string& mcp_name);

    /// 获取条目并校验 URL 绑定
    /// - 无条目 → nullopt
    /// - 无 server_url 字段 → nullopt（旧版本凭据）
    /// - URL 变更 → nullopt（凭据失效）
    [[nodiscard]] static std::optional<AuthEntry> get_for_url(
        const std::string& mcp_name,
        const std::string& server_url
    );

    static void set(const std::string& mcp_name, const AuthEntry& entry,
                    const std::optional<std::string>& server_url = {});
    static void remove(const std::string& mcp_name);

    static void update_tokens(const std::string& mcp_name, const Tokens& tokens,
                               const std::optional<std::string>& server_url = {});
    static void update_client_info(const std::string& mcp_name, const ClientInfo& info,
                                    const std::optional<std::string>& server_url = {});
    static void update_code_verifier(const std::string& mcp_name, const std::string& verifier);
    static void update_oauth_state(const std::string& mcp_name, const std::string& state);
    static void clear_oauth_state(const std::string& mcp_name);
    static void clear_code_verifier(const std::string& mcp_name);

    /// 三态返回（对齐 OpenCode isTokenExpired 返回 boolean | null 语义）：
    /// - nullopt：无 token 存储
    /// - false：无 expiresAt 字段或 token 未过期
    /// - true：token 已过期
    [[nodiscard]] static std::optional<bool> is_token_expired(const std::string& mcp_name);

    [[nodiscard]] static std::optional<std::string> get_oauth_state(const std::string& mcp_name);

    enum class AuthStatus { Authenticated, Expired, NotAuthenticated };
    [[nodiscard]] static AuthStatus get_auth_status(const std::string& mcp_name);

private:
    /// ~/.turbot/mcp-auth.json
    [[nodiscard]] static std::string auth_file_path();

    /// 读取全量 JSON（失败返回空对象）
    [[nodiscard]] static nlohmann::json read_all();

    /// 原子写入（先写临时文件再 rename），chmod 0600
    static void write_all(const nlohmann::json& data);

    /// Token 脱敏日志：截断为前 4 字符 + "****"
    [[nodiscard]] static std::string redact_token(const std::string& token);
};

}  // namespace turbot::core::mcp
