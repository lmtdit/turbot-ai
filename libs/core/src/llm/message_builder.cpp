#include "turbot/core/llm/message_builder.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace turbot::core {

// Constants for provider-specific requirements
namespace {
    constexpr size_t kMistralToolCallIdLength = 9;  // Mistral requires exactly 9 alphanumeric chars
}

// ===== LlmRole conversion =====

std::string_view llm_role_to_string(LlmRole role) noexcept {
    static const std::unordered_map<LlmRole, std::string_view> role_to_str = {
        {LlmRole::System, "system"},
        {LlmRole::User, "user"},
        {LlmRole::Assistant, "assistant"},
        {LlmRole::Tool, "tool"}
    };
    auto it = role_to_str.find(role);
    return it != role_to_str.end() ? it->second : "user";
}

LlmRole llm_role_from_string(std::string_view str) {
    static const std::unordered_map<std::string_view, LlmRole> str_to_role = {
        {"system", LlmRole::System},
        {"user", LlmRole::User},
        {"assistant", LlmRole::Assistant},
        {"tool", LlmRole::Tool}
    };
    auto it = str_to_role.find(str);
    if (it != str_to_role.end()) {
        return it->second;
    }
    throw std::invalid_argument("Invalid LlmRole: " + std::string(str));
}

// ===== LlmMessage factory methods =====

LlmMessage LlmMessage::create_system(const std::string& content) {
    LlmMessage msg;
    msg.role = LlmRole::System;
    msg.content = content;
    return msg;
}

LlmMessage LlmMessage::create_user(const std::string& content) {
    LlmMessage msg;
    msg.role = LlmRole::User;
    msg.content = content;
    return msg;
}

LlmMessage LlmMessage::create_assistant(const std::string& content) {
    LlmMessage msg;
    msg.role = LlmRole::Assistant;
    msg.content = content;
    return msg;
}

LlmMessage LlmMessage::create_user_parts(std::vector<nlohmann::json> parts) {
    LlmMessage msg;
    msg.role = LlmRole::User;
    msg.content_parts = std::move(parts);
    return msg;
}

LlmMessage LlmMessage::create_assistant_parts(std::vector<nlohmann::json> parts) {
    LlmMessage msg;
    msg.role = LlmRole::Assistant;
    msg.content_parts = std::move(parts);
    return msg;
}

LlmMessage LlmMessage::create_tool_response(const std::string& tool_call_id, const std::string& content) {
    LlmMessage msg;
    msg.role = LlmRole::Tool;
    msg.tool_call_id = tool_call_id;
    msg.content = content;
    return msg;
}

LlmMessage LlmMessage::create_assistant_with_tools(
    const std::optional<std::string>& content,
    const std::vector<nlohmann::json>& tool_calls
) {
    LlmMessage msg;
    msg.role = LlmRole::Assistant;
    msg.content = content;
    msg.tool_calls = tool_calls;
    return msg;
}

// ===== LlmMessage serialization =====

nlohmann::json LlmMessage::to_openai() const {
    nlohmann::json j;
    j["role"] = std::string(llm_role_to_string(role));

    // Handle content
    if (content_parts.has_value() && !content_parts->empty()) {
        j["content"] = *content_parts;
    } else if (content.has_value() && !content->empty()) {
        j["content"] = *content;
    } else if (role != LlmRole::Tool) {
        j["content"] = nullptr;
    }

    // Handle tool call ID (for tool messages)
    if (tool_call_id.has_value()) {
        j["tool_call_id"] = *tool_call_id;
    }

    // Handle tool calls (for assistant messages)
    if (tool_calls.has_value() && !tool_calls->empty()) {
        j["tool_calls"] = *tool_calls;
    }

    // Handle provider options
    if (provider_options.has_value()) {
        if (provider_options->contains("openai")) {
            j["provider_options"] = (*provider_options)["openai"];
        } else {
            j["provider_options"] = *provider_options;
        }
    }

    return j;
}

