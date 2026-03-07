# Turbot-AI 配置分级机制设计文档

## 1. 概述

本文档描述 Turbot-AI 的配置分级机制设计，支持多层级配置管理、动态加载扩展模块，以及统一的配置优先级策略。

## 2. 设计目标

1. **分层管理**：支持内置默认、用户级、项目级三级配置，优先级递增
2. **扩展性**：支持通过目录结构动态加载 agents、skills、rules、events、extensions
3. **可观测性**：统一的日志管理，便于问题排查
4. **兼容性**：与现有 Config 单例系统无缝集成
5. **安全性**：敏感配置支持环境变量覆盖

## 3. 配置层级架构

### 3.1 层级优先级

配置按以下优先级加载（后者覆盖前者）：

```
┌─────────────────────────────────────────────────────────┐
│                    环境变量覆盖                          │
│               TURBOT_* 环境变量（最高优先级）             │
└────────────────────────┬────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────┐
│                  项目级配置                              │
│                ./.turbot/                               │
│         当前工作目录下的项目配置                         │
└────────────────────────┬────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────┐
│                  用户级配置                              │
│                ~/.turbot/                               │
│           用户主目录下的全局配置                         │
└────────────────────────┬────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────┐
│                  内置默认配置                            │
│             compiled-in defaults                        │
│           编译时内置的默认配置（最低优先级）              │
└─────────────────────────────────────────────────────────┘
```

### 3.2 配置路径规范

| 层级     | 路径          | 说明                     |
| -------- | ------------- | ------------------------ |
| 内置默认 | (compiled-in) | 编译时硬编码的默认值     |
| 用户级   | `~/.turbot/`  | 用户主目录下的全局配置   |
| 项目级   | `./.turbot/`  | 当前工作目录下的项目配置 |

### 3.3 路径解析规则

```cpp
// 用户级配置路径
std::string user_config_path() {
    const char* home = std::getenv("HOME");
    if (!home) {
        home = std::getenv("USERPROFILE");  // Windows
    }
    return std::string(home ? home : "") + "/.turbot";
}

// 项目级配置路径
std::string project_config_path() {
    return std::filesystem::current_path().string() + "/.turbot";
}

// 环境变量覆盖
// TURBOT_CONFIG_PATH - 自定义配置根目录
// TURBOT_USER_CONFIG_PATH - 自定义用户配置目录
// TURBOT_PROJECT_CONFIG_PATH - 自定义项目配置目录
```

## 4. 配置目录结构

### 4.1 完整目录结构

```
.turbot/
├── turbot.json              # 主配置文件
├── agents/                  # 自定义 Agent 目录
│   ├── custom_agent_1.json
│   ├── custom_agent_2.json
│   └── ...
├── skills/                  # 自定义技能目录
│   ├── skill_1.json
│   ├── skill_1.lua          # 支持 Lua 脚本
│   └── ...
├── rules/                   # 自定义规则目录
│   ├── rule_1.json
│   ├── rule_2.md            # 支持 Markdown 格式
│   └── ...
├── events/                  # 自定义事件目录
│   ├── schedule.json        # 定时任务配置
│   └── webhook.json         # Webhook 配置
├── extensions/              # 插件目录
│   ├── plugin_1/
│   │   ├── manifest.json    # 插件清单
│   │   ├── main.lua         # 插件入口（Lua）
│   │   └── assets/          # 插件资源
│   └── ...
└── logs/                    # 日志目录
    ├── turbot.log           # 主日志文件
    ├── error.log            # 错误日志
    └── audit.log            # 审计日志
```

### 4.2 主配置文件 (turbot.json)

