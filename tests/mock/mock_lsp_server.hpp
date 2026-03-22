#pragma once

/**
 * @file mock_lsp_server.hpp
 * @brief Mock LSP Server for testing LSPClient and LSPManager
 *
 * This mock simulates an LSP server process by:
 * - Creating a pair of pipes for stdin/stdout communication
 * - Responding to LSP JSON-RPC requests
 * - Supporting customizable responses for different LSP methods
 */

#include <turbot/core/lsp/lsp.hpp>
#include <turbot/core/lsp/client.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <string>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>

namespace turbot::test {

/// LSP method identifiers
namespace lsp_method {
    constexpr const char* Initialize = "initialize";
    constexpr const char* Initialized = "initialized";
    constexpr const char* Shutdown = "shutdown";
    constexpr const char* Exit = "exit";
    constexpr const char* TextDocumentDidOpen = "textDocument/didOpen";
    constexpr const char* TextDocumentDidChange = "textDocument/didChange";
    constexpr const char* TextDocumentHover = "textDocument/hover";
    constexpr const char* TextDocumentDefinition = "textDocument/definition";
    constexpr const char* TextDocumentReferences = "textDocument/references";
    constexpr const char* TextDocumentPublishDiagnostics = "textDocument/publishDiagnostics";
    constexpr const char* WorkspaceSymbol = "workspace/symbol";
    constexpr const char* WorkspaceDidChangeConfiguration = "workspace/didChangeConfiguration";
}

/// Mock LSP Server configuration
struct MockLSPServerConfig {
    bool auto_respond_initialize = true;
    bool support_hover = true;
    bool support_definition = true;
    bool support_references = true;
    bool support_workspace_symbol = true;
    int response_delay_ms = 0;
    
    /// Server capabilities
    nlohmann::json capabilities = R"({
        "textDocumentSync": 1,
        "hoverProvider": true,
        "definitionProvider": true,
        "referencesProvider": true,
        "workspaceSymbolProvider": true,
        "documentSymbolProvider": true
    })"_json;
};

/// Mock LSP Server - simulates an LSP server process
class MockLSPServer {
public:
    MockLSPServer() = default;
    explicit MockLSPServer(const MockLSPServerConfig& config) : config_(config) {}
    
    ~MockLSPServer() {
        stop();
    }
    
    // Non-copyable
    MockLSPServer(const MockLSPServer&) = delete;
    MockLSPServer& operator=(const MockLSPServer&) = delete;
    
    /// Start the mock server with pipe file descriptors
    /// @param stdin_fd File descriptor to read from (client writes to this)
    /// @param stdout_fd File descriptor to write to (client reads from this)
    bool start(int stdin_fd, int stdout_fd) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) return false;
        
        stdin_fd_ = stdin_fd;
        stdout_fd_ = stdout_fd;
        running_ = true;
        
        // Start reader thread
        reader_thread_ = std::thread(&MockLSPServer::reader_loop, this);
        
        return true;
    }
    
    /// Stop the mock server
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) return;
            running_ = false;
        }
        cv_.notify_all();
        
        if (reader_thread_.joinable()) {
            reader_thread_.join();
        }
    }
    
    /// Check if server is running
    [[nodiscard]] bool is_running() const {
        return running_;
    }
    
    // === Response Configuration ===
    
    /// Set a custom response for a specific method
    void set_response(const std::string& method, const nlohmann::json& result) {
        std::lock_guard<std::mutex> lock(mutex_);
        custom_responses_[method] = result;
    }
    
    /// Set a response function for more complex scenarios
    using ResponseFunc = std::function<nlohmann::json(const nlohmann::json& params)>;
    void set_response_func(const std::string& method, ResponseFunc func) {
        std::lock_guard<std::mutex> lock(mutex_);
        response_funcs_[method] = std::move(func);
    }
    
    /// Queue a notification to be sent
    void queue_notification(const std::string& method, const nlohmann::json& params) {
        std::lock_guard<std::mutex> lock(mutex_);
        notification_queue_.push(make_notification(method, params));
        cv_.notify_one();
    }
    
    /// Queue a publishDiagnostics notification
    void queue_diagnostics(const std::string& uri, 
                          const std::vector<core::lsp::Diagnostic>& diagnostics) {
        nlohmann::json params;
        params["uri"] = uri;
        params["diagnostics"] = nlohmann::json::array();
        for (const auto& d : diagnostics) {
            params["diagnostics"].push_back(d.to_json());
        }
        queue_notification(lsp_method::TextDocumentPublishDiagnostics, params);
    }
    
    // === Request Recording ===
    
    /// Get all received requests
    [[nodiscard]] std::vector<nlohmann::json> get_requests() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return received_requests_;
    }
    
    /// Get the last request for a specific method
    [[nodiscard]] std::optional<nlohmann::json> get_last_request(const std::string& method) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = received_requests_.rbegin(); it != received_requests_.rend(); ++it) {
            if ((*it)["method"] == method) {
                return *it;
            }
        }
        return std::nullopt;
    }
    
    /// Clear recorded requests
    void clear_requests() {
        std::lock_guard<std::mutex> lock(mutex_);
        received_requests_.clear();
    }
    
    /// Get request count
    [[nodiscard]] size_t request_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return received_requests_.size();
    }
    
    // === Server State ===
    
    /// Check if initialize was called
    [[nodiscard]] bool was_initialized() const {
        return initialized_;
    }
    
    /// Check if shutdown was called
    [[nodiscard]] bool was_shutdown() const {
        return shutdown_;
    }
    
    /// Reset server state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        received_requests_.clear();
        custom_responses_.clear();
        response_funcs_.clear();
        notification_queue_ = {};
        initialized_ = false;
        shutdown_ = false;
    }
    
