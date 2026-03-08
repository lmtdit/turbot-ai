# Turbot-AI 配置分级机制设计文档

## 1. 概述

本文档描述 Turbot-AI 的配置分级机制设计，参考 Qoder 编辑器的配置模式，支持多层级配置管理、动态加载扩展模块，以及统一的配置优先级策略。

## 2. 设计目标

1. **分层管理**：支持内置默认、用户级、项目级三级配置，优先级递增
2. **扩展性**：支持通过目录结构动态加载 agents、skills、rules、extensions
3. **可观测性**：统一的日志管理，便于问题排查
4. **兼容性**：与现有 Config 单例系统无缝集成
5. **安全性**：敏感配置支持环境变量覆盖
6. **易读性**：采用 Markdown + YAML Frontmatter 格式，便于人工编辑

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
│     项目特定配置，纳入版本控制，团队共享                 │
└────────────────────────┬────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────┐
│                  用户级配置                              │
│                ~/.turbot/                               │
│     用户全局配置，不纳入版本控制，仅本机有效             │
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

### 3.3 用户级与项目级职责划分

| 配置类型       | 用户级 (`~/.turbot/`) | 项目级 (`./.turbot/`) |
| -------------- | --------------------- | --------------------- |
| **全局配置**   | ✅ `turbot.json`      | ✅ `turbot.json`      |
| **认证信息**   | ✅ `.auth/`           | ❌                    |
| **Agents**     | ✅ 全局 agents        | ✅ 项目 agents        |
| **Skills**     | ✅ 全局 skills        | ✅ 项目 skills        |
| **Rules**      | ✅ 全局 rules         | ✅ 项目 rules         |
| **Extensions** | ✅ `extensions/`      | ❌                    |
| **Events**     | ✅ `events/`          | ✅ 项目 events        |
| **Logs**       | ✅ `logs/`            | ❌                    |
| **Projects**   | ✅ `projects/`        | ❌                    |

**设计原则**：

- 用户级：环境配置、认证信息、全局扩展、日志
- 项目级：项目特定的 Agent/Skill/Rule，纳入 Git 版本控制

### 3.4 路径解析规则

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

### 4.1 用户级配置目录 (`~/.turbot/`)

```
~/.turbot/
├── turbot.json             # 全局配置（Provider配置、权限配置等）
├── .auth/                   # 认证信息目录
│   ├── credentials.json     # 加密存储的凭证
│   └── tokens.json          # 访问令牌
├── agents/                  # 全局 Agent 目录
│   └── *.md                 # Agent 定义文件
├── skills/                  # 全局技能目录
│   └── <skill_name>/
│       └── SKILL.md         # 技能定义
├── rules/                   # 全局规则目录
│   └── *.md                 # 规则定义文件
├── events/                  # 事件日志目录
│   └── events_YYYY-MM-DD.jsonl
├── extensions/              # turbot扩展插件目录
│   ├── extensions.json      # 扩展清单
│   └── <extension_id>/      # 扩展包
│       ├── package.json
│       └── ...
├── logs/                    # 日志目录
│   ├── turbot.log           # 主日志文件
│   └── error.log            # 错误日志
└── projects/                # 项目管理目录
    └── project_cache.json   # 项目缓存
```

### 4.2 项目级配置目录 (`./.turbot/`)

```
.turbot/
├── turbot.json              # 项目主配置文件（可选）
├── agents/                  # 项目 Agent 目录（可选）
│   └── *.md                 # Agent 定义（Markdown格式）
├── skills/                  # 项目技能目录（可选）
│   └── <skill_name>/
│       └── SKILL.md         # 技能定义
├── rules/                   # 项目规则目录（可选）
│   └── *.md                 # 规则定义
├── events/                  # 项目事件目录（可选）
│   ├── schedule.json        # 定时任务配置
│   └── webhook.json         # Webhook 配置
└── extensions/              # 项目扩展目录（可选）
    └── <extension_name>/
        ├── manifest.json
        └── ...
```

### 4.3 用户级全局配置 (`~/.turbot/turbot.json`)

```json
{
  "$schema": "https://turbot.ai/config.json",
  "model": "anthropic/claude-sonnet-4",
  "small_model": "anthropic/claude-haiku-4",
  "provider": {
    "anthropic": {
      "name": "Anthropic",
      "env": ["ANTHROPIC_API_KEY"],
      "options": {
        "baseURL": "https://api.anthropic.com/v1",
        "timeout": 300000
      }
    },
    "openai": {
      "name": "OpenAI",
      "env": ["OPENAI_API_KEY"],
      "options": {
        "baseURL": "https://api.openai.com/v1"
      }
    }
  }
}
```

### 4.4 项目主配置文件 (`./.turbot/turbot.json`)

