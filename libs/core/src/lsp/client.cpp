#include <turbot/core/lsp/client.hpp>
#include <turbot/core/lsp/lsp.hpp>
#include <turbot/core/event/event_bus.hpp>
#include <turbot/core/common/logger.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

namespace turbot::core::lsp {

// ─── Event payload ────────────────────────────────────────────────────────────

struct DiagnosticsEvent {
    std::string server_id;
    std::string path;
};

static constexpr const char* kDiagnosticsEventName = "lsp.client.diagnostics";
static constexpr int kInitializeTimeoutMs   = 45000;
static constexpr int kDiagnosticsTimeoutMs  = 3000;
static constexpr int kDiagnosticsDebounceMs = 150;

// ─── I/O helpers ──────────────────────────────────────────────────────────────

/// Write all bytes to fd; returns false on error.
static bool write_all(int fd, const char* data, size_t len) {
    size_t written = 0;
    while (written < len) {
        const ssize_t n = ::write(fd, data + written, len - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        written += static_cast<size_t>(n);
    }
    return true;
}

/// Read exactly `len` bytes from fd into buf; returns false on EOF/error.
static bool read_exactly(int fd, char* buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        const ssize_t n = ::read(fd, buf + got, len - got);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;  // EOF
        got += static_cast<size_t>(n);
    }
    return true;
}

/// Read one line (up to \n) from fd into out (strips trailing \r\n).
/// Returns false on EOF/error.
static bool read_line(int fd, std::string& out) {
    out.clear();
    while (true) {
        char c = 0;
        const ssize_t n = ::read(fd, &c, 1);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;  // EOF
        if (c == '\n') {
            if (!out.empty() && out.back() == '\r') out.pop_back();
            return true;
        }
        out += c;
    }
}

// ─── LSPClient::Impl ──────────────────────────────────────────────────────────

struct LSPClient::Impl {
    // Identity
    std::string server_id;
    std::string root;
    ServerHandle handle;

    // JSON-RPC id counter
    std::atomic<int> next_id{1};

    // Pending requests: id → promise<json>
    std::mutex                              pending_mutex;
    std::map<int, std::promise<nlohmann::json>> pending;

    // File open tracking: path → version
    std::mutex                   files_mutex;
    std::unordered_map<std::string, int> files;

    // Diagnostics
    mutable std::mutex                                                     diag_mutex;
    std::unordered_map<std::string, std::vector<Diagnostic>>               diagnostics_map;

    // Debounce state per URI
    struct DebounceEntry {
        std::thread           timer;
        std::atomic<bool>     cancelled{false};
    };
    std::mutex                                         debounce_mutex;
    std::unordered_map<std::string, std::shared_ptr<DebounceEntry>> debounce_timers;

    // IO thread
    std::thread reader_thread;
    std::atomic<bool> running{false};

    // Shutdown flag
    std::atomic<bool> shutdown_flag{false};

    // ── Send ───────────────────────────────────────────────────────────────

    void send_raw(const nlohmann::json& msg) {
        if (shutdown_flag.load()) return;
        const std::string body = msg.dump();
        const std::string header = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";
        if (!write_all(handle.stdin_fd, header.data(), header.size()) ||
            !write_all(handle.stdin_fd, body.data(), body.size())) {
            TURBOT_LOG_WARN("lsp[{}]: send failed: {}", server_id, strerror(errno));
        }
    }

    /// Send request; returns future that resolves when response arrives.
    std::future<nlohmann::json> send_request(const std::string& method, const nlohmann::json& params) {
        const int id = next_id.fetch_add(1, std::memory_order_relaxed);
        std::promise<nlohmann::json> prom;
        auto fut = prom.get_future();
        {
            std::lock_guard<std::mutex> lk(pending_mutex);
            pending.emplace(id, std::move(prom));
        }
        send_raw({
            {"jsonrpc", "2.0"},
            {"id",      id},
            {"method",  method},
            {"params",  params},
        });
        return fut;
    }

    /// Send notification (no id, no response expected).
    void send_notification(const std::string& method, const nlohmann::json& params) {
        send_raw({
            {"jsonrpc", "2.0"},
            {"method",  method},
            {"params",  params},
        });
    }

