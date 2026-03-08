# ADR-001: Agent 定义与动态生成模块架构

## 元数据

| 属性     | 值         |
| -------- | ---------- |
| 状态     | 已批准     |
| 日期     | 2026-03-08 |
| 决策者   | 架构团队   |
| 影响版本 | v2.0       |

## 背景

基于 OpenCode 项目架构分析，需要为 Turbot-AI 实现 Agent 定义扩展和动态生成功能：

1. **Agent 定义缺失字段**: OpenCode Agent 有 `prompt`、`temperature`、`topP`、`steps`、`color` 等字段，Turbot 当前实现缺失
2. **Agent 动态生成**: OpenCode 提供 `generate()` 函数，通过 LLM 根据用户描述自动生成 Agent 配置
3. **内置 Agent 提示词**: OpenCode 内置 Agent 有专属提示词文件 (`prompt/*.txt`)

## 决策

### 1. Agent 定义扩展

**位置**: `libs/core/include/turbot/core/agent/agent.hpp`

扩展现有 `AgentInfo` 结构，新增字段：

```cpp
struct AgentInfo {
    // 现有字段
    std::string name;
    std::optional<std::string> description;
    AgentMode mode = AgentMode::Primary;
    bool native = false;
    bool hidden = false;
    permission::Ruleset permission;
    std::optional<std::string> model_id;
    nlohmann::json options;

    // 新增字段 (v2.0)
    std::optional<std::string> prompt;       // Agent 专属提示词模板
    std::optional<double> temperature;       // 生成温度 (0.0-1.0)
    std::optional<double> top_p;             // Top-p 采样
    std::optional<int> steps;                // 最大步骤数
    std::optional<std::string> color;        // UI 颜色标识
    std::optional<std::string> variant;      // 模型变体
};
```

### 2. Agent 生成器模块

**位置**: `libs/core/include/turbot/core/agent/agent_generator.hpp`

新增独立模块，负责动态生成 Agent 配置：

```cpp
namespace turbot::core::agent {

struct GeneratedAgent {
    std::string identifier;     // Agent 标识
    std::string when_to_use;    // 使用场景说明
    std::string system_prompt;  // 系统提示词
};

class AgentGenerator {
public:
    /// 通过 LLM 生成 Agent 配置
    static std::future<GeneratedAgent> generate(
        const std::string& description,
        const std::optional<std::pair<std::string, std::string>>& model = std::nullopt
    );
};

} // namespace turbot::core::agent
```

### 3. 内置 Agent 提示词目录

**位置**: `libs/core/src/agent/builtin/prompts/`

存放内置 Agent 的专属提示词文件：

```
prompts/
├── explore.txt      # Explore Agent 提示词
├── compaction.txt   # Compaction Agent 提示词
├── summary.txt      # Summary Agent 提示词
├── title.txt        # Title Agent 提示词
└── generate.txt     # Agent 生成器的系统提示词
```

### 4. 模块职责划分

| 功能       | 模块位置                        | 说明                      |
| ---------- | ------------------------------- | ------------------------- |
| Agent 定义 | `agent/agent.hpp`               | AgentInfo 结构，扩展字段  |
| Agent 注册 | `agent/agent.cpp`               | AgentRegistry 单例        |
| Agent 生成 | `agent/agent_generator.hpp/cpp` | 动态生成 Agent 配置       |
| Agent 加载 | `agent/agent_loader.hpp/cpp`    | 从 MD 文件/配置加载 Agent |
| 内置 Agent | `agent/builtin/*.hpp/cpp`       | 内置 Agent 实现           |
| 提示词文件 | `agent/builtin/prompts/*.txt`   | 提示词内容                |

## 理由

### 为什么选择这个架构？

1. **模块内聚**: 所有 Agent 相关功能集中在 `libs/core/agent/` 目录，便于维护
2. **职责分离**: AgentGenerator 独立于 AgentRegistry，生成与注册解耦
3. **提示词外置**: 提示词作为独立文件存放，便于修改和国际化
4. **对齐 OpenCode**: 参考成熟实现，降低学习成本

### 为什么不是其他方案？

| 备选方案                        | 拒绝理由                          |
| ------------------------------- | --------------------------------- |
| 提示词硬编码在代码中            | 难以修改，需要重新编译            |
| Agent 生成放在 Config 模块      | 职责不清，Config 只负责配置加载   |
| 提示词存放在独立 resources 目录 | 增加目录复杂度，与 Agent 代码分离 |

## 影响

### 正面影响

- Agent 功能完整对齐 OpenCode
- 支持用户自定义 Agent 动态生成
- 内置 Agent 提示词可独立更新

### 风险

- 提示词文件需随二进制分发
- Agent 生成依赖 LLM 可用性

### 兼容性

- 现有 Agent 代码无需修改（新增字段为 optional）
- 新增模块不影响现有接口

## 实施计划

| 阶段 | 任务                        | 计划编号 |
| ---- | --------------------------- | -------- |
| 1    | Agent 字段补充              | 2.1      |
| 2    | 内置 Agent 完善（含提示词） | 2.16     |
| 3    | Agent 动态加载              | 2.17     |
| 4    | Agent 生成器                | 新增     |