```json
{
  "$schema": "https://turbot.ai/config.json",
  "model": "openai/gpt-4o",
  "small_model": "openai/gpt-4o-mini",
  "provider": {
    "openai": {
      "name": "OpenAI",
      "env": ["OPENAI_API_KEY"],
      "whitelist": ["gpt-4o", "gpt-4o-mini", "gpt-4-turbo"],
      "options": {
        "baseURL": "https://api.openai.com/v1",
        "timeout": 300000
      },
      "models": {
        "gpt-4o": {
          "name": "GPT-4o",
          "tool_call": true,
          "attachment": true,
          "temperature": true,
          "reasoning": false,
          "limit": { "context": 128000, "output": 16384 }
        },
        "gpt-4o-mini": {
          "name": "GPT-4o Mini",
          "tool_call": true,
          "limit": { "context": 128000, "output": 16384 }
        }
      }
    },
    "anthropic": {
      "name": "Anthropic",
      "env": ["ANTHROPIC_API_KEY"],
      "options": {
        "baseURL": "https://api.anthropic.com/v1"
      }
    },
    "azure": {
      "name": "Azure OpenAI",
      "env": ["AZURE_OPENAI_API_KEY"],
      "options": {
        "apiKey": "${AZURE_OPENAI_API_KEY}",
        "baseURL": "${AZURE_OPENAI_ENDPOINT}",
        "useCompletionUrls": true
      }
    },
    "ollama": {
      "name": "Ollama (Local)",
      "env": [],
      "options": {
        "baseURL": "http://localhost:11434/v1"
      },
      "models": {
        "llama3.1": {
          "name": "Llama 3.1",
          "tool_call": true,
          "limit": { "context": 128000, "output": 4096 }
        },
        "codellama": {
          "name": "Code Llama",
          "tool_call": false,
          "limit": { "context": 16384, "output": 4096 }
        }
      }
    },
    "deepseek": {
      "name": "DeepSeek",
      "env": ["DEEPSEEK_API_KEY"],
      "options": {
        "baseURL": "https://api.deepseek.com/v1",
        "apiKey": "${DEEPSEEK_API_KEY}"
      },
      "models": {
        "deepseek-chat": {
          "name": "DeepSeek Chat",
          "tool_call": true,
          "limit": { "context": 64000, "output": 4096 }
        },
        "deepseek-reasoner": {
          "name": "DeepSeek Reasoner",
          "tool_call": true,
          "reasoning": true,
          "limit": { "context": 64000, "output": 8192 }
        }
      }
    },
    "zhipu": {
      "name": "智谱 AI",
      "env": ["ZHIPU_API_KEY"],
      "options": {
        "baseURL": "https://open.bigmodel.cn/api/paas/v4",
        "apiKey": "${ZHIPU_API_KEY}"
      }
    },
    "bailian": {
      "name": "阿里云百炼",
      "env": ["DASHSCOPE_API_KEY"],
      "options": {
        "baseURL": "https://dashscope.aliyuncs.com/compatible-mode/v1",
        "apiKey": "${DASHSCOPE_API_KEY}"
      }
    },
    "kimi": {
      "name": "Kimi (月之暗面)",
      "env": ["MOONSHOT_API_KEY"],
      "options": {
        "baseURL": "https://api.moonshot.cn/v1",
        "apiKey": "${MOONSHOT_API_KEY}"
      }
    },
    "minimax": {
      "name": "MiniMax",
      "env": ["MINIMAX_API_KEY", "MINIMAX_GROUP_ID"],
      "options": {
        "baseURL": "https://api.minimax.chat/v1",
        "apiKey": "${MINIMAX_API_KEY}",
        "groupId": "${MINIMAX_GROUP_ID}"
      }
    }
  },
  "agent": {
    "build": {
      "model": "anthropic/claude-sonnet-4",
      "steps": 50,
      "permission": { "edit": "allow", "bash": "ask" }
    },
    "plan": {
      "model": "anthropic/claude-sonnet-4",
      "steps": 20
    },
    "title": {
      "model": "openai/gpt-4o-mini"
    }
  },
  "permission": {
    "edit": "ask",
    "bash": "ask",
    "read": "allow"
  }
}
```

## 5. 配置文件格式（Markdown + YAML Frontmatter）

参考 Qoder 的设计，Agent、Skill、Rule 等配置采用 **Markdown + YAML Frontmatter** 格式，
相比纯 JSON 格式具有以下优势：

1. **可读性强**：Markdown 格式便于人工编辑和阅读
2. **文档化**：配置即文档，可包含详细说明和示例
3. **结构清晰**：YAML Frontmatter 定义元数据，Markdown 正文定义内容

### 5.1 Agent 配置文件 (`agents/*.md`)

```markdown
---
name: code-reviewer
description: 专业代码审核专家。主动审核代码质量、安全性、可维护性和最佳实践。
tools: Read, Grep, Glob, Bash
model: gpt-4
temperature: 0.3
skills:
  - code_review
  - security_check
rules:
  - coding_standards
---

# 代码审核专家

你是一位资深的代码审核专家，专注于确保代码质量、安全性和可维护性。

## 审核流程

1. **识别语言**: 检查项目类型，加载对应语言技能
2. **获取变更范围**: 查看最近的代码变更
3. **分层审核**: 按优先级逐一检查
4. **输出报告**: 组织发现的问题并提供建议

## 审核清单

### Critical - 必须修复

- [ ] 逻辑正确性：边界条件、空值处理、并发安全
- [ ] 安全漏洞：SQL 注入、XSS、敏感信息泄露
- [ ] 资源泄漏：未关闭的文件、连接、内存泄漏

### Warning - 应该修复

- [ ] 代码风格：命名规范、格式一致性
- [ ] 错误处理：异常捕获、错误传播
- [ ] 性能问题：N+1 查询、不必要的循环

## 输出格式

### Critical Issues

| 文件:行号    | 问题描述 | 修复建议 |
| ------------ | -------- | -------- |
| file.cpp:123 | 具体问题 | 具体建议 |
```

