#include <turbot/utils/env_utils.hpp>
#include <cstdlib>
#include <optional>
#include <regex>
#include <cctype>
#include <shared_mutex>

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

// POSIX getenv/setenv/unsetenv are not thread-safe; protect with a shared mutex.
// On Windows, GetEnvironmentVariableA is thread-safe for reads, but _putenv_s
// modifies the CRT _environ array which is not thread-safe under concurrent access.
// A single mutex covers all platforms.
static std::shared_mutex s_env_mutex;

// Atomically check-and-get an environment variable under a single shared lock.
// Returns nullopt if the variable is not set, or the (possibly empty) value if set.
static std::optional<std::string> try_get_env_locked(std::string_view name) {
    std::string name_str(name);
    std::shared_lock<std::shared_mutex> lock(s_env_mutex);
#ifdef _WIN32
    DWORD size = GetEnvironmentVariableA(name_str.c_str(), nullptr, 0);
    if (size == 0) return std::nullopt;
    std::string result(size - 1, '\0');
    GetEnvironmentVariableA(name_str.c_str(), result.data(), size);
    return result;
#else
    const char* val = std::getenv(name_str.c_str());
    return val ? std::optional<std::string>(val) : std::nullopt;
#endif
}

std::string get_env(std::string_view name) {
    // string_view may not be null-terminated; convert to std::string first
    std::string name_str(name);
#ifdef _WIN32
    std::shared_lock<std::shared_mutex> lock(s_env_mutex);
    DWORD size = GetEnvironmentVariableA(name_str.c_str(), nullptr, 0);
    if (size == 0) {
        return "";
    }
    std::string result(size - 1, '\0');
    GetEnvironmentVariableA(name_str.c_str(), result.data(), size);
    return result;
#else
    std::shared_lock<std::shared_mutex> lock(s_env_mutex);
    const char* value = std::getenv(name_str.c_str());
    // Copy the string while the lock is held; getenv returns a pointer into
    // the environment block which may be invalidated by a concurrent setenv.
    return value ? std::string(value) : "";
#endif
}

std::string get_env_or(std::string_view name, std::string_view default_value) {
    // Single lock acquisition to avoid TOCTOU: check and read in one critical section.
    std::string name_str(name);
#ifdef _WIN32
    std::shared_lock<std::shared_mutex> lock(s_env_mutex);
    DWORD size = GetEnvironmentVariableA(name_str.c_str(), nullptr, 0);
    if (size == 0) return std::string(default_value);
    std::string result(size - 1, '\0');
    GetEnvironmentVariableA(name_str.c_str(), result.data(), size);
    return result;
#else
    std::shared_lock<std::shared_mutex> lock(s_env_mutex);
    const char* value = std::getenv(name_str.c_str());
    return value ? std::string(value) : std::string(default_value);
#endif
}

void set_env(std::string_view name, std::string_view value) {
    std::string name_str(name);
    std::string value_str(value);
#ifdef _WIN32
    std::unique_lock<std::shared_mutex> lock(s_env_mutex);
    _putenv_s(name_str.c_str(), value_str.c_str());
#else
    std::unique_lock<std::shared_mutex> lock(s_env_mutex);
    setenv(name_str.c_str(), value_str.c_str(), 1);
#endif
}

void unset_env(std::string_view name) {
    std::string name_str(name);
#ifdef _WIN32
    // _putenv_s("") only sets the variable to empty; use SetEnvironmentVariableA(nullptr) to truly delete
    std::unique_lock<std::shared_mutex> lock(s_env_mutex);
    SetEnvironmentVariableA(name_str.c_str(), nullptr);
#else
    std::unique_lock<std::shared_mutex> lock(s_env_mutex);
    unsetenv(name_str.c_str());
#endif
}

bool has_env(std::string_view name) {
    std::string name_str(name);
#ifdef _WIN32
    std::shared_lock<std::shared_mutex> lock(s_env_mutex);
    DWORD size = GetEnvironmentVariableA(name_str.c_str(), nullptr, 0);
    return size != 0;
#else
    std::shared_lock<std::shared_mutex> lock(s_env_mutex);
    return std::getenv(name_str.c_str()) != nullptr;
#endif
}

std::string env_key_to_config_key(std::string_view env_key, std::string_view prefix) {
    // Validate that env_key actually starts with prefix before stripping it;
    // a missing check would silently produce a mangled config key.
    if (env_key.size() <= prefix.size() || !env_key.starts_with(prefix)) {
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
    static const std::regex env_pattern(R"(\$\{([^}:]+)(?::-([^}]*))?\})");
    std::smatch match;

    // Guard against infinite loops caused by circular variable references
    // (e.g. VAR1=${VAR2} and VAR2=${VAR1}), or variables whose values contain
    // unexpanded ${...} patterns themselves.
    constexpr int MAX_ITERATIONS = 32;
    int iterations = 0;

    while (iterations++ < MAX_ITERATIONS && std::regex_search(result, match, env_pattern)) {
        std::string var_name = match[1].str();
        std::string default_value = match[2].matched ? match[2].str() : "";

        // Single-lock check+read to avoid TOCTOU: has_env + get_env would acquire
        // two separate locks, allowing a concurrent unset_env in between.
        auto env_val = try_get_env_locked(var_name);
        std::string env_value = env_val.has_value() ? *env_val : default_value;

        result = result.substr(0, match.position()) + env_value +
                 result.substr(match.position() + match.length());
    }

    return result;
}

} // namespace turbot::utils
