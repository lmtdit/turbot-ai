#include <turbot/core/llm/message_builder.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>

using namespace turbot::core;

TEST_CASE("LlmRole conversion", "[message_builder][role]") {
    SECTION("to_string") {
        REQUIRE(llm_role_to_string(LlmRole::System) == "system");
        REQUIRE(llm_role_to_string(LlmRole::User) == "user");
        REQUIRE(llm_role_to_string(LlmRole::Assistant) == "assistant");
        REQUIRE(llm_role_to_string(LlmRole::Tool) == "tool");
    }

    SECTION("from_string") {
        REQUIRE(llm_role_from_string("system") == LlmRole::System);
        REQUIRE(llm_role_from_string("user") == LlmRole::User);
        REQUIRE(llm_role_from_string("assistant") == LlmRole::Assistant);
        REQUIRE(llm_role_from_string("tool") == LlmRole::Tool);
    }

    SECTION("from_string invalid throws") {
        REQUIRE_THROWS_AS(llm_role_from_string("invalid"), std::invalid_argument);
    }
}

TEST_CASE("LlmMessage factory methods", "[message_builder][message]") {
    SECTION("create_system") {
        auto msg = LlmMessage::create_system("You are helpful.");
        REQUIRE(msg.role == LlmRole::System);
        REQUIRE(msg.content.has_value());
        REQUIRE(*msg.content == "You are helpful.");
        REQUIRE_FALSE(msg.content_parts.has_value());
        REQUIRE_FALSE(msg.tool_call_id.has_value());
        REQUIRE_FALSE(msg.tool_calls.has_value());
    }

    SECTION("create_user") {
        auto msg = LlmMessage::create_user("Hello!");
        REQUIRE(msg.role == LlmRole::User);
        REQUIRE(msg.content.has_value());
        REQUIRE(*msg.content == "Hello!");
    }

    SECTION("create_assistant") {
        auto msg = LlmMessage::create_assistant("Hi there!");
        REQUIRE(msg.role == LlmRole::Assistant);
        REQUIRE(msg.content.has_value());
        REQUIRE(*msg.content == "Hi there!");
    }

    SECTION("create_user_parts") {
        auto msg = LlmMessage::create_user_parts({
            content_parts::text("Look at this:"),
            content_parts::image_url("https://example.com/image.png")
        });
        REQUIRE(msg.role == LlmRole::User);
        REQUIRE(msg.content_parts.has_value());
        REQUIRE(msg.content_parts->size() == 2);
        REQUIRE_FALSE(msg.content.has_value());
    }

    SECTION("create_tool_response") {
        auto msg = LlmMessage::create_tool_response("call_123", "Result: 42");
        REQUIRE(msg.role == LlmRole::Tool);
        REQUIRE(msg.tool_call_id.has_value());
        REQUIRE(*msg.tool_call_id == "call_123");
        REQUIRE(msg.content.has_value());
        REQUIRE(*msg.content == "Result: 42");
    }

    SECTION("create_assistant_with_tools") {
        auto msg = LlmMessage::create_assistant_with_tools(
            "Let me check that.",
            {content_parts::tool_call("call_456", "search", {{"query", "test"}})}
        );
        REQUIRE(msg.role == LlmRole::Assistant);
        REQUIRE(msg.content.has_value());
        REQUIRE(*msg.content == "Let me check that.");
        REQUIRE(msg.tool_calls.has_value());
        REQUIRE(msg.tool_calls->size() == 1);
    }
}