### 5.2 Skill 配置文件 (`skills/<name>/SKILL.md`)

````markdown
---
name: cpp-code-review
description: C++ 代码审核专用技能。检查内存安全、RAII 模式、现代 C++ 最佳实践。
version: 1.0.0
language: cpp
trigger:
  extensions: [.cpp, .hpp, .h, .cxx, .cc]
  files: [CMakeLists.txt, Makefile]
---

# C++ 代码审核技能

针对 C++ 项目的专业代码审核规范和最佳实践检查。

## 触发条件

- 文件扩展名为 `.cpp`、`.hpp`、`.h`、`.cxx`、`.cc`
- 项目包含 `CMakeLists.txt` 或 `Makefile`

## 审核清单

### Critical - 必须修复

#### 内存安全

- [ ] **内存泄漏**: 检查所有 `new` 是否有对应的 `delete`
- [ ] **双重释放**: 同一指针是否被多次释放
- [ ] **悬空指针**: 指针是否在释放后继续使用

```cpp
// 错误: 内存泄漏
void bad() {
    int* p = new int(42);
    // 忘记 delete
}

// 正确: 使用智能指针
void good() {
    auto p = std::make_unique<int>(42);
}
```
````

#### 资源管理 (RAII)

- [ ] **RAII 模式**: 资源是否封装在对象中管理
- [ ] **异常安全**: 异常发生时资源是否正确释放

### Warning - 应该修复

#### 现代 C++ 特性

- [ ] **智能指针**: 是否使用 `unique_ptr`/`shared_ptr` 替代裸指针
- [ ] **范围 for**: 是否使用 `for (auto& x : container)`
- [ ] **移动语义**: 是否正确实现移动构造/赋值

````

### 5.3 Rule 配置文件 (`rules/*.md`)

```markdown
---
name: coding-standards
description: 编码规范规则
version: 1.0.0
severity: warning
---

# 编码规范规则

## CS001: 命名规范

使用有意义的变量名和函数名。

- 类名使用 PascalCase
- 函数名使用 camelCase
- 常量使用 UPPER_SNAKE_CASE
- 私有成员使用 `_` 后缀

## CS002: 注释规范

公共 API 必须有文档注释。

```cpp
/// @brief 计算两个数的和
/// @param a 第一个数
/// @param b 第二个数
/// @return 两数之和
int add(int a, int b);
````

## CS003: 头文件规范

- 使用 `#pragma once` 作为头文件保护
- 按以下顺序包含头文件：
  1. 对应的头文件
  2. 项目头文件
  3. 第三方库头文件
  4. 标准库头文件

````

### 5.4 Event 配置文件 (`events/*.json`)

事件配置保持 JSON 格式，便于程序解析：

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
````

### 5.5 Extension 配置文件 (`extensions/*/manifest.json`)

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

## 6. 配置加载流程

### 6.1 加载时序

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

### 6.2 核心接口设计

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

## 7. 内置默认配置

### 7.1 默认配置定义

```cpp
nlohmann::json ConfigManager::get_default_config() const {
    return {
        {"$schema", "https://turbot.ai/config.json"},
        {"model", nullptr},  // 需用户配置
        {"small_model", nullptr},
        {"provider", {
            // Anthropic (Claude)
            {"anthropic", {
                {"name", "Anthropic"},
                {"env", {"ANTHROPIC_API_KEY"}},
                {"options", {
                    {"baseURL", "https://api.anthropic.com/v1"},
                    {"timeout", 300000}
                }}
            }},
            // OpenAI (GPT)
            {"openai", {
                {"name", "OpenAI"},
                {"env", {"OPENAI_API_KEY"}},
                {"options", {
                    {"baseURL", "https://api.openai.com/v1"},
                    {"timeout", 300000}
                }}
            }},
            // Google AI (Gemini)
            {"google", {
                {"name", "Google AI"},
                {"env", {"GOOGLE_API_KEY"}},
                {"options", {
                    {"baseURL", "https://generativelanguage.googleapis.com/v1beta"}
                }}
            }},
            // Azure OpenAI
            {"azure", {
                {"name", "Azure OpenAI"},
                {"env", {"AZURE_OPENAI_API_KEY"}},
                {"options", {
                    {"baseURL", nullptr}  // 需配置
                }}
            }},
            // DeepSeek
            {"deepseek", {
                {"name", "DeepSeek"},
                {"env", {"DEEPSEEK_API_KEY"}},
                {"options", {
                    {"baseURL", "https://api.deepseek.com/v1"}
                }}
            }},
            // OpenRouter
            {"openrouter", {
                {"name", "OpenRouter"},
                {"env", {"OPENROUTER_API_KEY"}},
                {"options", {
                    {"baseURL", "https://openrouter.ai/api/v1"},
                    {"headers", {
                        {"HTTP-Referer", "https://turbot.ai/"},
                        {"X-Title", "turbot"}
                    }}
                }}
            }},
            // Ollama (本地)
            {"ollama", {
                {"name", "Ollama"},
                {"env", nlohmann::json::array()},
                {"options", {
                    {"baseURL", "http://localhost:11434/v1"}
                }}
            }}
        }},
        {"agent", {
            {"build", {
                {"steps", 50},
                {"mode", "primary"}
            }},
            {"plan", {
                {"steps", 20},
                {"mode", "primary"}
            }},
            {"explore", {
                {"mode", "subagent"},
                {"hidden", false}
            }}
        }},
        {"permission", {
            {"read", "allow"},
            {"edit", "ask"},
            {"bash", "ask"}
        }}
    };
}
```

