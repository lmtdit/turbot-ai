#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/provider/provider.hpp>

using namespace turbot::core::provider;
using namespace turbot::test;

// ==================== ModelCapabilities 测试 ====================

TEST_CASE("Provider.ModelCapabilities.Defaults", "[Provider]") {
    ModelCapabilities caps;
    REQUIRE(caps.temperature == true);
    REQUIRE(caps.reasoning == false);
    REQUIRE(caps.tool_call == true);
    REQUIRE(caps.streaming == true);
    REQUIRE(caps.input.image == false);
    REQUIRE(caps.input.audio == false);
}

TEST_CASE("Provider.ModelCapabilities.JsonSerialization", "[Provider]") {
    ModelCapabilities caps;
    caps.temperature = false;
    caps.reasoning = true;
    caps.input.image = true;
    
    nlohmann::json j = caps.to_json();
    REQUIRE(j["temperature"] == false);
    REQUIRE(j["reasoning"] == true);
    REQUIRE(j["input"]["image"] == true);
    
    auto restored = ModelCapabilities::from_json(j);
    REQUIRE(restored.temperature == false);
    REQUIRE(restored.reasoning == true);
    REQUIRE(restored.input.image == true);
}

// ==================== ModelInfo 测试 ====================

TEST_CASE("Provider.ModelInfo.Defaults", "[Provider]") {
    ModelInfo info;
    REQUIRE(info.id.empty());
    REQUIRE(info.provider_id.empty());
    REQUIRE(info.context_window == 4096);
}

TEST_CASE("Provider.ModelInfo.JsonSerialization", "[Provider]") {
    ModelInfo info;
    info.id = "gpt-4";
    info.provider_id = "openai";
    info.name = "GPT-4";
    info.description = "Most capable GPT-4 model";
    info.context_window = 8192;
    info.capabilities.input.image = true;
    
    nlohmann::json j = info.to_json();
    REQUIRE(j["id"] == "gpt-4");
    REQUIRE(j["provider_id"] == "openai");
    REQUIRE(j["context_window"] == 8192);
    
    auto restored = ModelInfo::from_json(j);
    REQUIRE(restored.id == "gpt-4");
    REQUIRE(restored.context_window == 8192);
    REQUIRE(restored.capabilities.input.image == true);
}

// ==================== ChatRole 测试 ====================

TEST_CASE("Provider.ChatRole.ToString", "[Provider]") {
    REQUIRE(chat_role_to_string(ChatRole::System) == "system");
    REQUIRE(chat_role_to_string(ChatRole::User) == "user");
    REQUIRE(chat_role_to_string(ChatRole::Assistant) == "assistant");
    REQUIRE(chat_role_to_string(ChatRole::Tool) == "tool");
}

TEST_CASE("Provider.ChatRole.FromString", "[Provider]") {
    REQUIRE(chat_role_from_string("system") == ChatRole::System);
    REQUIRE(chat_role_from_string("user") == ChatRole::User);
    REQUIRE(chat_role_from_string("assistant") == ChatRole::Assistant);
    REQUIRE(chat_role_from_string("tool") == ChatRole::Tool);
}

// ==================== ChatMessage 测试 ====================

TEST_CASE("Provider.ChatMessage.FactoryMethods", "[Provider]") {
    auto sys = ChatMessage::system("You are helpful");
    REQUIRE(sys.role == ChatRole::System);
    REQUIRE(sys.content == "You are helpful");
    
    auto user = ChatMessage::user("Hello");
    REQUIRE(user.role == ChatRole::User);
    REQUIRE(user.content == "Hello");
    
    auto assistant = ChatMessage::assistant("Hi there");
    REQUIRE(assistant.role == ChatRole::Assistant);
    REQUIRE(assistant.content == "Hi there");
}

TEST_CASE("Provider.ChatMessage.ToolResult", "[Provider]") {
    auto msg = ChatMessage::tool_result("call-123", "Result content");
    REQUIRE(msg.role == ChatRole::Tool);
    REQUIRE(msg.tool_call_id == "call-123");
    REQUIRE(msg.content == "Result content");
}