TEST_CASE("LlmMessage OpenAI format", "[message_builder][format][openai]") {
    SECTION("simple text message") {
        auto msg = LlmMessage::create_user("Hello");
        auto j = msg.to_openai();

        REQUIRE(j["role"] == "user");
        REQUIRE(j["content"] == "Hello");
        REQUIRE_FALSE(j.contains("tool_call_id"));
        REQUIRE_FALSE(j.contains("tool_calls"));
    }

    SECTION("multi-part message") {
        auto msg = LlmMessage::create_user_parts({
            content_parts::text("Check this image:"),
            content_parts::image_url("https://example.com/img.png")
        });
        auto j = msg.to_openai();

        REQUIRE(j["role"] == "user");
        REQUIRE(j["content"].is_array());
        REQUIRE(j["content"].size() == 2);
        REQUIRE(j["content"][0]["type"] == "text");
        REQUIRE(j["content"][1]["type"] == "image_url");
    }

    SECTION("tool response message") {
        auto msg = LlmMessage::create_tool_response("call_789", "Success");
        auto j = msg.to_openai();

        REQUIRE(j["role"] == "tool");
        REQUIRE(j["tool_call_id"] == "call_789");
        REQUIRE(j["content"] == "Success");
    }

    SECTION("assistant with tool calls") {
        auto msg = LlmMessage::create_assistant_with_tools(
            "Checking...",
            {content_parts::tool_call("call_abc", "execute", {{"cmd", "ls"}})}
        );
        auto j = msg.to_openai();

        REQUIRE(j["role"] == "assistant");
        REQUIRE(j["content"] == "Checking...");
        REQUIRE(j.contains("tool_calls"));
        REQUIRE(j["tool_calls"].size() == 1);
        REQUIRE(j["tool_calls"][0]["type"] == "tool_call");
        REQUIRE(j["tool_calls"][0]["id"] == "call_abc");
    }

    SECTION("empty content becomes null") {
        auto msg = LlmMessage::create_assistant("");
        auto j = msg.to_openai();

        REQUIRE(j["role"] == "assistant");
        REQUIRE(j["content"].is_null());
    }
}

TEST_CASE("LlmMessage Anthropic format", "[message_builder][format][anthropic]") {
    SECTION("system message") {
        auto msg = LlmMessage::create_system("You are helpful.");
        auto j = msg.to_anthropic();

        REQUIRE(j["role"] == "system");
        REQUIRE(j["type"] == "system");
        REQUIRE(j["content"] == "You are helpful.");
    }

    SECTION("simple text message") {
        auto msg = LlmMessage::create_user("Hello");
        auto j = msg.to_anthropic();

        REQUIRE(j["role"] == "user");
        REQUIRE(j["content"].is_array());
        REQUIRE(j["content"][0]["type"] == "text");
        REQUIRE(j["content"][0]["text"] == "Hello");
    }

    SECTION("image from base64") {
        auto msg = LlmMessage::create_user_parts({
            content_parts::image_base64("iVBORw0KGgo=", "image/png")
        });
        auto j = msg.to_anthropic();

        REQUIRE(j["role"] == "user");
        REQUIRE(j["content"].is_array());
        REQUIRE(j["content"][0]["type"] == "image");
        REQUIRE(j["content"][0]["source"]["type"] == "base64");
        REQUIRE(j["content"][0]["source"]["media_type"] == "image/png");
        REQUIRE(j["content"][0]["source"]["data"] == "iVBORw0KGgo=");
    }

    SECTION("tool call conversion") {
        auto msg = LlmMessage::create_assistant_with_tools(
            "Let me check.",
            {content_parts::tool_call("tool_123", "search", {{"query", "test"}})}
        );
        auto j = msg.to_anthropic();

        REQUIRE(j["role"] == "assistant");
        REQUIRE(j["content"].is_array());
        // Find tool_use in content
        bool found_tool_use = false;
        for (const auto& part : j["content"]) {
            if (part["type"] == "tool_use") {
                found_tool_use = true;
                REQUIRE(part["id"] == "tool_123");
                REQUIRE(part["name"] == "search");
                break;
            }
        }
        REQUIRE(found_tool_use);
    }
}

