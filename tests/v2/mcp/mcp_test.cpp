#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/mcp/mcp.hpp>

using namespace turbot::core::mcp;
using namespace turbot::test;

// ==================== MCPStatus 测试 ====================

TEST_CASE("MCP.Status.ToString", "[MCP]") {
    REQUIRE(mcp_status_to_string(MCPStatus::Connected) == "connected");
    REQUIRE(mcp_status_to_string(MCPStatus::Disabled) == "disabled");
    REQUIRE(mcp_status_to_string(MCPStatus::Failed) == "failed");
    REQUIRE(mcp_status_to_string(MCPStatus::NeedsAuth) == "needs_auth");
    REQUIRE(mcp_status_to_string(MCPStatus::NeedsClientRegistration) == "needs_client_registration");
}

// ==================== MCPTool 测试 ====================

TEST_CASE("MCP.Tool.Defaults", "[MCP]") {
    MCPTool tool;
    REQUIRE(tool.name.empty());
    REQUIRE(tool.description.empty());
}

TEST_CASE("MCP.Tool.JsonSerialization", "[MCP]") {
    MCPTool tool;
    tool.name = "search_files";
    tool.description = "Search for files in directory";
    tool.input_schema = R"({
        "type": "object",
        "properties": {
            "pattern": {"type": "string"}
        },
        "required": ["pattern"]
    })"_json;
    
    nlohmann::json j = tool.to_json();
    REQUIRE(j["name"] == "search_files");
    REQUIRE(j["description"] == "Search for files in directory");
    REQUIRE(j["inputSchema"]["type"] == "object");
    
    auto restored = MCPTool::from_json(j);
    REQUIRE(restored.name == "search_files");
    REQUIRE(restored.description == "Search for files in directory");
}

// ==================== MCPResource 测试 ====================

TEST_CASE("MCP.Resource.Defaults", "[MCP]") {
    MCPResource resource;
    REQUIRE(resource.name.empty());
    REQUIRE(resource.uri.empty());
    REQUIRE_FALSE(resource.description.has_value());
    REQUIRE_FALSE(resource.mime_type.has_value());
}

TEST_CASE("MCP.Resource.JsonSerialization", "[MCP]") {
    MCPResource resource;
    resource.name = "config";
    resource.uri = "file:///config.json";
    resource.description = "Configuration file";
    resource.mime_type = "application/json";
    resource.client = "filesystem-server";
    
    nlohmann::json j = resource.to_json();
    REQUIRE(j["name"] == "config");
    REQUIRE(j["uri"] == "file:///config.json");
    REQUIRE(j["description"] == "Configuration file");
    
    auto restored = MCPResource::from_json(j);
    REQUIRE(restored.name == "config");
    REQUIRE(restored.uri == "file:///config.json");
    REQUIRE(restored.client == "filesystem-server");
}

// ==================== MCPPrompt 测试 ====================

TEST_CASE("MCP.Prompt.Defaults", "[MCP]") {
    MCPPrompt prompt;
    REQUIRE(prompt.name.empty());
    REQUIRE(prompt.description.empty());
    REQUIRE(prompt.arguments.empty());
}

TEST_CASE("MCP.Prompt.JsonSerialization", "[MCP]") {
    MCPPrompt prompt;
    prompt.name = "code_review";
    prompt.description = "Review code for issues";
    prompt.arguments = {"file_path", "language"};
    
    nlohmann::json j = prompt.to_json();
    REQUIRE(j["name"] == "code_review");
    REQUIRE(j["description"] == "Review code for issues");
    REQUIRE(j["arguments"].size() == 2);
    
    auto restored = MCPPrompt::from_json(j);
    REQUIRE(restored.name == "code_review");
    REQUIRE(restored.arguments.size() == 2);
}

// ==================== MCPClientConfig 测试 ====================

TEST_CASE("MCP.ClientConfig.Defaults", "[MCP]") {
    MCPClientConfig config;
    REQUIRE(config.enabled == true);
    REQUIRE(config.timeout_ms == 30000);
}

