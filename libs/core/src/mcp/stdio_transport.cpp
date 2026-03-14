#include <turbot/core/mcp/stdio_transport.hpp>
#include <turbot/core/common/logger.hpp>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>

namespace turbot::core::mcp {

namespace {
// Token 脱敏：含 Authorization/Bearer/token/secret 关键字的行脱敏后再输出日志
bool is_sensitive_line(const std::string& line) {
    static const char* keywords[] = {"Authorization", "Bearer", "token", "secret"};
    for (const char* kw : keywords) {
        if (line.find(kw) != std::string::npos) return true;
    }
    return false;
}
}  // namespace

StdioTransport::StdioTransport(
    std::vector<std::string> command,
    std::unordered_map<std::string, std::string> env
) : command_(std::move(command)), env_(std::move(env)) {}

StdioTransport::~StdioTransport() {
    if (!closed_.load()) {
        try { close().get(); } catch (...) {}
    }
    if (read_thread_.joinable()) {
        read_thread_.join();
    }
    if (stderr_thread_.joinable()) {
        stderr_thread_.join();
    }
}

std::future<void> StdioTransport::connect() {
    return std::async(std::launch::async, [this]() {
        if (command_.empty()) {
            throw std::runtime_error("StdioTransport: empty command");
        }

        // Create pipes: [child_stdin_read, parent_stdin_write]
        //               [parent_stdout_read, child_stdout_write]
        //               [parent_stderr_read, child_stderr_write]
        int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
        auto close_pipe = [](int (&p)[2]) { ::close(p[0]); ::close(p[1]); };
        if (pipe(stdin_pipe) < 0) {
            throw std::runtime_error(std::string("pipe(stdin) failed: ") + strerror(errno));
        }
        if (pipe(stdout_pipe) < 0) {
            close_pipe(stdin_pipe);
            throw std::runtime_error(std::string("pipe(stdout) failed: ") + strerror(errno));
        }
        if (pipe(stderr_pipe) < 0) {
            close_pipe(stdin_pipe);
            close_pipe(stdout_pipe);
            throw std::runtime_error(std::string("pipe(stderr) failed: ") + strerror(errno));
        }

        pid_t pid = fork();
        if (pid < 0) {
            throw std::runtime_error(std::string("fork() failed: ") + strerror(errno));
        }

        if (pid == 0) {
            // Child process
            // Redirect stdin/stdout/stderr
            dup2(stdin_pipe[0], STDIN_FILENO);
            dup2(stdout_pipe[1], STDOUT_FILENO);
            dup2(stderr_pipe[1], STDERR_FILENO);

            // Close unused pipe ends
            ::close(stdin_pipe[1]);
            ::close(stdout_pipe[0]);
            ::close(stderr_pipe[0]);
            ::close(stdin_pipe[0]);
            ::close(stdout_pipe[1]);
            ::close(stderr_pipe[1]);

            // Set environment variables
            for (const auto& [key, val] : env_) {
                setenv(key.c_str(), val.c_str(), 1);
            }

            // Build argv
            std::vector<char*> argv;
            for (auto& arg : command_) {
                argv.push_back(const_cast<char*>(arg.c_str()));
            }
            argv.push_back(nullptr);

            execvp(argv[0], argv.data());
            // If execvp fails:
            _exit(127);
        }

        // Parent process
        child_pid_ = pid;
        stdin_fd_  = stdin_pipe[1];
        stdout_fd_ = stdout_pipe[0];
        stderr_fd_ = stderr_pipe[0];

        // Close unused pipe ends
        ::close(stdin_pipe[0]);
        ::close(stdout_pipe[1]);
        ::close(stderr_pipe[1]);

        // Start io read thread
        read_thread_ = std::thread([this]() { io_read_loop(); });

        // Start stderr logging thread (joined on close to avoid use-after-close)
        stderr_thread_ = std::thread([this]() {
            char buf[256];
            while (!closed_.load()) {
                ssize_t n = read(stderr_fd_, buf, sizeof(buf) - 1);
                if (n <= 0) break;
                buf[n] = '\0';
                std::string line(buf, static_cast<size_t>(n));
                if (is_sensitive_line(line)) {
                    TURBOT_LOG_DEBUG("MCP stderr: [REDACTED]");
                } else {
                    TURBOT_LOG_DEBUG("MCP stderr: {}", line);
                }
            }
        });
    });
}

void StdioTransport::write_line(const std::string& line) {
    std::lock_guard<std::mutex> lock(write_mutex_);
    if (stdin_fd_ < 0) return;
    std::string msg = line + "\n";
    ssize_t written = 0;
    ssize_t total = static_cast<ssize_t>(msg.size());
    while (written < total) {
        ssize_t n = write(stdin_fd_, msg.data() + written, static_cast<size_t>(total - written));
        if (n < 0) {
            if (errno == EINTR) continue;
            TURBOT_LOG_ERROR("StdioTransport: write failed: {}", strerror(errno));
            break;
        }
        written += n;
    }
}

std::future<JsonRpcResponse> StdioTransport::send_request(
    const std::string& method,
    const nlohmann::json& params
) {
    int64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);