```json
{
  "version": "1.0",
  "turbot": {
    "debug": false,
    "log_level": "info"
  },
  "providers": [
    {
      "name": "openai",
      "type": "openai",
      "api_key": "${OPENAI_API_KEY}",
      "base_url": "https://api.openai.com/v1",
      "models": ["gpt-4", "gpt-4-turbo", "gpt-3.5-turbo"],
      "default_model": "gpt-4",
      "enabled": true
    },
    {
      "name": "anthropic",
      "type": "anthropic",
      "api_key": "${ANTHROPIC_API_KEY}",
      "models": ["claude-3-opus", "claude-3-sonnet"],
      "default_model": "claude-3-sonnet",
      "enabled": true
    },
    {
      "name": "azure",
      "type": "azure",
      "api_key": "${AZURE_OPENAI_API_KEY}",
      "base_url": "${AZURE_OPENAI_ENDPOINT}",
      "api_version": "2024-02-15-preview",
      "enabled": true
    },
    {
      "name": "ollama",
      "type": "ollama",
      "base_url": "http://localhost:11434",
      "models": ["llama2", "codellama"],
      "enabled": true
    },
    {
      "name": "zhipu",
      "type": "zhipu",
      "api_key": "${ZHIPU_API_KEY}",
      "models": ["glm-4", "glm-4-flash"],
      "enabled": false
    }
  ],
  "permissions": {
    "mode": "ask",
    "rules": [
      {
        "pattern": "read:*",
        "action": "allow"
      },
      {
        "pattern": "write:./**",
        "action": "allow"
      },
      {
        "pattern": "bash:npm install*",
        "action": "allow"
      },
      {
        "pattern": "bash:rm -rf*",
        "action": "deny"
      }
    ]
  },
  "agents": {
    "default": "build",
    "enabled": ["build", "plan", "explore", "custom_*"]
  },
  "session": {
    "max_history": 100,
    "auto_save": true,
    "timeout_seconds": 300
  },
  "tools": {
    "enabled": ["read_file", "write_file", "bash", "search"],
    "disabled": []
  }
}
```

### 4.3 Agent 配置文件 (agents/\*.json)

```json
{
  "name": "code_reviewer",
  "version": "1.0.0",
  "description": "代码审查 Agent，专注于代码质量和最佳实践",
  "system_prompt": "你是一个专业的代码审查助手...",
  "model": "gpt-4",
  "tools": ["read_file", "search"],
  "skills": ["code_review", "security_check"],
  "rules": ["coding_standards", "security_guidelines"],
  "temperature": 0.3,
  "max_tokens": 4096
}
```

### 4.4 Skill 配置文件 (skills/\*.json)

```json
{
  "name": "code_review",
  "version": "1.0.0",
  "description": "代码审查技能",
  "type": "prompt",
  "prompt_template": "请审查以下代码：\n{{code}}\n\n关注点：\n- 代码质量\n- 安全性\n- 性能\n- 可维护性",
  "parameters": {
    "code": {
      "type": "string",
      "required": true,
      "description": "要审查的代码"
    }
  }
}
```

### 4.5 Rule 配置文件 (rules/\*.json)

```json
{
  "name": "coding_standards",
  "version": "1.0.0",
  "description": "编码规范规则",
  "type": "guideline",
  "rules": [
    {
      "id": "CS001",
      "title": "命名规范",
      "content": "使用有意义的变量名和函数名...",
      "severity": "warning"
    },
    {
      "id": "CS002",
      "title": "注释规范",
      "content": "公共 API 必须有文档注释...",
      "severity": "info"
    }
  ]
}
```

### 4.6 Event 配置文件 (events/\*.json)

```json
{
  "name": "scheduled_tasks",
  "version": "1.0.0",
  "events": [
    {
      "id": "daily_report",
      "name": "每日报告",
      "type": "cron",
      "schedule": "0 9 * * *",
      "action": {
        "type": "agent",
        "agent": "reporter",
        "prompt": "生成每日工作报告"
      },
      "enabled": true
    },
    {
      "id": "code_scan",
      "name": "代码扫描",
      "type": "cron",
      "schedule": "0 0 * * 0",
      "action": {
        "type": "script",
        "script": "scripts/scan.lua"
      },
      "enabled": true
    },
    {
      "id": "webhook_handler",
      "name": "Webhook 处理",
      "type": "webhook",
      "path": "/webhook/github",
      "action": {
        "type": "agent",
        "agent": "build",
        "prompt": "处理 GitHub Webhook 事件: {{event_type}}"
      },
      "enabled": true
    }
  ]
}
```