TEST_CASE("MCP.ClientConfig.LocalType", "[MCP]") {
    MCPClientConfig config;
    config.name = "local-server";
    config.type = "local";
    config.command = {"/usr/bin/node", "server.js"};
    config.environment = {{"NODE_ENV", "test"}};
    
    REQUIRE(config.name == "local-server");
    REQUIRE(config.type == "local");
    REQUIRE(config.command.size() == 2);
    REQUIRE(config.environment["NODE_ENV"] == "test");
}

TEST_CASE("MCP.ClientConfig.RemoteType", "[MCP]") {
    MCPClientConfig config;
    config.name = "remote-server";
    config.type = "remote";
    config.url = "https://api.example.com/mcp";
    config.headers = std::unordered_map<std::string, std::string>{{"Authorization", "Bearer token"}};
    
    REQUIRE(config.name == "remote-server");
    REQUIRE(config.type == "remote");
    REQUIRE(config.url == "https://api.example.com/mcp");
    REQUIRE(config.headers->at("Authorization") == "Bearer token");
}

// ==================== sanitize_mcp_name 测试 ====================

TEST_CASE("MCP.SanitizeName.Alphanumeric", "[MCP]") {
    REQUIRE(sanitize_mcp_name("simple") == "simple");
    REQUIRE(sanitize_mcp_name("with_underscore") == "with_underscore");
    REQUIRE(sanitize_mcp_name("with-dash") == "with-dash");
    REQUIRE(sanitize_mcp_name("with123numbers") == "with123numbers");
}

TEST_CASE("MCP.SanitizeName.SpecialChars", "[MCP]") {
    REQUIRE(sanitize_mcp_name("hello world") == "hello_world");
    REQUIRE(sanitize_mcp_name("test@server!") == "test_server_");
    REQUIRE(sanitize_mcp_name("path/to/file") == "path_to_file");
}

// ==================== JsonRpcRequest 测试 ====================

TEST_CASE("MCP.JsonRpcRequest.Defaults", "[MCP]") {
    JsonRpcRequest req;
    REQUIRE(req.jsonrpc == "2.0");
    REQUIRE_FALSE(req.id.has_value());
    REQUIRE(req.params.is_object());
}

TEST_CASE("MCP.JsonRpcRequest.JsonSerialization", "[MCP]") {
    JsonRpcRequest req;
    req.id = 1;
    req.method = "tools/list";
    req.params = R"({"cursor": "abc"})"_json;
    
    nlohmann::json j = req.to_json();
    REQUIRE(j["jsonrpc"] == "2.0");
    REQUIRE(j["id"] == 1);
    REQUIRE(j["method"] == "tools/list");
    REQUIRE(j["params"]["cursor"] == "abc");
}

// ==================== JsonRpcResponse 测试 ====================

TEST_CASE("MCP.JsonRpcResponse.Success", "[MCP]") {
    nlohmann::json j = R"({
        "jsonrpc": "2.0",
        "id": 1,
        "result": {"tools": []}
    })"_json;
    
    auto resp = JsonRpcResponse::from_json(j);
    REQUIRE(resp.jsonrpc == "2.0");
    REQUIRE(resp.id == 1);
    REQUIRE_FALSE(resp.is_error());
    REQUIRE(resp.result.has_value());
}

TEST_CASE("MCP.JsonRpcResponse.Error", "[MCP]") {
    nlohmann::json j = R"({
        "jsonrpc": "2.0",
        "id": 1,
        "error": {"code": -32600, "message": "Invalid Request"}
    })"_json;
    
    auto resp = JsonRpcResponse::from_json(j);
    REQUIRE(resp.is_error());
    REQUIRE(resp.error->at("code") == -32600);
}

TEST_CASE("MCP.JsonRpcResponse.MakeError", "[MCP]") {
    auto resp = JsonRpcResponse::make_error(1, -32601, "Method not found");
    REQUIRE(resp.is_error());
    REQUIRE(resp.error->at("code") == -32601);
    REQUIRE(resp.error->at("message") == "Method not found");
}
