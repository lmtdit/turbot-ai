#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <turbot/utils/export.hpp>

namespace turbot::utils {

[[nodiscard]] TURBOT_UTILS_API std::string trim(std::string_view str);
[[nodiscard]] TURBOT_UTILS_API std::string to_lower(std::string_view str);
[[nodiscard]] TURBOT_UTILS_API std::string to_upper(std::string_view str);
[[nodiscard]] TURBOT_UTILS_API std::vector<std::string> split(std::string_view str, char delimiter);
[[nodiscard]] TURBOT_UTILS_API bool starts_with(std::string_view str, std::string_view prefix);
[[nodiscard]] TURBOT_UTILS_API bool ends_with(std::string_view str, std::string_view suffix);

} // namespace turbot::utils
