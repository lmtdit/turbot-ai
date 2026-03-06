#include <catch2/catch_test_macros.hpp>
#include <turbot/core/config.hpp>
#include <fstream>
#include <filesystem>

using namespace turbot::core;

TEST_CASE("Config::instance", "[core][config]") {
    SECTION("singleton pattern") {
        auto& config1 = Config::instance();
        auto& config2 = Config::instance();

        REQUIRE(&config1 == &config2);
    }
}

TEST_CASE("Config::get and set", "[core][config]") {
    auto& config = Config::instance();

    SECTION("set and get string") {
        config.set("test.string", "hello");

        auto result = config.get<std::string>("test.string");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == "hello");
    }

    SECTION("set and get int") {
        config.set("test.int", 42);

        auto result = config.get<int>("test.int");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == 42);
    }

    SECTION("set and get bool") {
        config.set("test.bool", true);

        auto result = config.get<bool>("test.bool");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == true);
    }

    SECTION("get non-existing key") {
        auto result = config.get<std::string>("non.existing.key");

        REQUIRE_FALSE(result.has_value());
    }

    SECTION("get_or with default value") {
        config.set("test.value", "actual");

        auto result1 = config.get_or("test.value", std::string("default"));
        REQUIRE(result1 == "actual");

        auto result2 = config.get_or("non.existing", std::string("default"));
        REQUIRE(result2 == "default");
    }
}

TEST_CASE("Config::load_from_string", "[core][config]") {
    auto& config = Config::instance();

    SECTION("load valid JSON") {
        std::string json_str = R"({
            "database": {
                "host": "localhost",
                "port": 5432
            },
            "log": {
                "level": "debug"
            }
        })";

        config.load_from_string(json_str);

        auto host = config.get<std::string>("database.host");
        REQUIRE(host.has_value());
        REQUIRE(host.value() == "localhost");

        auto port = config.get<int>("database.port");
        REQUIRE(port.has_value());
        REQUIRE(port.value() == 5432);

        auto level = config.get<std::string>("log.level");
        REQUIRE(level.has_value());
        REQUIRE(level.value() == "debug");
    }

    SECTION("load invalid JSON") {
        std::string invalid_json = "{ invalid json }";

        // Should throw exception
        REQUIRE_THROWS(config.load_from_string(invalid_json));
    }
}

TEST_CASE("Config::load", "[core][config]") {
    auto& config = Config::instance();

    SECTION("load from file") {
        // Create temporary config file
        std::string temp_file = "test_config_temp.json";
        std::ofstream file(temp_file);
        file << R"({
            "test": {
                "value": "from file"
            }
        })";
        file.close();

        config.load(temp_file);

        auto result = config.get<std::string>("test.value");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == "from file");

        // Clean up
        std::filesystem::remove(temp_file);
    }

    SECTION("load non-existing file") {
        REQUIRE_THROWS(config.load("non_existing_file.json"));
    }
}

TEST_CASE("Config::save", "[core][config]") {
    auto& config = Config::instance();

    SECTION("save to file") {
        std::string temp_file = "test_config_save_temp.json";

        config.set("save.test", "data");
        config.save(temp_file);

        // Check if file exists
        REQUIRE(std::filesystem::exists(temp_file));

        // Load and verify
        Config new_config;
        new_config.load(temp_file);
        auto result = new_config.get<std::string>("save.test");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == "data");

        // Clean up
        std::filesystem::remove(temp_file);
    }
}

TEST_CASE("Config::load_from_env", "[core][config]") {
    auto& config = Config::instance();

    SECTION("load from environment variables") {
        // Set environment variable
        std::string key = "TURBOT_TEST_ENV_VALUE";
        std::string value = "from_env";

#ifdef _WIN32
        _putenv_s(key.c_str(), value.c_str());
#else
        setenv(key.c_str(), value.c_str(), 1);
#endif

        config.load_from_env("TURBOT_");

        auto result = config.get<std::string>("test.env.value");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == value);
    }
}

TEST_CASE("Config::watch", "[core][config]") {
    auto& config = Config::instance();

    SECTION("watch config change") {
        bool callback_called = false;
        std::string new_value;

        auto callback = [&callback_called, &new_value](const std::string& key,
                                                       const nlohmann::json& value) {
            callback_called = true;
            new_value = value.get<std::string>();
        };

        std::string watch_id = config.watch("test.watch", callback);

        // Change the value
        config.set("test.watch", "new value");

        REQUIRE(callback_called);
        REQUIRE(new_value == "new value");

        // Clean up
        config.unwatch("test.watch", watch_id);
    }

    SECTION("unwatch") {
        bool callback_called = false;

        auto callback = [&callback_called](const std::string&, const nlohmann::json&) {
            callback_called = true;
        };

        std::string watch_id = config.watch("test.unwatch", callback);
        config.unwatch("test.unwatch", watch_id);

        // Change the value
        config.set("test.unwatch", "should not trigger");

        REQUIRE_FALSE(callback_called);
    }
}