#include <turbot/core/version.hpp>
#include <turbot/core/logger.hpp>
#include <turbot/utils/string_utils.hpp>
#include <turbot/network/http_client.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string_view>

using namespace turbot;

void print_usage(std::string_view program_name) {
    fmt::print("Usage: {} <command> [options]\n", program_name);
    fmt::print("\nCommands:\n");
    fmt::print("  version    Show version information\n");
    fmt::print("  test       Run basic functionality test\n");
    fmt::print("  help       Show this help message\n");
}

void print_version() {
    fmt::print("{} version {}\n", core::Version::name(), core::Version::string());
    fmt::print("C++ Standard: {}\n", __cplusplus);
}

void run_tests() {
    fmt::print("Running Turbot AI tests...\n\n");

    // Test string utilities
    fmt::print("1. Testing string utilities:\n");
    auto trimmed = utils::trim("  hello world  ");
    fmt::print("   trim: '{}' -> '{}'\n", "  hello world  ", trimmed);

    auto upper = utils::to_upper("hello");
    fmt::print("   to_upper: 'hello' -> '{}'\n", upper);

    // Test HTTP client
    fmt::print("\n2. Testing HTTP client:\n");
    network::HttpClient client;
    auto response = client.get("https://api.example.com/test");
    fmt::print("   GET response status: {}\n", response.status_code);
    fmt::print("   GET response body: {}\n", response.body);

    // Test JSON
    fmt::print("\n3. Testing JSON parsing:\n");
    auto json = nlohmann::json::parse(response.body);
    fmt::print("   Parsed JSON: {}\n", json.dump(2));

    fmt::print("\nAll tests completed successfully!\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string_view command = argv[1];

    if (command == "version" || command == "-v" || command == "--version") {
        print_version();
        return 0;
    }

    if (command == "help" || command == "-h" || command == "--help") {
        print_usage(argv[0]);
        return 0;
    }

    if (command == "test") {
        run_tests();
        return 0;
    }

    fmt::print(stderr, "Unknown command: {}\n", command);
    print_usage(argv[0]);
    return 1;
}
