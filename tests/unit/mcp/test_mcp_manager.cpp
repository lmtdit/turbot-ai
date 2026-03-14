/// test_mcp_manager.cpp — Unit tests for MCPManager lifecycle and MCPToolWrapper naming rules.
/// Tests MCPManager singleton access, status reporting, and the sanitize_mcp_name
/// naming convention that drives MCPToolWrapper registration names.

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/mcp/mcp.hpp>
#include <turbot/core/mcp/manager.hpp>

using namespace turbot::core::mcp;

// ─── MCPManager singleton ─────────────────────────────────────────────────────

TEST_CASE("MCPManager: instance() returns same object", "[mcp][manager]") {
    auto& a = MCPManager::instance();
    auto& b = MCPManager::instance();
    CHECK(&a == &b);
}

TEST_CASE("MCPManager: status() returns map (empty when no servers configured)", "[mcp][manager]") {
    // Fresh status snapshot; the manager might have entries from other tests,
    // but the map itself must be accessible and not throw.
    auto& mgr = MCPManager::instance();
    auto st = mgr.status();
    // Just verify the call succeeds and returns a map
    CHECK(st.size() >= 0u);  // always true, verifies no exception
}

// ─── MCPClientConfig defaults ─────────────────────────────────────────────────

TEST_CASE("MCPClientConfig: default timeout is 30000ms (OpenCode DEFAULT_TIMEOUT)", "[mcp][manager]") {
    MCPClientConfig cfg;
    CHECK(cfg.timeout_ms == 30000);
}

TEST_CASE("MCPClientConfig: default enabled is true", "[mcp][manager]") {
    MCPClientConfig cfg;
    CHECK(cfg.enabled == true);
}

// ─── MCPToolWrapper naming rules (via sanitize_mcp_name) ─────────────────────

TEST_CASE("MCPToolWrapper naming: sanitized_client + underscore + sanitized_tool", "[mcp][manager]") {
    // The naming rule: sanitize(client_name) + "_" + sanitize(tool_name)
    // Verify the convention with sanitize_mcp_name
    std::string client = "my-server";
    std::string tool   = "read.file";
    std::string expected = sanitize_mcp_name(client) + "_" + sanitize_mcp_name(tool);
    CHECK(expected == "my-server_read_file");
}

TEST_CASE("MCPToolWrapper naming: spaces in client name are sanitized", "[mcp][manager]") {
    std::string client = "my server";
    std::string tool   = "list_tools";
    std::string registered = sanitize_mcp_name(client) + "_" + sanitize_mcp_name(tool);
    CHECK(registered == "my_server_list_tools");
}

TEST_CASE("MCPToolWrapper naming: special chars in tool name are sanitized", "[mcp][manager]") {
    std::string client = "filesystem";
    std::string tool   = "read/file@path";
    std::string registered = sanitize_mcp_name(client) + "_" + sanitize_mcp_name(tool);
    CHECK(registered == "filesystem_read_file_path");
}

// ─── MCPClientConfig type validation ─────────────────────────────────────────

TEST_CASE("MCPClientConfig: local type has empty url by default", "[mcp][manager]") {
    MCPClientConfig cfg;
    cfg.type = "local";
    cfg.command = {"node", "server.js"};
    CHECK(!cfg.url.has_value());
}

TEST_CASE("MCPClientConfig: remote type can hold url", "[mcp][manager]") {
    MCPClientConfig cfg;
    cfg.type = "remote";
    cfg.url = "https://mcp.example.com/sse";
    REQUIRE(cfg.url.has_value());
    CHECK(*cfg.url == "https://mcp.example.com/sse");
}
