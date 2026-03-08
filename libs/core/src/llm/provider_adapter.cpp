#include "turbot/core/llm/provider_adapter.hpp"
#include <algorithm>
#include <set>

namespace turbot::core::llm {

// ===== Message Conversion =====

provider::ChatMessage ProviderAdapter::to_provider_message(const LLMMessage& msg) {
    provider::ChatMessage pmsg;
    pmsg.role = msg.role;
    pmsg.content = msg.content;
    pmsg.name = msg.name;
    pmsg.tool_call_id = msg.tool_call_id;

    if (!msg.tool_calls.empty()) {
        pmsg.tool_calls = std::vector<provider::ToolCall>{};
        for (const auto& tc : msg.tool_calls) {
            pmsg.tool_calls->push_back(from_tool_call_chunk(tc));
        }
    }

    return pmsg;
}

LLMMessage ProviderAdapter::from_provider_message(const provider::ChatMessage& msg) {
    LLMMessage lmsg;
    lmsg.role = msg.role;
    lmsg.content = msg.content;
    lmsg.name = msg.name;
    lmsg.tool_call_id = msg.tool_call_id;

    if (msg.tool_calls.has_value()) {
        for (const auto& tc : *msg.tool_calls) {
            lmsg.tool_calls.push_back(to_tool_call_chunk(tc));
        }
    }

    return lmsg;
}

std::vector<provider::ChatMessage> ProviderAdapter::to_provider_messages(
    const std::vector<LLMMessage>& messages
) {
    std::vector<provider::ChatMessage> result;
    result.reserve(messages.size());
    for (const auto& msg : messages) {
        result.push_back(to_provider_message(msg));
    }
    return result;
}

// ===== Tool Conversion =====

provider::ToolDefinition ProviderAdapter::to_provider_tool(const LLMToolDefinition& tool) {
    provider::ToolDefinition ptool;
    ptool.type = "function";
    ptool.name = tool.name;
    ptool.description = tool.description;
    ptool.parameters = tool.parameters;
    return ptool;
}

LLMToolDefinition ProviderAdapter::from_provider_tool(const provider::ToolDefinition& tool) {
    LLMToolDefinition ltool;
    ltool.name = tool.name;
    ltool.description = tool.description;
    ltool.parameters = tool.parameters;
    return ltool;
}

std::vector<provider::ToolDefinition> ProviderAdapter::to_provider_tools(
    const std::vector<LLMToolDefinition>& tools
) {
    std::vector<provider::ToolDefinition> result;
    result.reserve(tools.size());
    for (const auto& tool : tools) {
        result.push_back(to_provider_tool(tool));
    }
    return result;
}

// ===== Options Conversion =====

provider::ChatOptions ProviderAdapter::to_chat_options(const StreamParams& params) {
    provider::ChatOptions options;
    options.temperature = params.temperature;
    options.top_p = params.top_p.value_or(1.0);
    options.max_tokens = params.max_tokens.value_or(4096);
    options.stop = params.stop;
    options.stream = true;
    options.tools = to_provider_tools(params.tools);
    return options;
}

StreamParams ProviderAdapter::from_chat_options(const provider::ChatOptions& options) {
    StreamParams params;
    params.temperature = options.temperature;
    params.top_p = options.top_p;
    params.max_tokens = options.max_tokens;
    params.stop = options.stop;

    for (const auto& tool : options.tools) {
        params.tools.push_back(from_provider_tool(tool));
    }

    return params;
}

// ===== Event Conversion =====

StreamEvent ProviderAdapter::to_stream_event(const provider::ChatStreamEvent& event) {
    switch (event.type) {
        case provider::StreamEventType::TextDelta:
            return StreamEvent::create_text_delta("text-0", event.content);

        case provider::StreamEventType::ToolCall:
            if (event.tool_call.has_value()) {
                return StreamEvent::create_tool_call(to_tool_call_chunk(*event.tool_call));
            }
            return StreamEvent::create_error("Invalid tool call event");

        case provider::StreamEventType::Reasoning:
            return StreamEvent::create_reasoning_delta("reasoning-0", event.content);

        case provider::StreamEventType::Finish: {
            FinishReason reason = FinishReason::Stop;
            if (event.finish_reason.has_value()) {
                reason = finish_reason_from_string(*event.finish_reason);
            }
            return StreamEvent::create_finish(reason, event.usage);
        }

        case provider::StreamEventType::Error: {
            std::string message = "Unknown error";
            std::optional<std::string> code;
            if (event.error.has_value()) {
                if (event.error->contains("message")) {
                    message = (*event.error)["message"].get<std::string>();
                }
                if (event.error->contains("code")) {
                    code = (*event.error)["code"].get<std::string>();
                }
            }
            return StreamEvent::create_error(message, code);
        }

        default:
            return StreamEvent::create_error("Unknown event type");
    }
}

StreamResult ProviderAdapter::to_stream_result(const provider::ChatResponse& response) {
    StreamResult result;
    result.id = response.id;
    result.model = response.model;
    result.finish_reason = finish_reason_from_string(response.finish_reason);
    result.usage = response.usage;

    if (response.is_error()) {
        result.error = "API error";
        if (response.error.has_value()) {
            if (response.error->contains("message")) {
                result.error = (*response.error)["message"].get<std::string>();
            }
        }
    }

    // Convert messages to events
    for (const auto& choice : response.choices) {
        if (!choice.content.empty()) {
            result.events.push_back(StreamEvent::create_text_start("text-0"));
            result.events.push_back(StreamEvent::create_text_delta("text-0", choice.content));
            result.events.push_back(StreamEvent::create_text_end("text-0"));
        }

        if (choice.tool_calls.has_value()) {
            for (const auto& tc : *choice.tool_calls) {
                result.events.push_back(StreamEvent::create_tool_call(to_tool_call_chunk(tc)));
            }
        }
    }

    result.events.push_back(StreamEvent::create_finish(result.finish_reason, result.usage));

    return result;
}

// ===== Tool Call Conversion =====

ToolCallChunk ProviderAdapter::to_tool_call_chunk(const provider::ToolCall& tc) {
    ToolCallChunk chunk;
    chunk.id = tc.id;
    chunk.name = tc.name;
    chunk.arguments = tc.arguments.dump();
    chunk.is_complete = true;
    return chunk;
}

provider::ToolCall ProviderAdapter::from_tool_call_chunk(const ToolCallChunk& chunk) {
    provider::ToolCall tc;
    tc.id = chunk.id;
    tc.type = "function";
    tc.name = chunk.name;
    try {
        tc.arguments = nlohmann::json::parse(chunk.arguments);
    } catch (...) {
        tc.arguments = chunk.arguments;
    }
    return tc;
}

// ===== Format Detection =====

MessageFormat ProviderAdapter::detect_format(const std::string& provider_id) {
    // Anthropic format providers
    static const std::set<std::string> anthropic_providers = {
        "anthropic", "claude", "bedrock", "amazon-bedrock"
    };
    
    // Native OpenAI format providers
    static const std::set<std::string> openai_providers = {
        "openai", "azure", "ollama", "lmstudio",
        "groq", "openrouter", "deepinfra", "togetherai", "together",
        "github-copilot", "cerebras"
    };
    
    // Google Gemini format (treated as OpenAI compatible for now)
    static const std::set<std::string> gemini_providers = {
        "gemini", "google", "google-vertex"
    };

    std::string lower_id = provider_id;
    std::transform(lower_id.begin(), lower_id.end(), lower_id.begin(), ::tolower);

    if (anthropic_providers.count(lower_id) > 0) {
        return MessageFormat::Anthropic;
    }
    if (openai_providers.count(lower_id) > 0) {
        return MessageFormat::OpenAI;
    }
    if (gemini_providers.count(lower_id) > 0) {
        return MessageFormat::OpenAI;  // Gemini uses OpenAI-compatible format
    }

    return MessageFormat::OpenAICompat;
}

bool ProviderAdapter::supports_streaming(const std::string& provider_id) {
    // Most modern providers support streaming
    static const std::set<std::string> non_streaming = {};
    std::string lower_id = provider_id;
    std::transform(lower_id.begin(), lower_id.end(), lower_id.begin(), ::tolower);
    return non_streaming.count(lower_id) == 0;
}

bool ProviderAdapter::supports_tool_calls(const std::string& provider_id) {
    static const std::set<std::string> tool_capable = {
        // OpenAI and compatible
        "openai", "azure", "groq", "openrouter", "deepinfra", 
        "togetherai", "together", "github-copilot", "cerebras",
        // Anthropic
        "anthropic", "claude", "bedrock", "amazon-bedrock",
        // Google
        "gemini", "google", "google-vertex",
        // Others
        "mistral", "deepseek", "xai", "cohere",
        // Chinese providers
        "bailian", "zhipu", "kimi", "moonshot", "minimax",
        // Local
        "ollama", "lmstudio"
    };
    std::string lower_id = provider_id;
    std::transform(lower_id.begin(), lower_id.end(), lower_id.begin(), ::tolower);
    return tool_capable.count(lower_id) > 0;
}

bool ProviderAdapter::supports_reasoning(const std::string& provider_id) {
    static const std::set<std::string> reasoning_capable = {
        // OpenAI o1/o3 models
        "openai",
        // Anthropic Claude extended thinking
        "anthropic", "claude", "bedrock", "amazon-bedrock",
        // DeepSeek R1
        "deepseek",
        // Google Gemini thinking
        "gemini", "google", "google-vertex",
        // xAI Grok
        "xai"
    };
    std::string lower_id = provider_id;
    std::transform(lower_id.begin(), lower_id.end(), lower_id.begin(), ::tolower);
    return reasoning_capable.count(lower_id) > 0;
}

} // namespace turbot::core::llm