### 4.7 Extension 配置文件 (extensions/\*/manifest.json)

```json
{
  "name": "git-helper",
  "version": "1.0.0",
  "description": "Git 辅助插件",
  "author": "Turbot Team",
  "main": "main.lua",
  "activation_events": ["onCommand:git.commit", "onCommand:git.push"],
  "contributes": {
    "commands": [
      {
        "id": "git.smartCommit",
        "title": "智能提交"
      }
    ],
    "skills": ["git_commit_msg"],
    "agents": ["git_expert"]
  },
  "dependencies": {
    "turbot": ">=1.0.0"
  }
}
```

## 5. 配置加载流程

### 5.1 加载时序

```
┌─────────────────────────────────────────────────────────────────┐
│                     Config::initialize()                        │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 1. 加载内置默认配置                                             │
│    load_default_config()                                        │
│    - 加载编译时预设的默认值                                      │
│    - 设置基本的安全策略                                          │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 2. 加载用户级配置                                               │
│    load_user_config()                                           │
│    - 检查 ~/.turbot/turbot.json                                 │
│    - 加载用户级扩展模块                                          │
│    - 合并到当前配置                                              │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 3. 加载项目级配置                                               │
│    load_project_config()                                        │
│    - 检查 ./.turbot/turbot.json                                 │
│    - 加载项目级扩展模块                                          │
│    - 合并到当前配置                                              │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 4. 加载环境变量覆盖                                             │
│    load_from_env()                                              │
│    - 处理 TURBOT_* 环境变量                                     │
│    - 最高优先级覆盖                                              │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 5. 验证配置                                                     │
│    validate_config()                                            │
│    - 检查必需配置项                                              │
│    - 验证配置值有效性                                            │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│ 6. 初始化扩展模块                                               │
│    init_extensions()                                            │
│    - 动态加载 agents                                            │
│    - 动态加载 skills                                            │
│    - 动态加载 rules                                             │
│    - 动态加载 events                                            │
│    - 动态加载 extensions                                        │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 核心接口设计

```cpp
namespace turbot::core {

class TURBOT_CORE_API ConfigManager {
public:
    /// 配置层级
    enum class ConfigLevel {
        Default,    ///< 内置默认配置
        User,       ///< 用户级配置
        Project     ///< 项目级配置
    };

    /// 扩展模块类型
    enum class ExtensionType {
        Agent,      ///< Agent 扩展
        Skill,      ///< 技能扩展
        Rule,       ///< 规则扩展
        Event,      ///< 事件扩展
        Extension   ///< 插件扩展
    };

    /// 配置加载结果
    struct LoadResult {
        bool success;
        ConfigLevel level;
        std::string path;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    /// 获取单例实例
    static ConfigManager& instance();

    /// 初始化配置系统（加载所有层级配置）
    std::vector<LoadResult> initialize();

    /// 加载指定层级配置
    LoadResult load_config(ConfigLevel level);

    /// 加载扩展模块
    std::vector<LoadResult> load_extensions(ConfigLevel level, ExtensionType type);

    /// 获取配置路径
    std::string get_config_path(ConfigLevel level) const;

    /// 获取扩展目录路径
    std::string get_extension_path(ConfigLevel level, ExtensionType type) const;

    /// 获取日志目录路径
    std::string get_log_path() const;

    /// 重新加载配置
    void reload();

    /// 保存配置到指定层级
    bool save_config(ConfigLevel level);

    /// 配置变更回调
    using ConfigChangeCallback = std::function<void(ConfigLevel, const std::string&, const nlohmann::json&)>;
    void on_config_change(ConfigChangeCallback callback);

private:
    ConfigManager() = default;
    ~ConfigManager() = default;

    /// 内置默认配置
    nlohmann::json get_default_config() const;

    /// 合并配置
    void merge_config(const nlohmann::json& config, ConfigLevel level);

    /// 解析环境变量引用
    std::string resolve_env_vars(const std::string& value) const;