nlohmann::json LlmMessage::to_anthropic() const {
    nlohmann::json j;
    j["role"] = std::string(llm_role_to_string(role));

    // Anthropic handles system messages separately
    if (role == LlmRole::System) {
        j["type"] = "system";
        j["content"] = content.value_or("");
        return j;
    }

    // Build content array
    nlohmann::json content_array = nlohmann::json::array();

    // Add text content first
    if (content_parts.has_value() && !content_parts->empty()) {
        // Convert content parts to Anthropic format
        for (const auto& part : *content_parts) {
            if (part.contains("type")) {
                std::string part_type = part["type"];
                if (part_type == "text") {
                    content_array.push_back({
                        {"type", "text"},
                        {"text", part.value("text", "")}
                    });
                } else if (part_type == "image_url" && part.contains("image_url")) {
                    // Convert OpenAI image format to Anthropic
                    std::string url = part["image_url"].value("url", "");
                    // Safe boundary check before substr
                    if (url.size() >= 5 && url.substr(0, 5) == "data:") {
                        // Parse data URL
                        size_t comma = url.find(',');
                        if (comma != std::string::npos && comma > 5) {
                            std::string media_type = url.substr(5, comma - 5);
                            size_t semicolon = media_type.find(';');
                            if (semicolon != std::string::npos) {
                                media_type = media_type.substr(0, semicolon);
                            }
                            std::string base64_data = url.substr(comma + 1);
                            content_array.push_back({
                                {"type", "image"},
                                {"source", {
                                    {"type", "base64"},
                                    {"media_type", media_type},
                                    {"data", base64_data}
                                }}
                            });
                        }
                    }
                } else if (part_type == "tool_call") {
                    // OpenAI-style tool_call to Anthropic tool_use
                    std::string id = part.value("id", "");
                    std::string name;
                    nlohmann::json input = nlohmann::json::object();

                    if (part.contains("function")) {
                        const auto& func = part["function"];
                        name = func.value("name", "");
                        std::string args_str = func.value("arguments", "{}");
                        try {
                            input = nlohmann::json::parse(args_str);
                        } catch (const nlohmann::json::parse_error&) {
                            // Invalid JSON arguments - use empty object as fallback
                            input = nlohmann::json::object();
                        }
                    }

                    content_array.push_back({
                        {"type", "tool_use"},
                        {"id", id},
                        {"name", name},
                        {"input", input}
                    });
                } else if (part_type == "reasoning") {
                    // Reasoning parts - skip for Anthropic (they handle thinking differently)
                    // Just include as text for now
                    content_array.push_back({
                        {"type", "text"},
                        {"text", part.value("text", "")}
                    });
                } else {
                    content_array.push_back(part);
                }
            }
        }
    } else if (content.has_value() && !content->empty()) {
        content_array.push_back({{"type", "text"}, {"text", *content}});
    }

    // Handle tool_calls field (from create_assistant_with_tools)
    if (tool_calls.has_value() && !tool_calls->empty()) {
        for (const auto& tc : *tool_calls) {
            std::string id = tc.value("id", "");
            std::string name;
            nlohmann::json input = nlohmann::json::object();

            if (tc.contains("function")) {
                const auto& func = tc["function"];
                name = func.value("name", "");
                std::string args_str = func.value("arguments", "{}");
                try {
                    input = nlohmann::json::parse(args_str);
                } catch (const nlohmann::json::parse_error&) {
                    // Invalid JSON arguments - use empty object as fallback
                    input = nlohmann::json::object();
                }
            }

            content_array.push_back({
                {"type", "tool_use"},
                {"id", id},
                {"name", name},
                {"input", input}
            });
        }
    }

    // Set content
    if (role == LlmRole::Tool) {
        // Tool result - Anthropic expects content as string
        j["content"] = content.value_or("");
    } else {
        j["content"] = content_array;
    }

    // Handle tool call ID (for tool result messages)
    if (role == LlmRole::Tool && tool_call_id.has_value()) {
        j["tool_call_id"] = *tool_call_id;
    }

    return j;
}

nlohmann::json LlmMessage::to_format(MessageFormat format) const {
    switch (format) {
        case MessageFormat::OpenAI:
        case MessageFormat::OpenAICompat:
            return to_openai();
        case MessageFormat::Anthropic:
            return to_anthropic();
    }
    return to_openai(); // fallback
}

