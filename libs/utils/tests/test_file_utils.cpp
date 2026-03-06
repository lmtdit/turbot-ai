#include <catch2/catch_test_macros.hpp>
#include <turbot/utils/file_utils.hpp>
#include <filesystem>
#include <fstream>

using namespace turbot::utils;

TEST_CASE("File extension extraction works", "[file_utils]") {
    REQUIRE(get_file_extension("test.txt") == "txt");
    REQUIRE(get_file_extension("path/to/file.cpp") == "cpp");
    REQUIRE(get_file_extension("no_extension") == "");
}

TEST_CASE("File write and read works", "[file_utils]") {
    const std::string test_path = "/tmp/turbot_test_file.txt";
    const std::string content = "Hello, Turbot!";

    REQUIRE(write_file(test_path, content));
    auto result = read_file(test_path);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == content);

    std::filesystem::remove(test_path);
}

TEST_CASE("File exists check works", "[file_utils]") {
    const std::string test_path = "/tmp/turbot_exist_test.txt";

    REQUIRE(!file_exists(test_path));
    std::ofstream(test_path) << "test";
    REQUIRE(file_exists(test_path));

    std::filesystem::remove(test_path);
}
