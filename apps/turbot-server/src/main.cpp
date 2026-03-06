#include <turbot/core/version.hpp>
#include <turbot/core/logger.hpp>
#include <fmt/format.h>
#include <iostream>
#include <csignal>

using namespace turbot;

volatile std::sig_atomic_t g_running = 1;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        TURBOT_LOG_INFO("Received signal {}, shutting down...", signal);
        g_running = 0;
    }
}

int main(int argc, char* argv[]) {
    // Set up signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    TURBOT_LOG_INFO("Starting {} server v{}",
        core::Version::name(),
        core::Version::string());

    fmt::print("Turbot AI Server\n");
    fmt::print("Version: {}\n\n", core::Version::string());

    // Server placeholder
    fmt::print("Server is running (placeholder implementation)\n");
    fmt::print("Press Ctrl+C to stop\n\n");

    // Main loop
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    TURBOT_LOG_INFO("Server stopped gracefully");
    fmt::print("\nServer stopped.\n");

    return 0;
}
