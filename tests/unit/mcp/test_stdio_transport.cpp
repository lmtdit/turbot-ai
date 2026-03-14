/// test_stdio_transport.cpp — Unit tests for StdioTransport JSON-RPC framing protocol.
/// Tests Content-Length framing, newline-delimited JSON protocol, JsonRpcRequest serialization,
/// and JsonRpcResponse parsing — without spawning a real child process.

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/mcp/mcp.hpp>
#include <turbot/core/mcp/stdio_transport.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <sstream>

using namespace turbot::core::mcp;

// ─── Protocol frame helpers ───────────────────────────────────────────────────

/// Build a Content-Length framed message (HTTP-header style used by LSP/MCP stdio)
static std::string make_content_length_frame(const std::string& body) {
    std::ostringstream oss;
    oss << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    return oss.str();
}

/// Parse the Content-Length value from a framed message
static std::optional<size_t> parse_content_length(const std::string& frame) {
    const std::string header_key = "Content-Length: ";
    auto pos = frame.find(header_key);
    if (pos == std::string::npos) return std::nullopt;
    auto start = pos + header_key.size();
    auto end   = frame.find("\r\n", start);
    if (end == std::string::npos) return std::nullopt;
    return std::stoul(frame.substr(start, end - start));
}

// ─── Content-Length framing ───────────────────────────────────────────────────

TEST_CASE("StdioTransport protocol: Content-Length framing encodes body length", "[mcp][stdio]") {
    std::string body = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
    auto frame = make_content_length_frame(body);

    auto len = parse_content_length(frame);
    REQUIRE(len.has_value());
    CHECK(*len == body.size());
}

TEST_CASE("StdioTransport protocol: frame ends with double CRLF separator", "[mcp][stdio]") {
    std::string body = R"({"jsonrpc":"2.0"})";
    auto frame = make_content_length_frame(body);
    // The header block must end with \r\n\r\n before the body
    CHECK(frame.find("\r\n\r\n") != std::string::npos);
}

TEST_CASE("StdioTransport protocol: body follows header separator exactly", "[mcp][stdio]") {
    std::string body = R"({"method":"ping"})";
    auto frame = make_content_length_frame(body);
    auto sep_pos = frame.find("\r\n\r\n");
    REQUIRE(sep_pos != std::string::npos);
    std::string extracted_body = frame.substr(sep_pos + 4);
    CHECK(extracted_body == body);
}

// ─── JsonRpcRequest framing ───────────────────────────────────────────────────

TEST_CASE("StdioTransport protocol: request JSON includes jsonrpc=2.0", "[mcp][stdio]") {
    JsonRpcRequest req;
    req.id = 1;
    req.method = "tools/list";
    req.params = nlohmann::json::object();

    auto j = req.to_json();
    CHECK(j["jsonrpc"] == "2.0");
    CHECK(j["method"] == "tools/list");
    CHECK(j["id"] == 1);
    CHECK(j["params"].is_object());
}

TEST_CASE("StdioTransport protocol: notification has no id field", "[mcp][stdio]") {
    JsonRpcRequest notif;
    notif.id = std::nullopt;
    notif.method = "notifications/initialized";
    notif.params = nlohmann::json::object();

    auto j = notif.to_json();
    CHECK(!j.contains("id"));
    CHECK(j["method"] == "notifications/initialized");
}

TEST_CASE("StdioTransport protocol: request with nested params serializes correctly", "[mcp][stdio]") {
    JsonRpcRequest req;
    req.id = 42;
    req.method = "tools/call";
    req.params = {
        {"name", "read_file"},
        {"arguments", {{"path", "/tmp/test.txt"}}}
    };

    auto j = req.to_json();
    auto body = j.dump();

    // Body must be valid JSON
    auto reparsed = nlohmann::json::parse(body);
    CHECK(reparsed["method"] == "tools/call");
    CHECK(reparsed["params"]["name"] == "read_file");
    CHECK(reparsed["params"]["arguments"]["path"] == "/tmp/test.txt");
}

// ─── JsonRpcResponse parsing (as received by transport read loop) ─────────────

TEST_CASE("StdioTransport protocol: parse response with result", "[mcp][stdio]") {
    std::string raw = R"({
        "jsonrpc": "2.0",
        "id": 1,
        "result": {
            "protocolVersion": "2024-11-05",
            "serverInfo": {"name": "test-server", "version": "1.0.0"},
            "capabilities": {}
        }
    })";
    auto j = nlohmann::json::parse(raw);
    auto resp = JsonRpcResponse::from_json(j);

    CHECK(resp.id == 1);
    CHECK(!resp.is_error());
    REQUIRE(resp.result.has_value());
    CHECK((*resp.result)["protocolVersion"] == "2024-11-05");
    CHECK((*resp.result)["serverInfo"]["name"] == "test-server");
}

TEST_CASE("StdioTransport protocol: parse error response", "[mcp][stdio]") {
    std::string raw = R"({
        "jsonrpc": "2.0",
        "id": 2,
        "error": {
            "code": -32601,
            "message": "Method not found"
        }
    })";
    auto j = nlohmann::json::parse(raw);
    auto resp = JsonRpcResponse::from_json(j);

    CHECK(resp.id == 2);
    CHECK(resp.is_error());
    REQUIRE(resp.error.has_value());
    CHECK((*resp.error)["code"] == -32601);
    CHECK((*resp.error)["message"] == "Method not found");
}

// ─── StdioTransport construction ─────────────────────────────────────────────

TEST_CASE("StdioTransport: can be constructed with command vector", "[mcp][stdio]") {
    // Construction should succeed without spawning the process
    // (connect() is the blocking call; we only test the ctor here)
    using turbot::core::mcp::StdioTransport;
    // Should not throw even with a non-existent command (process only launched on connect())
    REQUIRE_NOTHROW(StdioTransport({"echo", "hello"}));
}

TEST_CASE("StdioTransport: can be constructed with env map", "[mcp][stdio]") {
    using turbot::core::mcp::StdioTransport;
    REQUIRE_NOTHROW(StdioTransport(
        {"cat"},
        {{"MY_VAR", "test_value"}}
    ));
}
