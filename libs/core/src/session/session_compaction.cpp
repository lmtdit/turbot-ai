#include <turbot/core/session/session_compaction.hpp>
#include <algorithm>
#include <sstream>
#include <set>
#include <ctime>

namespace turbot::core::session {

// ============================================================================
// CompactionConfig Implementation
// ============================================================================

bool CompactionConfig::validate() const noexcept {
    if (overflow_threshold <= 0 || overflow_threshold > 1) return false;
    if (target_ratio <= 0 || target_ratio > 1) return false;
    if (min_messages_to_keep < 1) return false;
    if (max_summary_length < 100) return false;
    return true;
}

// ============================================================================
// CompactionResult Implementation
// ============================================================================

nlohmann::json CompactionResult::to_json() const {
    return nlohmann::json{
        {"summary", summary},
        {"retained_ids", retained_ids},
        {"removed_ids", removed_ids},
        {"original_tokens", original_tokens},
        {"compressed_tokens", compressed_tokens},
        {"compression_ratio", compression_ratio}
    };
}

// ============================================================================
// SessionCompaction Implementation
// ============================================================================

bool SessionCompaction::needs_compaction(
    int64_t current_tokens,
    int64_t max_tokens,
    const CompactionConfig& config
) {
    if (max_tokens <= 0) return false;
    double usage_ratio = static_cast<double>(current_tokens) / static_cast<double>(max_tokens);
    return usage_ratio >= config.overflow_threshold;
}

CompactionResult SessionCompaction::compact(
    const std::vector<Message>& messages,
    const CompactionConfig& config
) {
    CompactionResult result;

    if (messages.empty()) {
        return result;
    }

    // Calculate original tokens
    for (const auto& msg : messages) {
        result.original_tokens += estimate_tokens(msg);
    }

    // Select messages to retain
    auto retained_indices = select_retained(messages, config);

    // Build retained and removed ID lists
    std::set<size_t> retained_set(retained_indices.begin(), retained_indices.end());
    std::vector<Message> removed_messages;

    for (size_t i = 0; i < messages.size(); ++i) {
        if (retained_set.count(i) > 0) {
            result.retained_ids.push_back(messages[i].id());
        } else {
            result.removed_ids.push_back(messages[i].id());
            removed_messages.push_back(messages[i]);
        }
    }

    // Generate summary for removed messages
    if (!removed_messages.empty()) {
        result.summary = generate_summary(removed_messages, config.max_summary_length);
    }

    // Calculate compressed tokens
    for (size_t idx : retained_indices) {
        result.compressed_tokens += estimate_tokens(messages[idx]);
    }
    // Add summary tokens
    result.compressed_tokens += estimate_text_tokens(result.summary);

    // Calculate compression ratio
    if (result.original_tokens > 0) {
        result.compression_ratio = static_cast<double>(result.compressed_tokens) /
                                   static_cast<double>(result.original_tokens);
    }

    return result;
}

std::string SessionCompaction::generate_summary(
    const std::vector<Message>& messages,
    int max_length
) const {
    if (messages.empty()) {
        return "";
    }

    // If a custom summary generator is set, use it
    if (summary_generator_) {
        return summary_generator_(messages);
    }

    // Default: create a simple summary
    std::ostringstream oss;
    oss << "[Session Compaction Summary]\n";
    oss << "Removed " << messages.size() << " messages.\n\n";

    // Summarize key points
    int user_count = 0;
    int assistant_count = 0;
    int tool_calls = 0;

    for (const auto& msg : messages) {
        if (msg.role() == Role::User) {
            user_count++;
        } else if (msg.role() == Role::Assistant) {
            assistant_count++;
            // Count tool calls
            for (const auto& part : msg.parts()) {
                if (part.is_tool()) {
                    tool_calls++;
                }
            }
        }
    }

    oss << "Summary:\n";
    oss << "- User messages: " << user_count << "\n";
    oss << "- Assistant messages: " << assistant_count << "\n";
    oss << "- Tool calls: " << tool_calls << "\n";

    // Add first and last message snippets
    if (!messages.empty()) {
        auto get_text = [](const Message& m) -> std::string {
            for (const auto& part : m.parts()) {
                if (part.is_text()) {
                    std::string text = part.get_text();
                    return text.substr(0, 100);
                }
            }
            return "";
        };

        oss << "\nFirst message: " << get_text(messages.front()) << "...\n";
        oss << "Last message: " << get_text(messages.back()) << "...\n";
    }

    std::string summary = oss.str();
    if (static_cast<int>(summary.length()) > max_length) {
        summary = summary.substr(0, static_cast<size_t>(max_length - 3)) + "...";
    }

    return summary;
}

std::vector<size_t> SessionCompaction::select_retained(
    const std::vector<Message>& messages,
    const CompactionConfig& config
) const {
    if (messages.size() <= static_cast<size_t>(config.min_messages_to_keep)) {
        // Keep all messages
        std::vector<size_t> all_indices;
        for (size_t i = 0; i < messages.size(); ++i) {
            all_indices.push_back(i);
        }
        return all_indices;
    }

    // Rank messages by importance
    auto ranked = rank_messages(messages);

    // Sort by importance score (descending)
    std::sort(ranked.begin(), ranked.end(),
        [](const MessageImportance& a, const MessageImportance& b) {
            return a.score > b.score;
        });

    // Calculate how many messages to keep
    size_t target_count = static_cast<size_t>(
        static_cast<double>(messages.size()) * config.target_ratio
    );
    target_count = std::max(target_count, static_cast<size_t>(config.min_messages_to_keep));
    target_count = std::min(target_count, messages.size());

    // Select top messages
    std::vector<size_t> retained;
    for (size_t i = 0; i < target_count && i < ranked.size(); ++i) {
        retained.push_back(static_cast<size_t>(ranked[i].position));
    }

    // Always keep the last few messages for context continuity
    size_t keep_recent = std::min(static_cast<size_t>(3), messages.size());
    for (size_t i = messages.size() - keep_recent; i < messages.size(); ++i) {
        if (std::find(retained.begin(), retained.end(), i) == retained.end()) {
            retained.push_back(i);
        }
    }

    // Sort by position for proper ordering
    std::sort(retained.begin(), retained.end());

    return retained;
}

int64_t SessionCompaction::estimate_tokens(const Message& msg) {
    int64_t tokens = 0;

    // Base overhead for message structure
    tokens += 4;

    // Count tokens in each part
    for (const auto& part : msg.parts()) {
        switch (part.type) {
            case PartType::Text: {
                tokens += estimate_text_tokens(part.get_text());
                break;
            }
            case PartType::Tool: {
                nlohmann::json tool = part.get_tool();
                if (tool.contains("name")) {
                    tokens += estimate_text_tokens(tool["name"].get<std::string>());
                }
                if (tool.contains("input")) {
                    tokens += estimate_text_tokens(tool["input"].dump());
                }
                tokens += 5; // Overhead for tool call structure
                break;
            }
            default:
                tokens += estimate_text_tokens(part.data.dump());
                break;
        }
    }

    return tokens;
}

int64_t SessionCompaction::estimate_text_tokens(const std::string& text) {
    // Simple estimation: ~4 characters per token for English
    // This is a rough approximation; actual tokenization depends on the model
    return static_cast<int64_t>(text.length() / 4) + 1;
}

void SessionCompaction::set_summary_generator(
    std::function<std::string(const std::vector<Message>&)> callback
) {
    summary_generator_ = std::move(callback);
}

MessageImportance SessionCompaction::calculate_importance(
    const Message& msg,
    size_t position,
    size_t total
) const {
    MessageImportance importance;
    importance.message_id = msg.id();
    importance.position = static_cast<int>(position);

    // Base score from position (recent messages are more important)
    double position_score = static_cast<double>(position) / static_cast<double>(total);
    importance.score = position_score * 0.3;

    // Check for tool calls and results
    for (const auto& part : msg.parts()) {
        if (part.is_tool()) {
            importance.has_tool_call = true;
            importance.score += 0.3;
        }
    }

    // System messages are always important
    if (msg.role() == Role::System) {
        importance.is_system = true;
        importance.score = 1.0;
    }

    // User messages are moderately important
    if (msg.role() == Role::User) {
        importance.score += 0.2;
    }

    // Cap at 1.0
    importance.score = std::min(importance.score, 1.0);

    return importance;
}

std::vector<MessageImportance> SessionCompaction::rank_messages(
    const std::vector<Message>& messages
) const {
    std::vector<MessageImportance> ranked;
    ranked.reserve(messages.size());

    for (size_t i = 0; i < messages.size(); ++i) {
        ranked.push_back(calculate_importance(messages[i], i, messages.size()));
    }

    return ranked;
}

Message SessionCompaction::create_summary_message(
    const std::string& summary,
    [[maybe_unused]] const std::vector<std::string>& removed_ids
) const {
    // Create a system message with the summary
    Message msg("", Role::System, "", "", "");
    
    std::string content = "[Context Compaction]\n" + summary;
    msg.add_text(content);

    return msg;
}

} // namespace turbot::core::session