    /// Send response to a server-initiated request.
    void send_response(const nlohmann::json& id, const nlohmann::json& result) {
        send_raw({
            {"jsonrpc", "2.0"},
            {"id",      id},
            {"result",  result},
        });
    }

    // ── Request handler (server → client) ──────────────────────────────────

    void handle_server_request(const nlohmann::json& msg) {
        const std::string method = msg.value("method", std::string{});
        const auto& id           = msg["id"];

        if (method == "workspace/configuration") {
            // Return [initialization ?? {}]
            nlohmann::json cfg = (handle.initialization.is_null() || !handle.initialization.is_object())
                                    ? nlohmann::json::object()
                                    : handle.initialization;
            send_response(id, nlohmann::json::array({cfg}));
        } else if (method == "window/workDoneProgress/create") {
            TURBOT_LOG_DEBUG("lsp[{}]: window/workDoneProgress/create", server_id);
            send_response(id, nullptr);
        } else if (method == "client/registerCapability") {
            send_response(id, nullptr);
        } else if (method == "client/unregisterCapability") {
            send_response(id, nullptr);
        } else if (method == "workspace/workspaceFolders") {
            const std::string root_uri = path_to_uri(root);
            send_response(id, nlohmann::json::array({
                nlohmann::json::object({{"name", "workspace"}, {"uri", root_uri}})
            }));
        } else {
            TURBOT_LOG_DEBUG("lsp[{}]: unhandled server request: {}", server_id, method);
            // Send null response to acknowledge
            send_response(id, nullptr);
        }
    }

    // ── Debounce helper ────────────────────────────────────────────────────

