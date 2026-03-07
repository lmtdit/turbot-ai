#include <turbot/core/common/logger.hpp>
#include <turbot/core/session/session.hpp>
#include <turbot/core/session/session_loop.hpp>
#include <turbot/core/agent/agent.hpp>
#include <turbot/core/agent/builtin/build_agent.hpp>
#include <turbot/core/agent/builtin/plan_agent.hpp>
#include <turbot/core/agent/builtin/explore_agent.hpp>
#include <turbot/core/tool/tool_registry.hpp>
#include "server.hpp"
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace turbot::server {

using json = nlohmann::json;

// Global registry for active sessions
std::map<std::string, std::unique_ptr<core::session::SessionLoop>> g_sessions;
std::mutex g_sessions_mutex;

/// Initialize default agents
void init_agents() {
    auto& registry = core::agent::AgentRegistry::instance();
    registry.clear();
    registry.register_agent(std::make_shared<core::agent::BuildAgent>());
    registry.register_agent(std::make_shared<core::agent::PlanAgent>());
    registry.register_agent(std::make_shared<core::agent::ExploreAgent>());
}

/// Parse HTTP request
struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> headers;
    
    static HttpRequest parse(const std::string& raw) {
        HttpRequest req;
        size_t pos = 0;
        
        // Parse request line
        size_t line_end = raw.find("\r\n");
        if (line_end == std::string::npos) return req;
        
        std::string line = raw.substr(0, line_end);
        size_t first_space = line.find(' ');
        if (first_space != std::string::npos) {
            req.method = line.substr(0, first_space);
            size_t second_space = line.find(' ', first_space + 1);
            if (second_space != std::string::npos) {
                req.path = line.substr(first_space + 1, second_space - first_space - 1);
            }
        }
        
        // Parse headers and body
        pos = line_end + 2;
        while (pos < raw.size()) {
            line_end = raw.find("\r\n", pos);
            if (line_end == std::string::npos) break;
            
            line = raw.substr(pos, line_end - pos);
            if (line.empty()) {
                // Empty line marks end of headers
                pos = line_end + 2;
                if (pos < raw.size()) {
                    req.body = raw.substr(pos);
                }
                break;
            }
            
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string value = line.substr(colon + 1);
                // Trim leading space
                if (!value.empty() && value[0] == ' ') {
                    value = value.substr(1);
                }
                req.headers[key] = value;
            }
            
            pos = line_end + 2;
        }
        
        return req;
    }
};

/// Build HTTP response
std::string build_response(int status, const std::string& body, const std::string& content_type = "application/json") {
    std::string status_text;
    switch (status) {
        case 200: status_text = "OK"; break;
        case 201: status_text = "Created"; break;
        case 400: status_text = "Bad Request"; break;
        case 404: status_text = "Not Found"; break;
        case 500: status_text = "Internal Server Error"; break;
        default: status_text = "Unknown";
    }
    
    return fmt::format(
        "HTTP/1.1 {} {}\r\n"
        "Content-Type: {}\r\n"
        "Content-Length: {}\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{}",
        status, status_text, content_type, body.size(), body
    );
}

