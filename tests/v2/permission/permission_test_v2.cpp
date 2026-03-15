#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/permission/permission.hpp>

using namespace turbot::core::permission;
using namespace turbot::test;

// ==================== 权限规则创建辅助函数 ====================

namespace {

Ruleset create_ruleset(std::string_view permission, 
                       const std::vector<std::pair<std::string, PermissionAction>>& rules) {
    Ruleset result;
    for (const auto& [pattern, action] : rules) {
        result.push_back({std::string(permission), pattern, action});
    }
    return result;
}

Ruleset create_task_ruleset(const std::vector<std::pair<std::string, PermissionAction>>& rules) {
    return create_ruleset("task", rules);
}

Ruleset create_bash_ruleset(const std::vector<std::pair<std::string, PermissionAction>>& rules) {
    return create_ruleset("bash", rules);
}

} // anonymous namespace

// ==================== PermissionAction 测试 ====================

TEST_CASE("Permission.Action.ToString", "[Permission]") {
    REQUIRE(permission_action_to_string(PermissionAction::Allow) == "allow");
    REQUIRE(permission_action_to_string(PermissionAction::Deny) == "deny");
    REQUIRE(permission_action_to_string(PermissionAction::Ask) == "ask");
}

TEST_CASE("Permission.Action.FromString", "[Permission]") {
    REQUIRE(permission_action_from_string("allow") == PermissionAction::Allow);
    REQUIRE(permission_action_from_string("deny") == PermissionAction::Deny);
    REQUIRE(permission_action_from_string("ask") == PermissionAction::Ask);
}

// ==================== PermissionRule 测试 ====================

TEST_CASE("Permission.Rule.JsonSerialization", "[Permission]") {
    PermissionRule rule{"bash", "rm -rf *", PermissionAction::Deny};
    
    nlohmann::json j = rule.to_json();
    REQUIRE(j["permission"] == "bash");
    REQUIRE(j["pattern"] == "rm -rf *");
    REQUIRE(j["action"] == "deny");
    
    PermissionRule restored = PermissionRule::from_json(j);
    REQUIRE(restored == rule);
}

// ==================== evaluate 测试 ====================

TEST_CASE("Permission.Evaluate.NoMatch_ReturnsAsk", "[Permission]") {
    // OpenCode: "returns ask when no match (default)"
    auto ruleset = create_task_ruleset({});
    REQUIRE(PermissionSystem::evaluate("task", "code-reviewer", ruleset) == PermissionAction::Ask);
}

TEST_CASE("Permission.Evaluate.ExplicitDeny", "[Permission]") {
    // OpenCode: "returns deny for explicit deny"
    auto ruleset = create_task_ruleset({{"code-reviewer", PermissionAction::Deny}});
    REQUIRE(PermissionSystem::evaluate("task", "code-reviewer", ruleset) == PermissionAction::Deny);
}

TEST_CASE("Permission.Evaluate.ExplicitAllow", "[Permission]") {
    // OpenCode: "returns allow for explicit allow"
    auto ruleset = create_task_ruleset({{"code-reviewer", PermissionAction::Allow}});
    REQUIRE(PermissionSystem::evaluate("task", "code-reviewer", ruleset) == PermissionAction::Allow);
}

TEST_CASE("Permission.Evaluate.ExplicitAsk", "[Permission]") {
    // OpenCode: "returns ask for explicit ask"
    auto ruleset = create_task_ruleset({{"code-reviewer", PermissionAction::Ask}});
    REQUIRE(PermissionSystem::evaluate("task", "code-reviewer", ruleset) == PermissionAction::Ask);
}

TEST_CASE("Permission.Evaluate.WildcardPatternDeny", "[Permission]") {
    // OpenCode: "matches wildcard patterns with deny"
    auto ruleset = create_task_ruleset({{"orchestrator-*", PermissionAction::Deny}});
    REQUIRE(PermissionSystem::evaluate("task", "orchestrator-fast", ruleset) == PermissionAction::Deny);
    REQUIRE(PermissionSystem::evaluate("task", "orchestrator-slow", ruleset) == PermissionAction::Deny);
    REQUIRE(PermissionSystem::evaluate("task", "general", ruleset) == PermissionAction::Ask);
}