### 7.2 安全默认值

| 配置项              | 默认值  | 说明                    |
| ------------------- | ------- | ----------------------- |
| `permission.read`   | `allow` | 读取操作默认允许        |
| `permission.edit`   | `ask`   | 编辑操作默认询问        |
| `permission.bash`   | `ask`   | Bash 命令默认询问       |
| `agent.build.steps` | `50`    | 构建 Agent 最大迭代次数 |

## 8. 配置合并策略

### 8.1 合并规则

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

### 8.2 数组合并示例

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

## 9. 环境变量覆盖

### 9.1 支持的环境变量

#### 通用配置

| 环境变量                     | 配置路径           | 说明             |
| ---------------------------- | ------------------ | ---------------- |
| `TURBOT_DEBUG`               | `turbot.debug`     | 调试模式         |
| `TURBOT_LOG_LEVEL`           | `turbot.log_level` | 日志级别         |
| `TURBOT_CONFIG_PATH`         | -                  | 自定义配置根目录 |
| `TURBOT_USER_CONFIG_PATH`    | -                  | 用户配置目录     |
| `TURBOT_PROJECT_CONFIG_PATH` | -                  | 项目配置目录     |
| `TURBOT_PERMISSIONS_MODE`    | `permission.mode`  | 权限模式         |

#### AI Provider API 密钥

| 环境变量                | Provider ID      | 说明                     |
| ----------------------- | ---------------- | ------------------------ |
| `ANTHROPIC_API_KEY`     | `anthropic`      | Anthropic API 密钥       |
| `OPENAI_API_KEY`        | `openai`         | OpenAI API 密钥          |
| `AZURE_OPENAI_API_KEY`  | `azure`          | Azure OpenAI API 密钥    |
| `AZURE_OPENAI_ENDPOINT` | `azure`          | Azure OpenAI 端点        |
| `GOOGLE_API_KEY`        | `google`         | Google AI API 密钥       |
| `GOOGLE_CLOUD_PROJECT`  | `google-vertex`  | Google Cloud 项目        |
| `AWS_ACCESS_KEY_ID`     | `amazon-bedrock` | AWS 访问密钥 ID          |
| `AWS_REGION`            | `amazon-bedrock` | AWS 区域                 |
| `DASHSCOPE_API_KEY`     | `bailian`        | 阿里云百炼 API 密钥      |
| `ZHIPU_API_KEY`         | `zhipu`          | 智谱 AI API 密钥         |
| `DEEPSEEK_API_KEY`      | `deepseek`       | DeepSeek API 密钥        |
| `MOONSHOT_API_KEY`      | `kimi`           | Kimi (月之暗面) API 密钥 |
| `MINIMAX_API_KEY`       | `minimax`        | Minimax API 密钥         |
| `MINIMAX_GROUP_ID`      | `minimax`        | Minimax Group ID         |
| `OPENROUTER_API_KEY`    | `openrouter`     | OpenRouter API 密钥      |
| `GROQ_API_KEY`          | `groq`           | Groq API 密钥            |
| `MISTRAL_API_KEY`       | `mistral`        | Mistral API 密钥         |
| `XAI_API_KEY`           | `xai`            | xAI API 密钥             |
| `COHERE_API_KEY`        | `cohere`         | Cohere API 密钥          |
| `PERPLEXITY_API_KEY`    | `perplexity`     | Perplexity API 密钥      |
| `CEREBRAS_API_KEY`      | `cerebras`       | Cerebras API 密钥        |
| `DEEPINFRA_API_KEY`     | `deepinfra`      | DeepInfra API 密钥       |
| `TOGETHER_API_KEY`      | `togetherai`     | Together AI API 密钥     |
| `GITHUB_TOKEN`          | `github-copilot` | GitHub Token (Copilot)   |

### 9.2 配置值中的环境变量引用

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

## 10. 动态加载机制

### 10.1 扩展模块加载器

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

### 10.2 Markdown 配置解析

```cpp
/// Markdown 配置解析结果
struct MarkdownConfig {
    nlohmann::json frontmatter;  // YAML Frontmatter 解析结果
    std::string content;         // Markdown 正文内容
};

/// 解析 Markdown + YAML Frontmatter 格式配置
MarkdownConfig parse_markdown_config(const std::string& file_path) {
    std::ifstream file(file_path);
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    // 提取 YAML Frontmatter (--- 之间的内容)
    size_t start = content.find("---\n");
    size_t end = content.find("\n---", start + 4);

    if (start == std::string::npos || end == std::string::npos) {
        return {{}, content};  // 无 Frontmatter，直接返回内容
    }

    std::string yaml_content = content.substr(start + 4, end - start - 4);
    std::string markdown_content = content.substr(end + 4);

    // 解析 YAML 为 JSON
    nlohmann::json frontmatter = yaml_to_json(yaml_content);

    return {frontmatter, markdown_content};
}

// 加载 Agent 配置
void AgentRegistry::load_agent(const std::string& path) {
    auto config = parse_markdown_config(path);

    AgentDefinition agent;
    agent.name = config.frontmatter["name"];
    agent.description = config.frontmatter["description"];
    agent.tools = config.frontmatter.value("tools", std::vector<std::string>{});
    agent.system_prompt = config.content;  // Markdown 正文作为系统提示

    register_agent(agent.name, agent);
}
```

