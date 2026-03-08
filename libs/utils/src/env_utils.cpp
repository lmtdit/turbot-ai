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
#ifdef _WIN32
    // Windows 使用 GetEnvironmentVariable
    DWORD size = GetEnvironmentVariableA(name.data(), nullptr, 0);
    if (size == 0) {
        return "";
    }
    std::string result(size - 1, '\0');
    GetEnvironmentVariableA(name.data(), result.data(), size);
    return result;
#else
    const char* value = std::getenv(name.data());
    return value ? std::string(value) : "";
#endif
}

std::string get_env_or(std::string_view name, std::string_view default_value) {
    std::string value = get_env(name);
    return value.empty() ? std::string(default_value) : value;
}

void set_env(std::string_view name, std::string_view value) {
#ifdef _WIN32
    _putenv_s(name.data(), value.data());
#else
    setenv(name.data(), value.data(), 1);
#endif
}

void unset_env(std::string_view name) {
#ifdef _WIN32
    _putenv_s(name.data(), "");
#else
    unsetenv(name.data());
#endif
}

bool has_env(std::string_view name) {
#ifdef _WIN32
    DWORD size = GetEnvironmentVariableA(name.data(), nullptr, 0);
    return size != 0;
#else
    return std::getenv(name.data()) != nullptr;
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
