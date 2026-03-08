#include <turbot/utils/env_utils.hpp>
#include <cstdlib>
#include <regex>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// macOS environ workaround
#if defined(__APPLE__)
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
extern char** environ;
#endif

namespace turbot::utils {

std::string get_env(std::string_view name) {
    // string_view may not be null-terminated; convert to std::string first
    std::string name_str(name);
#ifdef _WIN32
    // Windows 使用 GetEnvironmentVariable
    DWORD size = GetEnvironmentVariableA(name_str.c_str(), nullptr, 0);
    if (size == 0) {
        return "";
    }
    std::string result(size - 1, '\0');
    GetEnvironmentVariableA(name_str.c_str(), result.data(), size);
    return result;
#else
    const char* value = std::getenv(name_str.c_str());
    return value ? std::string(value) : "";
#endif
}

std::string get_env_or(std::string_view name, std::string_view default_value) {
    std::string value = get_env(name);
    return value.empty() ? std::string(default_value) : value;
}

void set_env(std::string_view name, std::string_view value) {
    std::string name_str(name);
    std::string value_str(value);
#ifdef _WIN32
    _putenv_s(name_str.c_str(), value_str.c_str());
#else
    setenv(name_str.c_str(), value_str.c_str(), 1);
#endif
}

void unset_env(std::string_view name) {
    std::string name_str(name);
#ifdef _WIN32
    _putenv_s(name_str.c_str(), "");
#else
    unsetenv(name_str.c_str());
#endif
}

bool has_env(std::string_view name) {
    std::string name_str(name);
#ifdef _WIN32
    DWORD size = GetEnvironmentVariableA(name_str.c_str(), nullptr, 0);
    return size != 0;
#else
    return std::getenv(name_str.c_str()) != nullptr;
#endif
}

std::string env_key_to_config_key(std::string_view env_key, std::string_view prefix) {
    if (env_key.size() <= prefix.size()) {
        return "";
    }

    std::string key(env_key.substr(prefix.size()));
    std::string result;

    for (char c : key) {
        if (c == '_') {
            result += '.';
        } else if (std::isupper(static_cast<unsigned char>(c))) {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else {
            result += c;
        }
    }

    return result;
}

std::string resolve_env_refs(std::string_view value) {
    std::string result(value);

    // 匹配 ${VAR} 或 ${VAR:-default}
    std::regex env_pattern(R"(\$\{([^}:]+)(?::-([^}]*))?\})");
    std::smatch match;

    while (std::regex_search(result, match, env_pattern)) {
        std::string var_name = match[1].str();
        std::string default_value = match[2].matched ? match[2].str() : "";

        std::string env_value = get_env(var_name);
        if (env_value.empty()) {
            env_value = default_value;
        }

        result = result.substr(0, match.position()) + env_value +
                 result.substr(match.position() + match.length());
    }

    return result;
}

} // namespace turbot::utils