    void schedule_diagnostics_publish(const std::string& path) {
        std::lock_guard<std::mutex> lk(debounce_mutex);
        // Cancel existing timer for this path
        auto it = debounce_timers.find(path);
        if (it != debounce_timers.end()) {
            it->second->cancelled.store(true);
            if (it->second->timer.joinable()) {
                it->second->timer.detach();
            }
            debounce_timers.erase(it);
        }
        // Create new timer entry
        auto entry = std::make_shared<DebounceEntry>();
        debounce_timers[path] = entry;

        const std::string sid = server_id;
        entry->timer = std::thread([entry, path, sid]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(kDiagnosticsDebounceMs));
            if (entry->cancelled.load()) return;
            // Publish to EventBus
            DiagnosticsEvent ev;
            ev.server_id = sid;
            ev.path      = path;
            turbot::core::EventBus::instance().publish<DiagnosticsEvent>(
                kDiagnosticsEventName, std::move(ev));
        });
    }

    // ── Notification handler ───────────────────────────────────────────────

    void handle_notification(const nlohmann::json& msg) {
        const std::string method = msg.value("method", std::string{});

        if (method == "textDocument/publishDiagnostics") {
            const auto& params = msg.value("params", nlohmann::json::object());
            const std::string uri      = params.value("uri", std::string{});
            const std::string file_path = uri_to_path(uri);

            // Aligned with OpenCode: check exists BEFORE updating
            bool exists = false;
            {
                std::lock_guard<std::mutex> lk(diag_mutex);
                exists = diagnostics_map.count(file_path) > 0;
                // Update diagnostics first
                std::vector<Diagnostic> diags;
                if (params.contains("diagnostics") && params["diagnostics"].is_array()) {
                    for (const auto& d : params["diagnostics"]) {
                        diags.push_back(Diagnostic::from_json(d));
                    }
                }
                diagnostics_map[file_path] = std::move(diags);
            }

            TURBOT_LOG_DEBUG("lsp[{}]: publishDiagnostics path={} exists={}", server_id, file_path, exists);

            // TypeScript special rule: skip debounce on first notification
            if (!exists && server_id == "typescript") {
                return;
            }

            schedule_diagnostics_publish(file_path);
        } else {
            TURBOT_LOG_DEBUG("lsp[{}]: notification: {}", server_id, method);
        }
    }

    // ── Reader thread ──────────────────────────────────────────────────────

    void reader_loop() {
        while (running.load()) {
            // 1. Read headers until blank line
            std::string line;
            size_t content_length = 0;
            bool got_content_length = false;

            while (true) {
                if (!read_line(handle.stdout_fd, line)) {
                    TURBOT_LOG_DEBUG("lsp[{}]: EOF/error reading header", server_id);
                    goto done;
                }
                if (line.empty()) break;  // blank line = end of headers
                // Parse Content-Length
                static const std::string cl_prefix = "Content-Length: ";
                if (line.rfind(cl_prefix, 0) == 0) {
                    try {
                        content_length = static_cast<size_t>(std::stoul(line.substr(cl_prefix.size())));
                        got_content_length = true;
                    } catch (...) {
                        TURBOT_LOG_WARN("lsp[{}]: malformed Content-Length: {}", server_id, line);
                    }
                }
            }

            if (!got_content_length || content_length == 0) {
                TURBOT_LOG_WARN("lsp[{}]: missing/zero Content-Length, skipping", server_id);
                continue;
            }

            // 2. Read body
            std::string body(content_length, '\0');
            if (!read_exactly(handle.stdout_fd, body.data(), content_length)) {
                TURBOT_LOG_DEBUG("lsp[{}]: EOF/error reading body", server_id);
                goto done;
            }

            // 3. Parse JSON
            nlohmann::json msg;
            try {
                msg = nlohmann::json::parse(body);
            } catch (const std::exception& ex) {
                TURBOT_LOG_WARN("lsp[{}]: JSON parse error: {}", server_id, ex.what());
                continue;
            }

            // 4. Dispatch
            if (msg.contains("id") && !msg.contains("method")) {
                // Response to our request
                const int id = msg["id"].is_number() ? msg["id"].get<int>() : -1;
                std::promise<nlohmann::json> prom;
                bool found = false;
                {
                    std::lock_guard<std::mutex> lk(pending_mutex);
                    auto it = pending.find(id);
                    if (it != pending.end()) {
                        prom  = std::move(it->second);
                        pending.erase(it);
                        found = true;
                    }
                }
                if (found) {
                    if (msg.contains("error") && !msg["error"].is_null()) {
                        prom.set_exception(std::make_exception_ptr(
                            std::runtime_error(msg["error"].dump())));
                    } else {
                        prom.set_value(msg.value("result", nlohmann::json{}));
                    }
                }
            } else if (msg.contains("method") && msg.contains("id")) {
                // Server-initiated request
                handle_server_request(msg);
            } else if (msg.contains("method")) {
                // Notification
                handle_notification(msg);
            }
        }
    done:
        running.store(false);
        // Fail all pending requests
        std::lock_guard<std::mutex> lk(pending_mutex);
        for (auto& [id, prom] : pending) {
            prom.set_exception(std::make_exception_ptr(
                std::runtime_error("LSP connection closed")));
        }
        pending.clear();
    }

    // ── Initialize handshake ───────────────────────────────────────────────

    bool do_initialize() {
        const std::string root_uri = path_to_uri(root);
        nlohmann::json params = {
            {"rootUri",    root_uri},
            {"processId",  static_cast<int>(::getpid())},
            {"workspaceFolders", nlohmann::json::array({
                nlohmann::json::object({{"name", "workspace"}, {"uri", root_uri}})
            })},
            {"initializationOptions", handle.initialization.is_null()
                                          ? nlohmann::json::object()
                                          : handle.initialization},
            {"capabilities", {
                {"window", {{"workDoneProgress", true}}},
                {"workspace", {
                    {"configuration", true},
                    {"didChangeWatchedFiles", {{"dynamicRegistration", true}}},
                }},
                {"textDocument", {
                    {"synchronization", {{"didOpen", true}, {"didChange", true}}},
                    {"publishDiagnostics", {{"versionSupport", true}}},
                }},
            }},
        };

        TURBOT_LOG_INFO("lsp[{}]: sending initialize", server_id);
        auto fut = send_request("initialize", params);

        const auto status = fut.wait_for(std::chrono::milliseconds(kInitializeTimeoutMs));
        if (status != std::future_status::ready) {
            TURBOT_LOG_ERROR("lsp[{}]: initialize timed out ({}ms)", server_id, kInitializeTimeoutMs);
            return false;
        }
        try {
            fut.get();  // throws if server returned error
        } catch (const std::exception& ex) {
            TURBOT_LOG_ERROR("lsp[{}]: initialize error: {}", server_id, ex.what());
            return false;
        }

        // Send initialized notification
        send_notification("initialized", nlohmann::json::object());

        // Optional: workspace/didChangeConfiguration
        if (!handle.initialization.is_null() && handle.initialization.is_object()) {
            send_notification("workspace/didChangeConfiguration",
                              {{"settings", handle.initialization}});
        }

        TURBOT_LOG_INFO("lsp[{}]: initialized", server_id);
        return true;
    }
};

