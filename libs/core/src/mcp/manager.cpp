#include <turbot/core/mcp/manager.hpp>
#include <turbot/core/mcp/stdio_transport.hpp>
#include <turbot/core/mcp/http_transport.hpp>
#include <turbot/core/mcp/sse_transport.hpp>
#include <turbot/core/mcp/auth.hpp>
#include <turbot/core/mcp/oauth_provider.hpp>
#include <turbot/core/tool/tool.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include <turbot/core/event/event_bus.hpp>
#include <turbot/core/common/logger.hpp>

#include <csignal>
#include <cstdlib>
#include <deque>
#include <stdexcept>
#include <sstream>
#include <unordered_set>

#include <sys/wait.h>
#include <unistd.h>

namespace turbot::core::mcp {

namespace {

// ─── MCPToolWrapper ───────────────────────────────────────────────────────────

/// 将 MCP 工具包装为 turbot::core::tool::Tool，透传调用到 MCPClient
/// Schema 规则对齐 OpenCode convertMcpTool：type="object", additionalProperties=false
class MCPToolWrapper : public turbot::core::tool::Tool {
public:
    MCPToolWrapper(
        std::string registered_name,   // sanitized_client + "_" + sanitized_tool
        std::string description,
        nlohmann::json schema,
        std::string server_name,
        std::string original_tool_name
    )
        : registered_name_(std::move(registered_name))
        , description_(std::move(description))
        , schema_(std::move(schema))
        , server_name_(std::move(server_name))
        , original_tool_name_(std::move(original_tool_name))
    {
        // Enforce: type = "object", additionalProperties = false
        if (!schema_.is_object()) {
            schema_ = {{"type", "object"}, {"properties", nlohmann::json::object()}};
        }
        schema_["type"] = "object";
        schema_["additionalProperties"] = false;
        if (!schema_.contains("properties")) {
            schema_["properties"] = nlohmann::json::object();
        }
    }

    [[nodiscard]] std::string name() const override { return registered_name_; }
    [[nodiscard]] std::string description() const override { return description_; }
    [[nodiscard]] nlohmann::json input_schema() const override { return schema_; }

    [[nodiscard]] turbot::core::tool::ToolResult execute(
        const nlohmann::json& input,
        turbot::core::tool::ToolContext& /*ctx*/
    ) override {
        try {
            auto result = call_through_manager(input);
            return turbot::core::tool::ToolResult::success(
                registered_name_,
                result.is_string() ? result.get<std::string>() : result.dump()
            );
        } catch (const std::exception& e) {
            return turbot::core::tool::ToolResult::error(
                registered_name_,
                std::string("MCP tool call failed: ") + e.what()
            );
        }
    }

private:
    nlohmann::json call_through_manager(const nlohmann::json& args) {
        // Delegate through MCPManager public API
        return MCPManager::instance().call_tool(server_name_, original_tool_name_, args).get();
    }

    std::string registered_name_;
    std::string description_;
    nlohmann::json schema_;
    std::string server_name_;
    std::string original_tool_name_;
};

// ─── EventData for mcp.tools.changed ─────────────────────────────────────────

struct McpToolsChangedData {
    std::string server;
    [[nodiscard]] std::string to_json_str() const {
        return nlohmann::json{{"server", server}}.dump();
    }
};

}  // namespace

// ─── MCPManager singleton ─────────────────────────────────────────────────────

MCPManager& MCPManager::instance() {
    static MCPManager inst;
    return inst;
}

void MCPManager::initialize(const std::vector<MCPClientConfig>& configs) {
    // Parallel initialization
    std::vector<std::future<MCPStatus>> futs;
    for (const auto& cfg : configs) {
        if (!cfg.enabled) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            configs_[cfg.name] = cfg;
            status_map_[cfg.name] = MCPStatus::Disabled;
        } else {
            futs.push_back(add(cfg.name, cfg));
        }
    }
    for (auto& f : futs) {
        try { f.get(); } catch (const std::exception& e) {
            TURBOT_LOG_ERROR("MCPManager: initialize - server failed: {}", e.what());
        }
    }
}

std::future<MCPStatus> MCPManager::add(const std::string& name, const MCPClientConfig& config) {
    return std::async(std::launch::async, [this, name, config]() -> MCPStatus {
        // Close existing client if present
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            auto it = clients_.find(name);
            if (it != clients_.end() && it->second) {
                TURBOT_LOG_INFO("MCPManager: closing existing client '{}'", name);
                try { it->second->close().get(); } catch (...) {}
                clients_.erase(it);
            }
            configs_[name] = config;
        }

        MCPStatus st = create_and_connect(name, config);

        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            status_map_[name] = st;
        }
        return st;
    });
}

