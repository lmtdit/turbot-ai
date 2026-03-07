#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <turbot/core/permission/permission.hpp>

using namespace turbot::core::permission;

// ============================================================================
// PermissionAction helpers
// ============================================================================

TEST_CASE("permission_action_to_string", "[core][permission]") {
    REQUIRE(permission_action_to_string(PermissionAction::Allow) == "allow");
    REQUIRE(permission_action_to_string(PermissionAction::Deny)  == "deny");
    REQUIRE(permission_action_to_string(PermissionAction::Ask)   == "ask");
}

TEST_CASE("permission_action_from_string valid", "[core][permission]") {
    REQUIRE(permission_action_from_string("allow") == PermissionAction::Allow);
    REQUIRE(permission_action_from_string("deny")  == PermissionAction::Deny);
    REQUIRE(permission_action_from_string("ask")   == PermissionAction::Ask);
}

TEST_CASE("permission_action_from_string invalid throws", "[core][permission]") {
    REQUIRE_THROWS_AS(permission_action_from_string("unknown"), std::invalid_argument);
    REQUIRE_THROWS_AS(permission_action_from_string(""), std::invalid_argument);
    REQUIRE_THROWS_AS(permission_action_from_string("ALLOW"), std::invalid_argument);
}

// ============================================================================
// PermissionRule
// ============================================================================

TEST_CASE("PermissionRule::to_json", "[core][permission]") {
    PermissionRule rule;
    rule.permission = "read";
    rule.pattern    = "/tmp/*";
    rule.action     = PermissionAction::Allow;

    const auto j = rule.to_json();
    REQUIRE(j["permission"] == "read");
    REQUIRE(j["pattern"]    == "/tmp/*");
    REQUIRE(j["action"]     == "allow");
}

TEST_CASE("PermissionRule::from_json", "[core][permission]") {
    nlohmann::json j = {
        {"permission", "write"},
        {"pattern",    "/home/user/*"},
        {"action",     "ask"}
    };

    const auto rule = PermissionRule::from_json(j);
    REQUIRE(rule.permission == "write");
    REQUIRE(rule.pattern    == "/home/user/*");
    REQUIRE(rule.action     == PermissionAction::Ask);
}

TEST_CASE("PermissionRule::from_json deny action", "[core][permission]") {
    nlohmann::json j = {
        {"permission", "execute"},
        {"pattern",    "*"},
        {"action",     "deny"}
    };

    const auto rule = PermissionRule::from_json(j);
    REQUIRE(rule.action == PermissionAction::Deny);
}

TEST_CASE("PermissionRule equality", "[core][permission]") {
    PermissionRule r1{"read", "/tmp/*", PermissionAction::Allow};
    PermissionRule r2{"read", "/tmp/*", PermissionAction::Allow};
    PermissionRule r3{"read", "/tmp/*", PermissionAction::Deny};

    REQUIRE(r1 == r2);
    REQUIRE_FALSE(r1 == r3);
}

TEST_CASE("PermissionRule round-trip JSON", "[core][permission]") {
    PermissionRule original{"write", "/var/log/*.log", PermissionAction::Deny};
    const auto restored = PermissionRule::from_json(original.to_json());
    REQUIRE(original == restored);
}

// ============================================================================
// Ruleset helpers
// ============================================================================

TEST_CASE("ruleset_to_json and ruleset_from_json", "[core][permission]") {
    Ruleset rs = {
        {"read",  "*",      PermissionAction::Allow},
        {"write", "/tmp/*", PermissionAction::Allow},
        {"write", "*",      PermissionAction::Deny}
    };

    const auto j       = ruleset_to_json(rs);
    const auto restored = ruleset_from_json(j);

    REQUIRE(restored.size() == 3);
    REQUIRE(restored[0].permission == "read");
    REQUIRE(restored[1].pattern    == "/tmp/*");
    REQUIRE(restored[2].action     == PermissionAction::Deny);
}

TEST_CASE("ruleset_to_json empty", "[core][permission]") {
    const auto j = ruleset_to_json({});
    REQUIRE(j.is_array());
    REQUIRE(j.empty());
}

