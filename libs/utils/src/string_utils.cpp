#include <turbot/utils/string_utils.hpp>
#include <turbot/utils/crypto_utils.hpp>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <regex>

namespace turbot::utils {

std::string trim(std::string_view str) {
    auto start = str.find_first_not_of(" \t\n\r");
    if (start == std::string_view::npos) {
        return "";
    }
    auto end = str.find_last_not_of(" \t\n\r");
    return std::string(str.substr(start, end - start + 1));
}

std::string trim_left(std::string_view str) {
    auto start = str.find_first_not_of(" \t\n\r");
    if (start == std::string_view::npos) {
        return "";
    }
    return std::string(str.substr(start));
}

std::string trim_right(std::string_view str) {
    auto end = str.find_last_not_of(" \t\n\r");
    if (end == std::string_view::npos) {
        return "";
    }
    return std::string(str.substr(0, end + 1));
}

std::string to_lower(std::string_view str) {
    std::string result(str);
    std::ranges::transform(result, result.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return result;
}

std::string to_upper(std::string_view str) {
    std::string result(str);
    std::ranges::transform(result, result.begin(), [](unsigned char c) {
        return std::toupper(c);
    });
    return result;
}

std::vector<std::string> split(std::string_view str, std::string_view delimiter) {
    std::vector<std::string> result;
    if (delimiter.empty()) {
        result.emplace_back(str);
        return result;
    }
    
    size_t start = 0;
    size_t end = str.find(delimiter);

    while (end != std::string_view::npos) {
        result.emplace_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }

    result.emplace_back(str.substr(start));
    return result;
}

std::vector<std::string> split(std::string_view str, char delimiter) {
    return split(str, std::string_view(&delimiter, 1));
}

bool starts_with(std::string_view str, std::string_view prefix) {
    return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

bool ends_with(std::string_view str, std::string_view suffix) {
    return str.size() >= suffix.size() && str.substr(str.size() - suffix.size()) == suffix;
}

std::string join(const std::vector<std::string>& parts, std::string_view delimiter) {
    if (parts.empty()) {
        return "";
    }
    
    std::string result = parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        result += delimiter;
        result += parts[i];
    }
    return result;
}

std::string join(const std::vector<std::string>& parts, char delimiter) {
    return join(parts, std::string_view(&delimiter, 1));
}

std::string replace_all(std::string str, std::string_view from, std::string_view to) {
    if (from.empty()) {
        return str;
    }
    
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
    return str;
}

std::string generate_uuid() {
    // 使用 crypto::generate_uuid() 确保线程安全和 RFC 4122 兼容
    return crypto::generate_uuid();
}

std::string base64_encode(const std::string& data) {
    // 委托给 crypto_utils 的实现
    return crypto::base64_encode(data);
}

std::string base64_decode(const std::string& encoded) {
    // 委托给 crypto_utils 的实现
    return crypto::base64_decode(encoded);
}

bool wildcard_match(const std::string& pattern, const std::string& text) {
    // 将通配符模式转换为正则表达式
    std::string regex_pattern;
    regex_pattern.reserve(pattern.size() * 2);

    for (char c : pattern) {
        if (c == '*') {
            regex_pattern += ".*";
        } else if (c == '?') {
            regex_pattern += ".";
        } else if (c == '.' || c == '^' || c == '$' || c == '+' || c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || c == '|' || c == '\\') {
            regex_pattern += '\\';
            regex_pattern += c;
        } else {
            regex_pattern += c;
        }
    }

    try {
        std::regex regex(regex_pattern, std::regex_constants::icase);
        return std::regex_match(text, regex);
    } catch (const std::regex_error&) {
        return false;
    }
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::string join_lines(const std::vector<std::string>& lines) {
    std::string result;
    for (size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i < lines.size() - 1) {
            result += '\n';
        }
    }
    return result;
}

std::string normalize_line_endings(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                continue; // Skip \r in \r\n
            }
            result += '\n'; // Convert standalone \r to \n
        } else {
            result += text[i];
        }
    }
    return result;
}

std::string create_diff(
    const std::string& file_path,
    const std::string& old_content,
    const std::string& new_content)
{
    std::ostringstream diff;
    diff << "--- " << file_path << "\n";
    diff << "+++ " << file_path << "\n";

    auto old_lines = split_lines(normalize_line_endings(old_content));
    auto new_lines = split_lines(normalize_line_endings(new_content));

    size_t old_idx = 0;
    size_t new_idx = 0;

    while (old_idx < old_lines.size() || new_idx < new_lines.size()) {
        if (old_idx < old_lines.size() && new_idx < new_lines.size()) {
            if (old_lines[old_idx] == new_lines[new_idx]) {
                diff << " " << old_lines[old_idx] << "\n";
                ++old_idx;
                ++new_idx;
            } else if (old_idx + 1 < old_lines.size() &&
                       old_lines[old_idx + 1] == new_lines[new_idx]) {
                diff << "-" << old_lines[old_idx] << "\n";
                ++old_idx;
            } else if (new_idx + 1 < new_lines.size() &&
                       old_lines[old_idx] == new_lines[new_idx + 1]) {
                diff << "+" << new_lines[new_idx] << "\n";
                ++new_idx;
            } else {
                diff << "-" << old_lines[old_idx] << "\n";
                diff << "+" << new_lines[new_idx] << "\n";
                ++old_idx;
                ++new_idx;
            }
        } else if (old_idx < old_lines.size()) {
            diff << "-" << old_lines[old_idx] << "\n";
            ++old_idx;
        } else {
            diff << "+" << new_lines[new_idx] << "\n";
            ++new_idx;
        }
    }

    return diff.str();
}

} // namespace turbot::utils
