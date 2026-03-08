#include <turbot/core/config/config_manager.hpp>
#include <turbot/utils/string_utils.hpp>
#include <turbot/utils/json_utils.hpp>
#include <turbot/utils/crypto_utils.hpp>
#include <turbot/utils/env_utils.hpp>
#include <fstream>
#include <sstream>
#include <random>
#include <regex>
#include <set>
#include <cstdlib>

// macOS environ workaround
#if defined(__APPLE__)
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
extern char** environ;
#endif

namespace turbot::core {

namespace {

// ========== YAML 解析辅助函数 ==========

/**
 * @brief 解析 YAML 值为 JSON
 */
nlohmann::json parse_yaml_value(const std::string& value) {
    std::string trimmed = value;
    // 去除首尾空白
    size_t start = trimmed.find_first_not_of(" \t");
    size_t end = trimmed.find_last_not_of(" \t");
    if (start != std::string::npos && end != std::string::npos) {
        trimmed = trimmed.substr(start, end - start + 1);
    }

    // 布尔值
    if (trimmed == "true" || trimmed == "True" || trimmed == "TRUE") {
        return true;
    }
    if (trimmed == "false" || trimmed == "False" || trimmed == "FALSE") {
        return false;
    }

    // null
    if (trimmed == "null" || trimmed == "~" || trimmed.empty()) {
        return nullptr;
    }

    // 数字
    try {
        // 尝试解析为整数
        if (trimmed.find('.') == std::string::npos &&
            trimmed.find('e') == std::string::npos &&
            trimmed.find('E') == std::string::npos) {
            if (trimmed[0] == '-') {
                return std::stoll(trimmed);
            } else {
                return std::stoull(trimmed);
            }
        }
        // 尝试解析为浮点数
        return std::stod(trimmed);
    } catch (...) {
        // 不是数字，继续处理为字符串
    }

    // 字符串 - 去除引号
    if ((trimmed.front() == '"' && trimmed.back() == '"') ||
        (trimmed.front() == '\'' && trimmed.back() == '\'')) {
        return trimmed.substr(1, trimmed.size() - 2);
    }

    return trimmed;
}

/**
 * @brief 解析 YAML 行，返回缩进级别和键值对
 */
std::pair<int, std::pair<std::string, std::string>> parse_yaml_line(const std::string& line) {
    int indent = 0;
    size_t i = 0;

    // 计算缩进
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
        if (line[i] == '\t') {
            indent += 2;  // Tab 算作 2 空格
        } else {
            indent++;
        }
        i++;
    }

    // 跳过空行和注释
    if (i >= line.size() || line[i] == '#' || line[i] == '\n') {
        return {indent, {"", ""}};
    }

    // 查找冒号
    size_t colon_pos = line.find(':', i);
    if (colon_pos == std::string::npos) {
        return {indent, {"", ""}};
    }

    std::string key = line.substr(i, colon_pos - i);
    // 去除 key 首尾空白
    size_t key_start = key.find_first_not_of(" \t");
    size_t key_end = key.find_last_not_of(" \t");
    if (key_start != std::string::npos && key_end != std::string::npos) {
        key = key.substr(key_start, key_end - key_start + 1);
    }

    std::string value;
    if (colon_pos + 1 < line.size()) {
        value = line.substr(colon_pos + 1);
    }

