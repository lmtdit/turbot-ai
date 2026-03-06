#include <turbot/utils/string_utils.hpp>
#include <algorithm>
#include <cctype>
#include <random>
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
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    // 8-4-4-4-12格式
    for (int i = 0; i < 16; i++) {
        uint8_t byte = dis(gen);
        oss << std::setw(2) << static_cast<int>(byte);
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            oss << '-';
        }
    }

    return oss.str();
}

std::string base64_encode(const std::string& data) {
    // 这里使用一个简单的实现
    // 注意：完整的base64实现在crypto_utils.cpp中
    constexpr std::string_view chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string result;
    int val = 0;
    int valb = -6;

    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6) {
        result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }

    while (result.size() % 4) {
        result.push_back('=');
    }

    return result;
}

std::string base64_decode(const std::string& encoded) {
    // 这里使用一个简单的实现
    // 注意：完整的base64实现在crypto_utils.cpp中
    constexpr std::string_view chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string result;
    std::vector<int> T(256, -1);

    for (size_t i = 0; i < 64; i++) {
        T[chars[i]] = static_cast<int>(i);
    }

    int val = 0;
    int valb = -8;

    for (unsigned char c : encoded) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            result.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    return result;
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

} // namespace turbot::utils