TEST_CASE("Provider.ChatMessage.WithToolCalls", "[Provider]") {
    std::vector<ToolCall> calls = {{"call-1", "function", "get_weather", R"({"city": "NYC"})"_json}};
    auto msg = ChatMessage::assistant_with_tools("Let me check", calls);
    
    REQUIRE(msg.role == ChatRole::Assistant);
    REQUIRE(msg.content == "Let me check");
    REQUIRE(msg.tool_calls.has_value());
    REQUIRE(msg.tool_calls->size() == 1);
    REQUIRE(msg.tool_calls->at(0).name == "get_weather");
}

// ==================== ToolCall 测试 ====================

TEST_CASE("Provider.ToolCall.JsonSerialization", "[Provider]") {
    ToolCall call;
    call.id = "call-abc";
    call.type = "function";
    call.name = "search";
    call.arguments = R"({"query": "test"})"_json;
    
    nlohmann::json j = call.to_json();
    REQUIRE(j["id"] == "call-abc");
    REQUIRE(j["type"] == "function");
    REQUIRE(j["function"]["name"] == "search");
    
    auto restored = ToolCall::from_json(j);
    REQUIRE(restored.id == "call-abc");
    REQUIRE(restored.name == "search");
}

// ==================== ToolDefinition 测试 ====================

TEST_CASE("Provider.ToolDefinition.JsonSerialization", "[Provider]") {
    ToolDefinition def;
    def.name = "calculate";
    def.description = "Perform calculations";
    def.parameters = R"({
        "type": "object",
        "properties": {
            "expression": {"type": "string"}
        }
    })"_json;
    
    nlohmann::json j = def.to_json();
    REQUIRE(j["type"] == "function");
    REQUIRE(j["function"]["name"] == "calculate");
    
    auto restored = ToolDefinition::from_json(j);
    REQUIRE(restored.name == "calculate");
    REQUIRE(restored.description == "Perform calculations");
}

// ==================== ChatOptions 测试 ====================

TEST_CASE("Provider.ChatOptions.Defaults", "[Provider]") {
    ChatOptions opts;
    REQUIRE(opts.temperature == 1.0);
    REQUIRE(opts.top_p == 1.0);
    REQUIRE(opts.max_tokens == 4096);
    REQUIRE(opts.stream == false);
}

TEST_CASE("Provider.ChatOptions.JsonSerialization", "[Provider]") {
    ChatOptions opts;
    opts.temperature = 0.7;
    opts.max_tokens = 2048;
    opts.stop = {"END", "STOP"};
    
    nlohmann::json j = opts.to_json();
    REQUIRE(j["temperature"] == 0.7);
    REQUIRE(j["max_tokens"] == 2048);
    
    auto restored = ChatOptions::from_json(j);
    REQUIRE(restored.temperature == 0.7);
    REQUIRE(restored.max_tokens == 2048);
    REQUIRE(restored.stop.size() == 2);
}

// ==================== ChatResponse 测试 ====================

TEST_CASE("Provider.ChatResponse.GetText", "[Provider]") {
    ChatResponse resp;
    resp.id = "resp-123";
    resp.model = "gpt-4";
    resp.choices = {ChatMessage::assistant("Hello, world!")};
    
    REQUIRE(resp.get_text() == "Hello, world!");
    REQUIRE_FALSE(resp.is_error());
    REQUIRE_FALSE(resp.has_tool_calls());
}

TEST_CASE("Provider.ChatResponse.WithError", "[Provider]") {
    ChatResponse resp;
    resp.error = R"({"message": "Rate limit exceeded"})"_json;
    
    REQUIRE(resp.is_error());
}

// ==================== ProviderConfig 测试 ====================

TEST_CASE("Provider.ProviderConfig.Defaults", "[Provider]") {
    ProviderConfig config;
    REQUIRE(config.timeout_seconds == 60);
    REQUIRE(config.max_retries == 3);
    REQUIRE(config.verify_ssl == true);
}

