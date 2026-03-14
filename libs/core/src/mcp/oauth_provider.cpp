#include <turbot/core/mcp/oauth_provider.hpp>
#include <turbot/core/common/logger.hpp>
#include <turbot/utils/crypto_utils.hpp>
#include <turbot/network/http_client.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>

// POSIX socket for callback HTTP server
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace turbot::core::mcp {

namespace {

/// URL-encode a string (RFC 3986)
std::string url_encode(const std::string& s) {
    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex;
    for (const char raw : s) {
        const auto c = static_cast<unsigned char>(raw);
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << raw;
        } else {
            encoded << std::uppercase;
            encoded << '%' << std::setw(2) << static_cast<int>(c);
            encoded << std::nouppercase;
        }
    }
    return encoded.str();
}

/// Convert standard base64 to base64url (no padding)
std::string to_base64url(std::string b64) {
    for (char& c : b64) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    // Remove padding
    while (!b64.empty() && b64.back() == '=') b64.pop_back();
    return b64;
}

/// Parse URL-encoded query string into a map
std::map<std::string, std::string> parse_query(const std::string& query) {
    std::map<std::string, std::string> params;
    std::istringstream ss(query);
    std::string token;
    while (std::getline(ss, token, '&')) {
        const auto eq = token.find('=');
        if (eq == std::string::npos) {
            params[token] = "";
        } else {
            params[token.substr(0, eq)] = token.substr(eq + 1);
        }
    }
    return params;
}

// ─── Callback server HTML responses ──────────────────────────────────────────

const char* HTML_SUCCESS =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<!DOCTYPE html><html><head><title>Turbot - Authorization Successful</title>"
    "<style>body{font-family:system-ui;display:flex;justify-content:center;align-items:center;"
    "height:100vh;margin:0;background:#1a1a2e;color:#eee;}"
    ".container{text-align:center;padding:2rem;}h1{color:#4ade80;margin-bottom:1rem;}p{color:#aaa;}"
    "</style></head><body><div class=\"container\"><h1>Authorization Successful</h1>"
    "<p>You can close this window and return to Turbot.</p></div>"
    "<script>setTimeout(()=>window.close(),2000);</script></body></html>\r\n";

std::string make_error_html(const std::string& error) {
    return "HTTP/1.1 400 Bad Request\r\n"
           "Content-Type: text/html\r\n"
           "Connection: close\r\n"
           "\r\n"
           "<!DOCTYPE html><html><head><title>Turbot - Authorization Failed</title>"
           "<style>body{font-family:system-ui;display:flex;justify-content:center;align-items:center;"
           "height:100vh;margin:0;background:#1a1a2e;color:#eee;}"
           ".container{text-align:center;padding:2rem;}h1{color:#f87171;margin-bottom:1rem;}p{color:#aaa;}"
           "</style></head><body><div class=\"container\"><h1>Authorization Failed</h1>"
           "<p>" + error + "</p></div></body></html>\r\n";
}

const char* HTTP_NOT_FOUND =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: text/plain\r\n"
    "Connection: close\r\n"
    "\r\n"
    "Not found\r\n";

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// McpOAuthProvider
// ─────────────────────────────────────────────────────────────────────────────

McpOAuthProvider::McpOAuthProvider(std::string mcp_name, std::string server_url,
                                    McpOAuthConfig config)
    : mcp_name_(std::move(mcp_name))
    , server_url_(std::move(server_url))
    , config_(std::move(config))
{}

std::string McpOAuthProvider::generate_code_verifier() {
    const auto bytes = turbot::utils::crypto::random_bytes(32);
    return to_base64url(turbot::utils::crypto::base64_encode(bytes));
}

std::string McpOAuthProvider::generate_code_challenge(const std::string& verifier) {
    // SHA-256(verifier) → base64url
    std::vector<uint8_t> input(verifier.begin(), verifier.end());
    const auto hash = turbot::utils::crypto::sha256(input);  // returns vector<uint8_t>
    return to_base64url(turbot::utils::crypto::base64_encode(hash));
}

std::string McpOAuthProvider::generate_state() {
    // 32 random bytes as hex string (64 hex chars)
    const auto bytes = turbot::utils::crypto::random_bytes(32);
    std::ostringstream ss;
    ss << std::hex;
    for (uint8_t b : bytes) {
        ss << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return ss.str();
}

std::string McpOAuthProvider::build_auth_url(
    const std::string& auth_endpoint,
    const std::string& client_id,
    const std::string& redirect_uri,
    const std::string& scope,
    const std::string& state,
    const std::string& code_challenge
) const {
    std::ostringstream url;
    url << auth_endpoint
        << "?response_type=code"
        << "&client_id="           << url_encode(client_id)
        << "&redirect_uri="        << url_encode(redirect_uri)
        << "&scope="               << url_encode(scope)
        << "&state="               << url_encode(state)
        << "&code_challenge="      << url_encode(code_challenge)
        << "&code_challenge_method=S256";
    return url.str();
}

std::string McpOAuthProvider::redirect_url() {
    return std::string("http://127.0.0.1:")
        + std::to_string(OAUTH_CALLBACK_PORT)
        + OAUTH_CALLBACK_PATH;
}

Tokens McpOAuthProvider::parse_token_response(const std::string& body) {
    const auto j = nlohmann::json::parse(body);

    if (j.contains("error")) {
        const std::string err = j.value("error", "unknown_error");
        const std::string desc = j.value("error_description", err);
        throw std::runtime_error("OAuth token error: " + desc);
    }

    Tokens t;
    t.access_token = j.value("access_token", std::string{});
    if (t.access_token.empty()) {
        throw std::runtime_error("OAuth token response missing access_token");
    }

    if (j.contains("refresh_token") && !j["refresh_token"].is_null())
        t.refresh_token = j["refresh_token"].get<std::string>();
    if (j.contains("scope") && !j["scope"].is_null())
        t.scope = j["scope"].get<std::string>();

    // Compute absolute expires_at from expires_in
    if (j.contains("expires_in") && j["expires_in"].is_number()) {
        using namespace std::chrono;
        const int64_t now = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
        t.expires_at = now + j["expires_in"].get<int64_t>();
    }

    return t;
}

std::future<Tokens> McpOAuthProvider::exchange_code(
    const std::string& token_endpoint,
    const std::string& client_id,
    const std::optional<std::string>& client_secret,
    const std::string& code,
    const std::string& code_verifier,
    const std::string& redirect_uri
) {
    return std::async(std::launch::async,
        [token_endpoint, client_id, client_secret, code, code_verifier, redirect_uri]() -> Tokens {
            std::ostringstream body;
            body << "grant_type=authorization_code"
                 << "&code="          << url_encode(code)
                 << "&client_id="     << url_encode(client_id)
                 << "&code_verifier=" << url_encode(code_verifier)
                 << "&redirect_uri="  << url_encode(redirect_uri);
            if (client_secret)
                body << "&client_secret=" << url_encode(*client_secret);

            turbot::network::HttpClient http;
            const auto resp = http.post(
                token_endpoint,
                body.str(),
                {{"Content-Type", "application/x-www-form-urlencoded"}}
            );

            if (!resp.is_success()) {
                throw std::runtime_error("OAuth exchange_code failed: HTTP "
                    + std::to_string(resp.status_code) + " - " + resp.body);
            }

            return parse_token_response(resp.body);
        }
    );
}

std::future<Tokens> McpOAuthProvider::refresh_token(
    const std::string& token_endpoint,
    const std::string& client_id,
    const std::optional<std::string>& client_secret,
    const std::string& refresh_tok
) {
    return std::async(std::launch::async,
        [token_endpoint, client_id, client_secret, refresh_tok]() -> Tokens {
            std::ostringstream body;
            body << "grant_type=refresh_token"
                 << "&refresh_token=" << url_encode(refresh_tok)
                 << "&client_id="     << url_encode(client_id);
            if (client_secret)
                body << "&client_secret=" << url_encode(*client_secret);

            turbot::network::HttpClient http;
            const auto resp = http.post(
                token_endpoint,
                body.str(),
                {{"Content-Type", "application/x-www-form-urlencoded"}}
            );

            if (!resp.is_success()) {
                throw std::runtime_error("OAuth refresh_token failed: HTTP "
                    + std::to_string(resp.status_code) + " - " + resp.body);
            }

            return parse_token_response(resp.body);
        }
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// McpOAuthCallbackServer
// ─────────────────────────────────────────────────────────────────────────────

namespace {

struct PendingAuth {
    std::promise<std::string> promise;
};

struct CallbackServerState {
    std::atomic<bool> running{false};
    std::mutex mutex;
    std::map<std::string, PendingAuth> pending;   // state → promise
    std::thread server_thread;
    int server_fd = -1;

    ~CallbackServerState() {
        if (server_fd >= 0) ::close(server_fd);
    }
};

// Singleton state
CallbackServerState& cb_state() {
    static CallbackServerState s;
    return s;
}

/// Handle a single incoming HTTP connection for the callback server
void handle_connection(int conn_fd) {
    // Read request (we only need first line + query params)
    std::string request;
    request.reserve(2048);
    char buf[256];
    while (true) {
        const ssize_t n = ::recv(conn_fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        request.append(buf, static_cast<size_t>(n));
        if (request.find("\r\n\r\n") != std::string::npos) break;
    }

    // Parse: "GET /path?query HTTP/1.1"
    const auto first_newline = request.find("\r\n");
    const std::string first_line = (first_newline != std::string::npos)
        ? request.substr(0, first_newline) : request;

    // Extract path+query from "GET /path?q HTTP/1.1"
    std::string path_query;
    {
        const auto sp1 = first_line.find(' ');
        const auto sp2 = (sp1 != std::string::npos) ? first_line.find(' ', sp1 + 1) : std::string::npos;
        if (sp1 != std::string::npos && sp2 != std::string::npos) {
            path_query = first_line.substr(sp1 + 1, sp2 - sp1 - 1);
        }
    }

    // Split path?query
    std::string path, query_str;
    const auto qmark = path_query.find('?');
    if (qmark != std::string::npos) {
        path = path_query.substr(0, qmark);
        query_str = path_query.substr(qmark + 1);
    } else {
        path = path_query;
    }

    if (path != OAUTH_CALLBACK_PATH) {
        ::send(conn_fd, HTTP_NOT_FOUND, strlen(HTTP_NOT_FOUND), 0);
        ::close(conn_fd);
        return;
    }

    const auto params = parse_query(query_str);

    auto get_param = [&](const std::string& key) -> std::string {
        const auto it = params.find(key);
        return (it != params.end()) ? it->second : std::string{};
    };

    const std::string state = get_param("state");
    const std::string code  = get_param("code");
    const std::string error = get_param("error");

    if (state.empty()) {
        const auto html = make_error_html("Missing required state parameter");
        ::send(conn_fd, html.c_str(), html.size(), 0);
        ::close(conn_fd);
        return;
    }

    auto& s = cb_state();
    std::lock_guard<std::mutex> lock(s.mutex);

    auto it = s.pending.find(state);
    if (it == s.pending.end()) {
        const auto html = make_error_html("Invalid or expired state parameter");
        ::send(conn_fd, html.c_str(), html.size(), 0);
        ::close(conn_fd);
        return;
    }

    if (!error.empty()) {
        const auto html = make_error_html(error);
        ::send(conn_fd, html.c_str(), html.size(), 0);
        it->second.promise.set_exception(
            std::make_exception_ptr(std::runtime_error("OAuth error: " + error)));
        s.pending.erase(it);
        ::close(conn_fd);
        return;
    }

    if (code.empty()) {
        const auto html = make_error_html("No authorization code provided");
        ::send(conn_fd, html.c_str(), html.size(), 0);
        it->second.promise.set_exception(
            std::make_exception_ptr(std::runtime_error("OAuth: missing code")));
        s.pending.erase(it);
        ::close(conn_fd);
        return;
    }

    // Success: resolve promise and respond to browser
    ::send(conn_fd, HTML_SUCCESS, strlen(HTML_SUCCESS), 0);
    it->second.promise.set_value(code);
    s.pending.erase(it);
    ::close(conn_fd);
}

/// Server accept loop (runs in background thread)
void server_loop(int server_fd) {
    auto& s = cb_state();
    while (s.running.load()) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        const int conn = ::accept(server_fd,
            reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
        if (conn < 0) {
            if (!s.running.load()) break;  // server stopped
            if (errno == EINTR || errno == EAGAIN) continue;  // transient error
            TURBOT_LOG_WARN("McpOAuthCallbackServer: accept() error: {}", strerror(errno));
            continue;
        }
        // Handle synchronously (low-traffic OAuth server)
        handle_connection(conn);
    }
}

}  // namespace

// ─── McpOAuthCallbackServer static methods ────────────────────────────────────

bool McpOAuthCallbackServer::is_port_in_use() {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(OAUTH_CALLBACK_PORT));
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    const int rc = ::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    ::close(fd);
    return rc == 0;
}

void McpOAuthCallbackServer::ensure_running() {
    auto& s = cb_state();
    if (s.running.load()) return;

    // Check if another instance already holds the port
    if (is_port_in_use()) {
        TURBOT_LOG_INFO("McpOAuthCallbackServer: port {} already in use (another instance?)",
                        OAUTH_CALLBACK_PORT);
        return;
    }

    // Create TCP socket
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error(
            std::string("McpOAuthCallbackServer: socket() failed: ") + strerror(errno));
    }

    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(OAUTH_CALLBACK_PORT));
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        throw std::runtime_error(
            std::string("McpOAuthCallbackServer: bind() failed: ") + strerror(errno));
    }

    if (::listen(fd, 16) < 0) {
        ::close(fd);
        throw std::runtime_error(
            std::string("McpOAuthCallbackServer: listen() failed: ") + strerror(errno));
    }

    s.server_fd = fd;
    s.running.store(true);
    s.server_thread = std::thread(server_loop, fd);

    TURBOT_LOG_INFO("McpOAuthCallbackServer: listening on port {}", OAUTH_CALLBACK_PORT);
}

std::future<std::string> McpOAuthCallbackServer::wait_for_callback(
    const std::string& oauth_state
) {
    constexpr int CALLBACK_TIMEOUT_SECONDS = 300;  // 5 minutes

    auto& s = cb_state();
    std::lock_guard<std::mutex> lock(s.mutex);

    auto [it, inserted] = s.pending.emplace(oauth_state, PendingAuth{});
    if (!inserted) {
        // Already pending; return existing future by wrapping
        // (shouldn't happen normally)
        throw std::runtime_error("McpOAuthCallbackServer: duplicate state: " + oauth_state);
    }

    // Wrap in a timed future via async
    std::future<std::string> inner_future = it->second.promise.get_future();

    return std::async(std::launch::async,
        [state = oauth_state, fut = std::move(inner_future),
         timeout = CALLBACK_TIMEOUT_SECONDS]() mutable -> std::string {
            const auto deadline = std::chrono::steady_clock::now()
                + std::chrono::seconds(timeout);
            if (fut.wait_until(deadline) == std::future_status::timeout) {
                // Cleanup pending entry
                auto& s2 = cb_state();
                std::lock_guard<std::mutex> lock2(s2.mutex);
                s2.pending.erase(state);
                throw std::runtime_error(
                    "OAuth callback timeout - authorization took too long");
            }
            return fut.get();
        });
}

void McpOAuthCallbackServer::cancel_pending(const std::string& oauth_state) {
    auto& s = cb_state();
    std::lock_guard<std::mutex> lock(s.mutex);
    auto it = s.pending.find(oauth_state);
    if (it != s.pending.end()) {
        it->second.promise.set_exception(
            std::make_exception_ptr(std::runtime_error("Authorization cancelled")));
        s.pending.erase(it);
    }
}

void McpOAuthCallbackServer::stop() {
    auto& s = cb_state();
    if (!s.running.exchange(false)) return;

    // Close server socket to unblock accept()
    if (s.server_fd >= 0) {
        ::close(s.server_fd);
        s.server_fd = -1;
    }

    if (s.server_thread.joinable()) s.server_thread.join();

    // Reject all pending callbacks
    std::lock_guard<std::mutex> lock(s.mutex);
    for (auto& [state, pending] : s.pending) {
        pending.promise.set_exception(
            std::make_exception_ptr(std::runtime_error("OAuth callback server stopped")));
    }
    s.pending.clear();

    TURBOT_LOG_INFO("McpOAuthCallbackServer: stopped");
}

bool McpOAuthCallbackServer::is_running() {
    return cb_state().running.load();
}

}  // namespace turbot::core::mcp
