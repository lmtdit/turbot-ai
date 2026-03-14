/// test_mcp_client.cpp — Unit tests for MCPClient request/response serialization,
/// timeout defaults, and client state management without a live server.

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/mcp/mcp.hpp>
#include <turbot/core/mcp/client.hpp>
#include <nlohmann/json.hpp>
#include <future>
#include <functional>
#include <memory>
#include <stdexcept>

using namespace turbot::core::mcp;

// ─── Mock transport for testing MCPClient without a real server ───────────────

/// A minimal in-memory transport that allows injecting specific responses.
class MockTransport : public ITransport {
public:
    // Inject the response that will be returned for the next send_request call
    void set_next_response(JsonRpcResponse resp) {
        next_response_ = std::move(resp);
    }

    std::future<void> connect() override {
        connected_ = true;
        return std::async(std::launch::deferred, [](){});
    }

    std::future<JsonRpcResponse> send_request(
        const std::string& /*method*/,
        const nlohmann::json& /*params*/
    ) override {
        return std::async(std::launch::deferred, [this]() -> JsonRpcResponse {
            if (next_response_) {
                auto r = *next_response_;
                next_response_.reset();
                return r;
            }
            return JsonRpcResponse::make_error(1, -32603, "No response configured");
        });
    }

    void send_notification(const std::string& /*method*/, const nlohmann::json& /*params*/) override {}

    void on_notification(const std::string& method, std::function<void(const nlohmann::json&)> handler) override {
        handlers_[method] = std::move(handler);
    }

    std::future<void> close() override {
        connected_ = false;
        return std::async(std::launch::deferred, [](){});
    }

    bool connected_ = false;

private:
    std::optional<JsonRpcResponse> next_response_;
    std::unordered_map<std::string, std::function<void(const nlohmann::json&)>> handlers_;
};

// ─── MCPClient initial state ──────────────────────────────────────────────────

TEST_CASE("MCPClient: initial status is Failed before connect", "[mcp][client]") {
    auto transport = std::make_unique<MockTransport>();
    MCPClient client(std::move(transport));
    CHECK(client.status() == MCPStatus::Failed);
}

TEST_CASE("MCPClient: pid() returns -1 for non-stdio transport", "[mcp][client]") {
    auto transport = std::make_unique<MockTransport>();
    MCPClient client(std::move(transport));
    CHECK(client.pid() == -1);
}

// ─── MCPClient connect with mock transport ────────────────────────────────────

TEST_CASE("MCPClient: connect succeeds when initialize response is ok", "[mcp][client]") {
    auto* raw = new MockTransport();
    // Prepare initialize response
    JsonRpcResponse init_resp;
    init_resp.id = 1;
    init_resp.result = nlohmann::json{
        {"protocolVersion", "2024-11-05"},
        {"serverInfo", {{"name", "mock-server"}, {"version", "0.1.0"}}},
        {"capabilities", nlohmann::json::object()}
    };
    raw->set_next_response(init_resp);

    MCPClient client{std::unique_ptr<MockTransport>(raw)};
    auto status = client.connect("turbot-test", "1.0.0").get();
    CHECK(status == MCPStatus::Connected);
    CHECK(client.status() == MCPStatus::Connected);
}

TEST_CASE("MCPClient: connect fails when initialize returns error", "[mcp][client]") {
    auto* raw = new MockTransport();
    raw->set_next_response(JsonRpcResponse::make_error(1, -32600, "Invalid Request"));

    MCPClient client{std::unique_ptr<MockTransport>(raw)};
    auto status = client.connect("turbot-test", "1.0.0").get();
    CHECK(status == MCPStatus::Failed);
    CHECK(client.status() == MCPStatus::Failed);
}

// ─── Request/response serialization ──────────────────────────────────────────

TEST_CASE("MCPClient: timeout default is 30000ms (OpenCode DEFAULT_TIMEOUT)", "[mcp][client]") {
    // Verify the documented default appears in the call_tool signature.
    // This test checks the interface contract, not runtime behavior.
    // MCPClientConfig also defaults to 30000 for consistency.
    MCPClientConfig cfg;
    CHECK(cfg.timeout_ms == 30000);
}

TEST_CASE("MCPClient request serialization: tools/list params structure", "[mcp][client]") {
    // Verify the JSON-RPC message that would be sent for tools/list
    JsonRpcRequest req;
    req.id = 1;
    req.method = "tools/list";
    req.params = nlohmann::json::object();

    auto j = req.to_json();
    CHECK(j["method"] == "tools/list");
    CHECK(j["params"].is_object());
    CHECK(j["params"].empty());
}

TEST_CASE("MCPClient request serialization: tools/call includes name and arguments", "[mcp][client]") {
    JsonRpcRequest req;
    req.id = 5;
    req.method = "tools/call";
    req.params = {
        {"name", "read_file"},
        {"arguments", {{"path", "/tmp/test.txt"}}}
    };

    auto j = req.to_json();
    CHECK(j["params"]["name"] == "read_file");
    CHECK(j["params"]["arguments"]["path"] == "/tmp/test.txt");
}

// ─── JsonRpcResponse edge cases ──────────────────────────────────────────────

TEST_CASE("MCPClient response: empty tools array is valid result", "[mcp][client]") {
    nlohmann::json j = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"result", {{"tools", nlohmann::json::array()}}}
    };
    auto resp = JsonRpcResponse::from_json(j);
    CHECK(!resp.is_error());
    REQUIRE(resp.result.has_value());
    CHECK((*resp.result)["tools"].is_array());
    CHECK((*resp.result)["tools"].empty());
}

TEST_CASE("MCPClient response: large tool list parses correctly", "[mcp][client]") {
    nlohmann::json tools = nlohmann::json::array();
    for (int i = 0; i < 50; ++i) {
        tools.push_back({
            {"name", "tool_" + std::to_string(i)},
            {"description", "Tool number " + std::to_string(i)},
            {"inputSchema", {{"type", "object"}, {"properties", nlohmann::json::object()}}}
        });
    }
    nlohmann::json j = {
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"result", {{"tools", tools}}}
    };
    auto resp = JsonRpcResponse::from_json(j);
    CHECK(!resp.is_error());
    REQUIRE(resp.result.has_value());
    CHECK((*resp.result)["tools"].size() == 50u);
}