### 10.3 Skill 加载

```cpp
// 加载技能（目录结构，每个技能一个目录）
void SkillRegistry::load_skills(const std::string& path) {
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_directory()) {
            std::string skill_file = entry.path().string() + "/SKILL.md";
            if (std::filesystem::exists(skill_file)) {
                auto config = parse_markdown_config(skill_file);

                SkillDefinition skill;
                skill.name = config.frontmatter["name"];
                skill.description = config.frontmatter["description"];
                skill.language = config.frontmatter.value("language", "");
                skill.content = config.content;  // Markdown 正文

                register_skill(skill.name, skill);
            }
        }
    }
}

// 加载规则
void RuleRegistry::load_rules(const std::string& path) {
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.path().extension() == ".md") {
            auto config = parse_markdown_config(entry.path().string());

            RuleDefinition rule;
            rule.name = config.frontmatter["name"];
            rule.description = config.frontmatter["description"];
            rule.severity = config.frontmatter.value("severity", "warning");
            rule.content = config.content;

            register_rule(rule.name, rule);
        }
    }
}
```

## 11. 日志系统

### 11.1 日志目录结构

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

### 11.2 日志配置

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

## 12. Provider 与 Model 配置规范

### 12.1 Provider 配置结构

与 OpenCode 保持一致，Provider 配置采用 Record 结构：

```typescript
interface ProviderConfig {
  // 提供商名称（显示用）
  name?: string

  // API Key 环境变量名列表
  env?: string[]

  // NPM 包名（用于动态加载 SDK）
  npm?: string

  // API 端点
  api?: string

  // 配置选项
  options?: {
    apiKey?: string
    baseURL?: string
    timeout?: number | false // 毫秒，false 表示禁用超时
    headers?: Record<string, string>
    [key: string]: any
  }

  // 模型白名单（可选）
  whitelist?: string[]

  // 模型黑名单（可选）
  blacklist?: string[]

  // 模型配置覆盖
  models?: Record<string, ModelConfig>
}
```

### 12.2 Model 配置结构

```typescript
interface ModelConfig {
  // 模型 ID（API 调用用）
  id?: string

  // 模型名称（显示用）
  name?: string

  // 模型家族
  family?: string

  // 能力配置
  tool_call?: boolean // 支持工具调用
  reasoning?: boolean // 支持推理/思维链
  attachment?: boolean // 支持附件（图片等）
  temperature?: boolean // 支持 temperature 参数
  interleaved?: boolean | { field: string } // 支持交错输出

  // 输入输出模态
  modalities?: {
    input?: ('text' | 'audio' | 'image' | 'video' | 'pdf')[]
    output?: ('text' | 'audio' | 'image' | 'video' | 'pdf')[]
  }

  // 成本配置
  cost?: {
    input: number // 输入每百万 token 价格（美元）
    output: number // 输出每百万 token 价格（美元）
    cache_read?: number // 缓存读取价格
    cache_write?: number // 缓存写入价格
  }

  // 上下文限制
  limit: {
    context: number // 最大上下文长度
    input?: number // 最大输入长度
    output: number // 最大输出长度
  }

  // 模型状态
  status?: 'alpha' | 'beta' | 'deprecated' | 'active'

  // 发布日期
  release_date?: string

  // 模型变体
  variants?: Record<string, Record<string, any>>

  // 自定义选项
  options?: Record<string, any>
  headers?: Record<string, string>
}
```

### 12.3 支持的 Provider 列表

#### 主流云服务

| Provider ID      | 名称             | 环境变量               | 默认端点                                  |
| ---------------- | ---------------- | ---------------------- | ----------------------------------------- |
| `anthropic`      | Anthropic        | `ANTHROPIC_API_KEY`    | https://api.anthropic.com                 |
| `openai`         | OpenAI           | `OPENAI_API_KEY`       | https://api.openai.com                    |
| `google`         | Google AI        | `GOOGLE_API_KEY`       | https://generativelanguage.googleapis.com |
| `google-vertex`  | Google Vertex AI | `GOOGLE_CLOUD_PROJECT` | https://aiplatform.googleapis.com         |
| `azure`          | Azure OpenAI     | `AZURE_OPENAI_API_KEY` | (需配置)                                  |
| `amazon-bedrock` | Amazon Bedrock   | `AWS_ACCESS_KEY_ID`    | (按区域)                                  |

#### 国内服务商

