#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/lsp/client.hpp>
#include <turbot/core/lsp/server.hpp>

#include <nlohmann/json.hpp>

#include <future>
#include <memory>
#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace turbot::core::lsp {

// ─── LSPManager ───────────────────────────────────────────────────────────────

/// Singleton manager for LSP server lifecycle.
/// Aligned with OpenCode LSP namespace (index.ts):
///   - Lazy server startup per file extension
///   - broken_ set: marks failed/not-installed servers (key = root + server_id)
///   - spawning_: deduplicates concurrent startup requests for the same server+root
///   - Config "lsp" == false → disable all servers
class TURBOT_CORE_API LSPManager {
public:
    static LSPManager& instance() noexcept;

    LSPManager(const LSPManager&) = delete;
    LSPManager& operator=(const LSPManager&) = delete;

    /// Initialize: register builtin servers, read Config "lsp" for user-defined servers.
    /// Config "lsp" == false → no servers enabled (aligned with OpenCode cfg.lsp === false).
    void initialize(const std::string& workspace_root = {});

    /// Register a server descriptor. Must be called before get_clients().
    void register_server(LSPServerInfo server);

    /// Disable a server by ID (marks all its keys as broken).
    void disable_server(const std::string& id);

    /// Get (or lazily start) LSP clients for the given file.
    /// Aligned with OpenCode LSP.getClients().
    [[nodiscard]] std::future<std::vector<LSPClient*>> get_clients(const std::string& file);

    /// Check if any server matches this file WITHOUT starting it.
    /// Aligned with OpenCode LSP.hasClients().
    [[nodiscard]] bool has_clients(const std::string& file);

    /// Notify LSP servers about a file being opened/changed.
    /// Aligned with OpenCode LSP.touchFile():
    ///   - If wait_for_diagnostics: register wait BEFORE notify_open, then await
    /// @param file       Absolute path to the file
    /// @param wait_for_diagnostics  Block until diagnostics arrive (up to 3000ms)
    void touch_file(const std::string& file, bool wait_for_diagnostics = false);

    /// Merge diagnostics from all active clients.
    [[nodiscard]] std::unordered_map<std::string, std::vector<Diagnostic>> diagnostics() const;

    // ── LSP broadcast methods (aligned with OpenCode LSP.hover / definition / etc.)

    [[nodiscard]] std::future<std::optional<Hover>>
        hover(const std::string& file, Position pos);

    [[nodiscard]] std::future<std::vector<Location>>
        definition(const std::string& file, Position pos);

    [[nodiscard]] std::future<std::vector<Location>>
        references(const std::string& file, Position pos);

    [[nodiscard]] std::future<std::vector<Location>>
        implementation(const std::string& file, Position pos);

    [[nodiscard]] std::future<std::vector<Symbol>>
        workspace_symbol(const std::string& query);

    [[nodiscard]] std::future<std::vector<nlohmann::json>>
        document_symbol(const std::string& uri);

    /// Status of all active clients.
    /// Each entry: {"id": serverID, "root": relative_root, "status": "connected"}
    [[nodiscard]] std::vector<nlohmann::json> status() const;

    /// Shutdown all active clients.
    void shutdown();

private:
    LSPManager() = default;

    /// Internal: get clients without acquiring public lock (caller holds lock).
    std::vector<LSPClient*> get_clients_sync(const std::string& file);

    mutable std::shared_mutex  mutex_;
    std::string                workspace_root_;  ///< Project root (fallback for NearestRoot)
    std::unordered_map<std::string, LSPServerInfo> servers_;
    std::vector<std::unique_ptr<LSPClient>>        clients_;

    /// Broken servers: key = root + server_id (aligned with OpenCode: no separator)
    std::set<std::string>      broken_;

    /// In-flight spawns: key = root + server_id → shared_future<LSPClient*>
    /// shared_future allows multiple threads to wait on the same spawn.
    std::unordered_map<std::string, std::shared_future<LSPClient*>> spawning_;
};

}  // namespace turbot::core::lsp