private:
    MockLSPServerConfig config_;
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutdown_{false};
    
    int stdin_fd_ = -1;
    int stdout_fd_ = -1;
    
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread reader_thread_;
    
    std::vector<nlohmann::json> received_requests_;
    std::unordered_map<std::string, nlohmann::json> custom_responses_;
    std::unordered_map<std::string, ResponseFunc> response_funcs_;
    std::queue<nlohmann::json> notification_queue_;
    
    void reader_loop() {
        while (running_) {
            // Read Content-Length header
            std::string header;
            char c;
            while (running_ && read(stdin_fd_, &c, 1) == 1) {
                header += c;
                if (header.size() >= 4 && 
                    header.substr(header.size() - 4) == "\r\n\r\n") {
                    break;
                }
            }
            
            if (!running_) break;
            
            // Parse Content-Length
            size_t content_length = 0;
            size_t pos = header.find("Content-Length: ");
            if (pos != std::string::npos) {
                content_length = std::stoul(header.substr(pos + 16));
            }
            
            if (content_length == 0) continue;
            
            // Read content
            std::string content(content_length, '\0');
            size_t read_total = 0;
            while (running_ && read_total < content_length) {
                ssize_t n = read(stdin_fd_, &content[read_total], content_length - read_total);
                if (n <= 0) break;
                read_total += n;
            }
            
            if (!running_) break;
            
            // Parse JSON-RPC request
            try {
                auto request = nlohmann::json::parse(content);
                handle_request(request);
            } catch (const std::exception& e) {
                // Invalid JSON, ignore
            }
        }
    }
    
    void handle_request(const nlohmann::json& request) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            received_requests_.push_back(request);
        }
        
        std::string method = request["method"];
        auto id = request.contains("id") ? std::optional(request["id"]) : std::nullopt;
        
        // Handle special methods
        if (method == lsp_method::Initialize) {
            initialized_ = true;
            if (config_.auto_respond_initialize && id.has_value()) {
                nlohmann::json result;
                result["capabilities"] = config_.capabilities;
                result["serverInfo"] = {{"name", "MockLSPServer"}, {"version", "1.0.0"}};
                send_response(*id, result);
            }
            return;
        }
        
        if (method == lsp_method::Initialized) {
            // Notification, no response needed
            return;
        }
        
        if (method == lsp_method::Shutdown) {
            shutdown_ = true;
            if (id.has_value()) {
                send_response(*id, nullptr);
            }
            return;
        }
        
        if (method == lsp_method::Exit) {
            // Exit notification
            running_ = false;
            return;
        }
        
        // Check for custom response function
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (response_funcs_.count(method)) {
                auto params = request.contains("params") ? request["params"] : nlohmann::json::object();
                auto result = response_funcs_[method](params);
                if (id.has_value()) {
                    send_response(*id, result);
                }
                return;
            }
            
            // Check for custom response
            if (custom_responses_.count(method)) {
                if (id.has_value()) {
                    send_response(*id, custom_responses_[method]);
                }
                return;
            }
        }
        
        // Default responses for common methods
        if (id.has_value()) {
            auto result = make_default_response(method, request);
            send_response(*id, result);
        }
    }
    
    [[nodiscard]] nlohmann::json make_default_response(
        const std::string& method, 
        const nlohmann::json& request
    ) const {
        auto params = request.contains("params") ? request["params"] : nlohmann::json::object();
        
        if (method == lsp_method::TextDocumentHover) {
            nlohmann::json result;
            result["contents"] = "Mock hover content";
            return result;
        }
        
        if (method == lsp_method::TextDocumentDefinition) {
            // Return empty array
            return nlohmann::json::array();
        }
        
        if (method == lsp_method::TextDocumentReferences) {
            return nlohmann::json::array();
        }
        
        if (method == lsp_method::WorkspaceSymbol) {
            return nlohmann::json::array();
        }
        
        // Default: return null
        return nullptr;
    }
    
    void send_response(const nlohmann::json& id, const nlohmann::json& result) {
        nlohmann::json response;
        response["jsonrpc"] = "2.0";
        response["id"] = id;
        if (!result.is_null()) {
            response["result"] = result;
        }
        
        send_message(response);
    }
    
    void send_notification(const std::string& method, const nlohmann::json& params) {
        send_message(make_notification(method, params));
    }
    
    void send_message(const nlohmann::json& message) {
        std::string content = message.dump();
        std::string header = "Content-Length: " + std::to_string(content.size()) + "\r\n\r\n";
        
        // Apply delay if configured
        if (config_.response_delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.response_delay_ms));
        }
        
        write(stdout_fd_, header.c_str(), header.size());
        write(stdout_fd_, content.c_str(), content.size());
    }
    
    [[nodiscard]] static nlohmann::json make_notification(
        const std::string& method, 
        const nlohmann::json& params
    ) {
        nlohmann::json notification;
        notification["jsonrpc"] = "2.0";
        notification["method"] = method;
        notification["params"] = params;
        return notification;
    }
};