TEST_CASE("Permission.Evaluate.WildcardPatternAllow", "[Permission]") {
    // OpenCode: "matches wildcard patterns with allow"
    auto ruleset = create_task_ruleset({{"orchestrator-*", PermissionAction::Allow}});
    REQUIRE(PermissionSystem::evaluate("task", "orchestrator-fast", ruleset) == PermissionAction::Allow);
    REQUIRE(PermissionSystem::evaluate("task", "orchestrator-slow", ruleset) == PermissionAction::Allow);
}

TEST_CASE("Permission.Evaluate.WildcardPatternAsk", "[Permission]") {
    // OpenCode: "matches wildcard patterns with ask"
    auto ruleset = create_task_ruleset({{"orchestrator-*", PermissionAction::Ask}});
    REQUIRE(PermissionSystem::evaluate("task", "orchestrator-fast", ruleset) == PermissionAction::Ask);
    
    auto global_ruleset = create_task_ruleset({{"*", PermissionAction::Ask}});
    REQUIRE(PermissionSystem::evaluate("task", "code-reviewer", global_ruleset) == PermissionAction::Ask);
}

TEST_CASE("Permission.Evaluate.LastRuleWins", "[Permission]") {
    // OpenCode: "later rules take precedence (last match wins)"
    auto ruleset = create_task_ruleset({
        {"orchestrator-*", PermissionAction::Deny},
        {"orchestrator-fast", PermissionAction::Allow}
    });
    REQUIRE(PermissionSystem::evaluate("task", "orchestrator-fast", ruleset) == PermissionAction::Allow);
    REQUIRE(PermissionSystem::evaluate("task", "orchestrator-slow", ruleset) == PermissionAction::Deny);
}

TEST_CASE("Permission.Evaluate.GlobalWildcard", "[Permission]") {
    // OpenCode: "matches global wildcard"
    REQUIRE(PermissionSystem::evaluate("task", "any-agent", 
        create_task_ruleset({{"*", PermissionAction::Allow}})) == PermissionAction::Allow);
    REQUIRE(PermissionSystem::evaluate("task", "any-agent", 
        create_task_ruleset({{"*", PermissionAction::Deny}})) == PermissionAction::Deny);
    REQUIRE(PermissionSystem::evaluate("task", "any-agent", 
        create_task_ruleset({{"*", PermissionAction::Ask}})) == PermissionAction::Ask);
}

// ==================== wildcard_match 测试 ====================

TEST_CASE("Permission.WildcardMatch.ExactMatch", "[Permission]") {
    REQUIRE(PermissionSystem::wildcard_match("code-reviewer", "code-reviewer"));
    REQUIRE_FALSE(PermissionSystem::wildcard_match("code-reviewer", "code-reviewers"));
}

TEST_CASE("Permission.WildcardMatch.StarWildcard", "[Permission]") {
    REQUIRE(PermissionSystem::wildcard_match("orchestrator-*", "orchestrator-fast"));
    REQUIRE(PermissionSystem::wildcard_match("orchestrator-*", "orchestrator-slow"));
    REQUIRE_FALSE(PermissionSystem::wildcard_match("orchestrator-*", "general"));
    REQUIRE(PermissionSystem::wildcard_match("*", "anything"));
}

TEST_CASE("Permission.WildcardMatch.QuestionMark", "[Permission]") {
    REQUIRE(PermissionSystem::wildcard_match("test?", "test1"));
    REQUIRE(PermissionSystem::wildcard_match("test?", "testA"));
    REQUIRE_FALSE(PermissionSystem::wildcard_match("test?", "test12"));
}

TEST_CASE("Permission.WildcardMatch.PathPattern", "[Permission]") {
    REQUIRE(PermissionSystem::wildcard_match("src/*", "src/main.cpp"));
    REQUIRE(PermissionSystem::wildcard_match("src/**/*.cpp", "src/utils/helper.cpp"));
    REQUIRE_FALSE(PermissionSystem::wildcard_match("src/*", "lib/main.cpp"));
}

