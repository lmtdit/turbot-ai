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

// 行处理工具
/**
 * @brief 按行分割字符串
 * @param text 输入文本
 * @return 行列表
 */
[[nodiscard]] TURBOT_UTILS_API std::vector<std::string> split_lines(const std::string& text);

/**
 * @brief 将行列表合并为字符串
 * @param lines 行列表
 * @return 合并后的字符串
 */
[[nodiscard]] TURBOT_UTILS_API std::string join_lines(const std::vector<std::string>& lines);

/**
 * @brief 规范化行结束符为 \n
 * @param text 输入文本
 * @return 规范化后的文本
 */
[[nodiscard]] TURBOT_UTILS_API std::string normalize_line_endings(const std::string& text);

/**
 * @brief Generate a simple line-level diff between two text versions.
 *
 * Produces unified-diff-style output ("--- path", "+++ path", "-line",
 * "+line", " line"). The algorithm normalizes line endings before comparing.
 *
 * @param file_path  Path label used in the diff header.
 * @param old_content Original file content.
 * @param new_content Modified file content.
 * @return Diff string.
 */
[[nodiscard]] TURBOT_UTILS_API std::string create_diff(
    const std::string& file_path,
    const std::string& old_content,
    const std::string& new_content);

} // namespace turbot::utils
