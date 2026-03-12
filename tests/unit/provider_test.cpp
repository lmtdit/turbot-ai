#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <turbot/core/provider/provider.hpp>
#include <turbot/core/provider/provider_manager.hpp>
#include <turbot/core/provider/impl/openai_provider.hpp>
#include <turbot/core/provider/impl/bailian_provider.hpp>
#include <turbot/core/provider/impl/zhipu_provider.hpp>
#include <turbot/core/provider/impl/kimi_provider.hpp>
#include <turbot/core/provider/impl/iflow_provider.hpp>
#include <turbot/core/llm/provider_adapter.hpp>
#include <turbot/core/llm/llm.hpp>
#include <nlohmann/json.hpp>

using namespace turbot::core::provider;
using namespace turbot::core::llm;
namespace core = turbot::core;

// ============================================================================
// ModelCapabilities Tests
// ============================================================================

TEST_CASE("ModelCapabilities::to_json", "[provider][capabilities]") {
    ModelCapabilities caps;
    caps.temperature = true;
    caps.reasoning = true;
    caps.tool_call = false;
    caps.streaming = true;
    caps.vision = true;
    caps.audio = false;

    auto j = caps.to_json();

    REQUIRE(j["temperature"] == true);
    REQUIRE(j["reasoning"] == true);
    REQUIRE(j["tool_call"] == false);
    REQUIRE(j["streaming"] == true);
    REQUIRE(j["vision"] == true);
    REQUIRE(j["audio"] == false);
}

TEST_CASE("ModelCapabilities::from_json", "[provider][capabilities]") {
    nlohmann::json j = {
        {"temperature", false},
        {"reasoning", true},
        {"tool_call", true},
        {"streaming", false},
        {"vision", true},
        {"audio", true}
    };

    auto caps = ModelCapabilities::from_json(j);

    REQUIRE(caps.temperature == false);
    REQUIRE(caps.reasoning == true);
    REQUIRE(caps.tool_call == true);
    REQUIRE(caps.streaming == false);
    REQUIRE(caps.vision == true);
    REQUIRE(caps.audio == true);
}

TEST_CASE("ModelCapabilities::from_json_partial", "[provider][capabilities]") {
    nlohmann::json j = {{"temperature", false}};

    auto caps = ModelCapabilities::from_json(j);

    REQUIRE(caps.temperature == false);
    REQUIRE(caps.reasoning == false);  // default
    REQUIRE(caps.tool_call == true);   // default
}

// ============================================================================
// ModelInfo Tests
// ============================================================================

TEST_CASE("ModelInfo::to_json", "[provider][model_info]") {
    ModelInfo info;
    info.id = "gpt-4";
    info.provider_id = "openai";
    info.name = "GPT-4";
    info.description = "Most capable model";
    info.context_window = 8192;
    info.pricing = {{"input", 0.03}, {"output", 0.06}};
    info.limits = {{"rpm", 500}};
    info.capabilities.temperature = true;
    info.capabilities.reasoning = false;

    auto j = info.to_json();

    REQUIRE(j["id"] == "gpt-4");
    REQUIRE(j["provider_id"] == "openai");
    REQUIRE(j["name"] == "GPT-4");
    REQUIRE(j["description"] == "Most capable model");
    REQUIRE(j["context_window"] == 8192);
    REQUIRE(j["pricing"]["input"] == 0.03);
    REQUIRE(j["capabilities"]["temperature"] == true);
}

TEST_CASE("ModelInfo::from_json", "[provider][model_info]") {
    nlohmann::json j = {
        {"id", "gpt-3.5-turbo"},
        {"provider_id", "openai"},
        {"name", "GPT-3.5 Turbo"},
        {"description", "Fast model"},
        {"context_window", 16384},
        {"pricing", {{"input", 0.001}, {"output", 0.002}}},
        {"limits", {{"rpm", 3500}}},
        {"capabilities", {{"temperature", true}, {"tool_call", true}}}
    };

    auto info = ModelInfo::from_json(j);

    REQUIRE(info.id == "gpt-3.5-turbo");
    REQUIRE(info.provider_id == "openai");
    REQUIRE(info.name == "GPT-3.5 Turbo");
    REQUIRE(info.description == "Fast model");
    REQUIRE(info.context_window == 16384);
    REQUIRE(info.pricing["input"] == 0.001);
    REQUIRE(info.capabilities.temperature == true);
    REQUIRE(info.capabilities.tool_call == true);
}

// ============================================================================
// ChatRole Tests
// ============================================================================

TEST_CASE("chat_role_to_string", "[provider][chat_role]") {
    REQUIRE(chat_role_to_string(ChatRole::System) == "system");
    REQUIRE(chat_role_to_string(ChatRole::User) == "user");
    REQUIRE(chat_role_to_string(ChatRole::Assistant) == "assistant");
    REQUIRE(chat_role_to_string(ChatRole::Tool) == "tool");
}

TEST_CASE("chat_role_from_string", "[provider][chat_role]") {
    REQUIRE(chat_role_from_string("system") == ChatRole::System);
    REQUIRE(chat_role_from_string("user") == ChatRole::User);
    REQUIRE(chat_role_from_string("assistant") == ChatRole::Assistant);
    REQUIRE(chat_role_from_string("tool") == ChatRole::Tool);
    REQUIRE(chat_role_from_string("unknown") == ChatRole::User);  // default
}

// ============================================================================
// ToolCall Tests
// ============================================================================

TEST_CASE("ToolCall::to_json", "[provider][tool_call]") {
    ToolCall call;
    call.id = "call_123";
    call.type = "function";
    call.name = "get_weather";
    call.arguments = {{"location", "Beijing"}};

    auto j = call.to_json();

    REQUIRE(j["id"] == "call_123");
    REQUIRE(j["type"] == "function");
    REQUIRE(j["function"]["name"] == "get_weather");
    REQUIRE(j["function"]["arguments"] == R"({"location":"Beijing"})");
}

