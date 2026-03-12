#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <turbot/core/session/session_compaction.hpp>
#include <turbot/core/message/message.hpp>

using namespace turbot::core;
using namespace turbot::core::session;

// Helper to create test messages
static Message create_test_message(Role role, const std::string& text) {
    Message msg("test-session", role, "test-agent", "test-model", "test-provider");
    msg.add_text(text);
    return msg;
}

TEST_CASE("CompactionConfig validation", "[core][session][compaction]") {
    SECTION("default config is valid") {
        CompactionConfig config;
        REQUIRE(config.validate());
    }

    SECTION("invalid overflow_threshold") {
        CompactionConfig config;
        config.overflow_threshold = 0;
        REQUIRE_FALSE(config.validate());
        config.overflow_threshold = 1.5;
        REQUIRE_FALSE(config.validate());
    }

    SECTION("invalid target_ratio") {
        CompactionConfig config;
        config.target_ratio = 0;
        REQUIRE_FALSE(config.validate());
        config.target_ratio = 1.5;
        REQUIRE_FALSE(config.validate());
    }

    SECTION("invalid min_messages_to_keep") {
        CompactionConfig config;
        config.min_messages_to_keep = 0;
        REQUIRE_FALSE(config.validate());
    }

    SECTION("invalid max_summary_length") {
        CompactionConfig config;
        config.max_summary_length = 50;
        REQUIRE_FALSE(config.validate());
    }
}

TEST_CASE("SessionCompaction::needs_compaction", "[core][session][compaction]") {
    SECTION("no compaction needed under threshold") {
        REQUIRE_FALSE(SessionCompaction::needs_compaction(50000, 100000));
        REQUIRE_FALSE(SessionCompaction::needs_compaction(80000, 100000));
    }

    SECTION("compaction needed at threshold") {
        REQUIRE(SessionCompaction::needs_compaction(90000, 100000));
        REQUIRE(SessionCompaction::needs_compaction(95000, 100000));
    }

    SECTION("handles zero max_tokens") {
        REQUIRE_FALSE(SessionCompaction::needs_compaction(1000, 0));
    }

    SECTION("custom threshold") {
        CompactionConfig config;
        config.overflow_threshold = 0.8;
        REQUIRE(SessionCompaction::needs_compaction(85000, 100000, config));
        REQUIRE_FALSE(SessionCompaction::needs_compaction(75000, 100000, config));
    }
}

TEST_CASE("SessionCompaction::estimate_text_tokens", "[core][session][compaction]") {
    SECTION("empty string") {
        REQUIRE(SessionCompaction::estimate_text_tokens("") == 1);
    }

    SECTION("short text") {
        int64_t tokens = SessionCompaction::estimate_text_tokens("Hello");
        REQUIRE(tokens >= 1);
        REQUIRE(tokens <= 3);
    }

    SECTION("longer text") {
        int64_t tokens = SessionCompaction::estimate_text_tokens("This is a longer piece of text for testing.");
        REQUIRE(tokens > 5);
        REQUIRE(tokens < 20);
    }
}

TEST_CASE("SessionCompaction::estimate_tokens", "[core][session][compaction]") {
    SECTION("simple text message") {
        auto msg = create_test_message(Role::User, "Hello world");
        int64_t tokens = SessionCompaction::estimate_tokens(msg);
        REQUIRE(tokens > 0);
    }

    SECTION("message with tool call") {
        Message msg("test-session", Role::Assistant, "test-agent", "test-model", "test-provider");
        msg.add_text("Let me help you.");
        msg.add_tool("tool-123", "read_file", {{"path", "/test/file.txt"}});
        
        int64_t tokens = SessionCompaction::estimate_tokens(msg);
        REQUIRE(tokens > 10);
    }
}