    /// 验证配置
    bool validate_config(const nlohmann::json& config) const;
};

} // namespace turbot::core
```

## 6. 内置默认配置

### 6.1 默认配置定义

```cpp
nlohmann::json ConfigManager::get_default_config() const {
    return {
        {"version", "1.0"},
        {"turbot", {
            {"debug", false},
            {"log_level", "info"},
            {"log_path", "$PROJECT/.turbot/logs"}
        }},
        {"providers", {
            {
                {"name", "openai"},
                {"type", "openai"},
                {"api_key", "${OPENAI_API_KEY}"},
                {"base_url", "https://api.openai.com/v1"},
                {"default_model", "gpt-4"},
                {"enabled", true}
            }
        }},
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
```

### 6.2 安全默认值

| 配置项              | 默认值   | 说明                     |
| ------------------- | -------- | ------------------------ |
| `permissions.mode`  | `"ask"`  | 默认询问用户确认         |
| `permissions.rules` | `[]`     | 无额外规则，由 mode 控制 |
| `turbot.debug`      | `false`  | 生产环境安全             |
| `turbot.log_level`  | `"info"` | 合理的日志级别           |

## 7. 配置合并策略

### 7.1 合并规则

```cpp
nlohmann::json merge_config(const nlohmann::json& base, const nlohmann::json& override) {
    if (!base.is_object() || !override.is_object()) {
        return override;  // 非对象类型直接替换
    }

    nlohmann::json result = base;
    for (auto& [key, value] : override.items()) {
        if (result.contains(key) && result[key].is_object() && value.is_object()) {
            // 递归合并对象
            result[key] = merge_config(result[key], value);
        } else if (key.ends_with("_append") && result.contains(key.substr(0, key.size() - 7))) {
            // 特殊处理数组追加：xxx_append 追加到 xxx 数组
            auto array_key = key.substr(0, key.size() - 7);
            if (result[array_key].is_array() && value.is_array()) {
                for (const auto& item : value) {
                    result[array_key].push_back(item);
                }
            }
        } else {
            // 直接替换
            result[key] = value;
        }
    }
    return result;
}
```

### 7.2 数组合并示例

```json
// 用户级配置
{
  "providers_append": [
    {"name": "custom_provider", "type": "custom", "enabled": true}
  ]
}

// 合并后（追加到默认 providers）
{
  "providers": [
    {"name": "openai", ...},
    {"name": "custom_provider", ...}
  ]
}
```

## 8. 环境变量覆盖

### 8.1 支持的环境变量

| 环境变量                     | 配置路径                            | 说明               |
| ---------------------------- | ----------------------------------- | ------------------ |
| `TURBOT_DEBUG`               | `turbot.debug`                      | 调试模式           |
| `TURBOT_LOG_LEVEL`           | `turbot.log_level`                  | 日志级别           |
| `TURBOT_CONFIG_PATH`         | -                                   | 自定义配置根目录   |
| `TURBOT_USER_CONFIG_PATH`    | -                                   | 用户配置目录       |
| `TURBOT_PROJECT_CONFIG_PATH` | -                                   | 项目配置目录       |
| `OPENAI_API_KEY`             | `providers[name=openai].api_key`    | OpenAI API 密钥    |
| `ANTHROPIC_API_KEY`          | `providers[name=anthropic].api_key` | Anthropic API 密钥 |
| `TURBOT_PERMISSIONS_MODE`    | `permissions.mode`                  | 权限模式           |

### 8.2 配置值中的环境变量引用

```json
{
  "providers": [
    {
      "name": "openai",
      "api_key": "${OPENAI_API_KEY}", // 直接引用
      "base_url": "${OPENAI_BASE_URL:-https://api.openai.com/v1}" // 带默认值
    }
  ]
}
```

## 9. 动态加载机制

### 9.1 扩展模块加载器

```cpp
class ExtensionLoader {
public:
    /// 加载目录下所有扩展模块
    template<typename T>
    std::vector<std::shared_ptr<T>> load_directory(const std::string& path) {
        std::vector<std::shared_ptr<T>> extensions;

        if (!std::filesystem::exists(path)) {
            return extensions;
        }

        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.path().extension() == ".json") {
                try {
                    auto ext = load_from_file<T>(entry.path().string());
                    if (ext) {
                        extensions.push_back(ext);
                    }
                } catch (const std::exception& e) {
                    TURBOT_LOG_WARN("Failed to load extension {}: {}",
                                   entry.path().string(), e.what());
                }
            }
        }

        return extensions;
    }

    /// 热重载支持
    void watch_directory(const std::string& path,
                        std::function<void(const std::string&)> on_change);

    /// 卸载扩展
    void unload(const std::string& extension_id);
};
```

### 9.2 Agent 加载

```cpp
// 加载自定义 Agent
void AgentRegistry::load_custom_agents(const std::string& path) {
    auto agents = ExtensionLoader::instance().load_directory<AgentConfig>(path);
    for (const auto& config : agents) {
        register_agent(config->name, [config]() {
            return std::make_shared<CustomAgent>(*config);
        });
    }
}
```

### 9.3 Skill 加载

```cpp
// 加载自定义技能
void SkillRegistry::load_custom_skills(const std::string& path) {
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        auto ext = entry.path().extension().string();
        if (ext == ".json") {
            // JSON 配置型技能
            auto config = load_skill_config(entry.path().string());
            register_skill(config);
        } else if (ext == ".lua") {
            // Lua 脚本型技能
            auto skill = load_lua_skill(entry.path().string());
            register_skill(skill);
        }
    }
}
```

## 10. 日志系统

### 10.1 日志目录结构

```
.turbot/logs/
├── turbot.log           # 主日志（所有级别）
├── error.log            # 仅错误日志
├── audit.log            # 审计日志（操作记录）
├── performance.log      # 性能日志
└── archives/            # 日志归档
    ├── turbot-2024-01-01.log.gz
    └── ...
