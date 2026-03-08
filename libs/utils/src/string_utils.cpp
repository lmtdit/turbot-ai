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
    // Pre-process: collapse consecutive '*' into a single '*' to prevent ReDoS.
    // "***" converted to ".*.*.*" can trigger catastrophic backtracking on long
    // non-matching strings; collapsing first ensures at most O(n) '.*' groups.
    std::string collapsed;
    collapsed.reserve(pattern.size());
    bool last_star = false;
    for (char c : pattern) {
        if (c == '*') {
            if (!last_star) collapsed += c;
            last_star = true;
        } else {
            collapsed += c;
            last_star = false;
        }
    }

    // 将通配符模式转换为正则表达式
    std::string regex_pattern;
    regex_pattern.reserve(collapsed.size() * 2);

    for (char c : collapsed) {
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
    if (text.empty()) return lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            // No more newlines — emit remaining text (even if empty, only when
            // the original string ended with '\n' and start == text.size())
            if (start < text.size()) {
                lines.emplace_back(text.substr(start));
            }
            break;
        }
        lines.emplace_back(text.substr(start, end - start));
        start = end + 1;
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

    const size_t M = old_lines.size();
    const size_t N = new_lines.size();

    // Guard against O(M*N) memory blow-up for very large files.
    // At 5 K lines each the DP table is ~200 MB; at 50 K it would be ~20 GB.
    // Fall back to a simple delete-all / add-all diff that at least remains correct.
    constexpr size_t MAX_DIFF_LINES = 5000;
    if (M > MAX_DIFF_LINES || N > MAX_DIFF_LINES) {
        for (const auto& line : old_lines) diff << "-" << line << "\n";
        for (const auto& line : new_lines) diff << "+" << line << "\n";
        return diff.str();
    }

    // Build LCS DP table: dp[i][j] = LCS length of old[0..i-1] and new[0..j-1]
    std::vector<std::vector<size_t>> dp(M + 1, std::vector<size_t>(N + 1, 0));
    for (size_t i = 1; i <= M; ++i) {
        for (size_t j = 1; j <= N; ++j) {
            if (old_lines[i - 1] == new_lines[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
            }
        }
    }

    // Traceback to build the edit list (in reverse order)
    std::vector<std::pair<char, const std::string*>> ops;
    ops.reserve(M + N);
    size_t i = M, j = N;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && old_lines[i - 1] == new_lines[j - 1]) {
            ops.push_back({' ', &old_lines[i - 1]});
            --i; --j;
        } else if (j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j])) {
            ops.push_back({'+', &new_lines[j - 1]});
            --j;
        } else {
            ops.push_back({'-', &old_lines[i - 1]});
            --i;
        }
    }

    // Emit in forward order
    std::reverse(ops.begin(), ops.end());
    for (const auto& [op, line_ptr] : ops) {
        diff << op << *line_ptr << "\n";
    }

    return diff.str();
}

} // namespace turbot::utils