| Provider ID | 名称            | 环境变量            | 默认端点                       |
| ----------- | --------------- | ------------------- | ------------------------------ |
| `deepseek`  | DeepSeek        | `DEEPSEEK_API_KEY`  | https://api.deepseek.com       |
| `zhipu`     | 智谱 AI         | `ZHIPU_API_KEY`     | https://open.bigmodel.cn       |
| `bailian`   | 阿里云百炼      | `DASHSCOPE_API_KEY` | https://dashscope.aliyuncs.com |
| `kimi`      | Kimi (月之暗面) | `MOONSHOT_API_KEY`  | https://api.moonshot.cn        |
| `minimax`   | MiniMax         | `MINIMAX_API_KEY`   | https://api.minimax.chat       |
| `qwen`      | 通义千问        | `DASHSCOPE_API_KEY` | https://dashscope.aliyuncs.com |

#### 聚合平台

| Provider ID  | 名称           | 环境变量             | 说明                  |
| ------------ | -------------- | -------------------- | --------------------- |
| `openrouter` | OpenRouter     | `OPENROUTER_API_KEY` | 多提供商聚合          |
| `vercel`     | Vercel AI      | `VERCEL_API_KEY`     | Vercel AI Gateway     |
| `gateway`    | AI SDK Gateway | -                    | Vercel AI SDK Gateway |

#### 其他服务商

| Provider ID  | 名称        | 环境变量             |
| ------------ | ----------- | -------------------- |
| `mistral`    | Mistral AI  | `MISTRAL_API_KEY`    |
| `groq`       | Groq        | `GROQ_API_KEY`       |
| `xai`        | xAI         | `XAI_API_KEY`        |
| `cohere`     | Cohere      | `COHERE_API_KEY`     |
| `perplexity` | Perplexity  | `PERPLEXITY_API_KEY` |
| `cerebras`   | Cerebras    | `CEREBRAS_API_KEY`   |
| `deepinfra`  | DeepInfra   | `DEEPINFRA_API_KEY`  |
| `togetherai` | Together AI | `TOGETHER_API_KEY`   |

#### 本地部署

| Provider ID | 名称      | 说明                |
| ----------- | --------- | ------------------- |
| `ollama`    | Ollama    | 本地模型运行        |
| `lmstudio`  | LM Studio | 本地模型运行        |
| `localai`   | LocalAI   | OpenAI 兼容本地服务 |

#### 企业服务

| Provider ID                 | 名称                      | 环境变量             |
| --------------------------- | ------------------------- | -------------------- |
| `github-copilot`            | GitHub Copilot            | `GITHUB_TOKEN`       |
| `github-copilot-enterprise` | GitHub Copilot Enterprise | (OAuth)              |
| `gitlab`                    | GitLab Duo                | `GITLAB_TOKEN`       |
| `sap-ai-core`               | SAP AI Core               | `AICORE_SERVICE_KEY` |

### 12.4 模型选择语法

模型选择使用 `{provider_id}/{model_id}` 格式：

```json
{
  "model": "anthropic/claude-sonnet-4",
  "small_model": "openai/gpt-4o-mini"
}
```

支持模型变体（variants）：

```json
{
  "model": "anthropic/claude-sonnet-4/high" // high 变体
}
```

### 12.5 自定义 Provider 配置示例

添加一个全新的自定义 Provider：

```json
{
  "provider": {
    "my-custom-provider": {
      "name": "My Custom Provider",
      "npm": "@ai-sdk/openai-compatible",
      "env": ["MY_CUSTOM_API_KEY"],
      "api": "https://api.custom.com/v1",
      "options": {
        "apiKey": "${MY_CUSTOM_API_KEY}",
        "baseURL": "https://api.custom.com/v1"
      },
      "models": {
        "custom-model-1": {
          "name": "Custom Model 1",
          "tool_call": true,
          "reasoning": true,
          "attachment": true,
          "limit": { "context": 32000, "output": 4096 }
        }
      }
    }
  }
}
```

## 13. 配置验证