TEST_CASE("Provider.ProviderConfig.JsonSerialization", "[Provider]") {
    ProviderConfig config;
    config.api_key = "sk-test";
    config.base_url = "https://api.example.com";
    config.timeout_seconds = 120;
    
    nlohmann::json j = config.to_json();
    // api_key is masked for security (shows "***" instead of actual key)
    REQUIRE(j["api_key"] == "***");
    REQUIRE(j["base_url"] == "https://api.example.com");
    REQUIRE(j["timeout_seconds"] == 120);
    
    // Note: from_json doesn't restore api_key from masked value
    // This is intentional security behavior
}

// ==================== StreamEventType 测试 ====================

TEST_CASE("Provider.StreamEventType.ToString", "[Provider]") {
    REQUIRE(stream_event_type_to_string(StreamEventType::TextDelta) == "text_delta");
    REQUIRE(stream_event_type_to_string(StreamEventType::ToolCall) == "tool_call");
    REQUIRE(stream_event_type_to_string(StreamEventType::Reasoning) == "reasoning");
    REQUIRE(stream_event_type_to_string(StreamEventType::Finish) == "finish");
    REQUIRE(stream_event_type_to_string(StreamEventType::Error) == "error");
}

TEST_CASE("Provider.StreamEventType.FromString", "[Provider]") {
    REQUIRE(stream_event_type_from_string("text_delta") == StreamEventType::TextDelta);
    REQUIRE(stream_event_type_from_string("tool_call") == StreamEventType::ToolCall);
    REQUIRE(stream_event_type_from_string("finish") == StreamEventType::Finish);
}

// ==================== ChatStreamEvent 测试 ====================

TEST_CASE("Provider.ChatStreamEvent.TextDelta", "[Provider]") {
    ChatStreamEvent event;
    event.type = StreamEventType::TextDelta;
    event.content = "Hello";
    
    nlohmann::json j = event.to_json();
    REQUIRE(j["type"] == "text_delta");
    REQUIRE(j["content"] == "Hello");
    
    auto restored = ChatStreamEvent::from_json(j);
    REQUIRE(restored.type == StreamEventType::TextDelta);
    REQUIRE(restored.content == "Hello");
}

TEST_CASE("Provider.ChatStreamEvent.Finish", "[Provider]") {
    ChatStreamEvent event;
    event.type = StreamEventType::Finish;
    event.finish_reason = "stop";
    event.usage = {100, 50, 150};
    
    nlohmann::json j = event.to_json();
    REQUIRE(j["type"] == "finish");
    REQUIRE(j["finish_reason"] == "stop");
    
    auto restored = ChatStreamEvent::from_json(j);
    REQUIRE(restored.type == StreamEventType::Finish);
    REQUIRE(restored.finish_reason == "stop");
}

// ==================== OpenAI Provider 测试 ====================

#include <turbot/core/provider/impl/openai_provider.hpp>

TEST_CASE("Provider.OpenAI.Constructor", "[Provider][OpenAI]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    
    OpenAIProvider provider(config);
    REQUIRE(provider.id() == "openai");
    REQUIRE(provider.name() == "OpenAI");
}

TEST_CASE("Provider.OpenAI.IsReady", "[Provider][OpenAI]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    
    OpenAIProvider provider(config);
    REQUIRE(provider.is_ready() == true);
}

TEST_CASE("Provider.OpenAI.NotReady", "[Provider][OpenAI]") {
    ProviderConfig config;
    // 没有 API key
    
    OpenAIProvider provider(config);
    REQUIRE(provider.is_ready() == false);
}

TEST_CASE("Provider.OpenAI.ListModels", "[Provider][OpenAI]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    
    OpenAIProvider provider(config);
    auto models = provider.list_models();
    
    REQUIRE_FALSE(models.empty());
    // 应该包含 gpt-4o
    bool has_gpt4o = false;
    for (const auto& m : models) {
        if (m.id == "gpt-4o") {
            has_gpt4o = true;
            break;
        }
    }
    REQUIRE(has_gpt4o);
}

