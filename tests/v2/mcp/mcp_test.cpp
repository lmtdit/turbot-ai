#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/mcp/mcp.hpp>
#include <turbot/core/mcp/auth.hpp>
#include <fstream>
#include <filesystem>

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

// ==================== Tokens 测试 ====================

TEST_CASE("MCP.Tokens.Defaults", "[MCP][Auth]") {
    Tokens t;
    REQUIRE(t.access_token.empty());
    REQUIRE_FALSE(t.refresh_token.has_value());
    REQUIRE_FALSE(t.expires_at.has_value());
    REQUIRE_FALSE(t.scope.has_value());
}

TEST_CASE("MCP.Tokens.JsonSerialization", "[MCP][Auth]") {
    Tokens t;
    t.access_token = "abc123";
    t.refresh_token = "refresh456";
    t.expires_at = 1234567890;
    t.scope = "read write";
    
    nlohmann::json j = t.to_json();
    REQUIRE(j["accessToken"] == "abc123");
    REQUIRE(j["refreshToken"] == "refresh456");
    REQUIRE(j["expiresAt"] == 1234567890);
    REQUIRE(j["scope"] == "read write");
    
    auto restored = Tokens::from_json(j);
    REQUIRE(restored.access_token == "abc123");
    REQUIRE(restored.refresh_token == "refresh456");
    REQUIRE(restored.expires_at == 1234567890);
    REQUIRE(restored.scope == "read write");
}

TEST_CASE("MCP.Tokens.FromJsonPartial", "[MCP][Auth]") {
    nlohmann::json j = R"({"accessToken": "token123"})"_json;
    auto t = Tokens::from_json(j);
    REQUIRE(t.access_token == "token123");
    REQUIRE_FALSE(t.refresh_token.has_value());
    REQUIRE_FALSE(t.expires_at.has_value());
}

// ==================== ClientInfo 测试 ====================

TEST_CASE("MCP.ClientInfo.Defaults", "[MCP][Auth]") {
    ClientInfo c;
    REQUIRE(c.client_id.empty());
    REQUIRE_FALSE(c.client_secret.has_value());
    REQUIRE_FALSE(c.client_id_issued_at.has_value());
    REQUIRE_FALSE(c.client_secret_expires_at.has_value());
}

TEST_CASE("MCP.ClientInfo.JsonSerialization", "[MCP][Auth]") {
    ClientInfo c;
    c.client_id = "client-123";
    c.client_secret = "secret-456";
    c.client_id_issued_at = 1111111111;
    c.client_secret_expires_at = 2222222222;
    
    nlohmann::json j = c.to_json();
    REQUIRE(j["clientId"] == "client-123");
    REQUIRE(j["clientSecret"] == "secret-456");
    
    auto restored = ClientInfo::from_json(j);
    REQUIRE(restored.client_id == "client-123");
    REQUIRE(restored.client_secret == "secret-456");
}

// ==================== AuthEntry 测试 ====================

TEST_CASE("MCP.AuthEntry.Defaults", "[MCP][Auth]") {
    AuthEntry e;
    REQUIRE_FALSE(e.tokens.has_value());
    REQUIRE_FALSE(e.client_info.has_value());
    REQUIRE_FALSE(e.code_verifier.has_value());
    REQUIRE_FALSE(e.oauth_state.has_value());
    REQUIRE_FALSE(e.server_url.has_value());
}

TEST_CASE("MCP.AuthEntry.JsonSerialization", "[MCP][Auth]") {
    AuthEntry e;
    e.tokens = Tokens{};
    e.tokens->access_token = "test-token";
    e.client_info = ClientInfo{};
    e.client_info->client_id = "test-client";
    e.code_verifier = "verifier123";
    e.oauth_state = "state456";
    e.server_url = "https://example.com";
    
    nlohmann::json j = e.to_json();
    REQUIRE(j["tokens"]["accessToken"] == "test-token");
    REQUIRE(j["clientInfo"]["clientId"] == "test-client");
    REQUIRE(j["codeVerifier"] == "verifier123");
    REQUIRE(j["oauthState"] == "state456");
    REQUIRE(j["serverUrl"] == "https://example.com");
    
    auto restored = AuthEntry::from_json(j);
    REQUIRE(restored.tokens->access_token == "test-token");
    REQUIRE(restored.client_info->client_id == "test-client");
    REQUIRE(restored.code_verifier == "verifier123");
    REQUIRE(restored.oauth_state == "state456");
    REQUIRE(restored.server_url == "https://example.com");
}