// ─── LSPClient public interface ───────────────────────────────────────────────

LSPClient::LSPClient() : impl_(std::make_unique<Impl>()) {}
LSPClient::~LSPClient() { shutdown(); }

std::unique_ptr<LSPClient> LSPClient::create(
    const std::string& server_id,
    const ServerHandle& handle,
    const std::string& root)
{
    auto client = std::unique_ptr<LSPClient>(new LSPClient());
    auto* impl  = client->impl_.get();

    impl->server_id = server_id;
    impl->handle    = handle;
    impl->root      = root;
    impl->running.store(true);

    // Start reader thread before sending initialize (so we can receive responses)
    impl->reader_thread = std::thread([impl]() { impl->reader_loop(); });

    // Initialize handshake
    if (!impl->do_initialize()) {
        impl->shutdown_flag.store(true);
        impl->running.store(false);
        // Close stdout_fd to unblock reader_thread (which may be blocked in read())
        if (impl->handle.stdout_fd >= 0) {
            ::close(impl->handle.stdout_fd);
            impl->handle.stdout_fd = -1;
        }
        if (impl->reader_thread.joinable()) impl->reader_thread.join();
        return nullptr;
    }

    return client;
}

const std::string& LSPClient::server_id() const noexcept { return impl_->server_id; }
const std::string& LSPClient::root()      const noexcept { return impl_->root;      }
int LSPClient::pid()                       const noexcept { return impl_->handle.pid; }

void LSPClient::shutdown() {
    if (!impl_) return;
    if (impl_->shutdown_flag.exchange(true)) return;  // already shut down

    TURBOT_LOG_INFO("lsp[{}]: shutting down", impl_->server_id);

    // Try graceful shutdown
    if (impl_->running.load()) {
        try {
            auto fut = impl_->send_request("shutdown", nullptr);
            fut.wait_for(std::chrono::milliseconds(2000));
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("lsp[{}]: shutdown request failed: {}", impl_->server_id, e.what());
        } catch (...) {
            TURBOT_LOG_DEBUG("lsp[{}]: shutdown request failed with unknown error", impl_->server_id);
        }
        impl_->send_notification("exit", nullptr);
    }

    impl_->running.store(false);
    // Close stdout_fd to unblock the reader thread
    if (impl_->handle.stdout_fd >= 0) {
        ::close(impl_->handle.stdout_fd);
        impl_->handle.stdout_fd = -1;
    }
    if (impl_->reader_thread.joinable()) {
        impl_->reader_thread.join();
    }
    if (impl_->handle.stdin_fd >= 0) {
        ::close(impl_->handle.stdin_fd);
        impl_->handle.stdin_fd = -1;
    }
    // Kill process
    if (impl_->handle.pid > 0) {
        ::kill(impl_->handle.pid, SIGTERM);
        impl_->handle.pid = -1;
    }

    // Cancel all debounce timers
    {
        std::lock_guard<std::mutex> lk(impl_->debounce_mutex);
        for (auto& [path, entry] : impl_->debounce_timers) {
            entry->cancelled.store(true);
            if (entry->timer.joinable()) entry->timer.detach();
        }
        impl_->debounce_timers.clear();
    }
}

// ── File notifications ─────────────────────────────────────────────────────