/// Handle API request
std::string handle_api_request(const HttpRequest& req) {
    json response;
    
    try {
        // GET /api/v1/sessions - List sessions
        if (req.method == "GET" && req.path == "/api/v1/sessions") {
            json sessions = json::array();
            
            std::lock_guard<std::mutex> lock(g_sessions_mutex);
            for (const auto& [id, loop] : g_sessions) {
                sessions.push_back({
                    {"id", id},
                    {"state", core::session::session_state_to_string(loop->session().state())},
                    {"messages", loop->messages().size()},
                    {"tokens", loop->token_count()}
                });
            }
            
            response = {{"sessions", sessions}};
            return build_response(200, response.dump(2));
        }
        
        // POST /api/v1/sessions - Create session
        if (req.method == "POST" && req.path == "/api/v1/sessions") {
            auto body = json::parse(req.body);
            
            core::session::CreateParams params;
            params.project_id = body.value("project_id", "api");
            params.slug = body.value("slug", "session");
            params.directory = body.value("directory", "/tmp/turbot");
            params.title = body.value("title", "API Session");
            
            auto session = core::session::Session::create(params);
            if (!session) {
                response = {{"error", "Failed to create session"}};
                return build_response(500, response.dump(2));
            }
            
            auto loop = std::make_unique<core::session::SessionLoop>(*session);
            auto agent = core::agent::AgentRegistry::instance().get("build");
            if (agent) {
                loop->set_agent(agent);
            }
            
            std::string session_id = session->id();
            
            std::lock_guard<std::mutex> lock(g_sessions_mutex);
            g_sessions[session_id] = std::move(loop);
            
            response = {
                {"id", session_id},
                {"project_id", params.project_id},
                {"slug", params.slug},
                {"title", params.title}
            };
            return build_response(201, response.dump(2));
        }
        
        // GET /api/v1/sessions/{id} - Get session
        if (req.method == "GET" && req.path.find("/api/v1/sessions/") == 0) {
            std::string session_id = req.path.substr(19);
            
            std::lock_guard<std::mutex> lock(g_sessions_mutex);
            auto it = g_sessions.find(session_id);
            if (it == g_sessions.end()) {
                response = {{"error", "Session not found"}};
                return build_response(404, response.dump(2));
            }
            
            auto& loop = it->second;
            response = {
                {"id", session_id},
                {"state", core::session::session_state_to_string(loop->session().state())},
                {"messages", loop->messages().size()},
                {"tokens", loop->token_count()}
            };
            return build_response(200, response.dump(2));
        }
        
        // POST /api/v1/sessions/{id}/messages - Send message
        if (req.method == "POST" && req.path.find("/api/v1/sessions/") == 0 && 
            req.path.find("/messages") != std::string::npos) {
            
            size_t sessions_pos = req.path.find("/api/v1/sessions/");
            size_t messages_pos = req.path.find("/messages");
            std::string session_id = req.path.substr(sessions_pos + 19, messages_pos - sessions_pos - 19);
            
            auto body = json::parse(req.body);
            std::string message = body.value("message", "");
            
            if (message.empty()) {
                response = {{"error", "Message is required"}};
                return build_response(400, response.dump(2));
            }
            
            std::lock_guard<std::mutex> lock(g_sessions_mutex);
            auto it = g_sessions.find(session_id);
            if (it == g_sessions.end()) {
                response = {{"error", "Session not found"}};
                return build_response(404, response.dump(2));
            }
            
            auto result = it->second->run(message);
            
            response = {
                {"result", core::session::loop_result_to_string(result)},
                {"tokens", it->second->token_count()}
            };
            return build_response(200, response.dump(2));
        }
        
        // GET /api/v1/agents - List agents
        if (req.method == "GET" && req.path == "/api/v1/agents") {
            json agents = json::array();
            auto list = core::agent::AgentRegistry::instance().list();
            for (const auto& agent : list) {
                agents.push_back({
                    {"name", agent->name()},
                    {"mode", core::agent::agent_mode_to_string(agent->info().mode)},
                    {"native", agent->info().native}
                });
            }
            response = {{"agents", agents}};
            return build_response(200, response.dump(2));
        }
        
        // GET /health - Health check
        if (req.method == "GET" && req.path == "/health") {
            response = {{"status", "ok"}};
            return build_response(200, response.dump(2));
        }
        
        // Unknown endpoint
        response = {{"error", "Not found"}};
        return build_response(404, response.dump(2));
        
    } catch (const std::exception& e) {
        response = {{"error", e.what()}};
        return build_response(500, response.dump(2));
    }
}

/// Handle client connection
void handle_client(int client_fd) {
    char buffer[4096] = {0};
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }
    
    std::string raw_request(buffer, static_cast<size_t>(bytes_read));
    HttpRequest req = HttpRequest::parse(raw_request);
    
    TURBOT_LOG_INFO("{} {}", req.method, req.path);
    
    std::string response = handle_api_request(req);
    write(client_fd, response.c_str(), response.size());
    close(client_fd);
}

/// Server implementation
Server::Server() {
    TURBOT_LOG_INFO("Server instance created");
}

void Server::start(int port) {
    TURBOT_LOG_INFO("Starting server on port {}", port);
    
    // Initialize agents
    init_agents();
    
    // Create socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        TURBOT_LOG_ERROR("Failed to create socket");
        return;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd_, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
        TURBOT_LOG_ERROR("Failed to bind socket");
        close(server_fd_);
        return;
    }
    
    // Listen
    if (listen(server_fd_, 10) < 0) {
        TURBOT_LOG_ERROR("Failed to listen on socket");
        close(server_fd_);
        return;
    }
    
    running_ = true;
    TURBOT_LOG_INFO("Server listening on port {}", port);
    
    // Accept connections
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            if (running_) {
                TURBOT_LOG_ERROR("Failed to accept connection");
            }
            continue;
        }
        
        // Handle in background thread
        std::thread(handle_client, client_fd).detach();
    }
}

void Server::stop() {
    running_ = false;
    if (server_fd_ >= 0) {
        close(server_fd_);
    }
    TURBOT_LOG_INFO("Server stopped");
}

// Global server instance
static std::unique_ptr<Server> g_server;

/// Get the global server instance
std::unique_ptr<Server>& get_server_instance() {
    if (!g_server) {
        g_server = std::make_unique<Server>();
    }
    return g_server;
}

} // namespace turbot::server