TEST_CASE("ToolCall::from_json", "[provider][tool_call]") {
    nlohmann::json j = {
        {"id", "call_456"},
        {"type", "function"},
        {"function", {
            {"name", "search"},
            {"arguments", R"({"query":"test"})"}
        }}
    };

    auto call = ToolCall::from_json(j);

    REQUIRE(call.id == "call_456");
    REQUIRE(call.type == "function");
    REQUIRE(call.name == "search");
    REQUIRE(call.arguments["query"] == "test");
}

TEST_CASE("ToolCall::from_json_invalid_arguments", "[provider][tool_call]") {
    nlohmann::json j = {
        {"id", "call_789"},
        {"type", "function"},
        {"function", {
            {"name", "test"},
            {"arguments", "invalid json"}
        }}
    };

    auto call = ToolCall::from_json(j);

    REQUIRE(call.id == "call_789");
    REQUIRE(call.arguments.is_object());  // Should be empty object on parse error
}

// ============================================================================
// ChatMessage Tests
// ============================================================================

TEST_CASE("ChatMessage::to_json", "[provider][chat_message]") {
    ChatMessage msg;
    msg.role = ChatRole::User;
    msg.content = "Hello, world!";

    auto j = msg.to_json();

    REQUIRE(j["role"] == "user");
    REQUIRE(j["content"] == "Hello, world!");
}

TEST_CASE("ChatMessage::to_json_with_tool_calls", "[provider][chat_message]") {
    ChatMessage msg;
    msg.role = ChatRole::Assistant;
    msg.content = "";
    msg.tool_calls = std::vector<ToolCall>{
        ToolCall{.id = "call_1", .type = "function", .name = "test", .arguments = {}}
    };

    auto j = msg.to_json();

    REQUIRE(j["role"] == "assistant");
    REQUIRE(j["tool_calls"].is_array());
    REQUIRE(j["tool_calls"][0]["id"] == "call_1");
}

TEST_CASE("ChatMessage::from_json", "[provider][chat_message]") {
    nlohmann::json j = {
        {"role", "assistant"},
        {"content", "Hello!"},
        {"tool_calls", {
            {{"id", "tc1"}, {"type", "function"}, {"function", {{"name", "fn"}, {"arguments", "{}"}}}}
        }}
    };

    auto msg = ChatMessage::from_json(j);

    REQUIRE(msg.role == ChatRole::Assistant);
    REQUIRE(msg.content == "Hello!");
    REQUIRE(msg.tool_calls.has_value());
    REQUIRE(msg.tool_calls->size() == 1);
    REQUIRE(msg.tool_calls->at(0).id == "tc1");
}

TEST_CASE("ChatMessage::system factory", "[provider][chat_message]") {
    auto msg = ChatMessage::system("You are helpful.");

    REQUIRE(msg.role == ChatRole::System);
    REQUIRE(msg.content == "You are helpful.");
}

TEST_CASE("ChatMessage::user factory", "[provider][chat_message]") {
    auto msg = ChatMessage::user("Hello!");

    REQUIRE(msg.role == ChatRole::User);
    REQUIRE(msg.content == "Hello!");
}

TEST_CASE("ChatMessage::assistant factory", "[provider][chat_message]") {
    auto msg = ChatMessage::assistant("Hi there!");

    REQUIRE(msg.role == ChatRole::Assistant);
    REQUIRE(msg.content == "Hi there!");
}

TEST_CASE("ChatMessage::assistant_with_tools factory", "[provider][chat_message]") {
    std::vector<ToolCall> calls = {
        ToolCall{.id = "call_1", .name = "test_func", .arguments = {}}
    };

    auto msg = ChatMessage::assistant_with_tools("Let me help", calls);

    REQUIRE(msg.role == ChatRole::Assistant);
    REQUIRE(msg.content == "Let me help");
    REQUIRE(msg.tool_calls.has_value());
    REQUIRE(msg.tool_calls->size() == 1);
}

TEST_CASE("ChatMessage::tool_result factory", "[provider][chat_message]") {
    auto msg = ChatMessage::tool_result("call_123", "Result data");

    REQUIRE(msg.role == ChatRole::Tool);
    REQUIRE(msg.tool_call_id == "call_123");
    REQUIRE(msg.content == "Result data");
}

// ============================================================================
// ToolDefinition Tests
// ============================================================================

TEST_CASE("ToolDefinition::to_json", "[provider][tool_definition]") {
    ToolDefinition tool;
    tool.name = "get_weather";
    tool.description = "Get weather info";
    tool.parameters = {
        {"type", "object"},
        {"properties", {{"location", {{"type", "string"}}}}}
    };

    auto j = tool.to_json();

    REQUIRE(j["type"] == "function");
    REQUIRE(j["function"]["name"] == "get_weather");
    REQUIRE(j["function"]["description"] == "Get weather info");
    REQUIRE(j["function"]["parameters"]["type"] == "object");
}

TEST_CASE("ToolDefinition::from_json", "[provider][tool_definition]") {
    nlohmann::json j = {
        {"type", "function"},
        {"function", {
            {"name", "search"},
            {"description", "Search the web"},
            {"parameters", {{"type", "object"}}}
        }}
    };

    auto tool = ToolDefinition::from_json(j);

    REQUIRE(tool.type == "function");
    REQUIRE(tool.name == "search");
    REQUIRE(tool.description == "Search the web");
    REQUIRE(tool.parameters["type"] == "object");
}

// ============================================================================
// StreamEventType Tests
// ============================================================================

TEST_CASE("stream_event_type_to_string", "[provider][stream_event]") {
    REQUIRE(stream_event_type_to_string(StreamEventType::TextDelta) == "text_delta");
    REQUIRE(stream_event_type_to_string(StreamEventType::ToolCall) == "tool_call");
    REQUIRE(stream_event_type_to_string(StreamEventType::Reasoning) == "reasoning");
    REQUIRE(stream_event_type_to_string(StreamEventType::Finish) == "finish");
    REQUIRE(stream_event_type_to_string(StreamEventType::Error) == "error");
}

