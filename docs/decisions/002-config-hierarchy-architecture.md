# ADR-002: 配置分级机制架构

## 元数据

| 属性     | 值         |
| -------- | ---------- |
| 状态     | ✅ 已批准  |
| 日期     | 2026-03-08 |
| 决策者   | 架构团队   |
| 影响版本 | v1.0       |

## 背景

Turbot-AI 需要一个灵活的配置系统，支持：

1. **多环境适配**: 用户级全局配置与项目级配置分离
2. **团队协作**: 项目配置可纳入版本控制，团队共享
3. **安全性**: 敏感信息（API Key）支持环境变量覆盖
4. **扩展性**: 支持动态加载 Agent、Skill、Rule 等扩展模块

参考了 Qoder 编辑器的配置模式。

## 决策

### 1. 三级配置层级

采用优先级递增的三级配置架构：

```
环境变量覆盖 (最高优先级)
        ↓
项目级配置 (./.turbot/)
        ↓
用户级配置 (~/.turbot/)
        ↓
内置默认配置 (最低优先级)
```

**配置路径规范**：

| 层级     | 路径          | 说明         | 版本控制 |
| -------- | ------------- | ------------ | -------- |
| 内置默认 | (compiled-in) | 编译时硬编码 | -        |
| 用户级   | `~/.turbot/`  | 用户全局配置 | ❌       |
| 项目级   | `./.turbot/`  | 项目特定配置 | ✅       |

### 2. 用户级与项目级职责划分

| 配置类型   | 用户级 (`~/.turbot/`) | 项目级 (`./.turbot/`) |
| ---------- | --------------------- | --------------------- |
| 全局配置   | ✅ `turbot.json`      | ✅ `turbot.json`      |
| 认证信息   | ✅ `.auth/`           | ❌                    |
| Agents     | ✅ 全局 agents        | ✅ 项目 agents        |
| Skills     | ✅ 全局 skills        | ✅ 项目 skills        |
| Rules      | ✅ 全局 rules         | ✅ 项目 rules         |
| Extensions | ✅ `extensions/`      | ❌                    |
| Logs       | ✅ `logs/`            | ❌                    |

**设计原则**：

- 用户级：环境配置、认证信息、全局扩展、日志
- 项目级：项目特定的 Agent/Skill/Rule，纳入 Git 版本控制

### 3. 配置文件格式：Markdown + YAML Frontmatter

Agent、Skill、Rule 等配置采用 Markdown + YAML Frontmatter 格式：

```markdown
---
name: code-reviewer
description: 专业代码审核专家
tools: Read, Grep, Glob
model: gpt-4
---

# 系统提示词

你是一位资深的代码审核专家...
```

**选择理由**：

- 可读性强：Markdown 格式便于人工编辑
- 文档化：配置即文档，可包含详细说明
- 结构清晰：YAML Frontmatter 定义元数据，Markdown 正文定义提示词

### 4. 配置合并策略

**深度合并规则**：

```cpp
nlohmann::json merge_config(const nlohmann::json& base, const nlohmann::json& override) {
    // 1. 非对象类型直接替换
    // 2. 对象类型递归合并
    // 3. 特殊处理 xxx_append 后缀：追加到数组
}
```

**数组合并示例**：

```json
// 用户配置
{ "providers_append": [{"name": "custom"}] }

// 合并结果
{ "providers": [{"name": "openai"}, {"name": "custom"}] }
```

### 5. 环境变量覆盖

**优先级最高**，支持两类环境变量：

| 类型         | 示例                                  | 说明                     |
| ------------ | ------------------------------------- | ------------------------ |
| 配置覆盖     | `TURBOT_DEBUG`, `TURBOT_LOG_LEVEL`    | 覆盖配置项               |
| API Key 映射 | `ANTHROPIC_API_KEY`, `OPENAI_API_KEY` | 自动映射到 Provider 配置 |

**配置值中引用环境变量**：

```json
{
  "api_key": "${OPENAI_API_KEY}",
  "base_url": "${OPENAI_BASE_URL:-https://api.openai.com/v1}"
}
```

### 6. 动态加载机制

**扩展模块加载**：

```cpp
class ExtensionLoader {
public:
    /// 加载目录下所有扩展模块
    template<typename T>
    std::vector<std::shared_ptr<T>> load_directory(const std::string& path);

    /// 热重载支持
    void watch_directory(const std::string& path,
                        std::function<void(const std::string&)> on_change);
};
```

**扩展模块目录结构**：

```
.turbot/
├── agents/           # Agent 定义 (*.md)
├── skills/           # 技能定义 (*/SKILL.md)
├── rules/            # 规则定义 (*.md)
├── events/           # 事件配置 (*.json)
└── extensions/       # 插件扩展
```

## 理由

### 为什么选择三级配置？

| 备选方案              | 拒绝理由                               |
| --------------------- | -------------------------------------- |
| 单一配置文件          | 无法区分用户偏好与项目配置             |
| 两级配置（用户+项目） | 缺少内置默认值，首次使用体验差         |
| 四级配置（+系统级）   | 过于复杂，C++ 应用通常不需要系统级配置 |

### 为什么选择 Markdown + YAML Frontmatter？

| 备选方案                    | 拒绝理由                     |
| --------------------------- | ---------------------------- |
| 纯 JSON                     | 不支持注释，长提示词难以阅读 |
| 纯 YAML                     | 多行字符串处理复杂           |
| TOML                        | 生态不如 YAML 成熟           |
| Markdown + JSON Frontmatter | JSON 不支持注释              |