// ===== Content parts factory =====

namespace content_parts {

nlohmann::json text(const std::string& text) {
    return {
        {"type", "text"},
        {"text", text}
    };
}

nlohmann::json image_base64(const std::string& base64_data, const std::string& mime_type) {
    return {
        {"type", "image_url"},
        {"image_url", {
            {"url", "data:" + mime_type + ";base64," + base64_data}
        }}
    };
}

nlohmann::json image_url(const std::string& url) {
    return {
        {"type", "image_url"},
        {"image_url", {{"url", url}}}
    };
}

nlohmann::json file(const std::string& filename, const std::string& base64_data, const std::string& mime_type) {
    return {
        {"type", "file"},
        {"filename", filename},
        {"data", base64_data},
        {"mime_type", mime_type}
    };
}

nlohmann::json tool_call(const std::string& tool_call_id, const std::string& name, const nlohmann::json& arguments) {
    return {
        {"type", "tool_call"},
        {"id", tool_call_id},
        {"function", {
            {"name", name},
            {"arguments", arguments.dump()}
        }}
    };
}

nlohmann::json tool_result(const std::string& tool_call_id, const std::string& content) {
    return {
        {"type", "tool_result"},
        {"tool_call_id", tool_call_id},
        {"content", content}
    };
}

nlohmann::json reasoning(const std::string& text) {
    return {
        {"type", "reasoning"},
        {"text", text}
    };
}

} // namespace content_parts

// ===== MessageBuilder =====

MessageBuilder::MessageBuilder(const MessageBuilderOptions& options)
    : options_(options) {}

MessageBuilder& MessageBuilder::set_format(MessageFormat format) {
    options_.format = format;
    return *this;
}

MessageBuilder& MessageBuilder::set_capabilities(const ProviderCapabilities& caps) {
    options_.capabilities = caps;
    return *this;
}

MessageBuilder& MessageBuilder::set_provider(const std::string& provider_id, const std::string& model_id) {
    options_.provider_id = provider_id;
    options_.model_id = model_id;
    return *this;
}

MessageBuilder& MessageBuilder::set_caching(bool enabled) {
    options_.apply_caching = enabled;
    return *this;
}

MessageBuilder& MessageBuilder::add_system(const std::string& content) {
    messages_.push_back(LlmMessage::create_system(content));
    return *this;
}

MessageBuilder& MessageBuilder::add_user(const std::string& content) {
    messages_.push_back(LlmMessage::create_user(content));
    return *this;
}

MessageBuilder& MessageBuilder::add_user_parts(std::vector<nlohmann::json> parts) {
    messages_.push_back(LlmMessage::create_user_parts(std::move(parts)));
    return *this;
}

MessageBuilder& MessageBuilder::add_assistant(const std::string& content) {
    messages_.push_back(LlmMessage::create_assistant(content));
    return *this;
}

MessageBuilder& MessageBuilder::add_assistant_parts(std::vector<nlohmann::json> parts) {
    messages_.push_back(LlmMessage::create_assistant_parts(std::move(parts)));
    return *this;
}

MessageBuilder& MessageBuilder::add_assistant_with_tools(
    const std::optional<std::string>& content,
    const std::vector<nlohmann::json>& tool_calls
) {
    messages_.push_back(LlmMessage::create_assistant_with_tools(content, tool_calls));
    return *this;
}

MessageBuilder& MessageBuilder::add_tool_response(const std::string& tool_call_id, const std::string& content) {
    messages_.push_back(LlmMessage::create_tool_response(tool_call_id, content));
    return *this;
}

MessageBuilder& MessageBuilder::add_message(const LlmMessage& message) {
    messages_.push_back(message);
    return *this;
}

MessageBuilder& MessageBuilder::add_messages(const std::vector<LlmMessage>& messages) {
    messages_.insert(messages_.end(), messages.begin(), messages.end());
    return *this;
}