TEST_CASE("stream_event_type_from_string", "[provider][stream_event]") {
    REQUIRE(stream_event_type_from_string("text_delta") == StreamEventType::TextDelta);
    REQUIRE(stream_event_type_from_string("tool_call") == StreamEventType::ToolCall);
    REQUIRE(stream_event_type_from_string("reasoning") == StreamEventType::Reasoning);
    REQUIRE(stream_event_type_from_string("finish") == StreamEventType::Finish);
    REQUIRE(stream_event_type_from_string("error") == StreamEventType::Error);
    REQUIRE(stream_event_type_from_string("unknown") == StreamEventType::TextDelta);  // default
}

// ============================================================================
// ChatStreamEvent Tests
// ============================================================================

TEST_CASE("ChatStreamEvent::to_json", "[provider][stream_event]") {
    ChatStreamEvent event;
    event.type = StreamEventType::TextDelta;
    event.content = "Hello";

    auto j = event.to_json();

    REQUIRE(j["type"] == "text_delta");
    REQUIRE(j["content"] == "Hello");
}

TEST_CASE("ChatStreamEvent::from_json", "[provider][stream_event]") {
    nlohmann::json j = {
        {"type", "finish"},
        {"content", ""},
        {"finish_reason", "stop"},
        {"usage", {{"input", 100}, {"output", 50}}}
    };

    auto event = ChatStreamEvent::from_json(j);

    REQUIRE(event.type == StreamEventType::Finish);
    REQUIRE(event.finish_reason == "stop");
    REQUIRE(event.usage.input == 100);
    REQUIRE(event.usage.output == 50);
}

// ============================================================================
// ChatOptions Tests
// ============================================================================

TEST_CASE("ChatOptions::to_json", "[provider][chat_options]") {
    ChatOptions opts;
    opts.temperature = 0.7;
    opts.top_p = 0.9;
    opts.max_tokens = 2048;
    opts.stop = {"END", "STOP"};
    opts.user = "test_user";

    auto j = opts.to_json();

    REQUIRE(j["temperature"] == 0.7);
    REQUIRE(j["top_p"] == 0.9);
    REQUIRE(j["max_tokens"] == 2048);
    REQUIRE(j["stop"].is_array());
    REQUIRE(j["stop"][0] == "END");
    REQUIRE(j["user"] == "test_user");
}

TEST_CASE("ChatOptions::to_json_with_tools", "[provider][chat_options]") {
    ChatOptions opts;
    opts.tools.push_back(ToolDefinition{.name = "test", .description = "Test tool", .parameters = {}});

    auto j = opts.to_json();

    REQUIRE(j["tools"].is_array());
    REQUIRE(j["tools"][0]["function"]["name"] == "test");
}

TEST_CASE("ChatOptions::from_json", "[provider][chat_options]") {
    nlohmann::json j = {
        {"temperature", 0.5},
        {"top_p", 0.95},
        {"max_tokens", 1024},
        {"stop", {"END"}},
        {"tools", {
            {{"type", "function"}, {"function", {{"name", "fn"}, {"description", ""}, {"parameters", {}}}}}
        }}
    };

    auto opts = ChatOptions::from_json(j);

    REQUIRE(opts.temperature == 0.5);
    REQUIRE(opts.top_p == 0.95);
    REQUIRE(opts.max_tokens == 1024);
    REQUIRE(opts.stop.size() == 1);
    REQUIRE(opts.tools.size() == 1);
    REQUIRE(opts.tools[0].name == "fn");
}

// ============================================================================
// ChatResponse Tests
// ============================================================================

TEST_CASE("ChatResponse::to_json", "[provider][chat_response]") {
    ChatResponse resp;
    resp.id = "resp_123";
    resp.model = "gpt-4";
    resp.finish_reason = "stop";
    resp.choices.push_back(ChatMessage::assistant("Hello!"));
    resp.usage.input = 10;
    resp.usage.output = 5;

    auto j = resp.to_json();

    REQUIRE(j["id"] == "resp_123");
    REQUIRE(j["model"] == "gpt-4");
    REQUIRE(j["finish_reason"] == "stop");
    REQUIRE(j["choices"].is_array());
    REQUIRE(j["usage"]["input"] == 10);
}

TEST_CASE("ChatResponse::from_json", "[provider][chat_response]") {
    nlohmann::json j = {
        {"id", "resp_456"},
        {"model", "gpt-3.5-turbo"},
        {"finish_reason", "stop"},
        {"choices", {
            {{"role", "assistant"}, {"content", "Hi!"}}
        }},
        {"usage", {{"input", 20}, {"output", 10}}}
    };

    auto resp = ChatResponse::from_json(j);

    REQUIRE(resp.id == "resp_456");
    REQUIRE(resp.model == "gpt-3.5-turbo");
    REQUIRE(resp.finish_reason == "stop");
    REQUIRE(resp.choices.size() == 1);
    REQUIRE(resp.choices[0].content == "Hi!");
    REQUIRE(resp.usage.input == 20);
}

TEST_CASE("ChatResponse::get_text", "[provider][chat_response]") {
    ChatResponse resp;
    resp.choices.push_back(ChatMessage::assistant("Hello "));
    resp.choices.push_back(ChatMessage::assistant("World!"));

    REQUIRE(resp.get_text() == "Hello World!");
}

TEST_CASE("ChatResponse::is_error", "[provider][chat_response]") {
    ChatResponse resp1;
    REQUIRE_FALSE(resp1.is_error());

    ChatResponse resp2;
    resp2.error = {{"message", "error"}};
    REQUIRE(resp2.is_error());
}

TEST_CASE("ChatResponse::has_tool_calls", "[provider][chat_response]") {
    ChatResponse resp1;
    resp1.choices.push_back(ChatMessage::assistant("text"));
    REQUIRE_FALSE(resp1.has_tool_calls());

    ChatResponse resp2;
    resp2.choices.push_back(ChatMessage::assistant_with_tools("", {
        ToolCall{.id = "call_1", .name = "test", .arguments = {}}
    }));
    REQUIRE(resp2.has_tool_calls());
}