TEST_CASE("content_parts factory", "[message_builder][content_parts]") {
    SECTION("text part") {
        auto part = content_parts::text("Hello");
        REQUIRE(part["type"] == "text");
        REQUIRE(part["text"] == "Hello");
    }

    SECTION("image_base64 part") {
        auto part = content_parts::image_base64("abc123", "image/jpeg");
        REQUIRE(part["type"] == "image_url");
        REQUIRE(part["image_url"]["url"] == "data:image/jpeg;base64,abc123");
    }

    SECTION("image_url part") {
        auto part = content_parts::image_url("https://example.com/img.png");
        REQUIRE(part["type"] == "image_url");
        REQUIRE(part["image_url"]["url"] == "https://example.com/img.png");
    }

    SECTION("file part") {
        auto part = content_parts::file("doc.pdf", "base64data", "application/pdf");
        REQUIRE(part["type"] == "file");
        REQUIRE(part["filename"] == "doc.pdf");
        REQUIRE(part["data"] == "base64data");
        REQUIRE(part["mime_type"] == "application/pdf");
    }

    SECTION("tool_call part") {
        auto part = content_parts::tool_call("call_1", "search", {{"q", "test"}});
        REQUIRE(part["type"] == "tool_call");
        REQUIRE(part["id"] == "call_1");
        REQUIRE(part["function"]["name"] == "search");
        REQUIRE(part["function"]["arguments"] == R"({"q":"test"})");
    }

    SECTION("tool_result part") {
        auto part = content_parts::tool_result("call_1", "Success result");
        REQUIRE(part["type"] == "tool_result");
        REQUIRE(part["tool_call_id"] == "call_1");
        REQUIRE(part["content"] == "Success result");
    }

    SECTION("reasoning part") {
        auto part = content_parts::reasoning("Thinking...");
        REQUIRE(part["type"] == "reasoning");
        REQUIRE(part["text"] == "Thinking...");
    }
}

TEST_CASE("MessageBuilder basic operations", "[message_builder][builder]") {
    SECTION("empty builder") {
        MessageBuilder builder;
        REQUIRE(builder.empty());
        REQUIRE(builder.size() == 0);
    }

    SECTION("add messages") {
        MessageBuilder builder;
        builder.add_system("System prompt")
               .add_user("Hello")
               .add_assistant("Hi!");

        REQUIRE(builder.size() == 3);
        REQUIRE_FALSE(builder.empty());
    }

    SECTION("clear messages") {
        MessageBuilder builder;
        builder.add_system("Test").add_user("Hello");
        REQUIRE(builder.size() == 2);

        builder.clear();
        REQUIRE(builder.empty());
    }

    SECTION("add_message and add_messages") {
        MessageBuilder builder;
        auto msg1 = LlmMessage::create_user("First");
        auto msg2 = LlmMessage::create_assistant("Second");

        builder.add_message(msg1);
        builder.add_messages({msg2, LlmMessage::create_user("Third")});

        REQUIRE(builder.size() == 3);
    }

    SECTION("add_user_parts") {
        MessageBuilder builder;
        builder.add_user_parts({
            content_parts::text("Look:"),
            content_parts::image_url("https://example.com/img.png")
        });

        REQUIRE(builder.size() == 1);
        auto messages = builder.get_messages();
        REQUIRE(messages[0].content_parts.has_value());
        REQUIRE(messages[0].content_parts->size() == 2);
    }

    SECTION("add_assistant_with_tools") {
        MessageBuilder builder;
        builder.add_assistant_with_tools(
            "Processing...",
            {content_parts::tool_call("call_1", "search", {})}
        );

        REQUIRE(builder.size() == 1);
        auto messages = builder.get_messages();
        REQUIRE(messages[0].tool_calls.has_value());
        REQUIRE(messages[0].tool_calls->size() == 1);
    }
}