// ==================== McpAuth 测试 ====================

TEST_CASE("MCP.McpAuth.AuthStatus", "[MCP][Auth]") {
    // Test enum values
    REQUIRE(McpAuth::get_auth_status("__nonexistent__") == McpAuth::AuthStatus::NotAuthenticated);
}

// ==================== MCPTool 边界测试 ====================

TEST_CASE("MCP.Tool.FromJsonWithMissingInputSchema", "[MCP]") {
    nlohmann::json j = R"({"name": "test", "description": "Test tool"})"_json;
    auto tool = MCPTool::from_json(j);
    REQUIRE(tool.name == "test");
    REQUIRE(tool.input_schema["type"] == "object");
}

TEST_CASE("MCP.Tool.FromJsonWithInvalidInputSchema", "[MCP]") {
    // When inputSchema is not an object, the code will throw
    // This is expected behavior - invalid schemas should be rejected
    nlohmann::json j = R"({"name": "test", "description": "Test", "inputSchema": "invalid"})"_json;
    REQUIRE_THROWS(MCPTool::from_json(j));
}

// ==================== MCPResource 边界测试 ====================

TEST_CASE("MCP.Resource.FromJsonWithNullFields", "[MCP]") {
    nlohmann::json j = R"({
        "name": "res",
        "uri": "file:///test",
        "description": null,
        "mimeType": null
    })"_json;
    
    auto res = MCPResource::from_json(j);
    REQUIRE(res.name == "res");
    REQUIRE(res.uri == "file:///test");
    REQUIRE_FALSE(res.description.has_value());
    REQUIRE_FALSE(res.mime_type.has_value());
}

// ==================== MCPPrompt 边界测试 ====================

TEST_CASE("MCP.Prompt.FromJsonWithObjectArguments", "[MCP]") {
    nlohmann::json j = R"({
        "name": "prompt",
        "description": "Test prompt",
        "arguments": [
            {"name": "arg1"},
            {"name": "arg2"}
        ]
    })"_json;
    
    auto prompt = MCPPrompt::from_json(j);
    REQUIRE(prompt.name == "prompt");
    REQUIRE(prompt.arguments.size() == 2);
    REQUIRE(prompt.arguments[0] == "arg1");
    REQUIRE(prompt.arguments[1] == "arg2");
}

TEST_CASE("MCP.Prompt.FromJsonWithStringArguments", "[MCP]") {
    nlohmann::json j = R"({
        "name": "prompt",
        "description": "Test prompt",
        "arguments": ["arg1", "arg2"]
    })"_json;
    
    auto prompt = MCPPrompt::from_json(j);
    REQUIRE(prompt.arguments.size() == 2);
    REQUIRE(prompt.arguments[0] == "arg1");
    REQUIRE(prompt.arguments[1] == "arg2");
}

// ==================== JsonRpcResponse 边界测试 ====================

TEST_CASE("MCP.JsonRpcResponse.FromJsonWithNullId", "[MCP]") {
    nlohmann::json j = R"({
        "jsonrpc": "2.0",
        "id": null,
        "result": {}
    })"_json;
    
    auto resp = JsonRpcResponse::from_json(j);
    REQUIRE(resp.jsonrpc == "2.0");
}

TEST_CASE("MCP.JsonRpcResponse.FromJsonWithoutResultOrError", "[MCP]") {
    nlohmann::json j = R"({"jsonrpc": "2.0", "id": 1})"_json;
    auto resp = JsonRpcResponse::from_json(j);
    REQUIRE_FALSE(resp.is_error());
    REQUIRE_FALSE(resp.result.has_value());
}