    return {indent, {key, value}};
}

/**
 * @brief 解析简单 YAML 为 JSON（支持嵌套对象和数组）
 */
nlohmann::json parse_simple_yaml(const std::string& yaml_content) {
    std::istringstream stream(yaml_content);
    std::string line;
    nlohmann::json result = nlohmann::json::object();
    
    std::vector<std::pair<int, nlohmann::json*>> stack;
    stack.push_back({-1, &result});

    while (std::getline(stream, line)) {
        // 跳过空行
        if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }

        auto [indent, kv] = parse_yaml_line(line);
        const auto& [key, value] = kv;

        if (key.empty()) {
            continue;
        }

        // 弹出缩进大于等于当前行的元素
        while (!stack.empty() && stack.back().first >= indent) {
            stack.pop_back();
        }

        if (stack.empty()) {
            continue;
        }

        nlohmann::json* current = stack.back().second;

        // 解析值
        nlohmann::json json_value;
        if (value.empty() || value.find_first_not_of(" \t") == std::string::npos) {
            // 可能是嵌套对象
            json_value = nlohmann::json::object();
            (*current)[key] = json_value;
            stack.push_back({indent, &(*current)[key]});
        } else if (value.find_first_not_of(" \t") == std::string::npos) {
            json_value = nlohmann::json::object();
            (*current)[key] = json_value;
            stack.push_back({indent, &(*current)[key]});
        } else {
            json_value = parse_yaml_value(value);
            (*current)[key] = json_value;
        }
    }

    return result;
}

// ========== 环境变量辅助函数（已移至 turbot::utils::env_utils） ==========

// 使用 using 引入 utils 命名空间的函数
using turbot::utils::get_env;
using turbot::utils::json::merge;
using turbot::utils::json::split_path;
using turbot::utils::crypto::generate_uuid;
using turbot::utils::resolve_env_refs;

} // namespace

// ========== ConfigManager 实现 ==========

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

std::vector<LoadResult> ConfigManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<LoadResult> results;

    // 1. 加载内置默认配置
    default_config_ = get_default_config();
    merged_config_ = default_config_;
    LoadResult default_result;
    default_result.success = true;
    default_result.level = ConfigLevel::Default;
    default_result.path = "(compiled-in)";
    results.push_back(default_result);

    // 2. 加载用户级配置
    LoadResult user_result = load_config_internal(ConfigLevel::User);
    if (user_result.success) {
        user_config_ = load_json_content(user_result.path);
        merged_config_ = merge_config_with_append(merged_config_, user_config_);
    }
    results.push_back(user_result);

    // 3. 加载项目级配置
    LoadResult project_result = load_config_internal(ConfigLevel::Project);
    if (project_result.success) {
        project_config_ = load_json_content(project_result.path);
        merged_config_ = merge_config_with_append(merged_config_, project_config_);
    }
    results.push_back(project_result);

    // 4. 加载环境变量覆盖
    load_env_overrides();

    initialized_ = true;
    TURBOT_LOG_INFO("Config system initialized with {} levels", results.size());

    return results;
}

LoadResult ConfigManager::load_config(ConfigLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    return load_config_internal(level);
}

LoadResult ConfigManager::load_config_internal(ConfigLevel level) {
    LoadResult result;
    result.level = level;

    std::string config_path = get_config_path(level);
    result.path = config_path;

    if (!std::filesystem::exists(config_path)) {
        result.warnings.push_back("Config file not found: " + config_path);
        TURBOT_LOG_DEBUG("Config file not found for level {}: {}",
                        static_cast<int>(level), config_path);
        return result;
    }

    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            result.errors.push_back("Failed to open config file: " + config_path);
            return result;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        auto config = nlohmann::json::parse(content);

        // 验证配置
        if (!validate_config(config)) {
            result.errors.push_back("Config validation failed: " + config_path);
            return result;
        }

        result.success = true;
        TURBOT_LOG_INFO("Loaded config from: {}", config_path);

    } catch (const nlohmann::json::parse_error& e) {
        result.errors.push_back("JSON parse error: " + std::string(e.what()));
        TURBOT_LOG_ERROR("Failed to parse config file {}: {}", config_path, e.what());
    } catch (const std::exception& e) {
        result.errors.push_back("Error loading config: " + std::string(e.what()));
        TURBOT_LOG_ERROR("Failed to load config file {}: {}", config_path, e.what());
    }

    return result;
}