// ============================================================================
// PermissionReply::Type helpers
// ============================================================================

TEST_CASE("permission_reply_type_to_string", "[core][permission]") {
    REQUIRE(permission_reply_type_to_string(PermissionReply::Type::Once)   == "once");
    REQUIRE(permission_reply_type_to_string(PermissionReply::Type::Always) == "always");
    REQUIRE(permission_reply_type_to_string(PermissionReply::Type::Reject) == "reject");
}

TEST_CASE("permission_reply_type_from_string valid", "[core][permission]") {
    REQUIRE(permission_reply_type_from_string("once")   == PermissionReply::Type::Once);
    REQUIRE(permission_reply_type_from_string("always") == PermissionReply::Type::Always);
    REQUIRE(permission_reply_type_from_string("reject") == PermissionReply::Type::Reject);
}

TEST_CASE("permission_reply_type_from_string invalid", "[core][permission]") {
    REQUIRE_THROWS_AS(permission_reply_type_from_string("yes"), std::invalid_argument);
    REQUIRE_THROWS_AS(permission_reply_type_from_string(""),    std::invalid_argument);
}

// ============================================================================
// PermissionReply
// ============================================================================

TEST_CASE("PermissionReply factory methods", "[core][permission]") {
    const auto once   = PermissionReply::once();
    const auto always = PermissionReply::always("always message");
    const auto reject = PermissionReply::reject("rejection reason");

    REQUIRE(once.type   == PermissionReply::Type::Once);
    REQUIRE(!once.message.has_value());

    REQUIRE(always.type == PermissionReply::Type::Always);
    REQUIRE(always.message.has_value());
    REQUIRE(*always.message == "always message");

    REQUIRE(reject.type == PermissionReply::Type::Reject);
    REQUIRE(reject.message.has_value());
    REQUIRE(*reject.message == "rejection reason");
}

TEST_CASE("PermissionReply::to_json no message", "[core][permission]") {
    const auto reply = PermissionReply::once();
    const auto j     = reply.to_json();
    REQUIRE(j["type"] == "once");
    REQUIRE(!j.contains("message"));
}

TEST_CASE("PermissionReply::to_json with message", "[core][permission]") {
    const auto reply = PermissionReply::always("note");
    const auto j     = reply.to_json();
    REQUIRE(j["type"]    == "always");
    REQUIRE(j["message"] == "note");
}

TEST_CASE("PermissionReply::from_json", "[core][permission]") {
    nlohmann::json j = {{"type", "reject"}, {"message", "no"}};
    const auto reply = PermissionReply::from_json(j);
    REQUIRE(reply.type == PermissionReply::Type::Reject);
    REQUIRE(reply.message.has_value());
    REQUIRE(*reply.message == "no");
}

TEST_CASE("PermissionReply::from_json no message", "[core][permission]") {
    nlohmann::json j = {{"type", "once"}};
    const auto reply = PermissionReply::from_json(j);
    REQUIRE(reply.type == PermissionReply::Type::Once);
    REQUIRE(!reply.message.has_value());
}

TEST_CASE("PermissionReply round-trip JSON", "[core][permission]") {
    const auto original = PermissionReply::always("round-trip");
    const auto restored = PermissionReply::from_json(original.to_json());
    REQUIRE(restored.type == original.type);
    REQUIRE(restored.message == original.message);
}

// ============================================================================
// PermissionRequest
// ============================================================================

TEST_CASE("PermissionRequest::to_json with tool", "[core][permission]") {
    PermissionRequest req;
    req.id         = "req-001";
    req.permission = "read";
    req.patterns   = {"/tmp/file.txt"};
    req.tool       = "read_file";
    req.metadata   = {{"key", "value"}};

    const auto j = req.to_json();
    REQUIRE(j["id"]         == "req-001");
    REQUIRE(j["permission"] == "read");
    REQUIRE(j["patterns"]   == nlohmann::json::array({"/tmp/file.txt"}));
    REQUIRE(j["tool"]       == "read_file");
    REQUIRE(j["metadata"]["key"] == "value");
}