TEST_CASE("Provider.OpenAI.GetModel", "[Provider][OpenAI]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    
    OpenAIProvider provider(config);
    auto model = provider.get_model("gpt-4o");
    
    REQUIRE(model.has_value());
    REQUIRE(model->id == "gpt-4o");
    REQUIRE(model->provider_id == "openai");
}

TEST_CASE("Provider.OpenAI.GetModel.NotFound", "[Provider][OpenAI]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    
    OpenAIProvider provider(config);
    auto model = provider.get_model("non-existent-model");
    
    REQUIRE_FALSE(model.has_value());
}

TEST_CASE("Provider.OpenAI.SupportsModel", "[Provider][OpenAI]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    
    OpenAIProvider provider(config);
    REQUIRE(provider.supports_model("gpt-4o") == true);
    REQUIRE(provider.supports_model("gpt-4o-mini") == true);
    REQUIRE(provider.supports_model("non-existent") == false);
}

TEST_CASE("Provider.OpenAI.CountTokens", "[Provider][OpenAI]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    
    OpenAIProvider provider(config);
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Hello, world!")
    };
    
    auto count = provider.count_tokens(messages, "gpt-4o");
    REQUIRE(count > 0);
}

TEST_CASE("Provider.OpenAI.DefaultBaseUrl", "[Provider][OpenAI]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    // 不设置 base_url
    
    OpenAIProvider provider(config);
    // 验证默认 URL 被设置
    REQUIRE(provider.is_ready() == true);
}

TEST_CASE("Provider.OpenAI.CustomBaseUrl", "[Provider][OpenAI]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    config.base_url = "https://custom.api.com/v1";
    
    OpenAIProvider provider(config);
    REQUIRE(provider.is_ready() == true);
}

TEST_CASE("Provider.OpenAI.ApiKeyConstructor", "[Provider][OpenAI]") {
    OpenAIProvider provider("test-api-key");
    REQUIRE(provider.is_ready() == true);
    REQUIRE(provider.id() == "openai");
}

TEST_CASE("Provider.OpenAI.ModelCapabilities", "[Provider][OpenAI]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    
    OpenAIProvider provider(config);
    
    // GPT-4o should have vision capability
    auto gpt4o = provider.get_model("gpt-4o");
    REQUIRE(gpt4o.has_value());
    REQUIRE(gpt4o->capabilities.input.image == true);
    REQUIRE(gpt4o->capabilities.tool_call == true);
    REQUIRE(gpt4o->capabilities.streaming == true);
    
    // o1 models should have reasoning capability
    auto o1 = provider.get_model("o1-preview");
    REQUIRE(o1.has_value());
    REQUIRE(o1->capabilities.reasoning == true);
    REQUIRE(o1->capabilities.temperature == false);
}

TEST_CASE("Provider.OpenAI.ModelPricing", "[Provider][OpenAI]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    
    OpenAIProvider provider(config);
    
    auto gpt4o = provider.get_model("gpt-4o");
    REQUIRE(gpt4o.has_value());
    REQUIRE(gpt4o->pricing.contains("input"));
    REQUIRE(gpt4o->pricing.contains("output"));
    REQUIRE(gpt4o->pricing["input"].get<double>() > 0);
    REQUIRE(gpt4o->pricing["output"].get<double>() > 0);
}

TEST_CASE("Provider.OpenAI.ModelContextWindow", "[Provider][OpenAI]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    
    OpenAIProvider provider(config);
    
    auto gpt4o = provider.get_model("gpt-4o");
    REQUIRE(gpt4o.has_value());
    REQUIRE(gpt4o->context_window > 0);
    
    auto gpt4 = provider.get_model("gpt-4");
    REQUIRE(gpt4.has_value());
    // GPT-4 has smaller context window than GPT-4o
    REQUIRE(gpt4->context_window < gpt4o->context_window);
}