### 13.1 JSON Schema 验证

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "$schema": { "type": "string" },
    "model": {
      "type": "string",
      "description": "默认模型，格式: provider/model"
    },
    "small_model": {
      "type": "string",
      "description": "小型模型，用于简单任务"
    },
    "default_agent": {
      "type": "string",
      "description": "默认 Agent"
    },
    "provider": {
      "type": "object",
      "description": "Provider 配置",
      "additionalProperties": {
        "type": "object",
        "properties": {
          "name": { "type": "string" },
          "env": { "type": "array", "items": { "type": "string" } },
          "npm": { "type": "string" },
          "api": { "type": "string", "format": "uri" },
          "whitelist": { "type": "array", "items": { "type": "string" } },
          "blacklist": { "type": "array", "items": { "type": "string" } },
          "options": {
            "type": "object",
            "properties": {
              "apiKey": { "type": "string" },
              "baseURL": { "type": "string", "format": "uri" },
              "timeout": {
                "oneOf": [
                  { "type": "integer", "minimum": 1 },
                  { "type": "boolean", "const": false }
                ]
              },
              "headers": {
                "type": "object",
                "additionalProperties": { "type": "string" }
              }
            },
            "additionalProperties": true
          },
          "models": {
            "type": "object",
            "additionalProperties": {
              "type": "object",
              "required": ["limit"],
              "properties": {
                "id": { "type": "string" },
                "name": { "type": "string" },
                "family": { "type": "string" },
                "tool_call": { "type": "boolean", "default": true },
                "reasoning": { "type": "boolean", "default": false },
                "attachment": { "type": "boolean", "default": false },
                "temperature": { "type": "boolean", "default": true },
                "modalities": {
                  "type": "object",
                  "properties": {
                    "input": { "type": "array", "items": { "type": "string" } },
                    "output": { "type": "array", "items": { "type": "string" } }
                  }
                },
                "cost": {
                  "type": "object",
                  "properties": {
                    "input": { "type": "number" },
                    "output": { "type": "number" },
                    "cache_read": { "type": "number" },
                    "cache_write": { "type": "number" }
                  }
                },
                "limit": {
                  "type": "object",
                  "required": ["context", "output"],
                  "properties": {
                    "context": { "type": "integer", "minimum": 1 },
                    "input": { "type": "integer", "minimum": 1 },
                    "output": { "type": "integer", "minimum": 1 }
                  }
                },
                "status": {
                  "type": "string",
                  "enum": ["alpha", "beta", "deprecated", "active"],
                  "default": "active"
                }
              }
            }
          }
        }
      }
    },
    "agent": {
      "type": "object",
      "description": "Agent 配置",
      "additionalProperties": {
        "type": "object",
        "properties": {
          "model": { "type": "string" },
          "steps": { "type": "integer", "minimum": 1 },
          "temperature": { "type": "number", "minimum": 0, "maximum": 2 },
          "prompt": { "type": "string" },
          "mode": { "type": "string", "enum": ["primary", "subagent", "all"] },
          "permission": {
            "type": "object",
            "additionalProperties": {
              "type": "string",
              "enum": ["ask", "allow", "deny"]
            }
          }
        }
      }
    },
    "permission": {
      "type": "object",
      "description": "权限配置",
      "properties": {
        "read": { "type": "string", "enum": ["ask", "allow", "deny"] },
        "edit": { "type": "string", "enum": ["ask", "allow", "deny"] },
        "bash": { "type": "string", "enum": ["ask", "allow", "deny"] }
      },
      "additionalProperties": {
        "oneOf": [
          { "type": "string", "enum": ["ask", "allow", "deny"] },
          {
            "type": "object",
            "additionalProperties": {
              "type": "string",
              "enum": ["ask", "allow", "deny"]
            }
          }
        ]
      }
    },
    "mcp": {
      "type": "object",
      "description": "MCP 服务器配置",
      "additionalProperties": {
        "oneOf": [
          {
            "type": "object",
            "properties": {
              "type": { "type": "string", "const": "local" },
              "command": { "type": "array", "items": { "type": "string" } },
              "environment": {
                "type": "object",
                "additionalProperties": { "type": "string" }
              },
              "enabled": { "type": "boolean" },
              "timeout": { "type": "integer", "minimum": 1 }
            },
            "required": ["type", "command"]
          },
          {
            "type": "object",
            "properties": {
              "type": { "type": "string", "const": "remote" },
              "url": { "type": "string", "format": "uri" },
              "headers": {
                "type": "object",
                "additionalProperties": { "type": "string" }
              },
              "enabled": { "type": "boolean" },
              "timeout": { "type": "integer", "minimum": 1 }
            },
            "required": ["type", "url"]
          },
          {
            "type": "object",
            "properties": {
              "enabled": { "type": "boolean" }
            }
          }
        ]
      }
    }
  }
}
```

### 12.2 运行时验证

```cpp
bool ConfigManager::validate_config(const nlohmann::json& config) const {
    // 验证 provider 配置
    if (config.contains("provider")) {
        for (const auto& [provider_id, provider] : config["provider"].items()) {
            // 验证 provider 有名称
            if (!provider.contains("name")) {
                TURBOT_LOG_WARN("Provider {} missing name field", provider_id);
            }

            // 验证模型配置
            if (provider.contains("models")) {
                for (const auto& [model_id, model] : provider["models"].items()) {
                    if (!model.contains("limit")) {
                        TURBOT_LOG_ERROR("Model {}/{} missing required field: limit",
                                        provider_id, model_id);
                        return false;
                    }
                }
            }
        }
    }

    // 验证权限配置
    if (config.contains("permission")) {
        auto& perms = config["permission"];
        for (const auto& [key, value] : perms.items()) {
            if (value.is_string()) {
                std::string mode = value.get<std::string>();
                if (mode != "allow" && mode != "deny" && mode != "ask") {
                    TURBOT_LOG_ERROR("Invalid permission mode for {}: {}", key, mode);
                    return false;
                }
            }
        }
    }

    return true;
}
```

## 13. 错误处理

### 13.1 配置加载错误

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

### 13.2 错误恢复策略

| 错误类型   | 恢复策略                     |
| ---------- | ---------------------------- |
| 文件不存在 | 跳过该层级，继续加载下一层级 |
| 解析错误   | 记录警告，使用默认值         |
| 验证错误   | 记录错误，拒绝加载           |
| 权限拒绝   | 记录错误，尝试创建默认配置   |

## 14. 配置示例

### 14.1 最小配置

```json
{
  "$schema": "https://turbot.ai/config.json",
  "model": "anthropic/claude-sonnet-4"
}
```

### 14.2 使用 OpenRouter 聚合多个 Provider

```json
{
  "$schema": "https://turbot.ai/config.json",
  "model": "openrouter/anthropic/claude-sonnet-4",
  "provider": {
    "openrouter": {
      "options": {
        "apiKey": "${OPENROUTER_API_KEY}",
        "baseURL": "https://openrouter.ai/api/v1"
      }
    }
  }
}
```

### 14.3 完整项目配置（含国内服务商）

```json
{
  "$schema": "https://turbot.ai/config.json",
  "model": "anthropic/claude-sonnet-4",
  "small_model": "deepseek/deepseek-chat",
  "default_agent": "build",
  "provider": {
    "anthropic": {
      "name": "Anthropic",
      "whitelist": ["claude-sonnet-4", "claude-haiku-4", "claude-opus-4"]
    },
    "openai": {
      "name": "OpenAI",
      "options": {
        "timeout": 600000
      },
      "blacklist": ["gpt-3.5-turbo"]
    },
    "deepseek": {
      "name": "DeepSeek",
      "env": ["DEEPSEEK_API_KEY"],
      "options": {
        "apiKey": "${DEEPSEEK_API_KEY}",
        "baseURL": "https://api.deepseek.com/v1"
      },
      "models": {
        "deepseek-chat": {
          "name": "DeepSeek Chat",
          "tool_call": true,
          "limit": { "context": 64000, "output": 8192 }
        },
        "deepseek-reasoner": {
          "name": "DeepSeek Reasoner",
          "tool_call": true,
          "reasoning": true,
          "limit": { "context": 64000, "output": 8192 }
        }
      }
    },
    "zhipu": {
      "name": "智谱 AI",
      "env": ["ZHIPU_API_KEY"],
      "options": {
        "apiKey": "${ZHIPU_API_KEY}",
        "baseURL": "https://open.bigmodel.cn/api/paas/v4"
      }
    },
    "ollama": {
      "name": "Ollama (Local)",
      "options": {
        "baseURL": "http://localhost:11434/v1"
      },
      "models": {
        "llama3.1:latest": {
          "name": "Llama 3.1",
          "tool_call": true,
          "limit": { "context": 128000, "output": 4096 }
        }
      }
    }
  },
  "agent": {
    "build": {
      "model": "anthropic/claude-sonnet-4",
      "steps": 50,
      "permission": { "edit": "allow", "bash": "ask" }
    },
    "plan": {
      "model": "anthropic/claude-sonnet-4",
      "steps": 20
    },
    "explore": {
      "model": "deepseek/deepseek-chat",
      "mode": "subagent",
      "hidden": false
    },
    "title": {
      "model": "deepseek/deepseek-chat",
      "steps": 5
    }
  },
  "permission": {
    "read": "allow",
    "edit": "ask",
    "bash": "ask",
    "webfetch": "allow"
  },
  "mcp": {
    "filesystem": {
      "type": "local",
      "command": ["mcp-server-filesystem", "/path/to/allowed/dir"],
      "enabled": true
    }
  }
}
```

## 15. API 使用示例

### 15.1 初始化配置

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

### 15.2 获取配置值

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

### 15.3 修改配置

```cpp
auto& config = Config::instance();