// ============================================================================
// ProviderConfig Tests
// ============================================================================

TEST_CASE("ProviderConfig::to_json", "[provider][config]") {
    ProviderConfig config;
    config.api_key = "sk-test";
    config.base_url = "https://api.test.com";
    config.organization = "org-123";
    config.timeout_seconds = 30;
    config.max_retries = 5;
    config.verify_ssl = false;
    config.proxy = "http://proxy:8080";

    auto j = config.to_json();

    // api_key is masked in to_json() output to prevent credential leakage
    REQUIRE(j["api_key"] == "***");
    REQUIRE(j["base_url"] == "https://api.test.com");
    REQUIRE(j["organization"] == "org-123");
    REQUIRE(j["timeout_seconds"] == 30);
    REQUIRE(j["max_retries"] == 5);
    REQUIRE(j["verify_ssl"] == false);
    REQUIRE(j["proxy"] == "http://proxy:8080");
}

TEST_CASE("ProviderConfig::from_json", "[provider][config]") {
    nlohmann::json j = {
        {"api_key", "sk-xxx"},
        {"base_url", "https://api.example.com"},
        {"timeout_seconds", 60},
        {"max_retries", 3},
        {"verify_ssl", true}
    };

    auto config = ProviderConfig::from_json(j);

    REQUIRE(config.api_key == "sk-xxx");
    REQUIRE(config.base_url == "https://api.example.com");
    REQUIRE(config.timeout_seconds == 60);
    REQUIRE(config.max_retries == 3);
    REQUIRE(config.verify_ssl == true);
}

// ============================================================================
// ProviderManager Tests
// ============================================================================

TEST_CASE("ProviderManager::singleton", "[provider][manager]") {
    auto& inst1 = ProviderManager::instance();
    auto& inst2 = ProviderManager::instance();

    REQUIRE(&inst1 == &inst2);
}

TEST_CASE("ProviderManager::register_and_get_provider", "[provider][manager]") {
    ProviderManager::instance().clear();

    auto provider = std::make_shared<OpenAIProvider>("test-key");
    ProviderManager::instance().register_provider(provider);

    auto retrieved = ProviderManager::instance().get_provider("openai");
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved.value()->id() == "openai");

    ProviderManager::instance().clear();
}

TEST_CASE("ProviderManager::unregister_provider", "[provider][manager]") {
    ProviderManager::instance().clear();

    auto provider = std::make_shared<OpenAIProvider>("test-key");
    ProviderManager::instance().register_provider(provider);

    REQUIRE(ProviderManager::instance().has_provider("openai"));

    bool result = ProviderManager::instance().unregister_provider("openai");
    REQUIRE(result == true);
    REQUIRE_FALSE(ProviderManager::instance().has_provider("openai"));

    // Unregister non-existent provider
    result = ProviderManager::instance().unregister_provider("nonexistent");
    REQUIRE(result == false);

    ProviderManager::instance().clear();
}

TEST_CASE("ProviderManager::list_providers", "[provider][manager]") {
    ProviderManager::instance().clear();

    auto p1 = std::make_shared<OpenAIProvider>("key1");
    auto p2 = std::make_shared<BailianProvider>("key2");

    ProviderManager::instance().register_provider(p1);
    ProviderManager::instance().register_provider(p2);

    auto providers = ProviderManager::instance().list_providers();
    REQUIRE(providers.size() == 2);

    ProviderManager::instance().clear();
}

TEST_CASE("ProviderManager::list_all_models", "[provider][manager]") {
    ProviderManager::instance().clear();

    auto provider = std::make_shared<OpenAIProvider>("test-key");
    ProviderManager::instance().register_provider(provider);

    auto models = ProviderManager::instance().list_all_models();
    REQUIRE_FALSE(models.empty());

    ProviderManager::instance().clear();
}

TEST_CASE("ProviderManager::find_model", "[provider][manager]") {
    ProviderManager::instance().clear();

    auto provider = std::make_shared<OpenAIProvider>("test-key");
    ProviderManager::instance().register_provider(provider);

    auto model = ProviderManager::instance().find_model("gpt-4");
    REQUIRE(model.has_value());
    REQUIRE(model->id == "gpt-4");

    model = ProviderManager::instance().find_model("nonexistent");
    REQUIRE_FALSE(model.has_value());

    ProviderManager::instance().clear();
}

TEST_CASE("ProviderManager::find_provider_for_model", "[provider][manager]") {
    ProviderManager::instance().clear();

    auto provider = std::make_shared<OpenAIProvider>("test-key");
    ProviderManager::instance().register_provider(provider);

    auto found = ProviderManager::instance().find_provider_for_model("gpt-4o");
    REQUIRE(found.has_value());
    REQUIRE(found.value()->id() == "openai");

    found = ProviderManager::instance().find_provider_for_model("nonexistent");
    REQUIRE_FALSE(found.has_value());

    ProviderManager::instance().clear();
}

TEST_CASE("ProviderManager::default_provider", "[provider][manager]") {
    ProviderManager::instance().clear();

    auto p1 = std::make_shared<OpenAIProvider>("key1");
    auto p2 = std::make_shared<BailianProvider>("key2");

    ProviderManager::instance().register_provider(p1);
    ProviderManager::instance().register_provider(p2);

    ProviderManager::instance().set_default_provider("bailian");
    auto default_provider = ProviderManager::instance().get_default_provider();
    REQUIRE(default_provider.has_value());
    REQUIRE(default_provider.value()->id() == "bailian");

    ProviderManager::instance().clear();
}