void LSPClient::notify_open(const std::string& path) {
    const std::string uri        = path_to_uri(path);
    const std::string ext        = path.size() >= 2 ? path.substr(path.rfind('.')) : std::string{};
    const std::string language   = language_id_for_extension(ext);

    // Read file content
    std::string text;
    {
        FILE* f = ::fopen(path.c_str(), "rb");
        if (f) {
            ::fseek(f, 0, SEEK_END);
            const long sz = ::ftell(f);
            ::rewind(f);
            if (sz > 0) {
                text.resize(static_cast<size_t>(sz));
                const size_t rd = ::fread(text.data(), 1, static_cast<size_t>(sz), f);
                text.resize(rd);
            }
            ::fclose(f);
        }
    }

    bool already_open = false;
    int version = 0;
    {
        std::lock_guard<std::mutex> lk(impl_->files_mutex);
        auto it = impl_->files.find(path);
        already_open = (it != impl_->files.end());
        if (already_open) {
            version = ++(it->second);
        }
    }

    if (already_open) {
        // Changed: didChangeWatchedFiles(Changed) + didChange
        TURBOT_LOG_INFO("lsp[{}]: notify_open (change) path={}", impl_->server_id, path);
        impl_->send_notification("workspace/didChangeWatchedFiles", {
            {"changes", nlohmann::json::array({
                nlohmann::json::object({{"uri", uri}, {"type", 2}})
            })}
        });
        impl_->send_notification("textDocument/didChange", {
            {"textDocument", {{"uri", uri}, {"version", version}}},
            {"contentChanges", nlohmann::json::array({
                nlohmann::json::object({{"text", text}})
            })},
        });
    } else {
        // Created: didChangeWatchedFiles(Created) + didOpen
        TURBOT_LOG_INFO("lsp[{}]: notify_open (open) path={}", impl_->server_id, path);
        {
            std::lock_guard<std::mutex> lk(impl_->files_mutex);
            impl_->files[path] = 0;
        }
        // Clear existing diagnostics (aligned with OpenCode: diagnostics.delete(path))
        {
            std::lock_guard<std::mutex> lk(impl_->diag_mutex);
            impl_->diagnostics_map.erase(path);
        }
        impl_->send_notification("workspace/didChangeWatchedFiles", {
            {"changes", nlohmann::json::array({
                nlohmann::json::object({{"uri", uri}, {"type", 1}})
            })}
        });
        impl_->send_notification("textDocument/didOpen", {
            {"textDocument", {
                {"uri",        uri},
                {"languageId", language},
                {"version",    0},
                {"text",       text},
            }},
        });
    }
}

void LSPClient::notify_change(const std::string& path, const std::string& content) {
    const std::string uri = path_to_uri(path);
    int version = 0;
    {
        std::lock_guard<std::mutex> lk(impl_->files_mutex);
        auto it = impl_->files.find(path);
        if (it != impl_->files.end()) {
            version = ++(it->second);
        } else {
            impl_->files[path] = 0;
        }
    }
    impl_->send_notification("textDocument/didChange", {
        {"textDocument",  {{"uri", uri}, {"version", version}}},
        {"contentChanges", nlohmann::json::array({
            nlohmann::json::object({{"text", content}})
        })},
    });
}

// ── Diagnostics ───────────────────────────────────────────────────────────

std::future<void> LSPClient::wait_for_diagnostics(const std::string& path) {
    const std::string norm_path = path;
    const std::string sid       = impl_->server_id;

    return std::async(std::launch::async, [norm_path, sid]() {
        std::mutex                  mtx;
        std::condition_variable     cv;
        std::atomic<bool>           done{false};
        std::string                 sub_id;

        // Subscribe to diagnostics events
        sub_id = turbot::core::EventBus::instance().subscribe<DiagnosticsEvent>(
            kDiagnosticsEventName,
            [&](const turbot::core::Event<DiagnosticsEvent>& ev) {
                if (ev.data.server_id == sid && ev.data.path == norm_path) {
                    done.store(true);
                    cv.notify_one();
                }
            });

        // Wait with 3000ms timeout (silent on timeout, aligned with OpenCode .catch(()=>{}))
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait_until(lk,
                std::chrono::steady_clock::now() + std::chrono::milliseconds(kDiagnosticsTimeoutMs),
                [&]{ return done.load(); });
        }

        turbot::core::EventBus::instance().unsubscribe(kDiagnosticsEventName, sub_id);
    });
}