MCPStatus MCPManager::create_and_connect(const std::string& name, const MCPClientConfig& config) {
    if (!config.enabled) {
        return MCPStatus::Disabled;
    }

    std::unique_ptr<ITransport> transport;

    if (config.type == "local") {
        if (config.command.empty()) {
            TURBOT_LOG_ERROR("MCPManager: local MCP '{}' has empty command", name);
            return MCPStatus::Failed;
        }
        transport = std::make_unique<StdioTransport>(
            config.command,
            config.environment
        );
    } else if (config.type == "remote") {
        if (!config.url.has_value()) {
            TURBOT_LOG_ERROR("MCPManager: remote MCP '{}' has no url", name);
            return MCPStatus::Failed;
        }
        // Build headers from optional headers map
        std::unordered_map<std::string, std::string> headers;
        if (config.headers.has_value()) {
            headers = *config.headers;
        }
        try {
            // HttpTransport: tries StreamableHTTP first, falls back to SSE
            transport = std::make_unique<HttpTransport>(*config.url, headers);
        } catch (const std::invalid_argument& e) {
            TURBOT_LOG_ERROR("MCPManager: invalid url for '{}': {}", name, e.what());
            return MCPStatus::Failed;
        }
    } else {
        TURBOT_LOG_ERROR("MCPManager: unknown MCP type '{}' for '{}'", config.type, name);
        return MCPStatus::Failed;
    }

    auto client = std::make_shared<MCPClient>(std::move(transport));

    // Register tools_changed → re-sync
    client->on_tools_changed([this, name]() {
        TURBOT_LOG_INFO("MCPManager: tools changed for '{}', re-syncing", name);
        sync_tools_to_registry(name);  // fire-and-forget
    });

    MCPStatus st;
    try {
        st = client->connect().get();
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("MCPManager: connect '{}' threw: {}", name, e.what());
        return MCPStatus::Failed;
    }

    if (st == MCPStatus::Connected) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        clients_[name] = client;  // shared_ptr copy
        TURBOT_LOG_INFO("MCPManager: '{}' connected", name);
    } else {
        TURBOT_LOG_WARN("MCPManager: '{}' connection status = {}", name,
            mcp_status_to_string(st));
    }
    return st;
}

std::future<void> MCPManager::connect(const std::string& name) {
    return std::async(std::launch::async, [this, name]() {
        MCPClientConfig config;
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = configs_.find(name);
            if (it == configs_.end()) {
                TURBOT_LOG_ERROR("MCPManager: connect({}) - config not found", name);
                return;
            }
            config = it->second;
        }
        config.enabled = true;  // force enabled
        MCPStatus st = create_and_connect(name, config);
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            status_map_[name] = st;
            configs_[name].enabled = true;
        }
    });
}

std::future<void> MCPManager::disconnect(const std::string& name) {
    return std::async(std::launch::async, [this, name]() {
        std::shared_ptr<MCPClient> to_close;
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            auto it = clients_.find(name);
            if (it != clients_.end()) {
                to_close = std::move(it->second);
                clients_.erase(it);  // remove client, keep config
            }
            status_map_[name] = MCPStatus::Disabled;
            // configs_[name] kept intact for reconnect
        }
        if (to_close) {
            try { to_close->close().get(); } catch (...) {}
        }
        TURBOT_LOG_INFO("MCPManager: disconnected '{}'", name);
    });
}

void MCPManager::remove(const std::string& name) {
    std::shared_ptr<MCPClient> to_close;
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto cit = clients_.find(name);
        if (cit != clients_.end()) {
            to_close = std::move(cit->second);
            clients_.erase(cit);
        }
        status_map_.erase(name);
        configs_.erase(name);
    }
    if (to_close) {
        try { to_close->close().get(); } catch (...) {}
    }
}

std::unordered_map<std::string, MCPStatus> MCPManager::status() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return status_map_;
}

std::future<std::unordered_map<std::string, nlohmann::json>> MCPManager::tools() {
    return std::async(std::launch::async, [this]() {
        // Snapshot connected clients
        std::vector<std::pair<std::string, std::shared_ptr<MCPClient>>> snapshot;
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            for (const auto& [n, c] : clients_) {
                if (c && c->status() == MCPStatus::Connected) {
                    snapshot.emplace_back(n, c);
                }
            }
        }

        std::unordered_map<std::string, nlohmann::json> result;
        for (auto& [client_name, client] : snapshot) {
            std::vector<MCPTool> tool_list;
            try {
                tool_list = client->list_tools().get();
            } catch (const std::exception& e) {
                TURBOT_LOG_ERROR("MCPManager: list_tools('{}') failed: {}", client_name, e.what());
                continue;
            }
            const std::string sc = sanitize_mcp_name(client_name);
            for (const auto& t : tool_list) {
                const std::string key = sc + "_" + sanitize_mcp_name(t.name);
                result[key] = t.to_json();
            }
        }
        return result;
    });
}

