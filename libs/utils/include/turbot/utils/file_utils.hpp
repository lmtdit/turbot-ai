#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <turbot/utils/export.hpp>

namespace turbot::utils {

[[nodiscard]] TURBOT_UTILS_API std::optional<std::string> read_file(std::string_view path);
[[nodiscard]] TURBOT_UTILS_API bool write_file(std::string_view path, std::string_view content);
[[nodiscard]] TURBOT_UTILS_API bool file_exists(std::string_view path);
[[nodiscard]] TURBOT_UTILS_API std::string get_file_extension(std::string_view path);

} // namespace turbot::utils