std::unordered_map<std::string, std::vector<Diagnostic>> LSPClient::diagnostics() const {
    std::lock_guard<std::mutex> lk(impl_->diag_mutex);
    return impl_->diagnostics_map;
}

// ── LSP Requests ──────────────────────────────────────────────────────────

std::future<std::optional<Hover>> LSPClient::hover(const std::string& uri, Position pos) {
    return std::async(std::launch::async, [this, uri, pos]() -> std::optional<Hover> {
        try {
            auto fut = impl_->send_request("textDocument/hover", {
                {"textDocument", {{"uri", uri}}},
                {"position",     pos.to_json()},
            });
            const auto result = fut.get();
            if (result.is_null()) return std::nullopt;
            return Hover::from_json(result);
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("lsp[{}]: hover request failed: {}", impl_->server_id, e.what());
            return std::nullopt;
        } catch (...) {
            TURBOT_LOG_DEBUG("lsp[{}]: hover request failed with unknown error", impl_->server_id);
            return std::nullopt;
        }
    });
}

std::future<std::vector<Location>> LSPClient::definition(const std::string& uri, Position pos) {
    return std::async(std::launch::async, [this, uri, pos]() -> std::vector<Location> {
        try {
            auto fut = impl_->send_request("textDocument/definition", {
                {"textDocument", {{"uri", uri}}},
                {"position",     pos.to_json()},
            });
            const auto result = fut.get();
            std::vector<Location> out;
            if (result.is_array()) {
                for (const auto& item : result) out.push_back(Location::from_json(item));
            } else if (result.is_object()) {
                out.push_back(Location::from_json(result));
            }
            return out;
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("lsp[{}]: definition request failed: {}", impl_->server_id, e.what());
            return {};
        } catch (...) {
            TURBOT_LOG_DEBUG("lsp[{}]: definition request failed with unknown error", impl_->server_id);
            return {};
        }
    });
}

std::future<std::vector<Location>> LSPClient::references(const std::string& uri, Position pos) {
    return std::async(std::launch::async, [this, uri, pos]() -> std::vector<Location> {
        try {
            auto fut = impl_->send_request("textDocument/references", {
                {"textDocument", {{"uri", uri}}},
                {"position",     pos.to_json()},
                {"context",      {{"includeDeclaration", true}}},
            });
            const auto result = fut.get();
            std::vector<Location> out;
            if (result.is_array()) {
                for (const auto& item : result) out.push_back(Location::from_json(item));
            }
            return out;
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("lsp[{}]: references request failed: {}", impl_->server_id, e.what());
            return {};
        } catch (...) {
            TURBOT_LOG_DEBUG("lsp[{}]: references request failed with unknown error", impl_->server_id);
            return {};
        }
    });
}

std::future<std::vector<Location>> LSPClient::implementation(const std::string& uri, Position pos) {
    return std::async(std::launch::async, [this, uri, pos]() -> std::vector<Location> {
        try {
            auto fut = impl_->send_request("textDocument/implementation", {
                {"textDocument", {{"uri", uri}}},
                {"position",     pos.to_json()},
            });
            const auto result = fut.get();
            std::vector<Location> out;
            if (result.is_array()) {
                for (const auto& item : result) out.push_back(Location::from_json(item));
            } else if (result.is_object()) {
                out.push_back(Location::from_json(result));
            }
            return out;
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("lsp[{}]: implementation request failed: {}", impl_->server_id, e.what());
            return {};
        } catch (...) {
            TURBOT_LOG_DEBUG("lsp[{}]: implementation request failed with unknown error", impl_->server_id);
            return {};
        }
    });
}