```

### 10.2 日志配置

```json
{
  "logging": {
    "level": "info",
    "format": "json",
    "outputs": [
      { "type": "file", "path": "$PROJECT/.turbot/logs/turbot.log" },
      {
        "type": "file",
        "path": "$PROJECT/.turbot/logs/error.log",
        "level": "error"
      },
      { "type": "stdout", "level": "debug", "enabled": false }
    ],
    "rotation": {
      "max_size": "10MB",
      "max_files": 10,
      "compress": true
    }
  }
}
```

## 11. 配置验证

### 11.1 JSON Schema 验证

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "required": ["version"],
  "properties": {
    "version": {
      "type": "string",
      "pattern": "^\\d+\\.\\d+$"
    },
    "providers": {
      "type": "array",
      "items": {
        "type": "object",
        "required": ["name", "type"],
        "properties": {
          "name": { "type": "string" },
          "type": {
            "type": "string",
            "enum": [
              "openai",
              "anthropic",
              "azure",
              "ollama",
              "zhipu",
              "kimi",
              "bailian",
              "iflow",
              "custom"
            ]
          },
          "api_key": { "type": "string" },
          "base_url": { "type": "string", "format": "uri" },
          "enabled": { "type": "boolean" }
        }
      }
    },
    "permissions": {
      "type": "object",
      "properties": {
        "mode": { "type": "string", "enum": ["allow", "deny", "ask"] },
        "rules": {
          "type": "array",
          "items": {
            "type": "object",
            "required": ["pattern", "action"],
            "properties": {
              "pattern": { "type": "string" },
              "action": { "type": "string", "enum": ["allow", "deny", "ask"] }
            }
          }
        }
      }
    }
  }
}
```

### 11.2 运行时验证

```cpp
bool ConfigManager::validate_config(const nlohmann::json& config) const {
    // 检查版本兼容性
    if (!config.contains("version")) {
        TURBOT_LOG_ERROR("Config missing required field: version");
        return false;
    }

    // 验证 provider 配置
    if (config.contains("providers")) {
        for (const auto& provider : config["providers"]) {
            if (!provider.contains("name") || !provider.contains("type")) {
                TURBOT_LOG_ERROR("Provider config missing required fields");
                return false;
            }
        }
    }

    // 验证权限规则
    if (config.contains("permissions")) {
        auto& perms = config["permissions"];
        if (!perms["mode"].is_string() ||
            !std::set{"allow", "deny", "ask"}.contains(perms["mode"])) {
            TURBOT_LOG_ERROR("Invalid permission mode: {}", perms["mode"]);
            return false;
        }
    }

    return true;
}
```

