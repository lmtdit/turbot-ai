#include <catch2/catch_test_macros.hpp>
#include <turbot/utils/file_utils.hpp>
#include <fstream>
#include <filesystem>

using namespace turbot::utils;

TEST_CASE("file::read_file", "[utils][file]") {
    SECTION("read existing file") {
        // Create a temporary test file
        std::string test_file = "test_read_file_temp.txt";
        std::ofstream out(test_file);
        out << "Hello, World!";
        out.close();

        auto content = read_file(test_file);

        REQUIRE(content.has_value());
        REQUIRE(content.value() == "Hello, World!");

        // Clean up
        std::filesystem::remove(test_file);
    }

    SECTION("read non-existing file") {
        auto content = read_file("non_existing_file_12345.txt");

        REQUIRE_FALSE(content.has_value());
    }

    SECTION("read empty file") {
        std::string test_file = "test_empty_file_temp.txt";
        std::ofstream out(test_file);
        out.close();

        auto content = read_file(test_file);

        REQUIRE(content.has_value());
        REQUIRE(content.value().empty());

        // Clean up
        std::filesystem::remove(test_file);
    }

    SECTION("read binary file") {
        std::string test_file = "test_binary_file_temp.bin";
        std::ofstream out(test_file, std::ios::binary);
        const unsigned char data[] = {0x00, 0x01, 0x02, 0xFF, 0xFE};
        out.write(reinterpret_cast<const char*>(data), sizeof(data));
        out.close();

        auto content = read_file(test_file);

        REQUIRE(content.has_value());
        REQUIRE(content.value().size() == 5);

        // Clean up
        std::filesystem::remove(test_file);
    }
}

TEST_CASE("file::write_file", "[utils][file]") {
    SECTION("write new file") {
        std::string test_file = "test_write_file_temp.txt";

        bool success = write_file(test_file, "Test content");

        REQUIRE(success);
        REQUIRE(file_exists(test_file));

        // Verify content
        auto content = read_file(test_file);
        REQUIRE(content.has_value());
        REQUIRE(content.value() == "Test content");

        // Clean up
        std::filesystem::remove(test_file);
    }

    SECTION("overwrite existing file") {
        std::string test_file = "test_overwrite_file_temp.txt";

        // Create initial file
        REQUIRE(write_file(test_file, "Initial content"));

        // Overwrite
        bool success = write_file(test_file, "New content");

        REQUIRE(success);

        auto content = read_file(test_file);
        REQUIRE(content.has_value());
        REQUIRE(content.value() == "New content");

        // Clean up
        std::filesystem::remove(test_file);
    }

    SECTION("write to invalid path") {
        // Try to write to a path that doesn't exist (directory)
        bool success = write_file("/non/existing/directory/file.txt", "content");

        REQUIRE_FALSE(success);
    }
}

TEST_CASE("file::file_exists", "[utils][file]") {
    SECTION("existing file") {
        std::string test_file = "test_exists_file_temp.txt";
        std::ofstream out(test_file);
        out.close();

        REQUIRE(file_exists(test_file) == true);

        // Clean up
        std::filesystem::remove(test_file);
    }

    SECTION("non-existing file") {
        REQUIRE(file_exists("non_existing_file_12345.txt") == false);
    }

    SECTION("existing directory") {
        // Directories also return true for exists check
        REQUIRE(file_exists("/tmp") == true);
    }
}

TEST_CASE("file::get_file_extension", "[utils][file]") {
    SECTION("simple extension") {
        REQUIRE(get_file_extension("file.txt") == "txt");
        REQUIRE(get_file_extension("document.pdf") == "pdf");
        REQUIRE(get_file_extension("image.png") == "png");
    }

    SECTION("multiple dots") {
        REQUIRE(get_file_extension("archive.tar.gz") == "gz");
        REQUIRE(get_file_extension("file.name.txt") == "txt");
    }

    SECTION("no extension") {
        REQUIRE(get_file_extension("filename") == "");
        REQUIRE(get_file_extension("path/to/file") == "");
    }

    SECTION("hidden file (starts with dot)") {
        REQUIRE(get_file_extension(".gitignore") == "gitignore");
        REQUIRE(get_file_extension(".hidden") == "hidden");
    }

    SECTION("empty string") {
        REQUIRE(get_file_extension("") == "");
    }

    SECTION("path with directories") {
        REQUIRE(get_file_extension("/path/to/file.txt") == "txt");
        REQUIRE(get_file_extension("C:\\Users\\file.json") == "json");
    }
}