TEST_CASE("ProviderManager::factory", "[provider][manager]") {
    ProviderManager::instance().clear();

    ProviderManager::instance().register_factory("openai", [](const ProviderConfig& config) {
        return std::make_shared<OpenAIProvider>(config);
    });

    ProviderConfig config{.api_key = "factory-key"};
    auto provider = ProviderManager::instance().create_provider("openai", config);

    REQUIRE(provider.has_value());
    REQUIRE(provider.value()->id() == "openai");

    auto nonexistent = ProviderManager::instance().create_provider("nonexistent", config);
    REQUIRE_FALSE(nonexistent.has_value());

    ProviderManager::instance().clear();
}

// ============================================================================
// OpenAIProvider Tests
// ============================================================================

TEST_CASE("OpenAIProvider::constructor", "[provider][openai]") {
    OpenAIProvider provider("test-key");

    REQUIRE(provider.id() == "openai");
    REQUIRE(provider.name() == "OpenAI");
    REQUIRE(provider.is_ready());
}

TEST_CASE("OpenAIProvider::constructor_with_config", "[provider][openai]") {
    ProviderConfig config;
    config.api_key = "test-key";
    config.base_url = "https://custom.api.com";
    config.timeout_seconds = 120;

    OpenAIProvider provider(config);

    REQUIRE(provider.is_ready());
}

TEST_CASE("OpenAIProvider::list_models", "[provider][openai]") {
    OpenAIProvider provider("test-key");

    auto models = provider.list_models();

    REQUIRE_FALSE(models.empty());
    // Check that known models are present
    bool has_gpt4 = false;
    for (const auto& m : models) {
        if (m.id == "gpt-4o") has_gpt4 = true;
    }
    REQUIRE(has_gpt4);
}

TEST_CASE("OpenAIProvider::get_model", "[provider][openai]") {
    OpenAIProvider provider("test-key");

    auto model = provider.get_model("gpt-4o");
    REQUIRE(model.has_value());
    REQUIRE(model->id == "gpt-4o");
    REQUIRE(model->provider_id == "openai");

    model = provider.get_model("nonexistent");
    REQUIRE_FALSE(model.has_value());
}

TEST_CASE("OpenAIProvider::supports_model", "[provider][openai]") {
    OpenAIProvider provider("test-key");

    REQUIRE(provider.supports_model("gpt-4o"));
    REQUIRE(provider.supports_model("gpt-3.5-turbo"));
    REQUIRE(provider.supports_model("o1-preview"));
    REQUIRE_FALSE(provider.supports_model("nonexistent"));
}

TEST_CASE("OpenAIProvider::count_tokens", "[provider][openai]") {
    OpenAIProvider provider("test-key");

    std::vector<ChatMessage> messages = {
        ChatMessage::user("Hello, how are you?")
    };

    auto tokens = provider.count_tokens(messages, "gpt-4");
    REQUIRE(tokens > 0);
}

TEST_CASE("OpenAIProvider::is_ready", "[provider][openai]") {
    OpenAIProvider provider1("test-key");
    REQUIRE(provider1.is_ready());

    OpenAIProvider provider2(ProviderConfig{});
    REQUIRE_FALSE(provider2.is_ready());
}

// ============================================================================
// BailianProvider Tests
// ============================================================================

TEST_CASE("BailianProvider::constructor", "[provider][bailian]") {
    BailianProvider provider("test-key");

    REQUIRE(provider.id() == "bailian");
    REQUIRE(provider.name() == "阿里云百炼");
    REQUIRE(provider.is_ready());
}

TEST_CASE("BailianProvider::list_models", "[provider][bailian]") {
    BailianProvider provider("test-key");

    auto models = provider.list_models();

    REQUIRE_FALSE(models.empty());
    bool has_qwen = false;
    for (const auto& m : models) {
        if (m.id == "qwen-max") has_qwen = true;
    }
    REQUIRE(has_qwen);
}

TEST_CASE("BailianProvider::supports_model", "[provider][bailian]") {
    BailianProvider provider("test-key");

    REQUIRE(provider.supports_model("qwen-max"));
    REQUIRE(provider.supports_model("qwen-plus"));
    REQUIRE(provider.supports_model("qwen-turbo"));
    REQUIRE_FALSE(provider.supports_model("gpt-4"));
}

// ============================================================================
// ZhipuProvider Tests
// ============================================================================

TEST_CASE("ZhipuProvider::constructor", "[provider][zhipu]") {
    ZhipuProvider provider("test-key");

    REQUIRE(provider.id() == "zhipu");
    REQUIRE(provider.name() == "智谱AI");
    REQUIRE(provider.is_ready());
}

TEST_CASE("ZhipuProvider::list_models", "[provider][zhipu]") {
    ZhipuProvider provider("test-key");

    auto models = provider.list_models();

    REQUIRE_FALSE(models.empty());
    bool has_glm = false;
    for (const auto& m : models) {
        if (m.id == "glm-4-plus") has_glm = true;
    }
    REQUIRE(has_glm);
}

TEST_CASE("ZhipuProvider::supports_model", "[provider][zhipu]") {
    ZhipuProvider provider("test-key");

    REQUIRE(provider.supports_model("glm-4-plus"));
    REQUIRE(provider.supports_model("glm-4-flash"));
    REQUIRE_FALSE(provider.supports_model("gpt-4"));
}

// ============================================================================
// KimiProvider Tests
// ============================================================================

TEST_CASE("KimiProvider::constructor", "[provider][kimi]") {
    KimiProvider provider("test-key");

    REQUIRE(provider.id() == "kimi");
    REQUIRE(provider.name() == "Moonshot AI (Kimi)");
    REQUIRE(provider.is_ready());
}

TEST_CASE("KimiProvider::list_models", "[provider][kimi]") {
    KimiProvider provider("test-key");

    auto models = provider.list_models();

    REQUIRE_FALSE(models.empty());
    bool has_moonshot = false;
    for (const auto& m : models) {
        if (m.id == "moonshot-v1-8k") has_moonshot = true;
    }
    REQUIRE(has_moonshot);
}