std::future<void> MCPManager::sync_tools_to_registry(const std::string& server_name) {
    return std::async(std::launch::async, [this, server_name]() {
        auto& registry = turbot::core::tool::ToolRegistry::instance();

        // Determine which servers to sync
        std::vector<std::pair<std::string, std::shared_ptr<MCPClient>>> snapshot;
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            for (const auto& [n, c] : clients_) {
                if (!server_name.empty() && n != server_name) continue;
                if (c && c->status() == MCPStatus::Connected) {
                    snapshot.emplace_back(n, c);
                }
            }
        }

        for (auto& [client_name, client] : snapshot) {
            const std::string sc = sanitize_mcp_name(client_name);
            const std::string prefix = sc + "_";

            // Fetch new tools first — only replace if successful (avoids tool loss on failure)
            std::vector<MCPTool> tool_list;
            try {
                tool_list = client->list_tools().get();
            } catch (const std::exception& e) {
                TURBOT_LOG_ERROR("MCPManager: sync list_tools('{}') failed: {}",
                    client_name, e.what());
                continue;  // keep existing tools intact
            }

            // Remove existing MCP tools for this server (after successful fetch)
            for (const auto& existing_name : registry.names()) {
                if (existing_name.starts_with(prefix)) {
                    registry.remove(existing_name);
                }
            }

            for (const auto& t : tool_list) {
                const std::string registered_name = sc + "_" + sanitize_mcp_name(t.name);
                auto wrapper = std::make_unique<MCPToolWrapper>(
                    registered_name,
                    t.description,
                    t.input_schema,
                    client_name,
                    t.name
                );
                registry.register_tool(std::move(wrapper));
            }

            TURBOT_LOG_INFO("MCPManager: synced {} tools for '{}'", tool_list.size(), client_name);

            // Publish EventBus event: mcp.tools.changed
            turbot::core::EventBus::instance().publish<nlohmann::json>(
                "mcp.tools.changed",
                nlohmann::json{{"server", client_name}}
            );
        }
    });
}

std::future<std::vector<MCPResource>> MCPManager::all_resources() {
    return std::async(std::launch::async, [this]() -> std::vector<MCPResource> {
        std::vector<std::pair<std::string, std::shared_ptr<MCPClient>>> snapshot;
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            for (const auto& [n, c] : clients_) {
                if (c && c->status() == MCPStatus::Connected) {
                    snapshot.emplace_back(n, c);
                }
            }
        }

        std::vector<MCPResource> result;
        for (auto& [client_name, client] : snapshot) {
            try {
                auto resources = client->list_resources().get();
                for (auto& r : resources) {
                    r.client = client_name;
                    result.push_back(std::move(r));
                }
            } catch (const std::exception& e) {
                TURBOT_LOG_ERROR("MCPManager: list_resources('{}') failed: {}", client_name, e.what());
            }
        }
        return result;
    });
}

std::future<std::unordered_map<std::string, MCPPrompt>> MCPManager::all_prompts() {
    return std::async(std::launch::async, [this]() -> std::unordered_map<std::string, MCPPrompt> {
        std::vector<std::pair<std::string, std::shared_ptr<MCPClient>>> snapshot;
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            for (const auto& [n, c] : clients_) {
                if (c && c->status() == MCPStatus::Connected) {
                    snapshot.emplace_back(n, c);
                }
            }
        }

        std::unordered_map<std::string, MCPPrompt> result;
        for (auto& [client_name, client] : snapshot) {
            try {
                auto prompts = client->list_prompts().get();
                const std::string sc = sanitize_mcp_name(client_name);
                for (const auto& p : prompts) {
                    const std::string key = sc + ":" + sanitize_mcp_name(p.name);
                    result[key] = p;
                }
            } catch (const std::exception& e) {
                TURBOT_LOG_ERROR("MCPManager: list_prompts('{}') failed: {}", client_name, e.what());
            }
        }
        return result;
    });
}