std::future<std::vector<Symbol>> LSPClient::workspace_symbol(const std::string& query) {
    return std::async(std::launch::async, [this, query]() -> std::vector<Symbol> {
        try {
            auto fut = impl_->send_request("workspace/symbol", {{"query", query}});
            const auto result = fut.get();
            std::vector<Symbol> out;
            if (result.is_array()) {
                for (const auto& item : result) out.push_back(Symbol::from_json(item));
            }
            return out;
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("lsp[{}]: workspace_symbol request failed: {}", impl_->server_id, e.what());
            return {};
        } catch (...) {
            TURBOT_LOG_DEBUG("lsp[{}]: workspace_symbol request failed with unknown error", impl_->server_id);
            return {};
        }
    });
}

std::future<std::vector<DocumentSymbol>> LSPClient::document_symbol(const std::string& uri) {
    return std::async(std::launch::async, [this, uri]() -> std::vector<DocumentSymbol> {
        try {
            auto fut = impl_->send_request("textDocument/documentSymbol", {
                {"textDocument", {{"uri", uri}}},
            });
            const auto result = fut.get();
            std::vector<DocumentSymbol> out;
            if (result.is_array()) {
                for (const auto& item : result) out.push_back(DocumentSymbol::from_json(item));
            }
            return out;
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("lsp[{}]: document_symbol request failed: {}", impl_->server_id, e.what());
            return {};
        } catch (...) {
            TURBOT_LOG_DEBUG("lsp[{}]: document_symbol request failed with unknown error", impl_->server_id);
            return {};
        }
    });
}

// ── Call Hierarchy ────────────────────────────────────────────────────────

std::future<std::vector<nlohmann::json>> LSPClient::prepare_call_hierarchy(
    const std::string& uri, Position pos)
{
    return std::async(std::launch::async, [this, uri, pos]() -> std::vector<nlohmann::json> {
        try {
            auto fut = impl_->send_request("textDocument/prepareCallHierarchy", {
                {"textDocument", {{"uri", uri}}},
                {"position",     pos.to_json()},
            });
            const auto result = fut.get();
            std::vector<nlohmann::json> out;
            if (result.is_array()) {
                for (const auto& item : result) out.push_back(item);
            }
            return out;
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("lsp[{}]: prepare_call_hierarchy request failed: {}", impl_->server_id, e.what());
            return {};
        } catch (...) {
            TURBOT_LOG_DEBUG("lsp[{}]: prepare_call_hierarchy request failed with unknown error", impl_->server_id);
            return {};
        }
    });
}

std::future<std::vector<nlohmann::json>> LSPClient::incoming_calls(const nlohmann::json& item) {
    return std::async(std::launch::async, [this, item]() -> std::vector<nlohmann::json> {
        try {
            auto fut = impl_->send_request("callHierarchy/incomingCalls", {{"item", item}});
            const auto result = fut.get();
            std::vector<nlohmann::json> out;
            if (result.is_array()) {
                for (const auto& r : result) out.push_back(r);
            }
            return out;
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("lsp[{}]: incoming_calls request failed: {}", impl_->server_id, e.what());
            return {};
        } catch (...) {
            TURBOT_LOG_DEBUG("lsp[{}]: incoming_calls request failed with unknown error", impl_->server_id);
            return {};
        }
    });
}

std::future<std::vector<nlohmann::json>> LSPClient::outgoing_calls(const nlohmann::json& item) {
    return std::async(std::launch::async, [this, item]() -> std::vector<nlohmann::json> {
        try {
            auto fut = impl_->send_request("callHierarchy/outgoingCalls", {{"item", item}});
            const auto result = fut.get();
            std::vector<nlohmann::json> out;
            if (result.is_array()) {
                for (const auto& r : result) out.push_back(r);
            }
            return out;
        } catch (const std::exception& e) {
            TURBOT_LOG_DEBUG("lsp[{}]: outgoing_calls request failed: {}", impl_->server_id, e.what());
            return {};
        } catch (...) {
            TURBOT_LOG_DEBUG("lsp[{}]: outgoing_calls request failed with unknown error", impl_->server_id);
            return {};
        }
    });
}

}  // namespace turbot::core::lsp