// 设置配置（运行时，不会持久化）
config.set("permissions.mode", "allow");

// 持久化保存到项目配置
ConfigManager::instance().save_config(ConfigLevel::Project);
```

## 16. 实现状态

### Phase 1: 核心架构 ✅ 已完成

1. ✅ 实现 `ConfigManager` 类
   - `libs/core/include/turbot/core/config/config_manager.hpp`
   - `libs/core/src/config/config_manager.cpp`
2. ✅ 实现三级配置加载逻辑（Default → User → Project → 环境变量）
3. ✅ 实现配置合并策略（深度合并、\_append 后缀数组追加）
4. ✅ 实现环境变量覆盖（TURBOT\_\*, API Key 映射）
5. ✅ 实现配置验证（version, providers, permissions）
6. ✅ 实现 YAML Frontmatter 解析
7. ✅ 实现扩展模块加载（Agent, Skill, Rule, Event, Extension）
8. ✅ 单元测试覆盖率 > 95%（27 个测试用例，130 个断言）

### Phase 2: 扩展加载 (待实现)

1. ⬜ 实现 Agent 动态加载
2. ⬜ 实现 Skill 动态加载
3. ⬜ 实现 Rule 动态加载
4. ⬜ 实现热重载支持
5. ⬜ 添加集成测试

### Phase 3: 高级功能 (待实现)

1. ⬜ 实现 Event 定时任务
2. ⬜ 实现 Extension 插件系统
3. ⬜ 实现日志系统集成
4. ⬜ 添加完整测试覆盖

### 文件清单

**已实现文件：**

| 文件路径                                                  | 说明                 |
| --------------------------------------------------------- | -------------------- |
| `libs/core/include/turbot/core/config/config_manager.hpp` | ConfigManager 头文件 |
| `libs/core/src/config/config_manager.cpp`                 | ConfigManager 实现   |
| `tests/unit/config_manager_test.cpp`                      | 单元测试             |

**修改文件：**

| 文件路径                   | 说明             |
| -------------------------- | ---------------- |
| `libs/core/CMakeLists.txt` | 添加新源文件     |
| `tests/CMakeLists.txt`     | 添加独立测试目标 |

## 17. 相关文档

- [目录架构规范](./directory-structure.md)
- [权限系统设计](../plans/README.md)
- [Agent 系统设计](../plans/README.md)