TEST_CASE("PermissionRequest::to_json without tool", "[core][permission]") {
    PermissionRequest req;
    req.id         = "req-002";
    req.permission = "write";
    req.patterns   = {};

    const auto j = req.to_json();
    REQUIRE(!j.contains("tool"));
}

TEST_CASE("PermissionRequest::from_json", "[core][permission]") {
    nlohmann::json j = {
        {"id",         "req-003"},
        {"permission", "execute"},
        {"patterns",   nlohmann::json::array({"ls", "echo"})},
        {"tool",       "bash"},
        {"metadata",   nlohmann::json::object()}
    };

    const auto req = PermissionRequest::from_json(j);
    REQUIRE(req.id         == "req-003");
    REQUIRE(req.permission == "execute");
    REQUIRE(req.patterns.size() == 2);
    REQUIRE(req.patterns[0] == "ls");
    REQUIRE(req.tool.has_value());
    REQUIRE(*req.tool == "bash");
}

TEST_CASE("PermissionRequest round-trip JSON", "[core][permission]") {
    PermissionRequest original;
    original.id         = "rr-001";
    original.permission = "read";
    original.patterns   = {"/a", "/b"};
    original.tool       = "read_file";

    const auto restored = PermissionRequest::from_json(original.to_json());
    REQUIRE(restored.id         == original.id);
    REQUIRE(restored.permission == original.permission);
    REQUIRE(restored.patterns   == original.patterns);
    REQUIRE(restored.tool       == original.tool);
}

// ============================================================================
// PermissionSystem::wildcard_match
// ============================================================================

TEST_CASE("wildcard_match exact match", "[core][permission]") {
    REQUIRE(PermissionSystem::wildcard_match("/tmp/file.txt", "/tmp/file.txt"));
    REQUIRE_FALSE(PermissionSystem::wildcard_match("/tmp/file.txt", "/tmp/other.txt"));
}

TEST_CASE("wildcard_match star matches everything", "[core][permission]") {
    REQUIRE(PermissionSystem::wildcard_match("*", "anything"));
    REQUIRE(PermissionSystem::wildcard_match("*", ""));
    REQUIRE(PermissionSystem::wildcard_match("*", "/path/to/very/deep/file.txt"));
}

TEST_CASE("wildcard_match prefix star", "[core][permission]") {
    REQUIRE(PermissionSystem::wildcard_match("/tmp/*", "/tmp/file.txt"));
    REQUIRE(PermissionSystem::wildcard_match("/tmp/*", "/tmp/a/b/c"));
    REQUIRE_FALSE(PermissionSystem::wildcard_match("/tmp/*", "/var/file.txt"));
    REQUIRE_FALSE(PermissionSystem::wildcard_match("/tmp/*", "/tmpx/file.txt"));
}

TEST_CASE("wildcard_match suffix star", "[core][permission]") {
    REQUIRE(PermissionSystem::wildcard_match("*.log", "app.log"));
    REQUIRE(PermissionSystem::wildcard_match("*.log", "debug.log"));
    REQUIRE_FALSE(PermissionSystem::wildcard_match("*.log", "app.txt"));
}

TEST_CASE("wildcard_match middle star", "[core][permission]") {
    REQUIRE(PermissionSystem::wildcard_match("/home/*/docs", "/home/user/docs"));
    REQUIRE_FALSE(PermissionSystem::wildcard_match("/home/*/docs", "/home/user/other"));
}

TEST_CASE("wildcard_match question mark", "[core][permission]") {
    REQUIRE(PermissionSystem::wildcard_match("file?.txt", "file1.txt"));
    REQUIRE(PermissionSystem::wildcard_match("file?.txt", "fileA.txt"));
    REQUIRE_FALSE(PermissionSystem::wildcard_match("file?.txt", "file10.txt"));
    REQUIRE_FALSE(PermissionSystem::wildcard_match("file?.txt", "file.txt"));
}

TEST_CASE("wildcard_match multiple stars", "[core][permission]") {
    REQUIRE(PermissionSystem::wildcard_match("*/*.log", "logs/app.log"));
    REQUIRE(PermissionSystem::wildcard_match("**", "any/path/here"));
    REQUIRE(PermissionSystem::wildcard_match("*.*.txt", "a.b.txt"));
}