// ==================== McpAuth 文件操作测试 ====================

TEST_CASE("MCP.McpAuth.SetAndGet", "[MCP][Auth]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    // Set HOME to temp directory
    auto turbot_dir = tmp.path() / ".turbot";
    std::filesystem::create_directories(turbot_dir);
    setenv("HOME", tmp.path().c_str(), 1);
    
    AuthEntry entry;
    entry.tokens = Tokens{};
    entry.tokens->access_token = "test-access-token";
    entry.tokens->refresh_token = "test-refresh-token";
    
    McpAuth::set("test-mcp", entry);
    
    auto retrieved = McpAuth::get("test-mcp");
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->tokens->access_token == "test-access-token");
    REQUIRE(retrieved->tokens->refresh_token == "test-refresh-token");
    
    // Cleanup
    unsetenv("HOME");
}

TEST_CASE("MCP.McpAuth.GetForUrl", "[MCP][Auth]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto turbot_dir = tmp.path() / ".turbot";
    std::filesystem::create_directories(turbot_dir);
    setenv("HOME", tmp.path().c_str(), 1);
    
    AuthEntry entry;
    entry.tokens = Tokens{};
    entry.tokens->access_token = "url-test-token";
    
    McpAuth::set("url-mcp", entry, "https://example.com/mcp");
    
    // Should return entry for matching URL
    auto retrieved = McpAuth::get_for_url("url-mcp", "https://example.com/mcp");
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->tokens->access_token == "url-test-token");
    
    // Should return nullopt for different URL
    auto not_found = McpAuth::get_for_url("url-mcp", "https://different.com/mcp");
    REQUIRE_FALSE(not_found.has_value());
    
    unsetenv("HOME");
}

TEST_CASE("MCP.McpAuth.Remove", "[MCP][Auth]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto turbot_dir = tmp.path() / ".turbot";
    std::filesystem::create_directories(turbot_dir);
    setenv("HOME", tmp.path().c_str(), 1);
    
    AuthEntry entry;
    entry.tokens = Tokens{};
    entry.tokens->access_token = "to-remove";
    
    McpAuth::set("remove-mcp", entry);
    REQUIRE(McpAuth::get("remove-mcp").has_value());
    
    McpAuth::remove("remove-mcp");
    REQUIRE_FALSE(McpAuth::get("remove-mcp").has_value());
    
    unsetenv("HOME");
}

TEST_CASE("MCP.McpAuth.UpdateTokens", "[MCP][Auth]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto turbot_dir = tmp.path() / ".turbot";
    std::filesystem::create_directories(turbot_dir);
    setenv("HOME", tmp.path().c_str(), 1);
    
    Tokens tokens;
    tokens.access_token = "updated-token";
    tokens.expires_at = 9999999999;
    
    McpAuth::update_tokens("update-mcp", tokens);
    
    auto entry = McpAuth::get("update-mcp");
    REQUIRE(entry.has_value());
    REQUIRE(entry->tokens->access_token == "updated-token");
    REQUIRE(entry->tokens->expires_at == 9999999999);
    
    unsetenv("HOME");
}

TEST_CASE("MCP.McpAuth.UpdateClientInfo", "[MCP][Auth]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto turbot_dir = tmp.path() / ".turbot";
    std::filesystem::create_directories(turbot_dir);
    setenv("HOME", tmp.path().c_str(), 1);
    
    ClientInfo info;
    info.client_id = "test-client-id";
    info.client_secret = "test-secret";
    
    McpAuth::update_client_info("client-mcp", info);
    
    auto entry = McpAuth::get("client-mcp");
    REQUIRE(entry.has_value());
    REQUIRE(entry->client_info->client_id == "test-client-id");
    REQUIRE(entry->client_info->client_secret == "test-secret");
    
    unsetenv("HOME");
}