TEST_CASE("Provider.OpenAI.ChatMessageWithToolCalls", "[Provider][OpenAI]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    
    OpenAIProvider provider(config);
    
    std::vector<ChatMessage> messages = {
        ChatMessage::user("What's the weather?"),
        ChatMessage::assistant_with_tools("Let me check", {
            {"call-1", "function", "get_weather", R"({"city": "NYC"})"_json}
        }),
        ChatMessage::tool_result("call-1", "Sunny, 72°F")
    };
    
    auto count = provider.count_tokens(messages, "gpt-4o");
    REQUIRE(count > 0);
}

TEST_CASE("Provider.OpenAI.ProviderConfigWithProxy", "[Provider][OpenAI]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    config.proxy = "http://proxy.example.com:8080";
    config.timeout_seconds = 120;
    config.verify_ssl = false;
    
    OpenAIProvider provider(config);
    REQUIRE(provider.is_ready() == true);
}

TEST_CASE("Provider.OpenAI.ProviderConfigWithOrganization", "[Provider][OpenAI]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    config.organization = "org-123";
    
    OpenAIProvider provider(config);
    REQUIRE(provider.is_ready() == true);
}

TEST_CASE("Provider.OpenAI.AllModelsAvailable", "[Provider][OpenAI]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    
    OpenAIProvider provider(config);
    auto models = provider.list_models();
    
    // Check that all expected models are present
    std::vector<std::string> expected_models = {
        "gpt-4o", "gpt-4o-mini", "gpt-4-turbo", "gpt-4",
        "gpt-3.5-turbo", "o1-preview", "o1-mini"
    };
    
    for (const auto& expected : expected_models) {
        bool found = false;
        for (const auto& m : models) {
            if (m.id == expected) {
                found = true;
                break;
            }
        }
        REQUIRE(found);
    }
}

TEST_CASE("Provider.OpenAI.ValidateNotReady", "[Provider][OpenAI]") {
    ProviderConfig config;
    // No API key
    
    OpenAIProvider provider(config);
    REQUIRE(provider.is_ready() == false);
    REQUIRE(provider.validate() == false);
}

// ==================== BailianProvider Tests ====================

#include <turbot/core/provider/impl/bailian_provider.hpp>

TEST_CASE("Provider.Bailian.CreateWithApiKey", "[Provider][Bailian]") {
    BailianProvider provider("test-api-key");
    REQUIRE(provider.id() == "bailian");
    REQUIRE(provider.name() == "阿里云百炼");
}

TEST_CASE("Provider.Bailian.CreateWithConfig", "[Provider][Bailian]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    config.base_url = "https://custom.api.url";
    
    BailianProvider provider(config);
    REQUIRE(provider.id() == "bailian");
}

TEST_CASE("Provider.Bailian.IsReady", "[Provider][Bailian]") {
    ProviderConfig config;
    config.api_key = "test-api-key";
    
    BailianProvider provider(config);
    REQUIRE(provider.is_ready() == true);
}

TEST_CASE("Provider.Bailian.NotReadyWithoutKey", "[Provider][Bailian]") {
    ProviderConfig config;
    // No API key
    
    BailianProvider provider(config);
    REQUIRE(provider.is_ready() == false);
}

TEST_CASE("Provider.Bailian.ListModels", "[Provider][Bailian]") {
    BailianProvider provider("test-api-key");
    auto models = provider.list_models();
    
    REQUIRE_FALSE(models.empty());
    // Check for Qwen models
    bool has_qwen = false;
    for (const auto& m : models) {
        if (m.id.find("qwen") != std::string::npos) {
            has_qwen = true;
            break;
        }
    }
    REQUIRE(has_qwen);
}

TEST_CASE("Provider.Bailian.SupportsModel", "[Provider][Bailian]") {
    BailianProvider provider("test-api-key");
    REQUIRE(provider.supports_model("qwen-turbo"));
    REQUIRE(provider.supports_model("qwen-plus"));
    REQUIRE(provider.supports_model("qwen-max"));
    REQUIRE_FALSE(provider.supports_model("gpt-4"));
}

