#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/lsp/lsp.hpp>

#include <nlohmann/json.hpp>

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace turbot::core::lsp {

// ─── ServerHandle ─────────────────────────────────────────────────────────────

/// Handle to a running LSP server process.
/// Created by LSPManager (3.6); the fd pair must remain open for the lifetime of LSPClient.
struct TURBOT_CORE_API ServerHandle {
    int pid       = -1;
    int stdin_fd  = -1;  ///< Write end (send to server)
    int stdout_fd = -1;  ///< Read end (receive from server)
    /// workspace/configuration response payload (null JSON = don't send didChangeConfiguration)
    nlohmann::json initialization = nullptr;
};

// ─── LSPClient ────────────────────────────────────────────────────────────────

/// JSON-RPC 2.0 LSP client over subprocess stdio (Content-Length framing).
/// Aligned with OpenCode LSPClient.create() behavior:
///   - initialize handshake (45s timeout)
///   - notifications/initialized + optional workspace/didChangeConfiguration
///   - textDocument/publishDiagnostics with debounce 150ms
///   - TypeScript server first-diagnosis skip rule
///   - wait_for_diagnostics with 3000ms timeout (silent on timeout)
///
/// Factory method `create()` returns nullptr on initialize failure.
class TURBOT_CORE_API LSPClient {
public:
    /// Factory: send initialize + initialized, register notification handlers.
    /// Returns nullptr if initialize handshake fails (timeout or error).
    [[nodiscard]] static std::unique_ptr<LSPClient> create(
        const std::string& server_id,
        const ServerHandle& handle,
        const std::string& root  ///< workspace root (absolute path)
    );

    ~LSPClient();

    // Non-copyable, movable
    LSPClient(const LSPClient&) = delete;
    LSPClient& operator=(const LSPClient&) = delete;

    [[nodiscard]] const std::string& server_id() const noexcept;
    [[nodiscard]] const std::string& root() const noexcept;
    [[nodiscard]] int pid() const noexcept;

    /// Send shutdown + exit notification, close fds, kill process.
    void shutdown();

    // ── File notifications ────────────────────────────────────────────────────

    /// Notify server about file open or change.
    /// First call for a path:
    ///   workspace/didChangeWatchedFiles(Created) + textDocument/didOpen (reads file content)
    ///   Clears existing diagnostics for the path (diagnostics.delete in OpenCode).
    /// Subsequent calls:
    ///   workspace/didChangeWatchedFiles(Changed) + textDocument/didChange
    /// Aligned with OpenCode notify.open()
    void notify_open(const std::string& path);

    /// Notify server that file was changed with explicit content.
    /// Increments version and sends textDocument/didChange.
    void notify_change(const std::string& path, const std::string& content);

    // ── Diagnostics ──────────────────────────────────────────────────────────

    /// Wait for diagnostics to arrive for a given path.
    /// Internally subscribes to EventBus "lsp.client.diagnostics", debounces 150ms.
    /// Times out after 3000ms; on timeout returns silently (no exception).
    /// Aligned with OpenCode waitForDiagnostics.
    [[nodiscard]] std::future<void> wait_for_diagnostics(const std::string& path);

    /// Access current diagnostics snapshot (key = normalized absolute path)
    [[nodiscard]] std::unordered_map<std::string, std::vector<Diagnostic>>
        diagnostics() const;

    // ── LSP Requests ─────────────────────────────────────────────────────────

    [[nodiscard]] std::future<std::optional<Hover>>
        hover(const std::string& uri, Position pos);

    [[nodiscard]] std::future<std::vector<Location>>
        definition(const std::string& uri, Position pos);

    [[nodiscard]] std::future<std::vector<Location>>
        references(const std::string& uri, Position pos);

    [[nodiscard]] std::future<std::vector<Location>>
        implementation(const std::string& uri, Position pos);

    [[nodiscard]] std::future<std::vector<Symbol>>
        workspace_symbol(const std::string& query);

    [[nodiscard]] std::future<std::vector<DocumentSymbol>>
        document_symbol(const std::string& uri);

    // ── Call Hierarchy ───────────────────────────────────────────────────────

    [[nodiscard]] std::future<std::vector<nlohmann::json>>
        prepare_call_hierarchy(const std::string& uri, Position pos);

    [[nodiscard]] std::future<std::vector<nlohmann::json>>
        incoming_calls(const nlohmann::json& item);

    [[nodiscard]] std::future<std::vector<nlohmann::json>>
        outgoing_calls(const nlohmann::json& item);

private:
    LSPClient();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace turbot::core::lsp
