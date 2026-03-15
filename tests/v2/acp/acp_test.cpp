#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/acp/acp.hpp>

using namespace turbot::core::acp;
using namespace turbot::test;

// ==================== ModelOption 测试 ====================

TEST_CASE("ACP.ModelOption.Defaults", "[ACP]") {
    ModelOption opt;
    REQUIRE(opt.model_id.empty());
    REQUIRE(opt.name.empty());
}

TEST_CASE("ACP.ModelOption.WithValues", "[ACP]") {
    ModelOption opt;
    opt.model_id = "claude-3-opus";
    opt.name = "Claude 3 Opus";
    
    REQUIRE(opt.model_id == "claude-3-opus");
    REQUIRE(opt.name == "Claude 3 Opus");
}

// ==================== ModeOption 测试 ====================

TEST_CASE("ACP.ModeOption.Defaults", "[ACP]") {
    ModeOption opt;
    REQUIRE(opt.id.empty());
    REQUIRE(opt.name.empty());
    REQUIRE_FALSE(opt.description.has_value());
}

TEST_CASE("ACP.ModeOption.WithValues", "[ACP]") {
    ModeOption opt;
    opt.id = "code";
    opt.name = "Code Mode";
    opt.description = "For coding tasks";
    
    REQUIRE(opt.id == "code");
    REQUIRE(opt.name == "Code Mode");
    REQUIRE(opt.description == "For coding tasks");
}

// ==================== AgentCapabilities 测试 ====================

TEST_CASE("ACP.AgentCapabilities.Defaults", "[ACP]") {
    AgentCapabilities caps;
    REQUIRE(caps.load_session == true);
    REQUIRE(caps.mcp_capabilities.http == true);
    REQUIRE(caps.mcp_capabilities.sse == true);
    REQUIRE(caps.prompt_capabilities.embedded_context == true);
    REQUIRE(caps.prompt_capabilities.image == true);
    REQUIRE(caps.session_capabilities.fork == true);
    REQUIRE(caps.session_capabilities.list == true);
    REQUIRE(caps.session_capabilities.resume == true);
}

// ==================== AuthMethod 测试 ====================

TEST_CASE("ACP.AuthMethod.Defaults", "[ACP]") {
    AuthMethod method;
    REQUIRE(method.id.empty());
    REQUIRE(method.name.empty());
    REQUIRE(method.description.empty());
    REQUIRE(method.meta.is_object());
}

TEST_CASE("ACP.AuthMethod.WithValues", "[ACP]") {
    AuthMethod method;
    method.id = "api-key";
    method.name = "API Key";
    method.description = "Authenticate with API key";
    method.meta = R"({"type": "text"})"_json;
    
    REQUIRE(method.id == "api-key");
    REQUIRE(method.meta["type"] == "text");
}

// ==================== AgentInfo 测试 ====================

TEST_CASE("ACP.AgentInfo.Defaults", "[ACP]") {
    AgentInfo info;
    REQUIRE(info.name.empty());
    REQUIRE(info.version.empty());
}

TEST_CASE("ACP.AgentInfo.WithValues", "[ACP]") {
    AgentInfo info;
    info.name = "Turbot";
    info.version = "4.3.0";
    
    REQUIRE(info.name == "Turbot");
    REQUIRE(info.version == "4.3.0");
}

// ==================== InitializeRequest 测试 ====================

TEST_CASE("ACP.InitializeRequest.Defaults", "[ACP]") {
    InitializeRequest req;
    REQUIRE(req.protocol_version == 1);
    REQUIRE_FALSE(req.authentication.has_value());
    REQUIRE(req.client_capabilities.is_object());
}

TEST_CASE("ACP.InitializeRequest.WithAuth", "[ACP]") {
    InitializeRequest req;
    req.authentication = "Bearer token123";
    req.client_capabilities = R"({"features": ["streaming"]})"_json;
    
    REQUIRE(req.authentication == "Bearer token123");
    REQUIRE(req.client_capabilities["features"].size() == 1);
}

// ==================== InitializeResponse 测试 ====================

TEST_CASE("ACP.InitializeResponse.Defaults", "[ACP]") {
    InitializeResponse resp;
    REQUIRE(resp.protocol_version == 1);
    REQUIRE(resp.auth_methods.empty());
}

// ==================== SessionInfo 测试 ====================

TEST_CASE("ACP.SessionInfo.Defaults", "[ACP]") {
    SessionInfo info;
    REQUIRE(info.session_id.empty());
    REQUIRE(info.cwd.empty());
    REQUIRE_FALSE(info.model_id.has_value());
    REQUIRE_FALSE(info.mode.has_value());
    REQUIRE(info.mcp_servers.empty());
    REQUIRE_FALSE(info.title.has_value());
}