TEST_CASE("MCP.McpAuth.CodeVerifier", "[MCP][Auth]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto turbot_dir = tmp.path() / ".turbot";
    std::filesystem::create_directories(turbot_dir);
    setenv("HOME", tmp.path().c_str(), 1);
    
    McpAuth::update_code_verifier("verifier-mcp", "test-verifier-123");
    
    auto entry = McpAuth::get("verifier-mcp");
    REQUIRE(entry.has_value());
    REQUIRE(entry->code_verifier == "test-verifier-123");
    
    McpAuth::clear_code_verifier("verifier-mcp");
    
    entry = McpAuth::get("verifier-mcp");
    REQUIRE(entry.has_value());
    REQUIRE_FALSE(entry->code_verifier.has_value());
    
    unsetenv("HOME");
}

TEST_CASE("MCP.McpAuth.OAuthState", "[MCP][Auth]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto turbot_dir = tmp.path() / ".turbot";
    std::filesystem::create_directories(turbot_dir);
    setenv("HOME", tmp.path().c_str(), 1);
    
    McpAuth::update_oauth_state("state-mcp", "test-state-456");
    
    auto state = McpAuth::get_oauth_state("state-mcp");
    REQUIRE(state.has_value());
    REQUIRE(*state == "test-state-456");
    
    McpAuth::clear_oauth_state("state-mcp");
    
    state = McpAuth::get_oauth_state("state-mcp");
    REQUIRE_FALSE(state.has_value());
    
    unsetenv("HOME");
}

TEST_CASE("MCP.McpAuth.TokenExpiry", "[MCP][Auth]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto turbot_dir = tmp.path() / ".turbot";
    std::filesystem::create_directories(turbot_dir);
    setenv("HOME", tmp.path().c_str(), 1);
    
    // Test with expired token (expires_at in the past)
    AuthEntry entry;
    entry.tokens = Tokens{};
    entry.tokens->access_token = "expired-token";
    entry.tokens->expires_at = 1000000000;  // Past timestamp
    
    McpAuth::set("expired-mcp", entry);
    
    auto expired = McpAuth::is_token_expired("expired-mcp");
    REQUIRE(expired.has_value());
    REQUIRE(*expired == true);
    
    // Test with valid token (expires_at in the future)
    entry.tokens->expires_at = 9999999999;
    McpAuth::set("valid-mcp", entry);
    
    auto valid = McpAuth::is_token_expired("valid-mcp");
    REQUIRE(valid.has_value());
    REQUIRE(*valid == false);
    
    // Test with no expiry
    entry.tokens->expires_at = std::nullopt;
    McpAuth::set("no-expiry-mcp", entry);
    
    auto no_expiry = McpAuth::is_token_expired("no-expiry-mcp");
    REQUIRE(no_expiry.has_value());
    REQUIRE(*no_expiry == false);
    
    // Test with no token
    auto no_token = McpAuth::is_token_expired("nonexistent-mcp");
    REQUIRE_FALSE(no_token.has_value());
    
    unsetenv("HOME");
}

TEST_CASE("MCP.McpAuth.AuthStatusFull", "[MCP][Auth]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto turbot_dir = tmp.path() / ".turbot";
    std::filesystem::create_directories(turbot_dir);
    setenv("HOME", tmp.path().c_str(), 1);
    
    // Not authenticated
    REQUIRE(McpAuth::get_auth_status("no-auth-mcp") == McpAuth::AuthStatus::NotAuthenticated);
    
    // Authenticated (valid token)
    AuthEntry entry;
    entry.tokens = Tokens{};
    entry.tokens->access_token = "valid-token";
    entry.tokens->expires_at = 9999999999;
    McpAuth::set("auth-mcp", entry);
    REQUIRE(McpAuth::get_auth_status("auth-mcp") == McpAuth::AuthStatus::Authenticated);
    
    // Expired
    entry.tokens->expires_at = 1000000000;
    McpAuth::set("expired-auth-mcp", entry);
    REQUIRE(McpAuth::get_auth_status("expired-auth-mcp") == McpAuth::AuthStatus::Expired);
    
    unsetenv("HOME");
}