std::vector<nlohmann::json> MessageBuilder::build() const {
    std::vector<nlohmann::json> result;
    result.reserve(messages_.size());

    // Make a copy for transformations
    std::vector<LlmMessage> transformed = messages_;

    // Apply provider-specific transformations
    if (options_.provider_id.has_value()) {
        message_transform::normalize_messages(transformed, *options_.provider_id, options_.capabilities);
    }

    // Convert to JSON
    for (const auto& msg : transformed) {
        // Filter empty content for Anthropic
        if (options_.filter_empty_content &&
            options_.format == MessageFormat::Anthropic &&
            msg.content.has_value() && msg.content->empty()) {
            continue;
        }

        auto j = msg.to_format(options_.format);
        result.push_back(std::move(j));
    }

    // Apply caching hints for Anthropic
    if (options_.apply_caching && options_.format == MessageFormat::Anthropic) {
        apply_anthropic_caching(result);
    }

    return result;
}

std::pair<std::vector<std::string>, std::vector<nlohmann::json>> MessageBuilder::build_with_system() const {
    std::vector<std::string> system_messages;
    std::vector<nlohmann::json> other_messages;

    for (const auto& msg : messages_) {
        if (msg.role == LlmRole::System && msg.content.has_value()) {
            system_messages.push_back(*msg.content);
        } else {
            auto j = msg.to_format(options_.format);
            other_messages.push_back(std::move(j));
        }
    }

    return {system_messages, other_messages};
}

void MessageBuilder::apply_anthropic_caching(std::vector<nlohmann::json>& msgs) const {
    if (msgs.empty()) return;

    // Mark first 2 and last 2 messages for caching
    for (size_t i = 0; i < msgs.size(); ++i) {
        bool should_cache = (i < 2) || (i >= msgs.size() - 2);
        if (should_cache) {
            msgs[i]["cache_control"] = {{"type", "ephemeral"}};
        }
    }
}

// ===== Message transform utilities =====

