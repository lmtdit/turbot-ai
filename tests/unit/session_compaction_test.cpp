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
