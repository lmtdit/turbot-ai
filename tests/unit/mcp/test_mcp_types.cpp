/// test_mcp_types.cpp — Unit tests for MCPStatus, MCPTool, MCPResource, MCPPrompt
/// Covers: mcp_status_to_string, sanitize_mcp_name, MCPTool from/to JSON,
///         MCPResource from/to JSON, MCPPrompt from/to JSON, JsonRpcResponse.

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/mcp/mcp.hpp>

using namespace turbot::core::mcp;

// ─── MCPStatus ────────────────────────────────────────────────────────────────

TEST_CASE("MCPStatus: mcp_status_to_string covers all values", "[mcp][types]") {
    CHECK(mcp_status_to_string(MCPStatus::Connected)               == "connected");
    CHECK(mcp_status_to_string(MCPStatus::Disabled)                == "disabled");
    CHECK(mcp_status_to_string(MCPStatus::Failed)                  == "failed");
    CHECK(mcp_status_to_string(MCPStatus::NeedsAuth)               == "needs_auth");
    CHECK(mcp_status_to_string(MCPStatus::NeedsClientRegistration) == "needs_client_registration");
}

// ─── sanitize_mcp_name ────────────────────────────────────────────────────────

TEST_CASE("sanitize_mcp_name: alphanumeric, underscore and dash preserved", "[mcp][types]") {
    CHECK(sanitize_mcp_name("hello_world-123") == "hello_world-123");
    CHECK(sanitize_mcp_name("ABC") == "ABC");
    CHECK(sanitize_mcp_name("a1_b2-c3") == "a1_b2-c3");
}

TEST_CASE("sanitize_mcp_name: special chars replaced with underscore", "[mcp][types]") {
    CHECK(sanitize_mcp_name("my server") == "my_server");
    CHECK(sanitize_mcp_name("my.server") == "my_server");
    CHECK(sanitize_mcp_name("my/server") == "my_server");
    CHECK(sanitize_mcp_name("a@b#c") == "a_b_c");
}

TEST_CASE("sanitize_mcp_name: empty string returns empty", "[mcp][types]") {
    CHECK(sanitize_mcp_name("") == "");
}

// ─── MCPTool ──────────────────────────────────────────────────────────────────

TEST_CASE("MCPTool: to_json round-trip", "[mcp][types]") {
    MCPTool tool;
    tool.name = "my_tool";
    tool.description = "Does something";
    tool.input_schema = {{"type", "object"}, {"properties", nlohmann::json::object()}};

    auto j = tool.to_json();
    CHECK(j["name"] == "my_tool");
    CHECK(j["description"] == "Does something");
    CHECK(j.contains("inputSchema"));
    CHECK(j["inputSchema"]["type"] == "object");
}

TEST_CASE("MCPTool: from_json populates fields correctly", "[mcp][types]") {
    nlohmann::json j = {
        {"name", "file_reader"},
        {"description", "Reads a file"},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}}}
            }}
        }}
    };
    auto tool = MCPTool::from_json(j);
    CHECK(tool.name == "file_reader");
    CHECK(tool.description == "Reads a file");
    CHECK(tool.input_schema["type"] == "object");
    CHECK(tool.input_schema["properties"].contains("path"));
}

TEST_CASE("MCPTool: from_json without inputSchema inserts default", "[mcp][types]") {
    nlohmann::json j = {{"name", "noop"}, {"description", "no-op"}};
    auto tool = MCPTool::from_json(j);
    // Should have type=object even without explicit inputSchema
    CHECK(tool.input_schema["type"] == "object");
}

TEST_CASE("MCPTool: from_json enforces type=object when type is missing", "[mcp][types]") {
    // Only enforces type=object when no type field is present (schema is object without type)
    nlohmann::json j = {
        {"name", "notool"},
        {"description", ""},
        {"inputSchema", nlohmann::json::object()}  // object but no "type" key
    };
    auto tool = MCPTool::from_json(j);
    // When inputSchema is an object without type, from_json should inject type=object
    CHECK(tool.input_schema["type"] == "object");
}

// ─── MCPResource ─────────────────────────────────────────────────────────────

TEST_CASE("MCPResource: to_json round-trip", "[mcp][types]") {
    MCPResource res;
    res.name = "readme";
    res.uri = "file:///README.md";
    res.description = "Project readme";
    res.mime_type = "text/markdown";
    res.client = "filesystem";

    auto j = res.to_json();
    CHECK(j["name"] == "readme");
    CHECK(j["uri"] == "file:///README.md");
    CHECK(j["description"] == "Project readme");
    CHECK(j["mimeType"] == "text/markdown");
    CHECK(j["client"] == "filesystem");
}

