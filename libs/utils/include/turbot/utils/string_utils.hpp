#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <turbot/utils/export.hpp>

namespace turbot::utils {

[[nodiscard]] TURBOT_UTILS_API std::string trim(std::string_view str);
[[nodiscard]] TURBOT_UTILS_API std::string trim_left(std::string_view str);
[[nodiscard]] TURBOT_UTILS_API std::string trim_right(std::string_view str);
[[nodiscard]] TURBOT_UTILS_API std::string to_lower(std::string_view str);
[[nodiscard]] TURBOT_UTILS_API std::string to_upper(std::string_view str);
[[nodiscard]] TURBOT_UTILS_API std::vector<std::string> split(std::string_view str, std::string_view delimiter);
[[nodiscard]] TURBOT_UTILS_API std::vector<std::string> split(std::string_view str, char delimiter);
[[nodiscard]] TURBOT_UTILS_API std::string join(const std::vector<std::string>& parts, std::string_view delimiter);
[[nodiscard]] TURBOT_UTILS_API std::string join(const std::vector<std::string>& parts, char delimiter);
[[nodiscard]] TURBOT_UTILS_API std::string replace_all(std::string str, std::string_view from, std::string_view to);
[[nodiscard]] TURBOT_UTILS_API bool starts_with(std::string_view str, std::string_view prefix);
[[nodiscard]] TURBOT_UTILS_API bool ends_with(std::string_view str, std::string_view suffix);

// UUID生成
[[nodiscard]] TURBOT_UTILS_API std::string generate_uuid();

// Base64编解码
[[nodiscard]] TURBOT_UTILS_API std::string base64_encode(const std::string& data);
[[nodiscard]] TURBOT_UTILS_API std::string base64_decode(const std::string& encoded);

// 通配符匹配
[[nodiscard]] TURBOT_UTILS_API bool wildcard_match(const std::string& pattern, const std::string& text);

} // namespace turbot::utils
