#include <catch2/catch_test_macros.hpp>
#include "../fixture/test_macros.hpp"
#include <turbot/utils/file_utils.hpp>
#include <fstream>

using namespace turbot::utils;
using namespace turbot::test;

// ==================== file_exists 测试 ====================

TEST_CASE("File.Exists_True", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto filepath = tmp.create_file("test.txt", "content");
    REQUIRE(file_exists(filepath.string()));
}

TEST_CASE("File.Exists_False", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto filepath = tmp.path() / "nonexistent.txt";
    REQUIRE_FALSE(file_exists(filepath.string()));
}

TEST_CASE("File.Exists_Directory", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    // 目录也存在
    REQUIRE(file_exists(tmp.path().string()));
}

// ==================== read_file 测试 ====================

TEST_CASE("File.Read_ExistingFile", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto filepath = tmp.create_file("read_test.txt", "Hello, World!");
    
    auto content = read_file(filepath.string());
    REQUIRE(content.has_value());
    REQUIRE(*content == "Hello, World!");
}

TEST_CASE("File.Read_NonexistentFile", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto content = read_file((tmp.path() / "nonexistent.txt").string());
    REQUIRE_FALSE(content.has_value());
}

TEST_CASE("File.Read_EmptyFile", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto filepath = tmp.create_file("empty.txt", "");
    
    auto content = read_file(filepath.string());
    REQUIRE(content.has_value());
    REQUIRE(content->empty());
}

// ==================== write_file 测试 ====================

TEST_CASE("File.Write_NewFile", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto filepath = tmp.path() / "new_file.txt";
    REQUIRE(write_file(filepath.string(), "New content"));
    
    auto content = read_file(filepath.string());
    REQUIRE(content.has_value());
    REQUIRE(*content == "New content");
}

TEST_CASE("File.Write_Overwrite", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto filepath = tmp.create_file("overwrite.txt", "Original");
    
    REQUIRE(write_file(filepath.string(), "Overwritten"));
    
    auto content = read_file(filepath.string());
    REQUIRE(content.has_value());
    REQUIRE(*content == "Overwritten");
}

TEST_CASE("File.Write_Multiline", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto filepath = tmp.path() / "multiline.txt";
    std::string multiline = "Line 1\nLine 2\nLine 3\n";
    
    REQUIRE(write_file(filepath.string(), multiline));
    
    auto content = read_file(filepath.string());
    REQUIRE(content.has_value());
    REQUIRE(*content == multiline);
}

// ==================== get_file_extension 测试 ====================

TEST_CASE("File.Extension_Simple", "[File]") {
    REQUIRE(get_file_extension("test.txt") == "txt");
    REQUIRE(get_file_extension("document.pdf") == "pdf");
    REQUIRE(get_file_extension("image.png") == "png");
}

TEST_CASE("File.Extension_MultipleDots", "[File]") {
    REQUIRE(get_file_extension("archive.tar.gz") == "gz");
    REQUIRE(get_file_extension("config.local.json") == "json");
}

TEST_CASE("File.Extension_NoExtension", "[File]") {
    REQUIRE(get_file_extension("README").empty());
    REQUIRE(get_file_extension("Makefile").empty());
}

TEST_CASE("File.Extension_HiddenFile", "[File]") {
    // Hidden files like .gitignore are treated as having extension "gitignore"
    REQUIRE(get_file_extension(".gitignore") == "gitignore");
    // .env is a hidden file with no extension (just a dot prefix)
    auto env_ext = get_file_extension(".env");
    REQUIRE((env_ext.empty() || env_ext == "env"));
}

TEST_CASE("File.Extension_PathWithDirectories", "[File]") {
    REQUIRE(get_file_extension("/path/to/file.cpp") == "cpp");
    REQUIRE(get_file_extension("src/utils/helper.hpp") == "hpp");
}

// ==================== std::filesystem 集成测试 ====================

TEST_CASE("File.Filesystem_CreateDirectories", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto nested = tmp.path() / "a" / "b" / "c";
    std::filesystem::create_directories(nested);
    
    REQUIRE(std::filesystem::exists(nested));
    REQUIRE(std::filesystem::is_directory(nested));
}

TEST_CASE("File.Filesystem_CopyFile", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto src = tmp.create_file("source.txt", "Source content");
    auto dst = tmp.path() / "destination.txt";
    
    std::filesystem::copy_file(src, dst);
    
    REQUIRE(std::filesystem::exists(dst));
    auto content = read_file(dst.string());
    REQUIRE(content == "Source content");
}

TEST_CASE("File.Filesystem_Remove", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto filepath = tmp.create_file("to_delete.txt", "content");
    REQUIRE(std::filesystem::exists(filepath));
    
    std::filesystem::remove(filepath);
    REQUIRE_FALSE(std::filesystem::exists(filepath));
}

TEST_CASE("File.Filesystem_RemoveAll", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto subdir = tmp.create_subdir("subdir");
    tmp.create_file("subdir/file1.txt", "content1");
    tmp.create_file("subdir/file2.txt", "content2");
    
    REQUIRE(std::filesystem::exists(subdir));
    
    auto count = std::filesystem::remove_all(subdir);
    REQUIRE(count >= 2);
    REQUIRE_FALSE(std::filesystem::exists(subdir));
}

TEST_CASE("File.Filesystem_IterateDirectory", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    tmp.create_file("file1.txt", "a");
    tmp.create_file("file2.txt", "b");
    tmp.create_subdir("subdir");
    
    int file_count = 0;
    int dir_count = 0;
    
    for (const auto& entry : std::filesystem::directory_iterator(tmp.path())) {
        if (entry.is_regular_file()) {
            file_count++;
        } else if (entry.is_directory()) {
            dir_count++;
        }
    }
    
    REQUIRE(file_count == 2);
    REQUIRE(dir_count == 1);
}

TEST_CASE("File.Filesystem_FileSize", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto filepath = tmp.create_file("size_test.txt", "12345");
    
    auto size = std::filesystem::file_size(filepath);
    REQUIRE(size == 5);
}

TEST_CASE("File.Filesystem_LastWriteTime", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto filepath = tmp.create_file("time_test.txt", "content");
    
    auto ftime = std::filesystem::last_write_time(filepath);
    // 验证可以获取修改时间
    REQUIRE(ftime.time_since_epoch().count() > 0);
}

TEST_CASE("File.Filesystem_Canonical", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto canonical = std::filesystem::canonical(tmp.path());
    REQUIRE(canonical.is_absolute());
    REQUIRE(std::filesystem::exists(canonical));
}

TEST_CASE("File.Filesystem_Relative", "[File]") {
    TURBOT_TEST_TMPDIR(tmp, false);
    
    auto subdir = tmp.create_subdir("src");
    auto file = tmp.create_file("src/main.cpp", "int main() {}");
    
    auto relative = std::filesystem::relative(file, tmp.path());
    REQUIRE(relative == "src/main.cpp");
}
