/**
 * @file session_compaction_test.cpp
 * @brief Tests for SessionCompaction functionality
 *
 * Tests for:
 * - CompactionConfig::validate
 * - SessionCompaction::needs_compaction
 * - CompactionResult::to_json
 * - SessionCompaction::estimate_text_tokens
 */

#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/session/session_compaction.hpp>

using namespace turbot::core::session;

// ==================== CompactionConfig Tests ====================

TEST_CASE("CompactionConfig.Validate.Default", "[Session][Compaction]") {
    CompactionConfig config;
    REQUIRE(config.validate());
}

TEST_CASE("CompactionConfig.Validate.InvalidThreshold", "[Session][Compaction]") {
    CompactionConfig config;
    config.overflow_threshold = 0;
    REQUIRE_FALSE(config.validate());
    
    config.overflow_threshold = 1.5;
    REQUIRE_FALSE(config.validate());
}

TEST_CASE("CompactionConfig.Validate.ValidThreshold", "[Session][Compaction]") {
    CompactionConfig config;
    config.overflow_threshold = 0.5;
    REQUIRE(config.validate());
    
    config.overflow_threshold = 0.9;
    REQUIRE(config.validate());
    
    config.overflow_threshold = 1.0;
    REQUIRE(config.validate());
}

TEST_CASE("CompactionConfig.Validate.InvalidTargetRatio", "[Session][Compaction]") {
    CompactionConfig config;
    config.target_ratio = 0;
    REQUIRE_FALSE(config.validate());
    
    config.target_ratio = 1.5;
    REQUIRE_FALSE(config.validate());
}

TEST_CASE("CompactionConfig.Validate.ValidTargetRatio", "[Session][Compaction]") {
    CompactionConfig config;
    config.target_ratio = 0.3;
    REQUIRE(config.validate());
    
    config.target_ratio = 0.5;
    REQUIRE(config.validate());
    
    config.target_ratio = 1.0;
    REQUIRE(config.validate());
}

TEST_CASE("CompactionConfig.Validate.InvalidMinMessages", "[Session][Compaction]") {
    CompactionConfig config;
    config.min_messages_to_keep = 0;
    REQUIRE_FALSE(config.validate());
}

TEST_CASE("CompactionConfig.Validate.ValidMinMessages", "[Session][Compaction]") {
    CompactionConfig config;
    config.min_messages_to_keep = 1;
    REQUIRE(config.validate());
    
    config.min_messages_to_keep = 5;
    REQUIRE(config.validate());
    
    config.min_messages_to_keep = 100;
    REQUIRE(config.validate());
}

TEST_CASE("CompactionConfig.Validate.InvalidMaxSummaryLength", "[Session][Compaction]") {
    CompactionConfig config;
    config.max_summary_length = 50;
    REQUIRE_FALSE(config.validate());
    
    config.max_summary_length = 99;
    REQUIRE_FALSE(config.validate());
}

TEST_CASE("CompactionConfig.Validate.ValidMaxSummaryLength", "[Session][Compaction]") {
    CompactionConfig config;
    config.max_summary_length = 100;
    REQUIRE(config.validate());
    
    config.max_summary_length = 2000;
    REQUIRE(config.validate());
    
    config.max_summary_length = 10000;
    REQUIRE(config.validate());
}

// ==================== Needs Compaction Tests ====================

TEST_CASE("SessionCompaction.NeedsCompaction.BelowThreshold", "[Session][Compaction]") {
    CompactionConfig config;
    config.overflow_threshold = 0.8;
    
    REQUIRE_FALSE(SessionCompaction::needs_compaction(50, 100, config));
    REQUIRE_FALSE(SessionCompaction::needs_compaction(79, 100, config));
}

TEST_CASE("SessionCompaction.NeedsCompaction.AboveThreshold", "[Session][Compaction]") {
    CompactionConfig config;
    config.overflow_threshold = 0.8;
    
    REQUIRE(SessionCompaction::needs_compaction(80, 100, config));
    REQUIRE(SessionCompaction::needs_compaction(90, 100, config));
    REQUIRE(SessionCompaction::needs_compaction(100, 100, config));
}

TEST_CASE("SessionCompaction.NeedsCompaction.ZeroMaxTokens", "[Session][Compaction]") {
    CompactionConfig config;
    
    REQUIRE_FALSE(SessionCompaction::needs_compaction(100, 0, config));
    REQUIRE_FALSE(SessionCompaction::needs_compaction(0, 0, config));
}

TEST_CASE("SessionCompaction.NeedsCompaction.ExactThreshold", "[Session][Compaction]") {
    CompactionConfig config;
    config.overflow_threshold = 0.75;
    
    REQUIRE(SessionCompaction::needs_compaction(75, 100, config));
}

