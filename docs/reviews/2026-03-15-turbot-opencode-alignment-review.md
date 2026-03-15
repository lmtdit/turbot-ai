# 代码审核：Turbot-AI 与 OpenCode 功能对齐审查

> **审查日期**: 2026-03-15
> **审查范围**: Turbot-AI vs OpenCode 核心系统功能对齐
> **审查人**: code-reviewer

---

## 概述

本次审查针对 Turbot-AI (C++) 与 OpenCode (TypeScript) 两大系统的十一个核心子系统进行功能对齐分析，评估两者在架构设计、功能完整性和实现质量方面的差异。

---

## SPEARM 综合分：**74/100** — 🟡 需改进

| 维度          | 字母等级 | 分数   | 关键发现                               |
| ------------- | -------- | ------ | -------------------------------------- |
| 🔴 S 安全性   | B+       | 8.5/10 | 权限系统设计合理，敏感信息脱敏处理到位 |
| 🟡 P 性能     | B+       | 8.5/10 | C++ 实现具有天然性能优势，缓存机制完善 |
| 🟠 E 错误处理 | B        | 8/10   | 边界情况处理较完整，异常处理规范       |
| 🔵 A 架构     | C+       | 7/10   | 核心架构已对齐，但存在功能缺失         |
| 📊 R 可靠性   | C        | 6/10   | 测试覆盖需加强，部分边界场景缺失       |
| 🔧 M 可维护性 | B+       | 8.5/10 | 代码结构清晰，模块化程度高             |

---

## 功能对齐矩阵

| 系统                  | OpenCode          | Turbot-AI (C++)  | 对齐度 | 状态        |
| --------------------- | ----------------- | ---------------- | ------ | ----------- |
| **Session-Loop 系统** | ✅ 完整           | ✅ 完整          | 95%    | ✅ 已对齐   |
| **多 Agent 系统**     | ✅ 完整           | ✅ 完整          | 100%   | ✅ 已对齐   |
| **Prompt 系统**       | ✅ 模板系统       | ✅ 文件模板      | 95%    | ✅ 已对齐   |
| **Skill 系统**        | ✅ 多源发现       | ✅ SkillRegistry | 90%    | 🟡 部分对齐 |
| **工具系统**          | ✅ 45+ 工具       | ⚠️ 21 工具       | 75%    | 🟡 部分对齐 |
| **记忆系统**          | ✅ SessionStore   | ⚠️ 基础实现      | 70%    | 🟡 部分对齐 |
| **权限系统**          | ✅ 完整           | ✅ Ruleset       | 95%    | ✅ 已对齐   |
| **LLM 供应商**        | ✅ 30+ 供应商     | ✅ 23+ 供应商    | 95%    | ✅ 已对齐   |
| **LSP 服务器**        | ✅ 完整           | ✅ 20+ 语言      | 95%    | ✅ 已对齐   |
| **MCP 系统**          | ✅ stdio/SSE/HTTP | ✅ 三种传输      | 90%    | ✅ 已对齐   |
| **ACP 系统**          | ✅ 完整           | ✅ ACPServer     | 85%    | 🟡 部分对齐 |

---

## 发现

### [HIGH] [A] 工具系统功能缺失

**文件：** `libs/core/src/tool/tool_registry.cpp`

**问题：** Turbot-AI 注册了 21 个内置工具，OpenCode 有 45+ 工具，存在以下缺失：

| 缺失工具             | OpenCode 路径                | 功能描述                            |
| -------------------- | ---------------------------- | ----------------------------------- |
| `external-directory` | `tool/external-directory.ts` | 外部目录访问                        |
| `skill` (独立)       | `tool/skill.ts`              | Skill 工具（Turbot 已有 SkillTool） |
| 自定义工具发现       | `tool/registry.ts:38-64`     | 从 `.opencode/tools/` 目录动态加载  |

**影响：** 功能覆盖率不足，无法实现 100% 功能对齐

**修复建议：**

```cpp
// 在 tool_registry.cpp 中添加自定义工具发现机制
void ToolRegistry::discover_custom_tools(const std::string& project_dir) {
    std::string tools_dir = project_dir + "/.turbot/tools";
    if (std::filesystem::exists(tools_dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(tools_dir)) {
            if (entry.path().extension() == ".so" || entry.path().extension() == ".dylib") {
                // 动态加载工具库
            }
        }
    }
}
```

---

### [MEDIUM] [A] 记忆系统持久化差异

**文件：** `libs/core/src/session/session_loop.cpp:290-327`

**问题：**

- OpenCode 使用 SQLite + Drizzle ORM 进行会话持久化，支持完整的消息历史和游标分页
- Turbot-AI 的 Session 主要在内存中，持久化机制较为基础

**OpenCode 实现参考：** `opencode/packages/opencode/src/session/session.sql.ts`

**影响：** 大规模会话场景下内存压力较大，重启后状态恢复不完整

**修复建议：**

```cpp
// 添加 SessionStore 持久化层
class SessionStore {
public:
    void save_session(const Session& session);
    std::optional<Session> load_session(const std::string& session_id);
    std::vector<Session> list_sessions(const std::string& project_id, int limit, const std::string& cursor);
    void delete_session(const std::string& session_id);
};
```

---

### [MEDIUM] [A] MCP OAuth 流程未完整实现

**文件：** `libs/core/src/mcp/manager.cpp:157-190`

**问题：**