TEST_CASE("SessionCompaction::generate_summary", "[core][session][compaction]") {
    SessionCompaction compactor;

    SECTION("empty messages") {
        std::vector<Message> messages;
        std::string summary = compactor.generate_summary(messages);
        REQUIRE(summary.empty());
    }

    SECTION("single message") {
        std::vector<Message> messages;
        messages.push_back(create_test_message(Role::User, "Hello"));
        
        std::string summary = compactor.generate_summary(messages);
        REQUIRE_FALSE(summary.empty());
        REQUIRE(summary.find("User messages: 1") != std::string::npos);
    }

    SECTION("multiple messages") {
        std::vector<Message> messages;
        messages.push_back(create_test_message(Role::User, "What is AI?"));
        messages.push_back(create_test_message(Role::Assistant, "AI stands for..."));
        messages.push_back(create_test_message(Role::User, "Tell me more"));
        messages.push_back(create_test_message(Role::Assistant, "AI includes..."));
        
        std::string summary = compactor.generate_summary(messages);
        REQUIRE_FALSE(summary.empty());
        REQUIRE(summary.find("User messages: 2") != std::string::npos);
        REQUIRE(summary.find("Assistant messages: 2") != std::string::npos);
    }

    SECTION("max_length truncation") {
        std::vector<Message> messages;
        for (int i = 0; i < 100; i++) {
            messages.push_back(create_test_message(Role::User, "This is a test message."));
        }
        
        std::string summary = compactor.generate_summary(messages, 100);
        REQUIRE(summary.length() <= 103);  // 100 + "..."
    }
}

TEST_CASE("SessionCompaction::select_retained", "[core][session][compaction]") {
    SessionCompaction compactor;
    CompactionConfig config;
    config.min_messages_to_keep = 3;
    config.target_ratio = 0.5;

    SECTION("keeps minimum messages") {
        std::vector<Message> messages;
        for (int i = 0; i < 5; i++) {
            messages.push_back(create_test_message(Role::User, "Message " + std::to_string(i)));
        }
        
        auto retained = compactor.select_retained(messages, config);
        REQUIRE(retained.size() >= static_cast<size_t>(config.min_messages_to_keep));
    }

    SECTION("keeps recent messages") {
        std::vector<Message> messages;
        for (int i = 0; i < 20; i++) {
            messages.push_back(create_test_message(Role::User, "Message " + std::to_string(i)));
        }
        
        auto retained = compactor.select_retained(messages, config);
        
        // Last 3 messages should always be retained
        bool has_last = false;
        for (size_t idx : retained) {
            if (idx == messages.size() - 1) has_last = true;
        }
        REQUIRE(has_last);
    }

    SECTION("returns all when messages <= min") {
        std::vector<Message> messages;
        for (int i = 0; i < 3; i++) {
            messages.push_back(create_test_message(Role::User, "Message " + std::to_string(i)));
        }
        
        auto retained = compactor.select_retained(messages, config);
        REQUIRE(retained.size() == messages.size());
    }
}

TEST_CASE("SessionCompaction::compact", "[core][session][compaction]") {
    SessionCompaction compactor;
    CompactionConfig config;
    config.target_ratio = 0.5;
    config.min_messages_to_keep = 3;

    SECTION("empty messages") {
        std::vector<Message> messages;
        auto result = compactor.compact(messages, config);
        
        REQUIRE(result.summary.empty());
        REQUIRE(result.original_tokens == 0);
        REQUIRE(result.compressed_tokens == 0);
    }

    SECTION("small message list") {
        std::vector<Message> messages;
        for (int i = 0; i < 5; i++) {
            messages.push_back(create_test_message(Role::User, "Message " + std::to_string(i)));
        }
        
        auto result = compactor.compact(messages, config);
        
        REQUIRE(result.original_tokens > 0);
        REQUIRE_FALSE(result.retained_ids.empty());
    }

    SECTION("large message list") {
        std::vector<Message> messages;
        for (int i = 0; i < 50; i++) {
            messages.push_back(create_test_message(
                i % 2 == 0 ? Role::User : Role::Assistant,
                "This is a longer message with more content for testing compaction. Message " + std::to_string(i)
            ));
        }
        
        auto result = compactor.compact(messages, config);
        
        REQUIRE_FALSE(result.summary.empty());
        REQUIRE(result.original_tokens > 0);
        REQUIRE(result.compressed_tokens > 0);
        REQUIRE(result.compressed_tokens < result.original_tokens);
        REQUIRE(result.compression_ratio > 0);
        REQUIRE(result.compression_ratio < 1);
        REQUIRE(result.retained_ids.size() < messages.size());
        REQUIRE(result.removed_ids.size() > 0);
    }
}

TEST_CASE("CompactionResult::to_json", "[core][session][compaction]") {
    CompactionResult result;
    result.summary = "Test summary";
    result.retained_ids = {"id1", "id2"};
    result.removed_ids = {"id3"};
    result.original_tokens = 1000;
    result.compressed_tokens = 500;
    result.compression_ratio = 0.5;

    auto json = result.to_json();

    REQUIRE(json["summary"] == "Test summary");
    REQUIRE(json["retained_ids"].size() == 2);
    REQUIRE(json["removed_ids"].size() == 1);
    REQUIRE(json["original_tokens"] == 1000);
    REQUIRE(json["compressed_tokens"] == 500);
    REQUIRE(json["compression_ratio"] == Catch::Approx(0.5).epsilon(0.01));
}