## 12. 错误处理

### 12.1 配置加载错误

```cpp
enum class ConfigError {
    FileNotFound,
    ParseError,
    ValidationError,
    PermissionDenied,
    InvalidPath,
    CircularReference
};

class ConfigException : public std::runtime_error {
public:
    ConfigException(ConfigError error, const std::string& message)
        : std::runtime_error(message), error_(error) {}

    ConfigError error() const { return error_; }

private:
    ConfigError error_;
};
```

### 12.2 错误恢复策略

| 错误类型   | 恢复策略                     |
| ---------- | ---------------------------- |
| 文件不存在 | 跳过该层级，继续加载下一层级 |
| 解析错误   | 记录警告，使用默认值         |
| 验证错误   | 记录错误，拒绝加载           |
| 权限拒绝   | 记录错误，尝试创建默认配置   |

## 13. 配置示例

### 13.1 最小配置

```json
{
  "version": "1.0"
}
```

### 13.2 完整项目配置

```json
{
  "version": "1.0",
  "turbot": {
    "debug": true,
    "log_level": "debug"
  },
  "providers": [
    {
      "name": "openai",
      "type": "openai",
      "api_key": "${OPENAI_API_KEY}",
      "default_model": "gpt-4-turbo",
      "enabled": true
    },
    {
      "name": "local",
      "type": "ollama",
      "base_url": "http://localhost:11434",
      "default_model": "codellama",
      "enabled": true
    }
  ],
  "permissions": {
    "mode": "ask",
    "rules": [
      { "pattern": "read:*", "action": "allow" },
      { "pattern": "write:./src/**", "action": "allow" },
      { "pattern": "write:./**", "action": "ask" },
      { "pattern": "bash:npm*", "action": "allow" },
      { "pattern": "bash:rm*", "action": "deny" }
    ]
  },
  "agents": {
    "default": "custom_builder",
    "enabled": ["build", "plan", "explore", "custom_*"]
  },
  "session": {
    "max_history": 200,
    "auto_save": true,
    "timeout_seconds": 600
  }
}
```

## 14. API 使用示例

### 14.1 初始化配置

```cpp
// 初始化配置系统
auto& config_manager = ConfigManager::instance();
auto results = config_manager.initialize();

for (const auto& result : results) {
    if (!result.success) {
        TURBOT_LOG_WARN("Failed to load config at {}: {}",
                       result.path, result.errors[0]);
    }
}
```

### 14.2 获取配置值

```cpp
auto& config = Config::instance();

// 获取 Provider 配置
auto providers = config.get<std::vector<nlohmann::json>>("providers");

// 获取权限模式
auto perm_mode = config.get_or<std::string>("permissions.mode", "ask");

// 检查配置
if (config.has("providers.0.api_key")) {
    // API key 已配置
}
```

### 14.3 修改配置

```cpp
auto& config = Config::instance();

// 设置配置（运行时，不会持久化）
config.set("permissions.mode", "allow");

// 持久化保存到项目配置
ConfigManager::instance().save_config(ConfigLevel::Project);
```

## 15. 实现计划

### Phase 1: 核心架构（预计 2 天）

1. 实现 `ConfigManager` 类
2. 实现三级配置加载逻辑
3. 实现配置合并策略
4. 添加单元测试

### Phase 2: 扩展加载（预计 3 天）

1. 实现 Agent 动态加载
2. 实现 Skill 动态加载
3. 实现 Rule 动态加载
4. 实现热重载支持
5. 添加集成测试

### Phase 3: 高级功能（预计 2 天）

1. 实现 Event 定时任务
2. 实现 Extension 插件系统
3. 实现日志系统集成
4. 添加完整测试覆盖

### Phase 4: 文档和示例（预计 1 天）

1. 更新用户文档
2. 添加配置示例
3. 添加迁移指南

## 16. 相关文档

- [目录架构规范](./directory-structure.md)
- [权限系统设计](../plans/README.md)
- [Agent 系统设计](../plans/README.md)