TEST_CASE("ACP.SessionInfo.WithValues", "[ACP]") {
    SessionInfo info;
    info.session_id = "session-123";
    info.cwd = "/home/user/project";
    info.model_id = "claude-3-opus";
    info.mode = "code";
    info.title = "My Session";
    
    REQUIRE(info.session_id == "session-123");
    REQUIRE(info.cwd == "/home/user/project");
    REQUIRE(info.model_id == "claude-3-opus");
    REQUIRE(info.mode == "code");
    REQUIRE(info.title == "My Session");
}

// ==================== NewSessionRequest 测试 ====================

TEST_CASE("ACP.NewSessionRequest.Defaults", "[ACP]") {
    NewSessionRequest req;
    REQUIRE(req.cwd.empty());
    REQUIRE(req.mcp_servers.empty());
    REQUIRE_FALSE(req.model_id.has_value());
}

TEST_CASE("ACP.NewSessionRequest.WithValues", "[ACP]") {
    NewSessionRequest req;
    req.cwd = "/project";
    req.mcp_servers = {R"({"name": "filesystem"})"_json};
    req.model_id = "gpt-4";
    
    REQUIRE(req.cwd == "/project");
    REQUIRE(req.mcp_servers.size() == 1);
    REQUIRE(req.model_id == "gpt-4");
}

// ==================== LoadSessionRequest 测试 ====================

TEST_CASE("ACP.LoadSessionRequest.Defaults", "[ACP]") {
    LoadSessionRequest req;
    REQUIRE(req.session_id.empty());
    REQUIRE(req.cwd.empty());
    REQUIRE(req.mcp_servers.empty());
}

// ==================== ResumeSessionRequest 测试 ====================

TEST_CASE("ACP.ResumeSessionRequest.Defaults", "[ACP]") {
    ResumeSessionRequest req;
    REQUIRE(req.session_id.empty());
    REQUIRE(req.cwd.empty());
}

// ==================== ForkSessionRequest 测试 ====================

TEST_CASE("ACP.ForkSessionRequest.Defaults", "[ACP]") {
    ForkSessionRequest req;
    REQUIRE(req.session_id.empty());
    REQUIRE(req.cwd.empty());
}

// ==================== TextContent 测试 ====================

TEST_CASE("ACP.TextContent.Defaults", "[ACP]") {
    TextContent content;
    REQUIRE(content.type == "text");
    REQUIRE(content.text.empty());
}

TEST_CASE("ACP.TextContent.WithValues", "[ACP]") {
    TextContent content;
    content.text = "Hello, world!";
    
    REQUIRE(content.type == "text");
    REQUIRE(content.text == "Hello, world!");
}

// ==================== ResourceContent 测试 ====================

TEST_CASE("ACP.ResourceContent.Defaults", "[ACP]") {
    ResourceContent content;
    REQUIRE(content.type == "resource");
    REQUIRE(content.uri.empty());
    REQUIRE_FALSE(content.mime_type.has_value());
}

TEST_CASE("ACP.ResourceContent.WithValues", "[ACP]") {
    ResourceContent content;
    content.uri = "file:///document.pdf";
    content.mime_type = "application/pdf";
    
    REQUIRE(content.type == "resource");
    REQUIRE(content.uri == "file:///document.pdf");
    REQUIRE(content.mime_type == "application/pdf");
}

// ==================== PromptRequest 测试 ====================

TEST_CASE("ACP.PromptRequest.Defaults", "[ACP]") {
    PromptRequest req;
    REQUIRE(req.session_id.empty());
    REQUIRE(req.prompt.empty());
    REQUIRE_FALSE(req.mode.has_value());
    REQUIRE_FALSE(req.model.has_value());
}

TEST_CASE("ACP.PromptRequest.WithValues", "[ACP]") {
    PromptRequest req;
    req.session_id = "session-456";
    req.prompt = {R"({"type": "text", "text": "Hello"})"_json};
    req.mode = "chat";
    req.model = "claude-3-sonnet";
    
    REQUIRE(req.session_id == "session-456");
    REQUIRE(req.prompt.size() == 1);
    REQUIRE(req.mode == "chat");
    REQUIRE(req.model == "claude-3-sonnet");
}

// ==================== SetSessionModeRequest 测试 ====================

TEST_CASE("ACP.SetSessionModeRequest.Defaults", "[ACP]") {
    SetSessionModeRequest req;
    REQUIRE(req.session_id.empty());
    REQUIRE(req.mode_id.empty());
}

// ==================== SetSessionModeResponse 测试 ====================

TEST_CASE("ACP.SetSessionModeResponse.Defaults", "[ACP]") {
    SetSessionModeResponse resp;
    REQUIRE(resp.mode.empty());
}