nlohmann::json ConfigManager::load_json_content(const std::string& file_path) {
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            return nlohmann::json::object();
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        return nlohmann::json::parse(content);
    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("Failed to parse JSON from {}: {}", file_path, e.what());
        return nlohmann::json::object();
    }
}

void ConfigManager::load_env_overrides() {
    // 处理特殊环境变量映射
    static const std::vector<std::pair<std::string, std::string>> env_mappings = {
        {"TURBOT_DEBUG", "turbot.debug"},
        {"TURBOT_LOG_LEVEL", "turbot.log_level"},
        {"TURBOT_PERMISSIONS_MODE", "permissions.mode"},
        {"OPENAI_API_KEY", "providers_by_name.openai.api_key"},
        {"ANTHROPIC_API_KEY", "providers_by_name.anthropic.api_key"},
        {"AZURE_OPENAI_API_KEY", "providers_by_name.azure.api_key"},
        {"AZURE_OPENAI_ENDPOINT", "providers_by_name.azure.base_url"},
        {"DASHSCOPE_API_KEY", "providers_by_name.bailian.api_key"},
        {"ZHIPU_API_KEY", "providers_by_name.zhipu.api_key"},
        {"DEEPSEEK_API_KEY", "providers_by_name.deepseek.api_key"},
        {"MOONSHOT_API_KEY", "providers_by_name.kimi.api_key"},
        {"MINIMAX_API_KEY", "providers_by_name.minimax.api_key"},
        {"MINIMAX_GROUP_ID", "providers_by_name.minimax.group_id"},
    };

    for (const auto& [env_name, config_path] : env_mappings) {
        std::string value = get_env(env_name);
        if (!value.empty()) {
            // 尝试解析为 JSON（支持布尔值和数字）
            try {
                nlohmann::json json_value = nlohmann::json::parse(value);
                set_by_path(merged_config_, config_path, json_value);
            } catch (...) {
                set_by_path(merged_config_, config_path, value);
            }
            TURBOT_LOG_DEBUG("Applied env override: {} -> {}", env_name, config_path);
        }
    }

    // 处理 TURBOT_ 前缀的通用环境变量
    for (size_t i = 0; environ[i] != nullptr; ++i) {
        std::string env_var = environ[i];
        size_t equal_pos = env_var.find('=');
        if (equal_pos == std::string::npos) {
            continue;
        }

        std::string key = env_var.substr(0, equal_pos);
        std::string value = env_var.substr(equal_pos + 1);

        if (key.size() > 7 && key.substr(0, 7) == "TURBOT_") {
            std::string config_key = env_key_to_config_key(key.substr(7));

            // 跳过已经处理过的特殊变量
            bool handled = false;
            for (const auto& [env_name, _] : env_mappings) {
                if (key == env_name) {
                    handled = true;
                    break;
                }
            }
            if (handled) {
                continue;
            }

            // 设置配置值
            try {
                nlohmann::json json_value = nlohmann::json::parse(value);
                set_by_path(merged_config_, config_key, json_value);
            } catch (...) {
                set_by_path(merged_config_, config_key, value);
            }
        }
    }
}

void ConfigManager::set_by_path(nlohmann::json& config, const std::string& path, const nlohmann::json& value) {
    auto parts = split_path(path);
    if (parts.empty()) {
        return;
    }

    nlohmann::json* target = &config;
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        if (!target->contains(parts[i])) {
            (*target)[parts[i]] = nlohmann::json::object();
        }
        target = &(*target)[parts[i]];
    }

    (*target)[parts.back()] = value;
}

std::string ConfigManager::env_key_to_config_key(const std::string& env_key) {
    std::string result;
    for (char c : env_key) {
        if (c == '_') {
            result += '.';
        } else if (std::isupper(c)) {
            result += static_cast<char>(std::tolower(c));
        } else {
            result += c;
        }
    }
    return result;
}