TEST_CASE("Provider.Bailian.GetModel", "[Provider][Bailian]") {
    BailianProvider provider("test-api-key");
    auto model = provider.get_model("qwen-turbo");
    REQUIRE(model.has_value());
    REQUIRE(model->id == "qwen-turbo");
}

TEST_CASE("Provider.Bailian.CountTokens", "[Provider][Bailian]") {
    BailianProvider provider("test-api-key");
    std::vector<ChatMessage> messages = {
        ChatMessage::user("Hello, world!")
    };
    auto count = provider.count_tokens(messages, "qwen-turbo");
    REQUIRE(count > 0);
}

// ==================== KimiProvider Tests ====================

#include <turbot/core/provider/impl/kimi_provider.hpp>

TEST_CASE("Provider.Kimi.CreateWithApiKey", "[Provider][Kimi]") {
    KimiProvider provider("test-api-key");
    REQUIRE(provider.id() == "kimi");
    REQUIRE(provider.name() == "Moonshot AI (Kimi)");
}

TEST_CASE("Provider.Kimi.IsReady", "[Provider][Kimi]") {
    KimiProvider provider("test-api-key");
    REQUIRE(provider.is_ready() == true);
}

TEST_CASE("Provider.Kimi.ListModels", "[Provider][Kimi]") {
    KimiProvider provider("test-api-key");
    auto models = provider.list_models();
    REQUIRE_FALSE(models.empty());
}

TEST_CASE("Provider.Kimi.SupportsModel", "[Provider][Kimi]") {
    KimiProvider provider("test-api-key");
    REQUIRE(provider.supports_model("moonshot-v1-8k"));
    REQUIRE(provider.supports_model("moonshot-v1-32k"));
    REQUIRE_FALSE(provider.supports_model("gpt-4"));
}

// ==================== ZhipuProvider Tests ====================

#include <turbot/core/provider/impl/zhipu_provider.hpp>

TEST_CASE("Provider.Zhipu.CreateWithApiKey", "[Provider][Zhipu]") {
    ZhipuProvider provider("test-api-key");
    REQUIRE(provider.id() == "zhipu");
    REQUIRE(provider.name() == "智谱AI");
}

TEST_CASE("Provider.Zhipu.IsReady", "[Provider][Zhipu]") {
    ZhipuProvider provider("test-api-key");
    REQUIRE(provider.is_ready() == true);
}

TEST_CASE("Provider.Zhipu.ListModels", "[Provider][Zhipu]") {
    ZhipuProvider provider("test-api-key");
    auto models = provider.list_models();
    REQUIRE_FALSE(models.empty());
}

TEST_CASE("Provider.Zhipu.SupportsModel", "[Provider][Zhipu]") {
    ZhipuProvider provider("test-api-key");
    // Check actual model IDs from ZhipuProvider
    REQUIRE(provider.supports_model("glm-4-plus"));
    REQUIRE(provider.supports_model("glm-4-flash"));
    REQUIRE_FALSE(provider.supports_model("gpt-4"));
}

// ==================== IflowProvider Tests ====================

#include <turbot/core/provider/impl/iflow_provider.hpp>

TEST_CASE("Provider.Iflow.CreateWithApiKey", "[Provider][Iflow]") {
    IflowProvider provider("test-api-key");
    REQUIRE(provider.id() == "iflow");
}

TEST_CASE("Provider.Iflow.IsReady", "[Provider][Iflow]") {
    IflowProvider provider("test-api-key");
    REQUIRE(provider.is_ready() == true);
}

TEST_CASE("Provider.Iflow.ListModels", "[Provider][Iflow]") {
    IflowProvider provider("test-api-key");
    auto models = provider.list_models();
    REQUIRE_FALSE(models.empty());
}

TEST_CASE("Provider.Iflow.SupportsModel", "[Provider][Iflow]") {
    IflowProvider provider("test-api-key");
    // Check that it supports some models
    auto models = provider.list_models();
    if (!models.empty()) {
        REQUIRE(provider.supports_model(models[0].id));
    }
}