namespace message_transform {

nlohmann::json part_to_content(const Part& part, [[maybe_unused]] MessageFormat format) {
    switch (part.type) {
        case PartType::Text:
            return content_parts::text(part.get_text());

        case PartType::Tool: {
            auto tool = part.get_tool();
            return content_parts::tool_call(
                tool.value("tool_id", ""),
                tool.value("tool_name", ""),
                tool.value("arguments", nlohmann::json::object())
            );
        }

        case PartType::Reasoning:
            return content_parts::reasoning(part.get_reasoning());

        case PartType::File: {
            auto file = part.get_file();
            return content_parts::file(
                file.value("path", ""),
                file.value("content", ""),
                file.value("mime_type", "application/octet-stream")
            );
        }

        default:
            // For other part types, return the raw data
            return {
                {"type", part_type_to_string(part.type)},
                {"data", part.data}
            };
    }
}

std::vector<nlohmann::json> parts_to_content(
    const std::vector<Part>& parts,
    MessageFormat format,
    const ProviderCapabilities& caps
) {
    std::vector<nlohmann::json> result;
    result.reserve(parts.size());

    for (const auto& part : parts) {
        auto content = part_to_content(part, format);

        // Filter unsupported content types
        if (content.contains("type")) {
            std::string type = content["type"];
            if (type == "image_url" && !caps.supports_vision) {
                content = content_parts::text("[Image not supported by this model]");
            } else if (type == "file") {
                std::string mime = content.value("mime_type", "");
                if (mime.find("audio/") == 0 && !caps.supports_audio) {
                    content = content_parts::text("[Audio not supported by this model]");
                } else if (mime.find("video/") == 0 && !caps.supports_video) {
                    content = content_parts::text("[Video not supported by this model]");
                } else if (mime == "application/pdf" && !caps.supports_pdf) {
                    content = content_parts::text("[PDF not supported by this model]");
                }
            }
        }

        result.push_back(std::move(content));
    }

    return result;
}

void normalize_messages(
    std::vector<LlmMessage>& messages,
    const std::string& provider_id,
    const ProviderCapabilities& caps
) {
    // Normalize for Anthropic
    if (provider_id == "anthropic" || provider_id.find("claude") != std::string::npos) {
        // Remove empty messages
        messages.erase(
            std::remove_if(messages.begin(), messages.end(),
                [](const LlmMessage& msg) {
                    if (msg.content.has_value() && msg.content->empty()) {
                        return true;
                    }
                    if (msg.content_parts.has_value() && msg.content_parts->empty()) {
                        return true;
                    }
                    return false;
                }),
            messages.end()
        );

        // Normalize tool call IDs (Anthropic requires alphanumeric with dashes/underscores)
        for (auto& msg : messages) {
            if (msg.tool_call_id.has_value()) {
                std::string normalized = *msg.tool_call_id;
                std::replace_if(normalized.begin(), normalized.end(),
                    [](char c) { return !std::isalnum(c) && c != '-' && c != '_'; },
                    '_');
                msg.tool_call_id = normalized;
            }
        }
    }

    // Normalize for Mistral (requires 9-char alphanumeric tool call IDs)
    if (provider_id == "mistral" || provider_id.find("mistral") != std::string::npos) {
        for (auto& msg : messages) {
            if (msg.tool_call_id.has_value()) {
                std::string normalized = *msg.tool_call_id;
                // Remove non-alphanumeric
                normalized.erase(
                    std::remove_if(normalized.begin(), normalized.end(),
                        [](char c) { return !std::isalnum(c); }),
                    normalized.end()
                );
                // Mistral requires exactly kMistralToolCallIdLength chars
                if (normalized.length() > kMistralToolCallIdLength) {
                    normalized = normalized.substr(0, kMistralToolCallIdLength);
                } else if (normalized.length() < kMistralToolCallIdLength) {
                    normalized.append(kMistralToolCallIdLength - normalized.length(), '0');
                }
                msg.tool_call_id = normalized;
            }
        }

        // Fix message sequence: tool messages cannot be followed by user messages
        std::vector<LlmMessage> fixed;
        for (size_t i = 0; i < messages.size(); ++i) {
            fixed.push_back(messages[i]);

            if (messages[i].role == LlmRole::Tool &&
                i + 1 < messages.size() &&
                messages[i + 1].role == LlmRole::User) {
                // Insert assistant message between tool and user
                fixed.push_back(LlmMessage::create_assistant("Done."));
            }
        }
        messages = std::move(fixed);
    }

    // Handle interleaved thinking for models that support it
    if (caps.supports_interleaved_thinking && caps.thinking_field.has_value()) {
        for (auto& msg : messages) {
            if (msg.role == LlmRole::Assistant && msg.content_parts.has_value()) {
                // Extract reasoning parts
                std::string reasoning_text;
                std::vector<nlohmann::json> filtered_parts;

                for (const auto& part : *msg.content_parts) {
                    if (part.contains("type") && part["type"] == "reasoning") {
                        reasoning_text += part.value("text", "");
                    } else {
                        filtered_parts.push_back(part);
                    }
                }

                if (!reasoning_text.empty()) {
                    msg.content_parts = filtered_parts;
                    // Create proper JSON object (not array)
                    nlohmann::json opts = nlohmann::json::object();
                    opts["openaiCompatible"] = nlohmann::json::object();
                    opts["openaiCompatible"][*caps.thinking_field] = reasoning_text;
                    msg.provider_options = opts;
                }
            }
        }
    }
}

void apply_caching_hints(std::vector<nlohmann::json>& messages, const std::string& provider_id) {
    if (messages.empty()) return;

    // Anthropic caching
    if (provider_id == "anthropic" || provider_id.find("claude") != std::string::npos) {
        // Cache first 2 system messages and last 2 messages
        size_t system_count = 0;
        for (size_t i = 0; i < messages.size() && system_count < 2; ++i) {
            if (messages[i].value("role", "") == "system") {
                messages[i]["cache_control"] = {{"type", "ephemeral"}};
                ++system_count;
            }
        }

        // Cache last 2 non-system messages
        size_t cache_count = 0;
        for (size_t i = messages.size(); i > 0 && cache_count < 2; --i) {
            if (messages[i - 1].value("role", "") != "system") {
                messages[i - 1]["cache_control"] = {{"type", "ephemeral"}};
                ++cache_count;
            }
        }
    }

    // OpenRouter caching
    if (provider_id == "openrouter") {
        for (auto& msg : messages) {
            msg["cache_control"] = {{"type", "ephemeral"}};
        }
    }
}

} // namespace message_transform

} // namespace turbot::core
