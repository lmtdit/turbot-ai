#include <turbot/utils/file_utils.hpp>
#include <fstream>
#include <filesystem>

namespace turbot::utils {

std::optional<std::string> read_file(std::string_view path) {
    std::ifstream file(std::string(path), std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string content(size, '\0');
    file.read(content.data(), size);

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