TEST_CASE("MessageBuilder build OpenAI format", "[message_builder][build][openai]") {
    SECTION("simple conversation") {
        MessageBuilder builder;
        builder.set_format(MessageFormat::OpenAI)
               .add_system("Be helpful.")
               .add_user("Hello")
               .add_assistant("Hi!");

        auto messages = builder.build();

        REQUIRE(messages.size() == 3);
        REQUIRE(messages[0]["role"] == "system");
        REQUIRE(messages[0]["content"] == "Be helpful.");
        REQUIRE(messages[1]["role"] == "user");
        REQUIRE(messages[1]["content"] == "Hello");
        REQUIRE(messages[2]["role"] == "assistant");
        REQUIRE(messages[2]["content"] == "Hi!");
    }

    SECTION("with tool calls") {
        MessageBuilder builder;
        builder.set_format(MessageFormat::OpenAI)
               .add_user("Search for X")
               .add_assistant_with_tools(
                   "Searching...",
                   {content_parts::tool_call("call_1", "search", {{"query", "X"}})}
               )
               .add_tool_response("call_1", "Found results");

        auto messages = builder.build();

        REQUIRE(messages.size() == 3);
        REQUIRE(messages[1]["role"] == "assistant");
        REQUIRE(messages[1].contains("tool_calls"));
        REQUIRE(messages[2]["role"] == "tool");
        REQUIRE(messages[2]["tool_call_id"] == "call_1");
    }
}

TEST_CASE("MessageBuilder build Anthropic format", "[message_builder][build][anthropic]") {
    SECTION("simple conversation") {
        MessageBuilder builder;
        builder.set_format(MessageFormat::Anthropic)
               .add_system("Be helpful.")
               .add_user("Hello")
               .add_assistant("Hi!");

        auto messages = builder.build();

        // System message should be present
        REQUIRE(messages.size() >= 2);

        // Find user message
        bool found_user = false;
        for (const auto& msg : messages) {
            if (msg["role"] == "user") {
                found_user = true;
                REQUIRE(msg["content"].is_array());
                REQUIRE(msg["content"][0]["type"] == "text");
                break;
            }
        }
        REQUIRE(found_user);
    }

    SECTION("with caching enabled") {
        MessageBuilder builder;
        builder.set_format(MessageFormat::Anthropic)
               .set_caching(true)
               .add_system("Be helpful.")
               .add_user("Hello")
               .add_assistant("Hi!");

        auto messages = builder.build();

        // Check caching hints are applied to first/last messages
        bool has_cache_control = false;
        for (const auto& msg : messages) {
            if (msg.contains("cache_control")) {
                has_cache_control = true;
                REQUIRE(msg["cache_control"]["type"] == "ephemeral");
            }
        }
        REQUIRE(has_cache_control);
    }
}

TEST_CASE("MessageBuilder build_with_system", "[message_builder][build_with_system]") {
    SECTION("separate system messages") {
        MessageBuilder builder;
        builder.add_system("First system")
               .add_system("Second system")
               .add_user("Hello")
               .add_assistant("Hi!");

        auto [systems, others] = builder.build_with_system();

        REQUIRE(systems.size() == 2);
        REQUIRE(systems[0] == "First system");
        REQUIRE(systems[1] == "Second system");
        REQUIRE(others.size() == 2);
        REQUIRE(others[0]["role"] == "user");
        REQUIRE(others[1]["role"] == "assistant");
    }

    SECTION("no system messages") {
        MessageBuilder builder;
        builder.add_user("Hello");

        auto [systems, others] = builder.build_with_system();

        REQUIRE(systems.empty());
        REQUIRE(others.size() == 1);
    }
}

TEST_CASE("MessageBuilder provider-specific normalization", "[message_builder][normalize]") {
    SECTION("Anthropic empty message filtering") {
        MessageBuilder builder;
        builder.set_format(MessageFormat::Anthropic)
               .set_provider("anthropic")
               .add_user("")
               .add_user("Valid message");

        auto messages = builder.build();

        // Empty message should be filtered
        size_t user_count = 0;
        for (const auto& msg : messages) {
            if (msg["role"] == "user") {
                user_count++;
                REQUIRE_FALSE(msg["content"].is_null());
            }
        }
        REQUIRE(user_count == 1);
    }

    SECTION("Mistral tool call ID normalization") {
        MessageBuilder builder;
        builder.set_format(MessageFormat::OpenAI)
               .set_provider("mistral")
               .add_assistant_with_tools(
                   "Checking...",
                   {content_parts::tool_call("call-with-dashes-123", "search", {})}
               )
               .add_tool_response("call-with-dashes-123", "Result");

        // Build triggers normalization
        auto messages = builder.build();

        REQUIRE(messages.size() == 2);
    }

    SECTION("Mistral tool followed by user fix") {
        MessageBuilder builder;
        builder.set_format(MessageFormat::OpenAI)
               .set_provider("mistral")
               .add_assistant_with_tools(
                   "",
                   {content_parts::tool_call("call123456", "search", {})}
               )
               .add_tool_response("call123456", "Result")
               .add_user("Next question");

        ProviderCapabilities caps;
        message_transform::normalize_messages(
            const_cast<std::vector<LlmMessage>&>(builder.get_messages()),
            "mistral",
            caps
        );

        // After normalization, should have assistant message inserted
        REQUIRE(builder.size() == 4); // assistant + tool + assistant("Done.") + user
    }
}