// ==================== merge 测试 ====================

TEST_CASE("Permission.Merge.SimpleConcatenation", "[Permission]") {
    auto ruleset1 = create_bash_ruleset({{"rm", PermissionAction::Deny}});
    auto ruleset2 = create_task_ruleset({{"general", PermissionAction::Allow}});
    
    auto merged = PermissionSystem::merge({ruleset1, ruleset2});
    REQUIRE(merged.size() == 2);
    REQUIRE(merged[0].permission == "bash");
    REQUIRE(merged[1].permission == "task");
}

TEST_CASE("Permission.Merge.LastWins", "[Permission]") {
    auto ruleset1 = create_task_ruleset({{"general", PermissionAction::Deny}});
    auto ruleset2 = create_task_ruleset({{"general", PermissionAction::Allow}});
    
    auto merged = PermissionSystem::merge({ruleset1, ruleset2});
    REQUIRE(merged.size() == 2);
    // 最后一条规则生效
    REQUIRE(PermissionSystem::evaluate("task", "general", merged) == PermissionAction::Allow);
}

// ==================== is_allowed / should_ask 测试 ====================

TEST_CASE("Permission.IsAllowed.True", "[Permission]") {
    auto ruleset = create_task_ruleset({{"general", PermissionAction::Allow}});
    REQUIRE(PermissionSystem::is_allowed("task", "general", ruleset));
}

TEST_CASE("Permission.IsAllowed.False", "[Permission]") {
    auto ruleset = create_task_ruleset({{"general", PermissionAction::Deny}});
    REQUIRE_FALSE(PermissionSystem::is_allowed("task", "general", ruleset));
}

TEST_CASE("Permission.ShouldAsk.True", "[Permission]") {
    auto ruleset = create_task_ruleset({{"general", PermissionAction::Ask}});
    REQUIRE(PermissionSystem::should_ask("task", "general", ruleset));
}

TEST_CASE("Permission.ShouldAsk.False_WhenAllowed", "[Permission]") {
    auto ruleset = create_task_ruleset({{"general", PermissionAction::Allow}});
    REQUIRE_FALSE(PermissionSystem::should_ask("task", "general", ruleset));
}

// ==================== PermissionReply 测试 ====================

TEST_CASE("Permission.Reply.Once", "[Permission]") {
    auto reply = PermissionReply::once("test message");
    REQUIRE(reply.type == PermissionReply::Type::Once);
    REQUIRE(reply.message == "test message");
}

TEST_CASE("Permission.Reply.Always", "[Permission]") {
    auto reply = PermissionReply::always();
    REQUIRE(reply.type == PermissionReply::Type::Always);
    REQUIRE_FALSE(reply.message.has_value());
}

TEST_CASE("Permission.Reply.Reject", "[Permission]") {
    auto reply = PermissionReply::reject("denied");
    REQUIRE(reply.type == PermissionReply::Type::Reject);
    REQUIRE(reply.message == "denied");
}

TEST_CASE("Permission.Reply.JsonSerialization", "[Permission]") {
    auto reply = PermissionReply::once("test");
    nlohmann::json j = reply.to_json();
    REQUIRE(j["type"] == "once");
    REQUIRE(j["message"] == "test");
    
    auto restored = PermissionReply::from_json(j);
    REQUIRE(restored.type == reply.type);
    REQUIRE(restored.message == reply.message);
}

// ==================== PermissionRequest 测试 ====================

TEST_CASE("Permission.Request.JsonSerialization", "[Permission]") {
    PermissionRequest req{
        "req-123",
        "bash",
        {"rm -rf /"},
        "shell",
        {{"cwd", "/home/user"}}
    };
    
    nlohmann::json j = req.to_json();
    REQUIRE(j["id"] == "req-123");
    REQUIRE(j["permission"] == "bash");
    REQUIRE(j["patterns"].size() == 1);
    REQUIRE(j["tool"] == "shell");
    
    auto restored = PermissionRequest::from_json(j);
    REQUIRE(restored.id == req.id);
    REQUIRE(restored.permission == req.permission);
    REQUIRE(restored.patterns == req.patterns);
}
