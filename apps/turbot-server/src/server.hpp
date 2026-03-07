#pragma once

#include <memory>

namespace turbot::server {

/// Server class - HTTP API server for Turbot AI
class Server {
public:
    Server();
    ~Server() = default;
    
    // Non-copyable, non-movable
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator=(Server&&) = delete;
    
    /// Start the server on the given port
    void start(int port);
    
    /// Stop the server
    void stop();
    
private:
    int server_fd_ = -1;
    bool running_ = false;
};

/// Get the global server instance
std::unique_ptr<Server>& get_server_instance();

} // namespace turbot::server