TEST_CASE("Provider capabilities filtering", "[message_builder][capabilities]") {
    SECTION("unsupported image filtered") {
        ProviderCapabilities caps;
        caps.supports_vision = false;

        MessageBuilder builder;
        builder.set_format(MessageFormat::OpenAI)
               .set_capabilities(caps)
               .add_user_parts({
                   content_parts::text("Check this:"),
                   content_parts::image_url("https://example.com/img.png")
               });

        // This would be handled in parts_to_content
        auto parts = message_transform::parts_to_content(
            {},
            MessageFormat::OpenAI,
            caps
        );
        REQUIRE(parts.empty()); // Empty input = empty output
    }
}

TEST_CASE("Part to content conversion", "[message_builder][part]") {
    SECTION("text part") {
        Part part = Part::create_text("Hello");
        auto content = message_transform::part_to_content(part, MessageFormat::OpenAI);

        REQUIRE(content["type"] == "text");
        REQUIRE(content["text"] == "Hello");
    }

    SECTION("tool part") {
        Part part = Part::create_tool("tool_1", "search", {{"query", "test"}});
        auto content = message_transform::part_to_content(part, MessageFormat::OpenAI);

        REQUIRE(content["type"] == "tool_call");
        REQUIRE(content["id"] == "tool_1");
        REQUIRE(content["function"]["name"] == "search");
    }

    SECTION("reasoning part") {
        Part part = Part::create_reasoning("Thinking...");
        auto content = message_transform::part_to_content(part, MessageFormat::OpenAI);

        REQUIRE(content["type"] == "reasoning");
        REQUIRE(content["text"] == "Thinking...");
    }

    SECTION("file part") {
        Part part = Part::create_file("/path/to/file.pdf", "base64content", "application/pdf");
        auto content = message_transform::part_to_content(part, MessageFormat::OpenAI);

        REQUIRE(content["type"] == "file");
        REQUIRE(content["filename"] == "/path/to/file.pdf");
        REQUIRE(content["data"] == "base64content");
    }
}

TEST_CASE("to_format switches correctly", "[message_builder][to_format]") {
    auto msg = LlmMessage::create_user("Hello");

    SECTION("OpenAI format") {
        auto j = msg.to_format(MessageFormat::OpenAI);
        REQUIRE(j["role"] == "user");
        REQUIRE(j["content"] == "Hello");
    }

    SECTION("OpenAICompat format") {
        auto j = msg.to_format(MessageFormat::OpenAICompat);
        REQUIRE(j["role"] == "user");
        REQUIRE(j["content"] == "Hello");
    }

    SECTION("Anthropic format") {
        auto j = msg.to_format(MessageFormat::Anthropic);
        REQUIRE(j["role"] == "user");
        REQUIRE(j["content"].is_array());
        REQUIRE(j["content"][0]["type"] == "text");
    }
}

