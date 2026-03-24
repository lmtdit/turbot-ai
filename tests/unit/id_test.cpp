// id_test.cpp - Unit tests for turbot::core::id
//
// Covers:
//  - ID format validation (prefix_12hex14base62)
//  - ascending: timestamp encoded unmodified
//  - descending: timestamp bitwise-NOT'd (newer IDs sort first lexically)
//  - timestamp() extraction from ascending IDs
//  - monotonic counter (two IDs in same ms differ)
//  - convenience generators

#include <catch2/catch_test_macros.hpp>
#include <turbot/core/id/id.hpp>
#include <string>
#include <regex>
#include <thread>
#include <chrono>
#include <set>

using namespace turbot::core::id;

// ---- helpers ----

static bool matches_format(const std::string& id) {
    // pattern: word-chars + "_" + 26 chars (hex then base62)
    static const std::regex kPattern(R"([a-z]+_[0-9a-fA-F]{12}[0-9A-Za-z]{14})");
    return std::regex_match(id, kPattern);
}

// ---- tests ----

TEST_CASE("Identifier: ascending ID format", "[id][ascending]") {
    const auto id = ascending("msg");

    SECTION("matches expected regex") {
        REQUIRE(matches_format(id));
    }

    SECTION("starts with prefix and underscore") {
        REQUIRE(id.substr(0, 4) == "msg_");
    }

    SECTION("total length is prefix + 1 + 26 = 30") {
        REQUIRE(id.size() == 4 + 26);  // "msg_" + 26
    }
}

TEST_CASE("Identifier: descending ID format", "[id][descending]") {
    const auto id = descending("ses");

    SECTION("matches expected regex") {
        REQUIRE(matches_format(id));
    }

    SECTION("starts with prefix") {
        REQUIRE(id.substr(0, 4) == "ses_");
    }
}

TEST_CASE("Identifier: ascending IDs sort oldest-first", "[id][ordering]") {
    // IDs generated 1ms apart should sort older < newer
    const auto id1 = ascending("msg", 1000000LL);
    const auto id2 = ascending("msg", 1000001LL);  // 1ms later

    REQUIRE(id1 < id2);  // lexicographic < means older sorts first
}

TEST_CASE("Identifier: descending IDs sort newest-first", "[id][ordering]") {
    // Newest should sort BEFORE older in lexicographic order
    const auto id1 = descending("ses", 1000000LL);
    const auto id2 = descending("ses", 1000001LL);  // 1ms later (newer)

    // In descending encoding, newer timestamps produce smaller hex values
    REQUIRE(id2 < id1);  // newest sorts first
}

TEST_CASE("Identifier: timestamp extraction (ascending)", "[id][timestamp]") {
    // NOTE: The timestamp stored in the ID is truncated to the low 48 bits of
    //       (ts_ms * 0x1000 + counter).  For ts values that fit in 36 bits
    //       (< ~68 billion ms, i.e. well within the year 4147) the extraction
    //       is lossless.  For current epoch (~1.7e12 ms) only the low 36 bits
    //       of ts_ms are recoverable — this matches OpenCode's behaviour.
    //
    // We test with a small ts that round-trips exactly.
    const int64_t ts = 12345678LL;  // fits comfortably in 36 bits
    const auto id   = ascending("msg", ts);

    const int64_t extracted = timestamp(id);
    REQUIRE(extracted == ts);
}

TEST_CASE("Identifier: timestamp returns 0 for invalid input", "[id][timestamp]") {
    REQUIRE(timestamp("") == 0);
    REQUIRE(timestamp("invalid") == 0);
    REQUIRE(timestamp("msg_short") == 0);
}

TEST_CASE("Identifier: monotonic counter prevents duplicate IDs", "[id][uniqueness]") {
    // Generate 100 IDs at the same timestamp — all should be unique
    constexpr int64_t ts = 1234567890000LL;
    std::set<std::string> ids;
    for (int i = 0; i < 100; ++i) {
        ids.insert(ascending("msg", ts));
    }
    REQUIRE(ids.size() == 100);
}

TEST_CASE("Identifier: IDs are globally unique across calls", "[id][uniqueness]") {
    std::set<std::string> ids;
    for (int i = 0; i < 1000; ++i) {
        ids.insert(ascending("msg"));
    }
    REQUIRE(ids.size() == 1000);
}

TEST_CASE("Identifier: convenience generators have correct prefixes", "[id][convenience]") {
    REQUIRE(session_id().substr(0, 4)    == "ses_");
    REQUIRE(message_id().substr(0, 4)    == "msg_");
    REQUIRE(part_id().substr(0, 4)       == "prt_");
    REQUIRE(permission_id().substr(0, 4) == "per_");
    REQUIRE(question_id().substr(0, 4)   == "que_");
    REQUIRE(user_id().substr(0, 4)       == "usr_");
    REQUIRE(workspace_id().substr(0, 4)  == "wrk_");
    REQUIRE(pty_id().substr(0, 4)        == "pty_");
    REQUIRE(tool_call_id().substr(0, 5)  == "tool_");
}

TEST_CASE("Identifier: all formats match expected pattern", "[id][format]") {
    static const std::regex kPattern(R"([a-z]+_[0-9a-fA-F]{12}[0-9A-Za-z]{14})");

    auto check = [&](const std::string& id) {
        INFO("ID: " << id);
        REQUIRE(std::regex_match(id, kPattern));
    };

    check(session_id());
    check(message_id());
    check(part_id());
    check(permission_id());
    check(question_id());
    check(user_id());
    check(workspace_id());
    check(pty_id());
    check(tool_call_id());
}