/// LSP Server Process Mock - creates a mock process that can be used with LSPClient
class MockLSPServerProcess {
public:
    MockLSPServerProcess() = default;
    
    ~MockLSPServerProcess() {
        stop();
    }
    
    /// Start the mock server process
    /// @return ServerHandle that can be passed to LSPClient::create
    [[nodiscard]] std::optional<turbot::core::lsp::ServerHandle> start(const MockLSPServerConfig& config = {}) {
        // Create pipes
        int stdin_pipe[2];  // Client writes, server reads
        int stdout_pipe[2]; // Server writes, client reads
        
        if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0) {
            return std::nullopt;
        }
        
        // Fork to create a child process
        pid_t pid = fork();
        if (pid < 0) {
            ::close(stdin_pipe[0]);
            ::close(stdin_pipe[1]);
            ::close(stdout_pipe[0]);
            ::close(stdout_pipe[1]);
            return std::nullopt;
        }
        
        if (pid == 0) {
            // Child process - run the mock server
            ::close(stdin_pipe[1]);  // Close write end
            ::close(stdout_pipe[0]); // Close read end
            
            MockLSPServer server(config);
            server.start(stdin_pipe[0], stdout_pipe[1]);
            
            // Wait until stopped
            while (server.is_running()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            _exit(0);
        }
        
        // Parent process
        ::close(stdin_pipe[0]);  // Close read end
        ::close(stdout_pipe[1]); // Close write end
        
        turbot::core::lsp::ServerHandle handle;
        handle.pid = pid;
        handle.stdin_fd = stdin_pipe[1];   // Write to child's stdin
        handle.stdout_fd = stdout_pipe[0]; // Read from child's stdout
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            handle_ = handle;
            running_ = true;
        }
        
        return handle;
    }
    
    /// Stop the mock server process
    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;
        
        if (handle_.pid > 0) {
            kill(handle_.pid, SIGTERM);
            int status;
            waitpid(handle_.pid, &status, 0);
        }
        
        if (handle_.stdin_fd >= 0) ::close(handle_.stdin_fd);
        if (handle_.stdout_fd >= 0) ::close(handle_.stdout_fd);
        
        running_ = false;
    }
    
    /// Get the server handle
    [[nodiscard]] std::optional<turbot::core::lsp::ServerHandle> get_handle() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) return handle_;
        return std::nullopt;
    }
    
    /// Check if the server is running
    [[nodiscard]] bool is_running() const {
        return running_;
    }
    
private:
    mutable std::mutex mutex_;
    turbot::core::lsp::ServerHandle handle_;
    bool running_ = false;
};

/// In-process LSP Server Mock - for testing without fork
/// This version runs in the same process and uses socket pairs
class MockLSPServerInProcess {
public:
    MockLSPServerInProcess() = default;
    
    ~MockLSPServerInProcess() {
        stop();
    }
    
    /// Start the mock server
    /// @return Pair of file descriptors (stdin_fd for client to write, stdout_fd for client to read)
    [[nodiscard]] std::pair<int, int> start(const MockLSPServerConfig& config = {}) {
        // Create socket pairs for bidirectional communication
        int sv[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
            return {-1, -1};
        }
        
        // sv[0] will be used by the server
        // sv[1] will be used by the client
        
        server_ = std::make_unique<MockLSPServer>(config);
        server_->start(sv[0], sv[0]);  // Same fd for read and write
        
        return {sv[1], sv[1]};  // Client uses same fd for read and write
    }
    
    /// Stop the mock server
    void stop() {
        if (server_) {
            server_->stop();
            server_.reset();
        }
    }
    
    /// Get the underlying mock server
    [[nodiscard]] MockLSPServer* get_server() const {
        return server_.get();
    }
    
private:
    std::unique_ptr<MockLSPServer> server_;
};

} // namespace turbot::test