std::future<nlohmann::json> MCPManager::call_tool(
    const std::string& server_name,
    const std::string& tool_name,
    const nlohmann::json& args,
    int timeout_ms
) {
    return std::async(std::launch::async, [this, server_name, tool_name, args, timeout_ms]() -> nlohmann::json {
        std::shared_ptr<MCPClient> client;
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = clients_.find(server_name);
            if (it == clients_.end() || !it->second) {
                throw std::runtime_error("MCP server '" + server_name + "' not connected");
            }
            client = it->second;  // extend lifetime via shared_ptr copy
        }
        // client lifetime is now independent of clients_ map
        return client->call_tool(tool_name, args, timeout_ms).get();
    });
}

// ─── Auth ─────────────────────────────────────────────────────────────────────

bool MCPManager::supports_oauth(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = configs_.find(name);
    if (it == configs_.end()) return false;
    return it->second.type == "remote";
}

/// Open a URL in the system default browser (macOS / Linux)
/// Uses execvp() to avoid shell injection via URL characters.
static bool open_browser(const std::string& url) {
    const pid_t pid = ::fork();
    if (pid < 0) return false;
    if (pid == 0) {
        // Child process
        ::setsid();  // detach from terminal
#if defined(__APPLE__)
        ::execlp("open", "open", url.c_str(), nullptr);
#else
        ::execlp("xdg-open", "xdg-open", url.c_str(), nullptr);
#endif
        ::_exit(1);  // exec failed
    }
    // Parent: don't wait (fire-and-forget), check immediate failure
    int status = 0;
    const pid_t result = ::waitpid(pid, &status, WNOHANG);
    if (result == pid && WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        return false;
    }
    return true;
}

std::future<std::string> MCPManager::start_auth(const std::string& name) {
    // Snapshot config under lock
    MCPClientConfig cfg;
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = configs_.find(name);
        if (it == configs_.end()) {
            throw std::invalid_argument("MCPManager::start_auth: unknown server '" + name + "'");
        }
        cfg = it->second;
    }

    return std::async(std::launch::async, [name, cfg]() -> std::string {
        if (cfg.type != "remote") {
            throw std::runtime_error(
                "MCPManager::start_auth: server '" + name + "' is not a remote server");
        }
        if (!cfg.url) {
            throw std::runtime_error(
                "MCPManager::start_auth: server '" + name + "' has no URL configured");
        }
        const std::string server_url = *cfg.url;

        // Ensure callback server is running
        McpOAuthCallbackServer::ensure_running();

        // Generate and store oauth state
        const std::string oauth_state = McpOAuthProvider::generate_state();
        McpAuth::update_oauth_state(name, oauth_state);

        // Generate PKCE verifier + challenge
        const std::string verifier  = McpOAuthProvider::generate_code_verifier();
        const std::string challenge = McpOAuthProvider::generate_code_challenge(verifier);
        McpAuth::update_code_verifier(name, verifier);

        // Build authorization URL
        McpOAuthProvider provider(name, server_url);
        const std::string redirect = McpOAuthProvider::redirect_url();

        // Try to read client_id from stored client_info
        std::string client_id;
        const auto stored = McpAuth::get(name);
        if (stored && stored->client_info) {
            client_id = stored->client_info->client_id;
        }

        // Build auth URL
        const std::string auth_url = provider.build_auth_url(
            server_url + "/oauth/authorize",
            client_id.empty() ? "turbot" : client_id,
            redirect,
            "read",
            oauth_state,
            challenge
        );

        TURBOT_LOG_INFO("MCPManager::start_auth: auth URL generated for '{}'", name);
        return auth_url;
    });
}

std::future<MCPStatus> MCPManager::authenticate(const std::string& name) {
    return std::async(std::launch::async, [this, name]() -> MCPStatus {
        // Get authorization URL
        const std::string auth_url = start_auth(name).get();

        // Read the stored oauth state
        const auto oauth_state_opt = McpAuth::get_oauth_state(name);
        if (!oauth_state_opt) {
            throw std::runtime_error("MCPManager::authenticate: oauth state not found");
        }
        const std::string oauth_state = *oauth_state_opt;

        // ── CRITICAL: Register callback BEFORE opening browser ──
        // (prevents race condition when IdP has active SSO session)
        auto callback_future = McpOAuthCallbackServer::wait_for_callback(oauth_state);

        // Open browser
        if (!open_browser(auth_url)) {
            // Browser opening failed — publish EventBus event for CLI to display URL
            TURBOT_LOG_WARN("MCPManager::authenticate: browser open failed for '{}', publishing event", name);
            turbot::core::EventBus::instance().publish<nlohmann::json>(
                "mcp.browser.open.failed",
                nlohmann::json{{"mcpName", name}, {"url", auth_url}}
            );
        }

        // Wait for callback code (timeout is managed inside wait_for_callback)
        const std::string code = callback_future.get();

        // Validate and clear state
        const auto stored_state = McpAuth::get_oauth_state(name);
        if (!stored_state || *stored_state != oauth_state) {
            McpAuth::clear_oauth_state(name);
            throw std::runtime_error("MCPManager::authenticate: OAuth state mismatch");
        }
        McpAuth::clear_oauth_state(name);

        return finish_auth(name, code).get();
    });
}

