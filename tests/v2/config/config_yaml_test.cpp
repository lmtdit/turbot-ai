/**
 * @file config_yaml_test.cpp
 * @brief YAML parsing tests for ConfigManager
 *
 * Tests for:
 * - parse_yaml_value() - value type detection
 * - parse_simple_yaml() - nested object parsing
 * - parse_yaml_line() - line parsing with indentation
 * - parse_markdown_config() - frontmatter extraction
 */

#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/core/config/config_manager.hpp>
#include <fstream>

using namespace turbot::core;
using namespace turbot::test;

// ==================== Helper Functions ====================

namespace {

// Helper to create a temporary YAML file
void createYamlFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream file(path);
    file << content;
}

// Helper to create a markdown file with frontmatter
void createMarkdownFile(const std::filesystem::path& path, const std::string& frontmatter, const std::string& content) {
    std::ofstream file(path);
    file << "---\n";
    file << frontmatter;
    file << "---\n\n";
    file << content;
}

}

// ==================== parse_yaml_value Tests ====================
// Note: parse_yaml_value is a private function, tested through parse_markdown_config

TEST_CASE("Config.Yaml.Value.Boolean", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto yaml_path = tmp.path() / "test.yaml";
    createYamlFile(yaml_path, R"(
bool_true: true
bool_True: True
bool_TRUE: TRUE
bool_false: false
bool_False: False
bool_FALSE: FALSE
)");

    // Test through markdown config parsing
    auto md_path = tmp.path() / "test.md";
    createMarkdownFile(md_path,
        "bool_true: true\nbool_false: false\n",
        "# Content\n");

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE_FALSE(config.frontmatter.is_null());
    REQUIRE(config.frontmatter["bool_true"] == true);
    REQUIRE(config.frontmatter["bool_false"] == false);
}

TEST_CASE("Config.Yaml.Value.Null", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "null_test.md";
    createMarkdownFile(md_path,
        "null_value: null\ntilde_value: ~\nempty_value:\n",
        "# Content\n");

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE_FALSE(config.frontmatter.is_null());
    REQUIRE(config.frontmatter["null_value"].is_null());
    REQUIRE(config.frontmatter["tilde_value"].is_null());
}

TEST_CASE("Config.Yaml.Value.Number", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "number_test.md";
    createMarkdownFile(md_path,
        "int_pos: 42\nint_neg: -17\nfloat_val: 3.14\nlarge_int: 9999999999\n",
        "# Content\n");

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE_FALSE(config.frontmatter.is_null());
    REQUIRE(config.frontmatter["int_pos"] == 42);
    REQUIRE(config.frontmatter["int_neg"] == -17);
    REQUIRE(config.frontmatter["float_val"] == 3.14);
}

TEST_CASE("Config.Yaml.Value.String", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "string_test.md";
    createMarkdownFile(md_path,
        "plain_string: hello\nquoted_double: \"hello world\"\nquoted_single: 'hello world'\n",
        "# Content\n");

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE_FALSE(config.frontmatter.is_null());
    REQUIRE(config.frontmatter["plain_string"] == "hello");
    REQUIRE(config.frontmatter["quoted_double"] == "hello world");
    REQUIRE(config.frontmatter["quoted_single"] == "hello world");
}

// ==================== parse_simple_yaml Tests ====================

TEST_CASE("Config.Yaml.Simple.FlatObject", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "flat.md";
    createMarkdownFile(md_path,
        "name: test\nvalue: 123\nenabled: true\n",
        "# Content\n");

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE_FALSE(config.frontmatter.is_null());
    REQUIRE(config.frontmatter["name"] == "test");
    REQUIRE(config.frontmatter["value"] == 123);
    REQUIRE(config.frontmatter["enabled"] == true);
}

TEST_CASE("Config.Yaml.Simple.NestedObject", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "nested.md";
    createMarkdownFile(md_path,
        "server:\n  host: localhost\n  port: 8080\ndatabase:\n  name: testdb\n",
        "# Content\n");

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE_FALSE(config.frontmatter.is_null());
    REQUIRE(config.frontmatter["server"]["host"] == "localhost");
    REQUIRE(config.frontmatter["server"]["port"] == 8080);
    REQUIRE(config.frontmatter["database"]["name"] == "testdb");
}

TEST_CASE("Config.Yaml.Simple.DeepNesting", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "deep.md";
    createMarkdownFile(md_path,
        "level1:\n  level2:\n    level3:\n      value: deep\n",
        "# Content\n");

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE_FALSE(config.frontmatter.is_null());
    REQUIRE(config.frontmatter["level1"]["level2"]["level3"]["value"] == "deep");
}

TEST_CASE("Config.Yaml.Simple.MixedTypes", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "mixed.md";
    createMarkdownFile(md_path,
        "string_val: text\nint_val: 42\nbool_val: true\nfloat_val: 3.14\nnull_val: null\n",
        "# Content\n");

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE_FALSE(config.frontmatter.is_null());
    REQUIRE(config.frontmatter["string_val"].is_string());
    REQUIRE(config.frontmatter["int_val"].is_number_integer());
    REQUIRE(config.frontmatter["bool_val"].is_boolean());
    REQUIRE(config.frontmatter["float_val"].is_number_float());
    REQUIRE(config.frontmatter["null_val"].is_null());
}

TEST_CASE("Config.Yaml.Simple.EmptyFrontmatter", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "empty.md";
    createMarkdownFile(md_path, "", "# Content\n");

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    // Empty frontmatter should result in null or empty object
    REQUIRE((config.frontmatter.is_null() || config.frontmatter.empty()));
}

