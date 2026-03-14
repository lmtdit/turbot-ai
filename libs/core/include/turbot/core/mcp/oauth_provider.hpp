#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/mcp/auth.hpp>

#include <future>
#include <optional>
#include <string>

namespace turbot::core::mcp {

/// OAuth callback server fixed port (aligned with OpenCode OAUTH_CALLBACK_PORT)
inline constexpr int OAUTH_CALLBACK_PORT = 19876;

/// OAuth callback path (aligned with OpenCode OAUTH_CALLBACK_PATH)
inline constexpr const char* OAUTH_CALLBACK_PATH = "/mcp/oauth/callback";

// ─── McpOAuthConfig ───────────────────────────────────────────────────────────

struct TURBOT_CORE_API McpOAuthConfig {
    std::optional<std::string> client_id;
    std::optional<std::string> client_secret;
    std::optional<std::string> scope;
};

// ─── McpOAuthProvider ─────────────────────────────────────────────────────────

/// Implements OAuth 2.0 + PKCE authorization code flow.
/// Aligned with OpenCode McpOAuthProvider interface.
///
/// Usage pattern:
///   1. generate_code_verifier() + generate_code_challenge()
///   2. build_auth_url() → redirect user to browser
///   3. wait for callback code via McpOAuthCallbackServer
///   4. exchange_code() → Tokens
///   5. McpAuth::update_tokens()
class TURBOT_CORE_API McpOAuthProvider {
public:
    explicit McpOAuthProvider(std::string mcp_name, std::string server_url,
                               McpOAuthConfig config = {});

    // ── PKCE helpers (static, pure functions) ────────────────────────────────

    /// Generate 32-byte random code verifier (base64url-encoded, no padding)
    [[nodiscard]] static std::string generate_code_verifier();

    /// Generate SHA-256 code challenge from verifier (base64url-encoded, no padding)
    [[nodiscard]] static std::string generate_code_challenge(const std::string& verifier);

    /// Generate 32-byte random hex state parameter
    [[nodiscard]] static std::string generate_state();

    // ── Authorization URL ─────────────────────────────────────────────────────

    /// Build authorization URL with PKCE parameters.
    /// Appends: response_type, client_id, redirect_uri, scope, state, code_challenge, code_challenge_method
    [[nodiscard]] std::string build_auth_url(
        const std::string& auth_endpoint,
        const std::string& client_id,
        const std::string& redirect_uri,
        const std::string& scope,
        const std::string& state,
        const std::string& code_challenge
    ) const;

    // ── Token Exchange ────────────────────────────────────────────────────────

    /// Exchange authorization code for tokens (POST token_endpoint).
    /// Returns Tokens on success; throws on error.
    [[nodiscard]] std::future<Tokens> exchange_code(
        const std::string& token_endpoint,
        const std::string& client_id,
        const std::optional<std::string>& client_secret,
        const std::string& code,
        const std::string& code_verifier,
        const std::string& redirect_uri
    );

    /// Refresh access token using refresh_token (POST token_endpoint).
    /// Returns updated Tokens on success; throws on error.
    [[nodiscard]] std::future<Tokens> refresh_token(
        const std::string& token_endpoint,
        const std::string& client_id,
        const std::optional<std::string>& client_secret,
        const std::string& refresh_tok
    );

    // ── Redirect URL ──────────────────────────────────────────────────────────

    /// Standard redirect URI: http://127.0.0.1:19876/mcp/oauth/callback
    [[nodiscard]] static std::string redirect_url();

    // ── Accessors ─────────────────────────────────────────────────────────────

    [[nodiscard]] const std::string& mcp_name() const noexcept { return mcp_name_; }
    [[nodiscard]] const std::string& server_url() const noexcept { return server_url_; }
    [[nodiscard]] const McpOAuthConfig& config() const noexcept { return config_; }

private:
    std::string mcp_name_;
    std::string server_url_;
    McpOAuthConfig config_;

    /// Parse token response JSON → Tokens, computes expires_at from expires_in
    [[nodiscard]] static Tokens parse_token_response(const std::string& body);
};

// ─── McpOAuthCallbackServer ───────────────────────────────────────────────────

/// Minimal HTTP server that listens on localhost:19876 for OAuth callback.
/// Uses the oauth state parameter as pending auth key.
/// Aligned with OpenCode McpOAuthCallback namespace.
class TURBOT_CORE_API McpOAuthCallbackServer {
public:
    /// Ensure callback server is running (idempotent)
    static void ensure_running();

    /// Register a pending auth; returns a future that resolves with the code.
    /// Must be called BEFORE opening the browser (race condition prevention).
    [[nodiscard]] static std::future<std::string> wait_for_callback(const std::string& oauth_state);

    /// Cancel a pending auth by state
    static void cancel_pending(const std::string& oauth_state);

    /// Stop the server and reject all pending callbacks
    static void stop();

    /// Check if server is running
    [[nodiscard]] static bool is_running();

    /// Check if callback port is already in use
    [[nodiscard]] static bool is_port_in_use();

private:
    McpOAuthCallbackServer() = delete;
};

}  // namespace turbot::core::mcp