std::future<MCPStatus> MCPManager::finish_auth(
    const std::string& name, const std::string& code
) {
    MCPClientConfig cfg;
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = configs_.find(name);
        if (it == configs_.end()) {
            throw std::invalid_argument("MCPManager::finish_auth: unknown server '" + name + "'");
        }
        cfg = it->second;
    }

    return std::async(std::launch::async, [this, name, code, cfg]() -> MCPStatus {
        if (!cfg.url) {
            throw std::runtime_error("MCPManager::finish_auth: server '" + name + "' has no URL");
        }
        const std::string server_url = *cfg.url;

        // Read stored PKCE verifier
        const auto entry = McpAuth::get(name);
        if (!entry || !entry->code_verifier) {
            throw std::runtime_error("MCPManager::finish_auth: no code_verifier stored for '" + name + "'");
        }
        const std::string verifier = *entry->code_verifier;

        // Determine client_id
        std::string client_id;
        std::optional<std::string> client_secret;
        if (entry->client_info) {
            client_id = entry->client_info->client_id;
            client_secret = entry->client_info->client_secret;
        }

        // Exchange code for tokens
        McpOAuthProvider provider(name, server_url);
        auto tokens = provider.exchange_code(
            server_url + "/oauth/token",
            client_id.empty() ? "turbot" : client_id,
            client_secret,
            code,
            verifier,
            McpOAuthProvider::redirect_url()
        ).get();

        // Persist tokens
        McpAuth::update_tokens(name, tokens, server_url);
        McpAuth::clear_code_verifier(name);

        TURBOT_LOG_INFO("MCPManager::finish_auth: tokens stored for '{}', reconnecting", name);

        // Reconnect
        return add(name, cfg).get();
    });
}

std::future<void> MCPManager::remove_auth(const std::string& name) {
    return std::async(std::launch::async, [name]() {
        McpAuth::remove(name);
        TURBOT_LOG_INFO("MCPManager::remove_auth: credentials removed for '{}'", name);
    });
}

// ─── Shutdown ──────────────────────────────────────────────────────────────────

void MCPManager::shutdown() {
    std::vector<std::pair<int, std::shared_ptr<MCPClient>>> to_close;
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        for (auto& [name, client] : clients_) {
            if (!client) continue;
            int child_pid = client->pid();
            to_close.emplace_back(child_pid, client);  // shared_ptr copy
        }
        clients_.clear();
        status_map_.clear();
    }

    // Kill descendant processes first (for servers like chrome-devtools-mcp)
    for (auto& [child_pid, client] : to_close) {
        if (child_pid > 0) {
            auto desc = descendants(child_pid);
            for (int dpid : desc) {
                try { ::kill(dpid, SIGTERM); } catch (...) {}
            }
        }
    }

    // Close clients
    for (auto& [child_pid, client] : to_close) {
        try { client->close().get(); } catch (...) {}
    }
}

// ─── descendants ──────────────────────────────────────────────────────────────

std::vector<int> MCPManager::descendants(int pid) {
    // Use pgrep -P to find children, BFS
    std::vector<int> result;
    std::deque<int> queue = {pid};  // deque for O(1) pop_front
    std::unordered_set<int> seen{pid};  // O(1) dedup

    while (!queue.empty()) {
        int current = queue.front();
        queue.pop_front();

        // Run: pgrep -P <current>
        std::string cmd = "pgrep -P " + std::to_string(current) + " 2>/dev/null";
        FILE* fp = ::popen(cmd.c_str(), "r");
        if (!fp) continue;

        char line_buf[64];
        while (::fgets(line_buf, sizeof(line_buf), fp)) {
            int cpid = std::atoi(line_buf);
            if (cpid > 0 && seen.find(cpid) == seen.end()) {
                seen.insert(cpid);
                result.push_back(cpid);
                queue.push_back(cpid);
            }
        }
        ::pclose(fp);
    }
    return result;
}

}  // namespace turbot::core::mcp
