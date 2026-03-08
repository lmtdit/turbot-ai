#pragma once

#include <turbot/core/common/export.hpp>
#include <turbot/core/llm/llm.hpp>
#include <turbot/core/provider/provider.hpp>
#include <string>

namespace turbot::core::llm {

/// Provider adapter - converts between LLM types and provider-specific formats
/// 
/// This class provides utilities for adapting between the unified LLM interface
/// and provider-specific API formats (OpenAI, Anthropic, etc.)
class TURBOT_CORE_API ProviderAdapter {
public:
    // ===== Message Conversion =====

    /// Convert LLMMessage to provider ChatMessage
    [[nodiscard]] static provider::ChatMessage to_provider_message(const LLMMessage& msg);

    /// Convert provider ChatMessage to LLMMessage
    [[nodiscard]] static LLMMessage from_provider_message(const provider::ChatMessage& msg);

    /// Convert a vector of LLMMessage to provider format
    [[nodiscard]] static std::vector<provider::ChatMessage> to_provider_messages(
        const std::vector<LLMMessage>& messages
    );

    // ===== Tool Conversion =====

    /// Convert LLMToolDefinition to provider ToolDefinition
    [[nodiscard]] static provider::ToolDefinition to_provider_tool(const LLMToolDefinition& tool);

    /// Convert provider ToolDefinition to LLMToolDefinition
    [[nodiscard]] static LLMToolDefinition from_provider_tool(const provider::ToolDefinition& tool);

    /// Convert a vector of LLMToolDefinition to provider format
    [[nodiscard]] static std::vector<provider::ToolDefinition> to_provider_tools(
        const std::vector<LLMToolDefinition>& tools
    );

    // ===== Options Conversion =====

    /// Convert StreamParams to provider ChatOptions
    [[nodiscard]] static provider::ChatOptions to_chat_options(const StreamParams& params);

    /// Convert provider ChatOptions to StreamParams (partial - messages/tools not included)
    [[nodiscard]] static StreamParams from_chat_options(const provider::ChatOptions& options);

    // ===== Event Conversion =====

    /// Convert provider ChatStreamEvent to StreamEvent
    [[nodiscard]] static StreamEvent to_stream_event(const provider::ChatStreamEvent& event);

    /// Convert provider ChatResponse to StreamResult
    [[nodiscard]] static StreamResult to_stream_result(const provider::ChatResponse& response);

    // ===== Tool Call Conversion =====

    /// Convert provider ToolCall to ToolCallChunk
    [[nodiscard]] static ToolCallChunk to_tool_call_chunk(const provider::ToolCall& tc);

    /// Convert ToolCallChunk to provider ToolCall
    [[nodiscard]] static provider::ToolCall from_tool_call_chunk(const ToolCallChunk& chunk);

    // ===== Format Detection =====

    /// Detect message format from provider ID
    [[nodiscard]] static MessageFormat detect_format(const std::string& provider_id);

    /// Check if provider supports streaming
    [[nodiscard]] static bool supports_streaming(const std::string& provider_id);

    /// Check if provider supports tool calls
    [[nodiscard]] static bool supports_tool_calls(const std::string& provider_id);

    /// Check if provider supports reasoning (thinking models)
    [[nodiscard]] static bool supports_reasoning(const std::string& provider_id);
};

} // namespace turbot::core::llm