TEST_CASE("SessionCompaction.NeedsCompaction.VariousThresholds", "[Session][Compaction]") {
    CompactionConfig config;
    
    config.overflow_threshold = 0.5;
    REQUIRE(SessionCompaction::needs_compaction(50, 100, config));
    REQUIRE_FALSE(SessionCompaction::needs_compaction(49, 100, config));
    
    config.overflow_threshold = 0.9;
    REQUIRE(SessionCompaction::needs_compaction(90, 100, config));
    REQUIRE_FALSE(SessionCompaction::needs_compaction(89, 100, config));
    
    config.overflow_threshold = 1.0;
    REQUIRE(SessionCompaction::needs_compaction(100, 100, config));
    REQUIRE_FALSE(SessionCompaction::needs_compaction(99, 100, config));
}

// ==================== CompactionResult Tests ====================

TEST_CASE("CompactionResult.ToJson", "[Session][Compaction]") {
    CompactionResult result;
    result.summary = "Test summary";
    result.retained_ids = {"msg-1", "msg-2"};
    result.removed_ids = {"msg-3"};
    result.original_tokens = 1000;
    result.compressed_tokens = 500;
    result.compression_ratio = 0.5;
    
    auto j = result.to_json();
    REQUIRE(j["summary"] == "Test summary");
    REQUIRE(j["retained_ids"].size() == 2);
    REQUIRE(j["retained_ids"][0] == "msg-1");
    REQUIRE(j["retained_ids"][1] == "msg-2");
    REQUIRE(j["removed_ids"].size() == 1);
    REQUIRE(j["removed_ids"][0] == "msg-3");
    REQUIRE(j["original_tokens"] == 1000);
    REQUIRE(j["compressed_tokens"] == 500);
    REQUIRE(j["compression_ratio"] == 0.5);
}

TEST_CASE("CompactionResult.ToJson.Empty", "[Session][Compaction]") {
    CompactionResult result;
    
    auto j = result.to_json();
    REQUIRE(j["summary"] == "");
    REQUIRE(j["retained_ids"].empty());
    REQUIRE(j["removed_ids"].empty());
    REQUIRE(j["original_tokens"] == 0);
    REQUIRE(j["compressed_tokens"] == 0);
    REQUIRE(j["compression_ratio"] == 0.0);
}

// ==================== Token Estimation Tests ====================

TEST_CASE("SessionCompaction.EstimateTextTokens.Basic", "[Session][Compaction]") {
    std::string text = "Hello, world!";
    int64_t tokens = SessionCompaction::estimate_text_tokens(text);
    REQUIRE(tokens > 0);
}

TEST_CASE("SessionCompaction.EstimateTextTokens.Empty", "[Session][Compaction]") {
    std::string text = "";
    int64_t tokens = SessionCompaction::estimate_text_tokens(text);
    // Empty string still returns 1 due to implementation: length/4 + 1
    REQUIRE(tokens == 1);
}

TEST_CASE("SessionCompaction.EstimateTextTokens.LongerText", "[Session][Compaction]") {
    std::string text = "This is a test string for token estimation.";
    int64_t tokens = SessionCompaction::estimate_text_tokens(text);
    REQUIRE(tokens > 0);
    
    // Longer text should have more tokens
    std::string longer_text = text + text + text;
    int64_t longer_tokens = SessionCompaction::estimate_text_tokens(longer_text);
    REQUIRE(longer_tokens > tokens);
}

// ==================== PruneConfig Tests ====================

TEST_CASE("PruneConfig.Defaults", "[Session][Compaction]") {
    PruneConfig config;
    REQUIRE(config.protect_tokens == 40000);
    REQUIRE(config.minimum_prune == 20000);
    REQUIRE_FALSE(config.exempt_tools.empty());
}

TEST_CASE("PruneConfig.CustomValues", "[Session][Compaction]") {
    PruneConfig config;
    config.protect_tokens = 50000;
    config.minimum_prune = 10000;
    config.exempt_tools = {"tool1", "tool2"};
    
    REQUIRE(config.protect_tokens == 50000);
    REQUIRE(config.minimum_prune == 10000);
    REQUIRE(config.exempt_tools.size() == 2);
}

// ==================== PruneResult Tests ====================

TEST_CASE("PruneResult.Defaults", "[Session][Compaction]") {
    PruneResult result;
    REQUIRE(result.pruned_parts == 0);
    REQUIRE(result.freed_tokens == 0);
    REQUIRE_FALSE(result.did_prune);
}