TEST_CASE("wildcard_match empty strings", "[core][permission]") {
    REQUIRE(PermissionSystem::wildcard_match("", ""));
    REQUIRE_FALSE(PermissionSystem::wildcard_match("", "nonempty"));
    REQUIRE_FALSE(PermissionSystem::wildcard_match("nonempty", ""));
}

TEST_CASE("wildcard_match star matches empty", "[core][permission]") {
    REQUIRE(PermissionSystem::wildcard_match("prefix*", "prefix"));
    REQUIRE(PermissionSystem::wildcard_match("*suffix", "suffix"));
}

TEST_CASE("wildcard_match consecutive stars", "[core][permission]") {
    REQUIRE(PermissionSystem::wildcard_match("**", "hello"));
    REQUIRE(PermissionSystem::wildcard_match("a**b", "ab"));
    REQUIRE(PermissionSystem::wildcard_match("a**b", "axyzb"));
}

// ============================================================================
// PermissionSystem::evaluate
// ============================================================================

TEST_CASE("evaluate empty ruleset returns Deny", "[core][permission]") {
    Ruleset empty;
    REQUIRE(PermissionSystem::evaluate("read", "/tmp/file", empty) == PermissionAction::Deny);
}

TEST_CASE("evaluate simple allow rule", "[core][permission]") {
    Ruleset rs = {{"read", "*", PermissionAction::Allow}};
    REQUIRE(PermissionSystem::evaluate("read", "/tmp/file.txt", rs) == PermissionAction::Allow);
}

TEST_CASE("evaluate simple deny rule", "[core][permission]") {
    Ruleset rs = {{"read", "*", PermissionAction::Deny}};
    REQUIRE(PermissionSystem::evaluate("read", "/tmp/file.txt", rs) == PermissionAction::Deny);
}

TEST_CASE("evaluate ask rule", "[core][permission]") {
    Ruleset rs = {{"write", "*", PermissionAction::Ask}};
    REQUIRE(PermissionSystem::evaluate("write", "/tmp/file.txt", rs) == PermissionAction::Ask);
}

TEST_CASE("evaluate last matching rule wins", "[core][permission]") {
    Ruleset rs = {
        {"read", "*",      PermissionAction::Allow},
        {"read", "/tmp/*", PermissionAction::Deny}
    };
    // /tmp/file.txt matches both rules; last one (Deny) wins
    REQUIRE(PermissionSystem::evaluate("read", "/tmp/file.txt", rs) == PermissionAction::Deny);
    // /var/file.txt only matches first rule (Allow)
    REQUIRE(PermissionSystem::evaluate("read", "/var/file.txt", rs) == PermissionAction::Allow);
}

TEST_CASE("evaluate permission type mismatch", "[core][permission]") {
    Ruleset rs = {{"read", "*", PermissionAction::Allow}};
    // "write" permission doesn't match "read" rule
    REQUIRE(PermissionSystem::evaluate("write", "/tmp/file.txt", rs) == PermissionAction::Deny);
}

TEST_CASE("evaluate pattern mismatch", "[core][permission]") {
    Ruleset rs = {{"read", "/tmp/*", PermissionAction::Allow}};
    REQUIRE(PermissionSystem::evaluate("read", "/var/file.txt", rs) == PermissionAction::Deny);
}

TEST_CASE("evaluate complex ruleset", "[core][permission]") {
    Ruleset rs = {
        {"read",  "*",      PermissionAction::Allow},
        {"write", "/tmp/*", PermissionAction::Allow},
        {"write", "*",      PermissionAction::Ask},
        {"*",     "/secret/*", PermissionAction::Deny}
    };

    REQUIRE(PermissionSystem::evaluate("read",  "/any/file.txt",   rs) == PermissionAction::Allow);
    REQUIRE(PermissionSystem::evaluate("write", "/tmp/file.txt",   rs) == PermissionAction::Ask);   // /tmp/* Allow then * Ask => Ask wins
    REQUIRE(PermissionSystem::evaluate("write", "/var/file.txt",   rs) == PermissionAction::Ask);
    REQUIRE(PermissionSystem::evaluate("read",  "/secret/key.pem", rs) == PermissionAction::Deny);
    REQUIRE(PermissionSystem::evaluate("write", "/secret/key.pem", rs) == PermissionAction::Deny);
}