- OpenCode 有完整的 MCP OAuth 流程（`McpOAuthProvider`、`oauth-provider.ts`）
- Turbot-AI 的 MCP 系统支持 stdio/SSE/HTTP 三种传输，但 OAuth 认证流程未完整实现

**OpenCode 实现参考：** `opencode/packages/opencode/src/mcp/oauth-provider.ts`

**影响：** 远程 MCP 服务器需要 OAuth 认证时无法自动处理

**修复建议：**

```cpp
// 添加 OAuth 支持
class MCPOAuthProvider {
public:
    void start_auth_flow(const std::string& mcp_name, const std::string& url);
    std::string wait_for_callback();
    void store_tokens(const std::string& mcp_name, const std::string& access_token);
    std::optional<std::string> get_access_token(const std::string& mcp_name);
};
```

---

### [LOW] [A] ACP 会话管理差异

**文件：** `libs/core/include/turbot/core/acp/agent.hpp:36-129`

**问题：**

- OpenCode ACP Agent 有 `usage_update`、`cost` 计算等完整的状态同步
- Turbot-AI 的 `TurbotACPAgent` 已实现核心接口，但缺少 `sendUsageUpdate` 等辅助功能

**OpenCode 实现参考：** `opencode/packages/opencode/src/acp/agent.ts:76-124`

**影响：** ACP 客户端无法获取实时使用量和成本信息

---

### [LOW] [M] 工具数量统计

**OpenCode 工具清单（45+）：**

```
apply_patch, bash, batch, codesearch, edit, external-directory, glob, grep,
invalid, ls, lsp, multiedit, plan, question, read, skill, task, todo,
todoread, todowrite, truncation, webfetch, websearch, write, ...
+ 自定义工具发现
```

**Turbot-AI 工具清单（21）：**

```
read_file, write_file, edit, multiedit, apply_patch, batch, invalid,
bash, glob, grep, list, codesearch, webfetch, websearch,
task, todoread, todowrite, plan_enter, plan_exit, skill, question(可选)
+ lsp(实验性)
```

---

## 架构问题（公共模块）

| 函数/模块  | 重复次数 | 重复位置 | 目标位置 | 提升理由                   |
| ---------- | -------- | -------- | -------- | -------------------------- |
| 无明显重复 | -        | -        | -        | 架构设计合理，模块边界清晰 |

---

## 做得好的地方

1. **Session-Loop 架构完整**：`libs/core/src/session/session_loop.cpp` 实现了与 OpenCode 一致的循环逻辑，包括 doom loop 检测、compaction、权限请求等
2. **MCP 三传输层支持**：完整实现了 stdio/SSE/HTTP 三种传输，并有 SSE fallback 机制
3. **多 Agent 系统完善**：`libs/core/src/agent/agent.cpp` 实现了 `list_visible`、`list_primary`、`default_agent` 等关键接口
4. **LLM 供应商覆盖广泛**：支持 23+ 供应商，包括国产模型（百炼、智谱、Kimi、Minimax）
5. **敏感信息脱敏**：`libs/core/src/mcp/stdio_transport.cpp:14-22` 实现了 Authorization/Bearer/token 等关键字的日志脱敏

---

## 修复优先级

| 优先级 | 问题             | 行动项                                           | 预估工时 |
| ------ | ---------------- | ------------------------------------------------ | -------- |
| P0     | 工具系统缺失     | 补充 external-directory 工具，实现自定义工具发现 | 2 天     |
| P1     | 记忆系统持久化   | 实现 SQLite SessionStore，支持游标分页           | 3 天     |
| P1     | MCP OAuth 流程   | 实现 McpOAuthProvider 类                         | 2 天     |
| P2     | ACP usage_update | 补充 sendUsageUpdate 功能                        | 1 天     |

---

## 总结

Turbot-AI 已实现与 OpenCode 约 **85%** 的功能对齐，核心架构（Session-Loop、多 Agent、权限、LSP、MCP）已完整实现。主要差距集中在：

1. **工具系统**：缺少自定义工具发现机制和部分工具
2. **记忆系统**：持久化机制需要加强
3. **MCP OAuth**：认证流程未完整实现

建议按照 P0 → P1 → P2 的优先级顺序进行修复，预计总工时 **8 天** 可达到 100% 功能对齐。

---

## 参考文件

### Turbot-AI 核心文件

- `libs/core/src/session/session_loop.cpp` - Session-Loop 实现
- `libs/core/src/agent/agent.cpp` - AgentRegistry 实现
- `libs/core/src/tool/tool_registry.cpp` - 工具注册表
- `libs/core/src/mcp/manager.cpp` - MCP 管理器
- `libs/core/src/mcp/stdio_transport.cpp` - stdio 传输层
- `libs/core/src/mcp/sse_transport.cpp` - SSE 传输层
- `libs/core/src/mcp/http_transport.cpp` - HTTP 传输层
- `libs/core/include/turbot/core/acp/agent.hpp` - ACP Agent 接口

### OpenCode 参考文件

- `packages/opencode/src/session/processor.ts` - Session 处理器
- `packages/opencode/src/session/prompt.ts` - Session Loop
- `packages/opencode/src/tool/registry.ts` - 工具注册表
- `packages/opencode/src/mcp/index.ts` - MCP 实现
- `packages/opencode/src/mcp/oauth-provider.ts` - OAuth Provider
- `packages/opencode/src/acp/agent.ts` - ACP Agent