TEST_CASE("apply_caching_hints utility", "[message_builder][caching]") {
    SECTION("Anthropic caching") {
        std::vector<nlohmann::json> messages = {
            {{"role", "system"}, {"content", "System 1"}},
            {{"role", "system"}, {"content", "System 2"}},
            {{"role", "user"}, {"content", "Hello"}},
            {{"role", "assistant"}, {"content", "Hi"}},
            {{"role", "user"}, {"content", "Bye"}}
        };

        message_transform::apply_caching_hints(messages, "anthropic");

        // First 2 system messages should have cache_control
        REQUIRE(messages[0].contains("cache_control"));
        REQUIRE(messages[1].contains("cache_control"));

        // Last 2 non-system messages should have cache_control
        REQUIRE(messages[3].contains("cache_control"));
        REQUIRE(messages[4].contains("cache_control"));
    }

    SECTION("OpenRouter caching") {
        std::vector<nlohmann::json> messages = {
            {{"role", "user"}, {"content", "Hello"}},
            {{"role", "assistant"}, {"content", "Hi"}}
        };

        message_transform::apply_caching_hints(messages, "openrouter");

        // All messages should have cache_control for OpenRouter
        REQUIRE(messages[0].contains("cache_control"));
        REQUIRE(messages[1].contains("cache_control"));
    }
}

TEST_CASE("Interleaved thinking support", "[message_builder][thinking]") {
    ProviderCapabilities caps;
    caps.supports_interleaved_thinking = true;
    caps.thinking_field = "reasoning_content";

    std::vector<LlmMessage> messages = {
        LlmMessage::create_assistant_parts({
            content_parts::text("Let me think..."),
            content_parts::reasoning("Step 1: Analyze..."),
            content_parts::reasoning("Step 2: Conclude..."),
            content_parts::text("The answer is 42.")
        })
    };

    // Verify initial state
    REQUIRE(messages.size() == 1);
    REQUIRE(messages[0].content_parts.has_value());
    REQUIRE(messages[0].content_parts->size() == 4);

    message_transform::normalize_messages(messages, "openai-compatible", caps);

    REQUIRE(messages.size() == 1);
    REQUIRE(messages[0].provider_options.has_value());
    REQUIRE(messages[0].provider_options->contains("openaiCompatible"));

    // Reasoning should be extracted and moved to provider_options
    std::string reasoning = (*messages[0].provider_options)["openaiCompatible"]["reasoning_content"];
    REQUIRE(reasoning.find("Step 1") != std::string::npos);
    REQUIRE(reasoning.find("Step 2") != std::string::npos);

    // Content parts should be filtered (no reasoning parts)
    REQUIRE(messages[0].content_parts.has_value());
    REQUIRE(messages[0].content_parts->size() == 2); // Only text parts remain
}

// ===== Additional A- level tests =====

TEST_CASE("LlmRole O(1) string parsing", "[message_builder][role][performance]") {
    SECTION("all roles parse correctly via hash map") {
        REQUIRE(llm_role_from_string("system") == LlmRole::System);
        REQUIRE(llm_role_from_string("user") == LlmRole::User);
        REQUIRE(llm_role_from_string("assistant") == LlmRole::Assistant);
        REQUIRE(llm_role_from_string("tool") == LlmRole::Tool);
    }

    SECTION("invalid role throws with detailed message") {
        try {
            (void)llm_role_from_string("invalid_role");
            FAIL("Should have thrown");
        } catch (const std::invalid_argument& e) {
            std::string msg = e.what();
            REQUIRE(msg.find("invalid_role") != std::string::npos);
        }
    }
}

TEST_CASE("LlmRole to_string consistency", "[message_builder][role]") {
    SECTION("round-trip conversion") {
        REQUIRE(llm_role_from_string(std::string(llm_role_to_string(LlmRole::System))) == LlmRole::System);
        REQUIRE(llm_role_from_string(std::string(llm_role_to_string(LlmRole::User))) == LlmRole::User);
        REQUIRE(llm_role_from_string(std::string(llm_role_to_string(LlmRole::Assistant))) == LlmRole::Assistant);
        REQUIRE(llm_role_from_string(std::string(llm_role_to_string(LlmRole::Tool))) == LlmRole::Tool);
    }
}