TEST_CASE("SessionCompaction custom summary generator", "[core][session][compaction]") {
    SessionCompaction compactor;
    
    compactor.set_summary_generator([](const std::vector<Message>& messages) {
        return "Custom summary: " + std::to_string(messages.size()) + " messages";
    });

    std::vector<Message> messages;
    for (int i = 0; i < 5; i++) {
        messages.push_back(create_test_message(Role::User, "Message " + std::to_string(i)));
    }

    std::string summary = compactor.generate_summary(messages);
    REQUIRE(summary == "Custom summary: 5 messages");
}

// =============================================================================
// SessionCompaction::prune tests
// =============================================================================

/// Helper: add a tool-result part to an existing Message (assistant role).
static void add_tool_result(Message& msg,
                             const std::string& tool_name,
                             const std::string& result_text) {
    nlohmann::json result = {{"output", result_text}};
    msg.add_tool("tool-" + tool_name, tool_name, {{"path", "/test"}}, result);
}

/// Helper: create an assistant message that contains a single tool call + result.
static Message create_tool_message(const std::string& tool_name,
                                   const std::string& result_text,
                                   bool with_result = true) {
    Message msg("test-session", Role::Assistant, "agent", "model", "provider");
    if (with_result) {
        add_tool_result(msg, tool_name, result_text);
    } else {
        msg.add_tool("tool-" + tool_name, tool_name, {{"path", "/test"}});
    }
    return msg;
}

TEST_CASE("Part::is_tool_compacted / set_tool_compacted_at / get_tool_compacted_at",
          "[core][message][part][prune]") {
    SECTION("fresh tool part is not compacted") {
        nlohmann::json result = {{"output", "hello"}};
        Part p = Part::create_tool("id1", "read_file", {{"path", "/f"}}, result);
        REQUIRE_FALSE(p.is_tool_compacted());
        REQUIRE_FALSE(p.get_tool_compacted_at().has_value());
    }

    SECTION("set_tool_compacted_at marks the part and clears result") {
        nlohmann::json result = {{"output", "big result content"}};
        Part p = Part::create_tool("id1", "read_file", {{"path", "/f"}}, result);

        REQUIRE_FALSE(p.is_tool_compacted());
        p.set_tool_compacted_at(1234567890LL);

        REQUIRE(p.is_tool_compacted());
        REQUIRE(p.get_tool_compacted_at().has_value());
        REQUIRE(*p.get_tool_compacted_at() == 1234567890LL);

        // result payload should have been cleared
        const auto tool_data = p.get_tool();
        REQUIRE_FALSE(tool_data.contains("result"));

        // metadata fields should still exist
        REQUIRE(tool_data.contains("tool_name"));
        REQUIRE(tool_data.contains("tool_id"));
    }

    SECTION("set_tool_compacted_at is no-op on non-tool part") {
        Part p = Part::create_text("hello");
        p.set_tool_compacted_at(999LL);
        REQUIRE_FALSE(p.is_tool_compacted());
    }
}

TEST_CASE("SessionCompaction::prune — no-op when not enough tokens to prune",
          "[core][session][compaction][prune]") {
    // Create a small history that barely exceeds protect_tokens.
    // protect_tokens=40K but result payloads are tiny → freed_tokens < minimum_prune.
    std::vector<Message> messages;
    for (int i = 0; i < 10; ++i) {
        messages.push_back(create_tool_message("read_file", "small result " + std::to_string(i)));
    }

    PruneConfig cfg;
    cfg.protect_tokens = 40'000;
    cfg.minimum_prune  = 20'000;  // far more than tiny payloads can supply

    auto result = SessionCompaction::prune(messages, cfg);

    REQUIRE_FALSE(result.did_prune);
    REQUIRE(result.pruned_parts == 0);
    REQUIRE(result.freed_tokens == 0);

    // No part should have been compacted.
    for (const auto& msg : messages) {
        for (const auto& part : msg.parts()) {
            if (part.is_tool()) {
                REQUIRE_FALSE(part.is_tool_compacted());
            }
        }
    }
}