TEST_CASE("KimiProvider::supports_model", "[provider][kimi]") {
    KimiProvider provider("test-key");

    REQUIRE(provider.supports_model("moonshot-v1-8k"));
    REQUIRE(provider.supports_model("moonshot-v1-128k"));
    REQUIRE_FALSE(provider.supports_model("gpt-4"));
}

// ============================================================================
// IflowProvider Tests
// ============================================================================

TEST_CASE("IflowProvider::constructor", "[provider][iflow]") {
    IflowProvider provider("test-key");

    REQUIRE(provider.id() == "iflow");
    REQUIRE(provider.name() == "iFlow AI");
    REQUIRE(provider.is_ready());
}

TEST_CASE("IflowProvider::list_models", "[provider][iflow]") {
    IflowProvider provider("test-key");

    auto models = provider.list_models();

    REQUIRE_FALSE(models.empty());
}

TEST_CASE("IflowProvider::supports_model", "[provider][iflow]") {
    IflowProvider provider("test-key");

    REQUIRE(provider.supports_model("iflow-4"));
    REQUIRE_FALSE(provider.supports_model("gpt-4"));
}

// ============================================================================
// Provider Integration Tests
// ============================================================================

TEST_CASE("Multiple providers in manager", "[provider][integration]") {
    ProviderManager::instance().clear();

    auto openai = std::make_shared<OpenAIProvider>("openai-key");
    auto bailian = std::make_shared<BailianProvider>("bailian-key");
    auto zhipu = std::make_shared<ZhipuProvider>("zhipu-key");
    auto kimi = std::make_shared<KimiProvider>("kimi-key");
    auto iflow = std::make_shared<IflowProvider>("iflow-key");

    ProviderManager::instance().register_provider(openai);
    ProviderManager::instance().register_provider(bailian);
    ProviderManager::instance().register_provider(zhipu);
    ProviderManager::instance().register_provider(kimi);
    ProviderManager::instance().register_provider(iflow);

    auto providers = ProviderManager::instance().list_providers();
    REQUIRE(providers.size() == 5);

    auto all_models = ProviderManager::instance().list_all_models();
    REQUIRE_FALSE(all_models.empty());

    ProviderManager::instance().clear();
}

TEST_CASE("Find model across providers", "[provider][integration]") {
    ProviderManager::instance().clear();

    auto openai = std::make_shared<OpenAIProvider>("key");
    auto bailian = std::make_shared<BailianProvider>("key");

    ProviderManager::instance().register_provider(openai);
    ProviderManager::instance().register_provider(bailian);

    auto gpt4 = ProviderManager::instance().find_model("gpt-4o");
    REQUIRE(gpt4.has_value());
    REQUIRE(gpt4->provider_id == "openai");

    auto qwen = ProviderManager::instance().find_model("qwen-max");
    REQUIRE(qwen.has_value());
    REQUIRE(qwen->provider_id == "bailian");

    ProviderManager::instance().clear();
}

// ============================================================================
// ProviderAdapter Tests
// ============================================================================

TEST_CASE("ProviderAdapter::detect_format", "[provider][adapter]") {
    SECTION("anthropic providers return Anthropic format") {
        REQUIRE(ProviderAdapter::detect_format("anthropic") == core::MessageFormat::Anthropic);
        REQUIRE(ProviderAdapter::detect_format("claude") == core::MessageFormat::Anthropic);
        REQUIRE(ProviderAdapter::detect_format("bedrock") == core::MessageFormat::Anthropic);
        REQUIRE(ProviderAdapter::detect_format("amazon-bedrock") == core::MessageFormat::Anthropic);
    }

    SECTION("openai providers return OpenAI format") {
        REQUIRE(ProviderAdapter::detect_format("openai") == core::MessageFormat::OpenAI);
        REQUIRE(ProviderAdapter::detect_format("azure") == core::MessageFormat::OpenAI);
        REQUIRE(ProviderAdapter::detect_format("ollama") == core::MessageFormat::OpenAI);
        REQUIRE(ProviderAdapter::detect_format("groq") == core::MessageFormat::OpenAI);
    }

    SECTION("gemini providers return OpenAI format (compat)") {
        REQUIRE(ProviderAdapter::detect_format("gemini") == core::MessageFormat::OpenAI);
        REQUIRE(ProviderAdapter::detect_format("google") == core::MessageFormat::OpenAI);
        REQUIRE(ProviderAdapter::detect_format("google-vertex") == core::MessageFormat::OpenAI);
    }

    SECTION("unknown provider returns OpenAICompat") {
        REQUIRE(ProviderAdapter::detect_format("unknown_provider") == core::MessageFormat::OpenAICompat);
        REQUIRE(ProviderAdapter::detect_format("my-custom-llm") == core::MessageFormat::OpenAICompat);
    }

    SECTION("case-insensitive") {
        REQUIRE(ProviderAdapter::detect_format("Anthropic") == core::MessageFormat::Anthropic);
        REQUIRE(ProviderAdapter::detect_format("OPENAI") == core::MessageFormat::OpenAI);
        REQUIRE(ProviderAdapter::detect_format("Gemini") == core::MessageFormat::OpenAI);
    }
}

TEST_CASE("ProviderAdapter::supports_streaming", "[provider][adapter]") {
    SECTION("all providers support streaming by default") {
        REQUIRE(ProviderAdapter::supports_streaming("openai") == true);
        REQUIRE(ProviderAdapter::supports_streaming("anthropic") == true);
        REQUIRE(ProviderAdapter::supports_streaming("gemini") == true);
        REQUIRE(ProviderAdapter::supports_streaming("unknown") == true);
    }
}