std::vector<LoadResult> ConfigManager::load_extensions(ConfigLevel level, ExtensionType type) {
    std::vector<LoadResult> results;

    std::string ext_path = get_extension_path(level, type);
    if (!std::filesystem::exists(ext_path)) {
        return results;
    }

    for (const auto& entry : std::filesystem::directory_iterator(ext_path)) {
        LoadResult result;
        result.level = level;
        result.path = entry.path().string();

        if (entry.path().extension() == ".json") {
            try {
                std::ifstream file(entry.path());
                std::string content((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
                (void)nlohmann::json::parse(content);  // 验证 JSON 有效性
                result.success = true;
            } catch (const std::exception& e) {
                result.errors.push_back(e.what());
            }
        } else if (entry.path().extension() == ".md") {
            auto md_config = parse_markdown_config(entry.path().string());
            if (!md_config.frontmatter.is_null()) {
                result.success = true;
            } else {
                result.warnings.push_back("No frontmatter found");
            }
        } else if (entry.is_directory()) {
            // 检查目录中是否有 SKILL.md 或 manifest.json
            std::string skill_file = entry.path().string() + "/SKILL.md";
            std::string manifest_file = entry.path().string() + "/manifest.json";

            if (std::filesystem::exists(skill_file)) {
                result.success = true;
                result.path = skill_file;
            } else if (std::filesystem::exists(manifest_file)) {
                result.success = true;
                result.path = manifest_file;
            }
        }

        results.push_back(result);
    }

    return results;
}

void ConfigManager::reload() {
    std::lock_guard<std::mutex> lock(mutex_);

    merged_config_ = default_config_;

    if (!user_config_.is_null()) {
        merged_config_ = merge_config_with_append(merged_config_, user_config_);
    }

    if (!project_config_.is_null()) {
        merged_config_ = merge_config_with_append(merged_config_, project_config_);
    }

    load_env_overrides();

    // 通知变更
    for (const auto& callback : change_callbacks_) {
        try {
            callback(ConfigLevel::Default, "", merged_config_);
        } catch (const std::exception& e) {
            TURBOT_LOG_ERROR("Config change callback error: {}", e.what());
        }
    }

    TURBOT_LOG_INFO("Config reloaded");
}

std::string ConfigManager::get_config_path(ConfigLevel level) const {
    switch (level) {
        case ConfigLevel::Default:
            return "(compiled-in)";
        case ConfigLevel::User: {
            std::string env_path = get_env("TURBOT_USER_CONFIG_PATH");
            if (!env_path.empty()) {
                return env_path + "/turbot.json";
            }
            const char* home = std::getenv("HOME");
            if (!home) {
                home = std::getenv("USERPROFILE");  // Windows
            }
            return std::string(home ? home : "") + "/.turbot/turbot.json";
        }
        case ConfigLevel::Project: {
            std::string env_path = get_env("TURBOT_PROJECT_CONFIG_PATH");
            if (!env_path.empty()) {
                return env_path + "/turbot.json";
            }
            return std::filesystem::current_path().string() + "/.turbot/turbot.json";
        }
        default:
            return "";
    }
}

std::string ConfigManager::get_extension_path(ConfigLevel level, ExtensionType type) const {
    std::string base_path;
    std::string type_dir = extension_type_to_dir(type);

    switch (level) {
        case ConfigLevel::User: {
            std::string env_path = get_env("TURBOT_USER_CONFIG_PATH");
            if (!env_path.empty()) {
                base_path = env_path;
            } else {
                const char* home = std::getenv("HOME");
                if (!home) {
                    home = std::getenv("USERPROFILE");
                }
                base_path = std::string(home ? home : "") + "/.turbot";
            }
            break;
        }
        case ConfigLevel::Project: {
            std::string env_path = get_env("TURBOT_PROJECT_CONFIG_PATH");
            if (!env_path.empty()) {
                base_path = env_path;
            } else {
                base_path = std::filesystem::current_path().string() + "/.turbot";
            }
            break;
        }
        default:
            return "";
    }

    return base_path + "/" + type_dir;
}

std::string ConfigManager::get_log_path() const {
    // 优先使用项目级日志目录
    std::string project_log = std::filesystem::current_path().string() + "/.turbot/logs";
    if (std::filesystem::exists(project_log)) {
        return project_log;
    }

    // 回退到用户级日志目录
    const char* home = std::getenv("HOME");
    if (!home) {
        home = std::getenv("USERPROFILE");
    }
    return std::string(home ? home : "") + "/.turbot/logs";
}

bool ConfigManager::save_config(ConfigLevel level) {
    std::string config_path = get_config_path(level);
    if (config_path.empty() || config_path == "(compiled-in)") {
        return false;
    }

    try {
        // 确保目录存在
        std::filesystem::path p(config_path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        std::ofstream file(config_path);
        if (!file.is_open()) {
            TURBOT_LOG_ERROR("Failed to open config file for writing: {}", config_path);
            return false;
        }

        nlohmann::json config_to_save;
        switch (level) {
            case ConfigLevel::User:
                config_to_save = user_config_;
                break;
            case ConfigLevel::Project:
                config_to_save = project_config_;
                break;
            default:
                return false;
        }

        file << config_to_save.dump(2);
        TURBOT_LOG_INFO("Saved config to: {}", config_path);
        return true;

    } catch (const std::exception& e) {
        TURBOT_LOG_ERROR("Failed to save config: {}", e.what());
        return false;
    }
}

bool ConfigManager::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    nlohmann::json value = merged_config_;
    auto parts = split_path(key);

    for (const auto& part : parts) {
        if (value.contains(part)) {
            value = value[part];
        } else {
            return false;
        }
    }

    return true;
}

nlohmann::json ConfigManager::get_all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return merged_config_;
}

void ConfigManager::on_config_change(ConfigChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    change_callbacks_.push_back(std::move(callback));
}

bool ConfigManager::validate_config(const nlohmann::json& config) const {
    // 检查版本字段（可选）
    if (config.contains("version")) {
        if (!config["version"].is_string()) {
            TURBOT_LOG_ERROR("Config version must be a string");
            return false;
        }
    }

    // 验证 providers 配置
    if (config.contains("providers")) {
        if (!config["providers"].is_array()) {
            TURBOT_LOG_ERROR("Config providers must be an array");
            return false;
        }

        static const std::set<std::string> valid_types = {
            "openai", "anthropic", "azure", "ollama", "zhipu",
            "kimi", "bailian", "iflow", "deepseek", "minimax", "custom"
        };

        for (const auto& provider : config["providers"]) {
            if (!provider.contains("name") || !provider.contains("type")) {
                TURBOT_LOG_ERROR("Provider config missing required fields: name or type");
                return false;
            }

            std::string type = provider["type"].get<std::string>();
            if (valid_types.find(type) == valid_types.end()) {
                TURBOT_LOG_WARN("Unknown provider type: {}", type);
                // 不阻止加载，只是警告
            }
        }
    }

    // 验证 permissions 配置
    if (config.contains("permissions")) {
        const auto& perms = config["permissions"];

        if (perms.contains("mode")) {
            static const std::set<std::string> valid_modes = {"allow", "deny", "ask"};
            std::string mode = perms["mode"].get<std::string>();
            if (valid_modes.find(mode) == valid_modes.end()) {
                TURBOT_LOG_ERROR("Invalid permission mode: {}", mode);
                return false;
            }
        }

        if (perms.contains("rules")) {
            if (!perms["rules"].is_array()) {
                TURBOT_LOG_ERROR("Permission rules must be an array");
                return false;
            }

            for (const auto& rule : perms["rules"]) {
                if (!rule.contains("pattern") || !rule.contains("action")) {
                    TURBOT_LOG_ERROR("Permission rule missing required fields: pattern or action");
                    return false;
                }
            }
        }
    }

    return true;
}

MarkdownConfig ConfigManager::parse_markdown_config(const std::string& file_path) const {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return {nlohmann::json::object(), ""};
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // 查找 YAML Frontmatter (--- 之间的内容)
    size_t start = content.find("---\n");
    if (start == std::string::npos) {
        start = content.find("---\r\n");
    }

    if (start == std::string::npos) {
        return {nlohmann::json::object(), content};
    }

    // 跳过第一个 ---
    start += 4;
    while (start < content.size() && (content[start] == '\n' || content[start] == '\r')) {
        start++;
    }

    // 查找结束的 ---
    size_t end = content.find("\n---", start);
    if (end == std::string::npos) {
        end = content.find("\r\n---", start);
    }

    if (end == std::string::npos) {
        return {nlohmann::json::object(), content};
    }

    std::string yaml_content = content.substr(start, end - start);
    std::string markdown_content = content.substr(end + 4);

    // 去除 markdown_content 开头的空白行
    size_t md_start = markdown_content.find_first_not_of("\r\n");
    if (md_start != std::string::npos) {
        markdown_content = markdown_content.substr(md_start);
    }

    // 解析 YAML 为 JSON
    nlohmann::json frontmatter = parse_yaml(yaml_content);

    return {frontmatter, markdown_content};
}

nlohmann::json ConfigManager::parse_yaml(const std::string& yaml_content) const {
    return parse_simple_yaml(yaml_content);
}

std::string ConfigManager::resolve_env_vars(const std::string& value) const {
    std::string result = value;

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

nlohmann::json ConfigManager::get_default_config() const {
    return {
        {"version", "1.0"},
        {"turbot", {
            {"debug", false},
            {"log_level", "info"}
        }},
        {"providers", nlohmann::json::array()},
        {"permissions", {
            {"mode", "ask"},
            {"rules", nlohmann::json::array()}
        }},
        {"agents", {
            {"default", "build"},
            {"enabled", {"build", "plan", "explore"}}
        }},
        {"session", {
            {"max_history", 100},
            {"auto_save", true},
            {"timeout_seconds", 300}
        }},
        {"tools", {
            {"enabled", {"read_file", "write_file", "bash", "search"}},
            {"disabled", nlohmann::json::array()}
        }}
    };
}

void ConfigManager::merge_config(const nlohmann::json& config, ConfigLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);

    switch (level) {
        case ConfigLevel::User:
            user_config_ = merge_config_with_append(user_config_, config);
            break;
        case ConfigLevel::Project:
            project_config_ = merge_config_with_append(project_config_, config);
            break;
        default:
            break;
    }

    merged_config_ = merge_config_with_append(merged_config_, config);
}

nlohmann::json ConfigManager::merge_config_with_append(const nlohmann::json& base, const nlohmann::json& override) {
    if (!base.is_object() || !override.is_object()) {
        return override;
    }

    nlohmann::json result = base;

    for (auto& [key, value] : override.items()) {
        // 处理 _append 后缀的数组追加
        if (key.size() > 7 && key.substr(key.size() - 7) == "_append") {
            std::string array_key = key.substr(0, key.size() - 7);
            if (result.contains(array_key) && result[array_key].is_array() && value.is_array()) {
                for (const auto& item : value) {
                    result[array_key].push_back(item);
                }
            }
        } else if (result.contains(key) && result[key].is_object() && value.is_object()) {
            // 递归合并对象
            result[key] = merge_config_with_append(result[key], value);
        } else {
            // 直接替换
            result[key] = value;
        }
    }

    return result;
}

std::string ConfigManager::extension_type_to_dir(ExtensionType type) {
    switch (type) {
        case ExtensionType::Agent: return "agents";
        case ExtensionType::Skill: return "skills";
        case ExtensionType::Rule: return "rules";
        case ExtensionType::Event: return "events";
        case ExtensionType::Extension: return "extensions";
        default: return "";
    }
}

} // namespace turbot::core