TEST_CASE("MCPResource: from_json populates fields", "[mcp][types]") {
    nlohmann::json j = {
        {"name", "data"},
        {"uri", "file:///data.csv"},
        {"client", "local_server"}
    };
    auto res = MCPResource::from_json(j);
    CHECK(res.name == "data");
    CHECK(res.uri == "file:///data.csv");
    CHECK(!res.description.has_value());
    CHECK(!res.mime_type.has_value());
    CHECK(res.client == "local_server");
}

TEST_CASE("MCPResource: from_json with optional fields", "[mcp][types]") {
    nlohmann::json j = {
        {"name", "img"},
        {"uri", "file:///image.png"},
        {"description", "A picture"},
        {"mimeType", "image/png"},
        {"client", "srv"}
    };
    auto res = MCPResource::from_json(j);
    REQUIRE(res.description.has_value());
    CHECK(*res.description == "A picture");
    REQUIRE(res.mime_type.has_value());
    CHECK(*res.mime_type == "image/png");
}

// ─── MCPPrompt ───────────────────────────────────────────────────────────────

TEST_CASE("MCPPrompt: to_json includes name and description", "[mcp][types]") {
    MCPPrompt prompt;
    prompt.name = "summarize";
    prompt.description = "Summarizes text";
    prompt.arguments = {"text", "max_words"};

    auto j = prompt.to_json();
    CHECK(j["name"] == "summarize");
    CHECK(j["description"] == "Summarizes text");
}

TEST_CASE("MCPPrompt: from_json populates fields", "[mcp][types]") {
    nlohmann::json j = {
        {"name", "translate"},
        {"description", "Translates text"},
        {"arguments", {"text", "target_lang"}}
    };
    auto prompt = MCPPrompt::from_json(j);
    CHECK(prompt.name == "translate");
    CHECK(prompt.description == "Translates text");
    CHECK(prompt.arguments.size() == 2);
    CHECK(prompt.arguments[0] == "text");
    CHECK(prompt.arguments[1] == "target_lang");
}

// ─── JsonRpcResponse ─────────────────────────────────────────────────────────

TEST_CASE("JsonRpcResponse: make_error constructs error response", "[mcp][types]") {
    auto resp = JsonRpcResponse::make_error(42, -32600, "Invalid Request");
    CHECK(resp.id == 42);
    CHECK(resp.is_error());
    REQUIRE(resp.error.has_value());
    CHECK((*resp.error)["message"] == "Invalid Request");
    CHECK((*resp.error)["code"] == -32600);
}

TEST_CASE("JsonRpcResponse: from_json with result field", "[mcp][types]") {
    nlohmann::json j = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"result", {{"tools", nlohmann::json::array()}}}
    };
    auto resp = JsonRpcResponse::from_json(j);
    CHECK(resp.id == 1);
    CHECK(!resp.is_error());
    REQUIRE(resp.result.has_value());
    CHECK(resp.result->contains("tools"));
}

TEST_CASE("JsonRpcResponse: from_json with error field", "[mcp][types]") {
    nlohmann::json j = {
        {"jsonrpc", "2.0"},
        {"id", 2},
        {"error", {{"code", -32700}, {"message", "Parse error"}}}
    };
    auto resp = JsonRpcResponse::from_json(j);
    CHECK(resp.id == 2);
    CHECK(resp.is_error());
}

// ─── JsonRpcRequest ──────────────────────────────────────────────────────────

TEST_CASE("JsonRpcRequest: to_json with id", "[mcp][types]") {
    JsonRpcRequest req;
    req.id = 10;
    req.method = "tools/list";
    req.params = {{"cursor", nullptr}};

    auto j = req.to_json();
    CHECK(j["jsonrpc"] == "2.0");
    CHECK(j["method"] == "tools/list");
    CHECK(j.contains("id"));
    CHECK(j["id"] == 10);
}

TEST_CASE("JsonRpcRequest: to_json notification (no id)", "[mcp][types]") {
    JsonRpcRequest req;
    req.id = std::nullopt;
    req.method = "notifications/initialized";

    auto j = req.to_json();
    CHECK(j["method"] == "notifications/initialized");
    // Notification: no "id" field in the JSON
    CHECK(!j.contains("id"));
}