## 影响

### 正面影响

- 用户可在不同项目使用不同 Provider 配置
- 项目配置可纳入版本控制，团队共享
- 敏感信息可通过环境变量安全注入
- 支持动态扩展 Agent/Skill/Rule

### 风险

- 配置优先级需要清晰的文档说明
- 环境变量命名需要避免冲突

### 兼容性

- 与现有 Config 单例系统无缝集成
- 新增字段向后兼容

## 实施计划

| 阶段    | 任务                                          | 状态      |
| ------- | --------------------------------------------- | --------- |
| Phase 1 | 核心架构 (ConfigManager, 三级加载, 合并策略)  | ✅ 已完成 |
| Phase 2 | 扩展加载 (Agent/Skill/Rule 动态加载, 热重载)  | 📋 待实现 |
| Phase 3 | 高级功能 (Event 定时任务, Extension 插件系统) | 📋 待实现 |

## 配置文件示例

### 项目级配置 (`.turbot/turbot.json`)

```json
{
  "version": "1.0",
  "turbot": {
    "debug": false,
    "log_level": "info"
  },
  "providers": [
    {
      "name": "bailian",
      "type": "bailian",
      "api_key": "${DASHSCOPE_API_KEY}",
      "base_url": "https://dashscope.aliyuncs.com/compatible-mode/v1",
      "default_model": "qwen3-coder-plus",
      "models": [
        {
          "id": "qwen3-coder-plus",
          "name": "通义千问3-Coder-Plus",
          "description": "通义千问3代码专用模型，编程能力最强",
          "context_window": 131072,
          "capabilities": {
            "temperature": true,
            "reasoning": false,
            "tool_call": true,
            "streaming": true,
            "vision": false
          }
        }
      ]
    }
  ],
  "permissions": {
    "mode": "ask",
    "rules": [
      { "pattern": "read_file:*", "action": "allow" },
      { "pattern": "write_file:*", "action": "ask" },
      { "pattern": "bash:rm*", "action": "deny" }
    ]
  },
  "agents": {
    "default": "build",
    "enabled": ["build", "plan", "explore"]
  },
  "session": {
    "max_history": 100,
    "auto_save": true,
    "timeout_seconds": 300,
    "max_iterations": 50,
    "compaction_threshold": 0.8
  },
  "tools": {
    "enabled": [
      "read_file",
      "write_file",
      "bash",
      "search",
      "grep",
      "glob",
      "list_dir"
    ],
    "disabled": []
  }
}
```

### 配置字段说明

| 字段                        | 类型   | 说明                                          |
| --------------------------- | ------ | --------------------------------------------- |
| `providers`                 | Array  | Provider 配置数组                             |
| `providers[].name`          | String | Provider 名称标识                             |
| `providers[].type`          | String | Provider 类型 (bailian, openai, anthropic 等) |
| `providers[].api_key`       | String | API 密钥，支持 `${ENV_VAR}` 环境变量引用      |
| `providers[].base_url`      | String | API 基础 URL                                  |
| `providers[].default_model` | String | 默认模型 ID                                   |
| `providers[].models`        | Array  | 可用模型列表                                  |
| `permissions`               | Object | 权限配置                                      |
| `permissions.mode`          | String | 默认权限模式 (allow/deny/ask)                 |
| `permissions.rules`         | Array  | 权限规则列表                                  |
| `agents`                    | Object | Agent 配置                                    |
| `session`                   | Object | 会话配置                                      |
| `tools`                     | Object | 工具配置                                      |

### 默认 Provider 配置

项目默认使用阿里云百炼 (Bailian) Provider，模型配置：

| 模型 ID            | 名称                  | 上下文窗口 | 特点                    |
| ------------------ | --------------------- | ---------- | ----------------------- |
| `qwen3-coder-plus` | 通义千问 3-Coder-Plus | 128K       | 代码专用，稳定版 (默认) |
| `qwen3-coder-next` | 通义千问 3-Coder-Next | 128K       | 代码专用，预览版        |
| `qwen3.5-plus`     | 通义千问 3.5-Plus     | 128K       | 性价比高                |
| `qwen3-max`        | 通义千问 3-Max        | 32K        | 能力最强                |
| `qwen3-vl-plus`    | 通义千问 3-VL-Plus    | 128K       | 视觉模型                |

## 核心接口

```cpp
namespace turbot::core {

class TURBOT_CORE_API ConfigManager {
public:
    enum class ConfigLevel { Default, User, Project };
    enum class ExtensionType { Agent, Skill, Rule, Event, Extension };

    static ConfigManager& instance();

    /// 初始化配置系统（加载所有层级配置）
    std::vector<LoadResult> initialize();

    /// 加载指定层级配置
    LoadResult load_config(ConfigLevel level);

    /// 加载扩展模块
    std::vector<LoadResult> load_extensions(ConfigLevel level, ExtensionType type);

    /// 获取配置路径
    std::string get_config_path(ConfigLevel level) const;

    /// 重新加载配置
    void reload();

    /// 保存配置到指定层级
    bool save_config(ConfigLevel level);
};

} // namespace turbot::core
```

## 参考

- 详细设计文档: [config-hierarchy.md](../architecture/config-hierarchy.md)
