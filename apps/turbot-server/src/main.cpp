#include <turbot/core/common/version.hpp>
#include <turbot/core/common/logger.hpp>
#include "server.hpp"
#include <fmt/format.h>
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>

using namespace turbot;

volatile std::sig_atomic_t g_running = 1;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        TURBOT_LOG_INFO("Received signal {}, shutting down...", signal);
        g_running = 0;
        // Stop the server
        auto& server = server::get_server_instance();
        if (server) {
            server->stop();
        }
    }
}

void print_usage(const char* program_name) {
    fmt::print("Usage: {} [options]\n", program_name);
    fmt::print("\nOptions:\n");
    fmt::print("  --port <port>  Server port (default: 4096)\n");
    fmt::print("  --help         Show this help message\n");
    fmt::print("  --version      Show version information\n");
    fmt::print("\nAPI Endpoints:\n");
    fmt::print("  GET  /health              Health check\n");
    fmt::print("  GET  /api/v1/sessions     List sessions\n");
    fmt::print("  POST /api/v1/sessions     Create session\n");
    fmt::print("  GET  /api/v1/sessions/<id> Get session\n");
    fmt::print("  POST /api/v1/sessions/<id>/messages  Send message\n");
    fmt::print("  GET  /api/v1/agents       List agents\n");
}

int main(int argc, char* argv[]) {
    // Set up signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Parse command line arguments
    int port = 4096;
    
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            fmt::print("{} version {}\n", core::Version::name(), core::Version::string());
            return 0;
        } else {
            fmt::print(stderr, "Unknown option: {}\n", arg);
            print_usage(argv[0]);
            return 1;
        }
    }

    TURBOT_LOG_INFO("Starting {} server v{}",
        core::Version::name(),
        core::Version::string());

    fmt::print("Turbot AI Server\n");
    fmt::print("Version: {}\n\n", core::Version::string());
    fmt::print("Starting server on port {}...\n", port);

    // Create and start the server
    auto& server = server::get_server_instance();
    
    // Run server in a separate thread
    std::thread server_thread([&server, port]() {
        server->start(port);
    });
    
    // Wait for shutdown signal
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // Stop the server
    server->stop();
    
    // Wait for server thread to finish
    if (server_thread.joinable()) {
        server_thread.join();
    }

    TURBOT_LOG_INFO("Server stopped gracefully");
    fmt::print("\nServer stopped.\n");

    return 0;
}