TEST_CASE("evaluate wildcard permission type", "[core][permission]") {
    Ruleset rs = {{"*", "/secret/*", PermissionAction::Deny}};
    REQUIRE(PermissionSystem::evaluate("read",    "/secret/key", rs) == PermissionAction::Deny);
    REQUIRE(PermissionSystem::evaluate("write",   "/secret/key", rs) == PermissionAction::Deny);
    REQUIRE(PermissionSystem::evaluate("execute", "/secret/key", rs) == PermissionAction::Deny);
}

// ============================================================================
// PermissionSystem::is_allowed / should_ask
// ============================================================================

TEST_CASE("is_allowed returns true for Allow", "[core][permission]") {
    Ruleset rs = {{"read", "*", PermissionAction::Allow}};
    REQUIRE(PermissionSystem::is_allowed("read", "/tmp/file", rs));
    REQUIRE_FALSE(PermissionSystem::is_allowed("write", "/tmp/file", rs));
}

TEST_CASE("should_ask returns true for Ask", "[core][permission]") {
    Ruleset rs = {{"write", "*", PermissionAction::Ask}};
    REQUIRE(PermissionSystem::should_ask("write", "/tmp/file", rs));
    REQUIRE_FALSE(PermissionSystem::should_ask("read", "/tmp/file", rs));
}

// ============================================================================
// PermissionSystem::merge
// ============================================================================

TEST_CASE("merge empty list", "[core][permission]") {
    const auto merged = PermissionSystem::merge({});
    REQUIRE(merged.empty());
}

TEST_CASE("merge single ruleset", "[core][permission]") {
    Ruleset rs = {{"read", "*", PermissionAction::Allow}};
    const auto merged = PermissionSystem::merge({rs});
    REQUIRE(merged.size() == 1);
    REQUIRE(merged[0] == rs[0]);
}

TEST_CASE("merge multiple rulesets preserves order", "[core][permission]") {
    Ruleset rs1 = {
        {"read",  "*",      PermissionAction::Allow},
        {"write", "/tmp/*", PermissionAction::Allow}
    };
    Ruleset rs2 = {
        {"write", "*", PermissionAction::Deny}
    };

    const auto merged = PermissionSystem::merge({rs1, rs2});
    REQUIRE(merged.size() == 3);
    REQUIRE(merged[0].permission == "read");
    REQUIRE(merged[1].permission == "write");
    REQUIRE(merged[1].pattern    == "/tmp/*");
    REQUIRE(merged[2].action     == PermissionAction::Deny);
}

TEST_CASE("merge three rulesets", "[core][permission]") {
    Ruleset r1 = {{"a", "*", PermissionAction::Allow}};
    Ruleset r2 = {{"b", "*", PermissionAction::Deny}};
    Ruleset r3 = {{"c", "*", PermissionAction::Ask}};

    const auto merged = PermissionSystem::merge({r1, r2, r3});
    REQUIRE(merged.size() == 3);
    REQUIRE(merged[0].permission == "a");
    REQUIRE(merged[1].permission == "b");
    REQUIRE(merged[2].permission == "c");
}

TEST_CASE("merge with evaluate - last ruleset overrides", "[core][permission]") {
    Ruleset base_rules = {
        {"read",  "*", PermissionAction::Allow},
        {"write", "*", PermissionAction::Ask}
    };
    Ruleset override_rules = {
        {"write", "/tmp/*", PermissionAction::Allow}
    };

    const auto merged = PermissionSystem::merge({base_rules, override_rules});
    // write /tmp/file.txt: first matches Ask, then matches Allow => Allow wins
    REQUIRE(PermissionSystem::evaluate("write", "/tmp/file.txt", merged) == PermissionAction::Allow);
    // write /var/file: only matches Ask
    REQUIRE(PermissionSystem::evaluate("write", "/var/file.txt", merged) == PermissionAction::Ask);
}