TEST_CASE("Config.Yaml.Simple.NoFrontmatter", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "no_frontmatter.md";
    createYamlFile(md_path, "# Just Content\n\nNo frontmatter here.\n");

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE(config.frontmatter.is_null());
    REQUIRE_FALSE(config.content.empty());
}

// ==================== parse_markdown_config Tests ====================

TEST_CASE("Config.Markdown.FrontmatterExtraction", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "skill.md";
    createMarkdownFile(md_path,
        "name: my-skill\ndescription: A test skill\nversion: \"1.0\"\n",
        "# My Skill\n\nThis is the skill content.\n\n## Usage\n\nInstructions here.\n");

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE_FALSE(config.frontmatter.is_null());
    REQUIRE(config.frontmatter["name"] == "my-skill");
    REQUIRE(config.frontmatter["description"] == "A test skill");
    REQUIRE(config.frontmatter["version"] == "1.0");
    REQUIRE(config.content.find("# My Skill") != std::string::npos);
    REQUIRE(config.content.find("Instructions here") != std::string::npos);
}

TEST_CASE("Config.Markdown.MultilineContent", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "multiline.md";
    std::string frontmatter = "title: Test\n";
    std::string content = "# Heading\n\nParagraph 1\n\nParagraph 2\n\n```\ncode block\n```\n";

    createMarkdownFile(md_path, frontmatter, content);

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE_FALSE(config.frontmatter.is_null());
    REQUIRE(config.frontmatter["title"] == "Test");
    REQUIRE(config.content.find("Paragraph 1") != std::string::npos);
    REQUIRE(config.content.find("code block") != std::string::npos);
}

TEST_CASE("Config.Markdown.SpecialCharacters", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "special.md";
    createMarkdownFile(md_path,
        "path: /usr/local/bin\ndescription: \"A skill with: colons and 'quotes'\"\n",
        "# Content\n");

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE_FALSE(config.frontmatter.is_null());
    REQUIRE(config.frontmatter["path"] == "/usr/local/bin");
}

TEST_CASE("Config.Markdown.NonExistentFile", "[Config][Yaml]") {
    auto config = ConfigManager::instance().parse_markdown_config("/nonexistent/path/file.md");
    REQUIRE(config.frontmatter.is_null());
    REQUIRE(config.content.empty());
}

// ==================== Edge Cases ====================

TEST_CASE("Config.Yaml.Edge.OnlyDashes", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "dashes.md";
    createYamlFile(md_path, "---\n---\n# Content\n");

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    // Should handle gracefully
    REQUIRE((config.frontmatter.is_null() || config.frontmatter.empty()));
}

TEST_CASE("Config.Yaml.Edge.UnixLineEndings", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "unix.md";
    std::string content = "---\nkey: value\n---\n\n# Content\n";
    createYamlFile(md_path, content);

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE_FALSE(config.frontmatter.is_null());
    REQUIRE(config.frontmatter["key"] == "value");
}

TEST_CASE("Config.Yaml.Edge.WindowsLineEndings", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "windows.md";
    std::string content = "---\r\nkey: value\r\n---\r\n\r\n# Content\r\n";
    createYamlFile(md_path, content);

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE_FALSE(config.frontmatter.is_null());
    REQUIRE(config.frontmatter["key"] == "value");
}

TEST_CASE("Config.Yaml.Edge.CommentLines", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "comments.md";
    createMarkdownFile(md_path,
        "# This is a comment\nname: test\n# Another comment\nvalue: 123\n",
        "# Content\n");

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE_FALSE(config.frontmatter.is_null());
    REQUIRE(config.frontmatter["name"] == "test");
    REQUIRE(config.frontmatter["value"] == 123);
}

TEST_CASE("Config.Yaml.Edge.TabIndentation", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "tabs.md";
    std::string content = "---\nserver:\n\tport: 8080\n\thost: localhost\n---\n\n# Content\n";
    createYamlFile(md_path, content);

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE_FALSE(config.frontmatter.is_null());
    REQUIRE(config.frontmatter["server"]["port"] == 8080);
    REQUIRE(config.frontmatter["server"]["host"] == "localhost");
}

TEST_CASE("Config.Yaml.Edge.EmptyValues", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "empty_values.md";
    createMarkdownFile(md_path,
        "empty_string:\nnull_value: null\n",
        "# Content\n");

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE_FALSE(config.frontmatter.is_null());
    // Empty value after colon should be handled
    REQUIRE(config.frontmatter.contains("empty_string"));
    REQUIRE(config.frontmatter["null_value"].is_null());
}

// ==================== Complex YAML Structures ====================

TEST_CASE("Config.Yaml.Complex.MixedNesting", "[Config][Yaml]") {
    TURBOT_TEST_TMPDIR(tmp, false);

    auto md_path = tmp.path() / "complex.md";
    createMarkdownFile(md_path,
        "app:\n  name: myapp\n  version: \"1.0\"\n  database:\n    host: localhost\n    port: 5432\n  features:\n    auth: true\n    cache: false\n",
        "# Content\n");

    auto config = ConfigManager::instance().parse_markdown_config(md_path.string());
    REQUIRE_FALSE(config.frontmatter.is_null());
    REQUIRE(config.frontmatter["app"]["name"] == "myapp");
    REQUIRE(config.frontmatter["app"]["database"]["host"] == "localhost");
    REQUIRE(config.frontmatter["app"]["database"]["port"] == 5432);
    REQUIRE(config.frontmatter["app"]["features"]["auth"] == true);
    REQUIRE(config.frontmatter["app"]["features"]["cache"] == false);
}