TEST_CASE("SessionCompaction::prune — prunes old tool results when tokens exceed protect window",
          "[core][session][compaction][prune]") {
    // Build ~100K-token history: 200 tool-result messages, each result ~500 tokens
    // (≈2000 chars × 1/4 chars-per-token).
    const std::string big_result(2000, 'x');  // ~500 tokens per result

    std::vector<Message> messages;
    // 200 old tool-result messages (oldest first)
    for (int i = 0; i < 200; ++i) {
        messages.push_back(create_tool_message("read_file", big_result));
    }
    // Append a few recent messages to act as the protected window
    for (int i = 0; i < 10; ++i) {
        messages.push_back(create_test_message(Role::User, "Recent user message " + std::to_string(i)));
    }

    PruneConfig cfg;
    cfg.protect_tokens = 40'000;   // ~80 recent tool results
    cfg.minimum_prune  = 20'000;   // need ≥20K freed

    auto result = SessionCompaction::prune(messages, cfg);

    REQUIRE(result.did_prune);
    REQUIRE(result.pruned_parts > 0);
    REQUIRE(result.freed_tokens >= cfg.minimum_prune);

    // Count compacted vs uncompacted tool parts
    int compacted = 0;
    int uncompacted = 0;
    for (const auto& msg : messages) {
        for (const auto& part : msg.parts()) {
            if (!part.is_tool()) continue;
            if (part.is_tool_compacted()) {
                compacted++;
                // Compacted parts must not carry the result payload.
                REQUIRE_FALSE(part.get_tool().contains("result"));
            } else {
                uncompacted++;
            }
        }
    }

    REQUIRE(compacted > 0);
    REQUIRE(uncompacted > 0);  // protected window remains intact

    // Compacted parts must be older than uncompacted ones
    // (i.e., all compacted come from the first N messages).
    // Find last compacted message index and first uncompacted message index.
    int last_compacted_msg  = -1;
    int first_uncompacted_msg = INT_MAX;
    for (int mi = 0; mi < static_cast<int>(messages.size()); ++mi) {
        for (const auto& part : messages[static_cast<size_t>(mi)].parts()) {
            if (!part.is_tool()) continue;
            if (part.is_tool_compacted())    last_compacted_msg  = std::max(last_compacted_msg, mi);
            else                             first_uncompacted_msg = std::min(first_uncompacted_msg, mi);
        }
    }
    // The protected (uncompacted) window must start at or after all compacted messages.
    if (first_uncompacted_msg != INT_MAX && last_compacted_msg != -1) {
        REQUIRE(last_compacted_msg < first_uncompacted_msg);
    }
}

TEST_CASE("SessionCompaction::prune — exempt tools are never pruned",
          "[core][session][compaction][prune]") {
    const std::string big_result(2000, 'x');

    std::vector<Message> messages;
    // Mix of 'skill' (exempt) and 'read_file' (non-exempt) calls
    for (int i = 0; i < 100; ++i) {
        messages.push_back(create_tool_message(i % 2 == 0 ? "skill" : "read_file", big_result));
    }

    PruneConfig cfg;
    cfg.protect_tokens = 1'000;  // very small window → prune aggressively
    cfg.minimum_prune  = 1;      // accept any savings
    cfg.exempt_tools   = {"skill"};

    auto result = SessionCompaction::prune(messages, cfg);

    REQUIRE(result.did_prune);

    for (const auto& msg : messages) {
        for (const auto& part : msg.parts()) {
            if (!part.is_tool()) continue;
            const auto tool_name = part.get_tool().value("tool_name", "");
            if (tool_name == "skill") {
                // Exempt tools must never be compacted.
                REQUIRE_FALSE(part.is_tool_compacted());
            }
        }
    }
}

TEST_CASE("SessionCompaction::prune — already compacted parts are skipped",
          "[core][session][compaction][prune]") {
    const std::string big_result(2000, 'x');

    std::vector<Message> messages;
    for (int i = 0; i < 50; ++i) {
        messages.push_back(create_tool_message("read_file", big_result));
    }

    PruneConfig cfg;
    cfg.protect_tokens = 1'000;
    cfg.minimum_prune  = 1;

    // First prune
    auto r1 = SessionCompaction::prune(messages, cfg);
    REQUIRE(r1.did_prune);
    int first_pruned = r1.pruned_parts;
    REQUIRE(first_pruned > 0);

    // Second prune — nothing new to prune (all candidates already compacted)
    auto r2 = SessionCompaction::prune(messages, cfg);
    // Either did_prune=false (no new parts) or pruned_parts=0
    REQUIRE(r2.pruned_parts == 0);
}