    std::promise<JsonRpcResponse> promise;
    auto future = promise.get_future();

    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_[id] = PendingRequest{std::move(promise)};
    }

    JsonRpcRequest req;
    req.id = id;
    req.method = method;
    req.params = params;

    try {
        write_line(req.to_json().dump());
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        auto it = pending_.find(id);
        if (it != pending_.end()) {
            it->second.promise.set_value(
                JsonRpcResponse::make_error(id, -32000, std::string("write failed: ") + e.what())
            );
            pending_.erase(it);
        }
    }

    return future;
}

void StdioTransport::send_notification(const std::string& method, const nlohmann::json& params) {
    JsonRpcRequest req;
    // no id → notification
    req.method = method;
    req.params = params;
    write_line(req.to_json().dump());
}

void StdioTransport::on_notification(
    const std::string& method,
    std::function<void(const nlohmann::json&)> handler
) {
    std::lock_guard<std::mutex> lock(notification_mutex_);
    notification_handlers_[method] = std::move(handler);
}

std::future<void> StdioTransport::close() {
    return std::async(std::launch::async, [this]() {
        if (closed_.exchange(true)) return;  // already closed

        // Close stdin pipe → signal EOF to child
        {
            std::lock_guard<std::mutex> lock(write_mutex_);
            if (stdin_fd_ >= 0) {
                ::close(stdin_fd_);
                stdin_fd_ = -1;
            }
        }

        // Send SIGTERM to child
        if (child_pid_ > 0) {
            ::kill(child_pid_, SIGTERM);
            // Wait briefly for graceful exit
            int status;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (waitpid(child_pid_, &status, WNOHANG) == 0) {
                ::kill(child_pid_, SIGKILL);
                waitpid(child_pid_, &status, 0);
            }
            child_pid_ = -1;
        }

        // Close remaining fds
        if (stdout_fd_ >= 0) { ::close(stdout_fd_); stdout_fd_ = -1; }
        if (stderr_fd_ >= 0) { ::close(stderr_fd_); stderr_fd_ = -1; }

        // Fail all pending requests
        cleanup_pending("Transport closed");
    });
}

int StdioTransport::pid() const noexcept {
    return child_pid_;
}

void StdioTransport::io_read_loop() {
    std::string buffer;
    char chunk[4096];

    while (!closed_.load()) {
        ssize_t n = read(stdout_fd_, chunk, sizeof(chunk));
        if (n <= 0) {
            // EOF or error
            break;
        }
        buffer.append(chunk, static_cast<size_t>(n));

        // Process complete newline-delimited JSON lines
        size_t pos = 0;
        while (true) {
            size_t nl = buffer.find('\n', pos);
            if (nl == std::string::npos) break;

            std::string line = buffer.substr(pos, nl - pos);
            // Trim trailing \r
            if (!line.empty() && line.back() == '\r') line.pop_back();
            pos = nl + 1;

            if (line.empty()) continue;

            try {
                nlohmann::json msg = nlohmann::json::parse(line);

                if (msg.contains("id") && !msg["id"].is_null()) {
                    // Response: dispatch to pending promise
                    int64_t id = msg["id"].get<int64_t>();
                    std::lock_guard<std::mutex> lock(pending_mutex_);
                    auto it = pending_.find(id);
                    if (it != pending_.end()) {
                        it->second.promise.set_value(JsonRpcResponse::from_json(msg));
                        pending_.erase(it);
                    }
                } else if (msg.contains("method")) {
                    // Notification: dispatch to handler
                    std::string method = msg["method"].get<std::string>();
                    std::function<void(const nlohmann::json&)> handler;
                    {
                        std::lock_guard<std::mutex> lock(notification_mutex_);
                        auto it = notification_handlers_.find(method);
                        if (it != notification_handlers_.end()) {
                            handler = it->second;
                        }
                    }
                    if (handler) {
                        try {
                            handler(msg.value("params", nlohmann::json::object()));
                        } catch (const std::exception& e) {
                            TURBOT_LOG_ERROR("StdioTransport: notification handler threw: {}", e.what());
                        }
                    }
                }
            } catch (const nlohmann::json::parse_error& e) {
                TURBOT_LOG_ERROR("StdioTransport: JSON parse error: {}", e.what());
                // Continue reading — do not crash
            }
        }
        buffer = buffer.substr(pos);
    }

    cleanup_pending("Transport connection closed");
}

void StdioTransport::cleanup_pending(const std::string& error_message) {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    for (auto& [id, req] : pending_) {
        try {
            req.promise.set_value(JsonRpcResponse::make_error(id, -32000, error_message));
        } catch (...) {
            // promise already satisfied
        }
    }
    pending_.clear();
}

}  // namespace turbot::core::mcp