TEST_CASE("ProviderAdapter::supports_tool_calls", "[provider][adapter]") {
    SECTION("known providers support tool calls") {
        REQUIRE(ProviderAdapter::supports_tool_calls("openai") == true);
        REQUIRE(ProviderAdapter::supports_tool_calls("anthropic") == true);
        REQUIRE(ProviderAdapter::supports_tool_calls("gemini") == true);
        REQUIRE(ProviderAdapter::supports_tool_calls("claude") == true);
        REQUIRE(ProviderAdapter::supports_tool_calls("ollama") == true);
        REQUIRE(ProviderAdapter::supports_tool_calls("bailian") == true);
        REQUIRE(ProviderAdapter::supports_tool_calls("zhipu") == true);
        REQUIRE(ProviderAdapter::supports_tool_calls("kimi") == true);
    }

    SECTION("unknown provider does not support tool calls") {
        REQUIRE(ProviderAdapter::supports_tool_calls("unknown_provider") == false);
    }

    SECTION("case-insensitive") {
        REQUIRE(ProviderAdapter::supports_tool_calls("OpenAI") == true);
        REQUIRE(ProviderAdapter::supports_tool_calls("ANTHROPIC") == true);
    }
}

TEST_CASE("ProviderAdapter::supports_reasoning", "[provider][adapter]") {
    SECTION("reasoning-capable providers") {
        REQUIRE(ProviderAdapter::supports_reasoning("openai") == true);
        REQUIRE(ProviderAdapter::supports_reasoning("anthropic") == true);
        REQUIRE(ProviderAdapter::supports_reasoning("deepseek") == true);
        REQUIRE(ProviderAdapter::supports_reasoning("gemini") == true);
        REQUIRE(ProviderAdapter::supports_reasoning("xai") == true);
    }

    SECTION("non-reasoning providers") {
        REQUIRE(ProviderAdapter::supports_reasoning("ollama") == false);
        REQUIRE(ProviderAdapter::supports_reasoning("unknown") == false);
    }

    SECTION("case-insensitive") {
        REQUIRE(ProviderAdapter::supports_reasoning("Anthropic") == true);
        REQUIRE(ProviderAdapter::supports_reasoning("DEEPSEEK") == true);
    }
}

TEST_CASE("ProviderAdapter::to_provider_message", "[provider][adapter]") {
    SECTION("basic user message") {
        LLMMessage msg = LLMMessage::user("Hello");
        auto pmsg = ProviderAdapter::to_provider_message(msg);
        REQUIRE(pmsg.role == ChatRole::User);
        REQUIRE(pmsg.content == "Hello");
    }

    SECTION("assistant message with tool calls") {
        core::ToolCallChunk tc;
        tc.id = "call_1";
        tc.name = "my_tool";
        tc.arguments = R"({"key":"val"})";
        tc.is_complete = true;

        LLMMessage msg = LLMMessage::assistant_with_tools("", {tc});
        auto pmsg = ProviderAdapter::to_provider_message(msg);

        REQUIRE(pmsg.tool_calls.has_value());
        REQUIRE(pmsg.tool_calls->size() == 1);
        REQUIRE((*pmsg.tool_calls)[0].id == "call_1");
        REQUIRE((*pmsg.tool_calls)[0].name == "my_tool");
    }

    SECTION("tool result message") {
        LLMMessage msg = LLMMessage::tool_result("call_abc", "result content");
        auto pmsg = ProviderAdapter::to_provider_message(msg);
        REQUIRE(pmsg.role == ChatRole::Tool);
        REQUIRE(pmsg.tool_call_id.has_value());
        REQUIRE(*pmsg.tool_call_id == "call_abc");
        REQUIRE(pmsg.content == "result content");
    }
}

TEST_CASE("ProviderAdapter::from_provider_message", "[provider][adapter]") {
    SECTION("basic assistant message") {
        ChatMessage pmsg;
        pmsg.role = ChatRole::Assistant;
        pmsg.content = "I can help you.";

        auto msg = ProviderAdapter::from_provider_message(pmsg);
        REQUIRE(msg.role == ChatRole::Assistant);
        REQUIRE(msg.content == "I can help you.");
        REQUIRE(msg.tool_calls.empty());
    }

    SECTION("assistant message with tool calls") {
        ToolCall tc;
        tc.id = "tc_1";
        tc.name = "search";
        tc.type = "function";
        tc.arguments = nlohmann::json{{"query", "test"}};

        ChatMessage pmsg;
        pmsg.role = ChatRole::Assistant;
        pmsg.content = "";
        pmsg.tool_calls = std::vector<ToolCall>{tc};

        auto msg = ProviderAdapter::from_provider_message(pmsg);
        REQUIRE(msg.tool_calls.size() == 1);
        REQUIRE(msg.tool_calls[0].id == "tc_1");
        REQUIRE(msg.tool_calls[0].name == "search");
    }
}

TEST_CASE("ProviderAdapter::to_provider_messages", "[provider][adapter]") {
    std::vector<LLMMessage> msgs = {
        LLMMessage::system("You are helpful."),
        LLMMessage::user("What is 2+2?"),
        LLMMessage::assistant("4")
    };

    auto pmsgs = ProviderAdapter::to_provider_messages(msgs);
    REQUIRE(pmsgs.size() == 3);
    REQUIRE(pmsgs[0].role == ChatRole::System);
    REQUIRE(pmsgs[1].role == ChatRole::User);
    REQUIRE(pmsgs[2].role == ChatRole::Assistant);
}

TEST_CASE("ProviderAdapter::to_provider_tool", "[provider][adapter]") {
    LLMToolDefinition tool;
    tool.name = "calculator";
    tool.description = "Perform arithmetic";
    tool.parameters = nlohmann::json{{"type", "object"}};

    auto ptool = ProviderAdapter::to_provider_tool(tool);
    REQUIRE(ptool.type == "function");
    REQUIRE(ptool.name == "calculator");
    REQUIRE(ptool.description == "Perform arithmetic");
    REQUIRE(ptool.parameters == tool.parameters);
}

TEST_CASE("ProviderAdapter::from_provider_tool", "[provider][adapter]") {
    ToolDefinition ptool;
    ptool.type = "function";
    ptool.name = "search";
    ptool.description = "Search the web";
    ptool.parameters = nlohmann::json{{"type", "object"}};

    auto tool = ProviderAdapter::from_provider_tool(ptool);
    REQUIRE(tool.name == "search");
    REQUIRE(tool.description == "Search the web");
}

