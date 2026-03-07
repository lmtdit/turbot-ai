#include <turbot/utils/file_utils.hpp>
#include <fstream>
#include <filesystem>
#include <stdexcept>

namespace turbot::utils {

// 最大允许的文件大小 (100MB)
constexpr size_t MAX_FILE_SIZE = 100 * 1024 * 1024;

std::optional<std::string> read_file(std::string_view path) {
    std::filesystem::path fs_path(path);
    
    // 检查文件是否存在
    if (!std::filesystem::exists(fs_path)) {
        return std::nullopt;
    }
    
    // 检查文件大小，避免内存溢出
    std::error_code ec;
    auto file_size = std::filesystem::file_size(fs_path, ec);
    if (ec || file_size == static_cast<std::uintmax_t>(-1)) {
        return std::nullopt;
    }
    
    // 检查文件大小限制
    if (file_size > MAX_FILE_SIZE) {
        throw std::runtime_error("File too large: " + std::string(path) + 
                                 " (size: " + std::to_string(file_size) + 
                                 ", max: " + std::to_string(MAX_FILE_SIZE) + ")");
    }

    std::ifstream file(std::string(path), std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    // 使用已获取的文件大小
    std::string content(static_cast<size_t>(file_size), '\0');
    file.read(content.data(), file_size);

    if (!file) {
        return std::nullopt;  // 读取失败
    }

    return content;
}

bool write_file(std::string_view path, std::string_view content) {
    std::ofstream file(std::string(path), std::ios::binary);
    if (!file) {
        return false;
    }

    file.write(content.data(), content.size());
    return file.good();
}

bool file_exists(std::string_view path) {
    return std::filesystem::exists(std::string(path));
}

std::string get_file_extension(std::string_view path) {
    auto dot_pos = path.find_last_of('.');
    if (dot_pos == std::string_view::npos) {
        return "";
    }
    return std::string(path.substr(dot_pos + 1));
}

} // namespace turbot::utils