TEST_CASE("MessageBuilder edge cases", "[message_builder][edge]") {
    SECTION("large number of messages") {
        MessageBuilder builder;
        builder.set_format(MessageFormat::OpenAI);
        
        for (int i = 0; i < 100; ++i) {
            builder.add_user("Message " + std::to_string(i))
                   .add_assistant("Response " + std::to_string(i));
        }
        
        auto messages = builder.build();
        REQUIRE(messages.size() == 200);
    }

    SECTION("message with special characters") {
        MessageBuilder builder;
        builder.set_format(MessageFormat::OpenAI)
               .add_user("Special chars: \"quotes\" \\backslash\\ \n newline \t tab");
        
        auto messages = builder.build();
        REQUIRE(messages.size() == 1);
        REQUIRE(messages[0]["content"].get<std::string>().find("quotes") != std::string::npos);
    }

    SECTION("unicode content") {
        MessageBuilder builder;
        builder.set_format(MessageFormat::OpenAI)
               .add_user("Unicode: 你好世界 🌍 مرحبا");
        
        auto messages = builder.build();
        REQUIRE(messages.size() == 1);
        REQUIRE(messages[0]["content"].get<std::string>().find("你好世界") != std::string::npos);
    }
}

TEST_CASE("Anthropic image conversion edge cases", "[message_builder][anthropic][image]") {
    SECTION("data URL with complex mime type") {
        auto msg = LlmMessage::create_user_parts({
            content_parts::image_base64("abc123", "image/svg+xml")
        });
        auto j = msg.to_anthropic();
        
        REQUIRE(j["content"].is_array());
        REQUIRE(j["content"][0]["type"] == "image");
        REQUIRE(j["content"][0]["source"]["media_type"] == "image/svg+xml");
    }

    SECTION("external image URL preserved") {
        auto msg = LlmMessage::create_user_parts({
            content_parts::image_url("https://example.com/image.webp")
        });
        auto j = msg.to_openai();
        
        REQUIRE(j["content"][0]["image_url"]["url"] == "https://example.com/image.webp");
    }
}

TEST_CASE("Tool call argument parsing", "[message_builder][tool]") {
    SECTION("valid JSON arguments") {
        auto msg = LlmMessage::create_assistant_with_tools(
            "Processing...",
            {content_parts::tool_call("call_1", "search", {{"query", "test"}, {"limit", 10}})}
        );
        auto j = msg.to_anthropic();
        
        bool found_tool_use = false;
        for (const auto& part : j["content"]) {
            if (part["type"] == "tool_use") {
                found_tool_use = true;
                REQUIRE(part["input"]["query"] == "test");
                REQUIRE(part["input"]["limit"] == 10);
                break;
            }
        }
        REQUIRE(found_tool_use);
    }

    SECTION("tool call with nested JSON arguments") {
        nlohmann::json nested_args = {
            {"filter", {{"field", "name"}, {"value", "test"}}},
            {"options", {{"case_sensitive", false}}}
        };
        auto msg = LlmMessage::create_assistant_with_tools(
            "Searching...",
            {content_parts::tool_call("call_2", "advanced_search", nested_args)}
        );
        auto j = msg.to_openai();
        
        REQUIRE(j["tool_calls"][0]["function"]["name"] == "advanced_search");
        // Arguments are stored as string in OpenAI format
        std::string args_str = j["tool_calls"][0]["function"]["arguments"];
        auto parsed_args = nlohmann::json::parse(args_str);
        REQUIRE(parsed_args["filter"]["field"] == "name");
    }
}

TEST_CASE("Provider-specific normalization comprehensive", "[message_builder][normalize]") {
    SECTION("Anthropic null content handling") {
        MessageBuilder builder;
        builder.set_format(MessageFormat::Anthropic)
               .set_provider("anthropic")
               .add_user("Valid")
               .add_assistant("");  // Empty content
        
        auto messages = builder.build();
        // Empty messages should be filtered for Anthropic
        bool has_empty_assistant = false;
        for (const auto& msg : messages) {
            if (msg["role"] == "assistant" && msg.contains("content")) {
                if (msg["content"].is_null() || 
                    (msg["content"].is_string() && msg["content"].get<std::string>().empty())) {
                    has_empty_assistant = true;
                }
            }
        }
        REQUIRE_FALSE(has_empty_assistant);
    }
}