TEST_CASE("ProviderAdapter::to_chat_options", "[provider][adapter]") {
    StreamParams params;
    params.temperature = 0.7;
    params.top_p = 0.9;
    params.max_tokens = 2048;
    params.stop = {"<|end|>", "###"};

    LLMToolDefinition tool;
    tool.name = "get_time";
    tool.description = "Get current time";
    params.tools.push_back(tool);

    auto opts = ProviderAdapter::to_chat_options(params);
    REQUIRE(opts.temperature == 0.7);
    REQUIRE(opts.top_p == 0.9);
    REQUIRE(opts.max_tokens == 2048);
    REQUIRE(opts.stop == params.stop);
    REQUIRE(opts.stream == true);
    REQUIRE(opts.tools.size() == 1);
    REQUIRE(opts.tools[0].name == "get_time");
}

// StreamEvent is in turbot::core; ChatStreamEvent is in turbot::core::provider
// Use core:: prefix to disambiguate StreamEventType
TEST_CASE("ProviderAdapter::to_stream_event - TextDelta", "[provider][adapter]") {
    ChatStreamEvent ev;
    ev.type = StreamEventType::TextDelta;  // provider::StreamEventType
    ev.content = "Hello world";

    auto se = ProviderAdapter::to_stream_event(ev);
    REQUIRE(se.type == core::StreamEventType::TextDelta);
    REQUIRE(se.delta == "Hello world");
}

TEST_CASE("ProviderAdapter::to_stream_event - ToolCall", "[provider][adapter]") {
    ToolCall tc;
    tc.id = "call_xyz";
    tc.name = "my_func";
    tc.type = "function";
    tc.arguments = nlohmann::json{{"x", 1}};

    ChatStreamEvent ev;
    ev.type = StreamEventType::ToolCall;
    ev.tool_call = tc;

    auto se = ProviderAdapter::to_stream_event(ev);
    REQUIRE(se.type == core::StreamEventType::ToolCall);
    REQUIRE(se.tool_call.has_value());
    REQUIRE(se.tool_call->id == "call_xyz");
    REQUIRE(se.tool_call->name == "my_func");
}

TEST_CASE("ProviderAdapter::to_stream_event - ToolCall without tool_call field", "[provider][adapter]") {
    ChatStreamEvent ev;
    ev.type = StreamEventType::ToolCall;
    // No tool_call set -> should produce error event

    auto se = ProviderAdapter::to_stream_event(ev);
    REQUIRE(se.type == core::StreamEventType::Error);
}

TEST_CASE("ProviderAdapter::to_stream_event - Reasoning", "[provider][adapter]") {
    ChatStreamEvent ev;
    ev.type = StreamEventType::Reasoning;
    ev.content = "Let me think...";

    auto se = ProviderAdapter::to_stream_event(ev);
    REQUIRE(se.type == core::StreamEventType::ReasoningDelta);
    REQUIRE(se.delta == "Let me think...");
}

TEST_CASE("ProviderAdapter::to_stream_event - Finish", "[provider][adapter]") {
    ChatStreamEvent ev;
    ev.type = StreamEventType::Finish;
    ev.finish_reason = "stop";

    auto se = ProviderAdapter::to_stream_event(ev);
    REQUIRE(se.type == core::StreamEventType::Finish);
    REQUIRE(se.finish_reason.has_value());
    REQUIRE(se.finish_reason.value() == core::FinishReason::Stop);
}

TEST_CASE("ProviderAdapter::to_stream_event - Error", "[provider][adapter]") {
    ChatStreamEvent ev;
    ev.type = StreamEventType::Error;
    ev.error = nlohmann::json{{"message", "rate limit"}, {"code", "429"}};

    auto se = ProviderAdapter::to_stream_event(ev);
    REQUIRE(se.type == core::StreamEventType::Error);
    REQUIRE(se.error_message.has_value());
    REQUIRE(se.error_message.value() == "rate limit");
    REQUIRE(se.error_code.has_value());
    REQUIRE(*se.error_code == "429");
}

TEST_CASE("ProviderAdapter::to_tool_call_chunk", "[provider][adapter]") {
    ToolCall tc;
    tc.id = "call_1";
    tc.name = "search";
    tc.type = "function";
    tc.arguments = nlohmann::json{{"query", "hello"}};

    auto chunk = ProviderAdapter::to_tool_call_chunk(tc);
    REQUIRE(chunk.id == "call_1");
    REQUIRE(chunk.name == "search");
    REQUIRE(chunk.is_complete == true);
    // arguments should be serialized JSON string
    auto parsed = nlohmann::json::parse(chunk.arguments);
    REQUIRE(parsed["query"] == "hello");
}

TEST_CASE("ProviderAdapter::from_tool_call_chunk", "[provider][adapter]") {
    core::ToolCallChunk chunk;
    chunk.id = "call_2";
    chunk.name = "calculator";
    chunk.arguments = R"({"op":"add","a":1,"b":2})";
    chunk.is_complete = true;

    auto tc = ProviderAdapter::from_tool_call_chunk(chunk);
    REQUIRE(tc.id == "call_2");
    REQUIRE(tc.name == "calculator");
    REQUIRE(tc.type == "function");
    REQUIRE(tc.arguments["op"] == "add");
    REQUIRE(tc.arguments["a"] == 1);
}

TEST_CASE("ProviderAdapter::from_tool_call_chunk - invalid json falls back to string", "[provider][adapter]") {
    core::ToolCallChunk chunk;
    chunk.id = "call_3";
    chunk.name = "tool";
    chunk.arguments = "not valid json {{{";

    auto tc = ProviderAdapter::from_tool_call_chunk(chunk);
    // Should not throw; arguments stored as string
    REQUIRE(tc.id == "call_3");
    REQUIRE(tc.arguments.is_string());
}
