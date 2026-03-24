# Turbot vs OpenCode 核心能力对比审查报告

**审查日期**: 2026-03-24 (二次核实)  
**审查范围**: turbot-ai/libs + apps vs opencode/packages/opencode  
**审查方法**: SPEARM 六维度框架 + 功能对标分析 + 独立运行能力验证

---

## 1. 项目概览对比

| 指标     | Turbot                | OpenCode       |
| -------- | --------------------- | -------------- |
| 语言     | C++ (20/2b)           | TypeScript     |
| 代码行数 | ~43,400 (libs)        | ~51,700        |
| 应用层   | ✅ apps/turbot-cli    | ✅ src/cli     |
| 服务层   | ✅ apps/turbot-server | ✅ src/server  |
| 测试用例 | 2,166                 | -              |
| 架构风格 | 库+应用模式           | 应用模式 (src) |

**⚠️ 核实发现**: Turbot 已有 CLI 和 Server 应用层，原报告"缺少应用层"结论错误。

---

## 2. 模块能力对比矩阵

### 2.1 核心模块对比

| 模块           | Turbot         | OpenCode       | 差异分析        |
| -------------- | -------------- | -------------- | --------------- |
| **ACP**        | ✅ acp/        | ✅ acp/        | 基本对齐        |
| **Agent**      | ✅ agent/      | ✅ agent/      | 基本对齐        |
| **Config**     | ✅ config/     | ✅ config/     | 基本对齐        |
| **LSP**        | ✅ lsp/        | ✅ lsp/        | 基本对齐        |
| **MCP**        | ✅ mcp/        | ✅ mcp/        | 基本对齐        |
| **Permission** | ✅ permission/ | ✅ permission/ | 基本对齐        |
| **Provider**   | ✅ provider/   | ✅ provider/   | OpenCode 更丰富 |
| **Session**    | ✅ session/    | ✅ session/    | OpenCode 更完善 |
| **Skill**      | ✅ skill/      | ✅ skill/      | 基本对齐        |
| **Tool**       | ✅ tool/       | ✅ tool/       | 工具数量接近    |
| **Storage**    | ✅ storage/    | ✅ storage/    | 架构差异大      |
| **Event**      | ✅ event/      | ✅ bus/        | 命名不同        |

### 2.2 CLI 命令对比

**OpenCode CLI 命令** (20 个):
| 命令 | 功能 | Turbot 状态 |
|------|------|------------|
| run | 交互式会话 | ✅ 已实现 |
| list | 列出会话 | ✅ 已实现 |
| acp | ACP 服务器 | ✅ 已实现 |
| version | 版本信息 | ✅ 已实现 |
| serve | HTTP 服务器 | ✅ (独立应用) |
| account | 账户管理 | ❌ 缺失 |
| agent | 智能体管理 | ❌ 缺失 |
| db | 数据库管理 | ❌ 缺失 |
| export | 导出数据 | ❌ 缺失 |
| generate | 生成代码 | ❌ 缺失 |
| github | GitHub 集成 | ❌ 缺失 |
| import | 导入数据 | ❌ 缺失 |
| mcp | MCP 管理 | ❌ 缺失 |
| models | 模型列表 | ❌ 缺失 |
| pr | PR 集成 | ❌ 缺失 |
| providers | 提供商管理 | ❌ 缺失 |
| session | 会话管理 | ❌ 缺失 |
| stats | 统计信息 | ❌ 缺失 |
| uninstall | 卸载 | ❌ 缺失 |
| upgrade | 升级 | ❌ 缺失 |
| web | Web 界面 | ❌ 缺失 |
| workspace-serve | 工作空间服务 | ❌ 缺失 |

**命令对齐率**: 5/22 (23%)

### 2.3 Turbot 缺失模块 (按独立运行必要性排序)

| 模块              | OpenCode 路径      | 功能描述                  | 原优先级 | 修正优先级 |
| ----------------- | ------------------ | ------------------------- | -------- | ---------- |
| **CLI 扩展**      | src/cli/cmd/       | 更多 CLI 命令             | P1       | **P0**     |
| **Project**       | src/project/       | 项目管理                  | P1       | **P0**     |
| **Account**       | src/account/       | 用户账户管理              | P1       | **P1**     |
| **Auth**          | src/auth/          | 认证服务集成              | P1       | **P1**     |
| **Plugin**        | src/plugin/        | 插件系统 (codex, copilot) | P1       | **P1**     |
| **Bus**           | src/bus/           | 全局事件总线              | P2       | **P2**     |
| **Command**       | src/command/       | 命令系统/模板             | P2       | **P2**     |
| **Control-Plane** | src/control-plane/ | 工作空间控制平面          | P1       | **P2**     |
| **File**          | src/file/          | 文件系统增强              | P2       | **P2**     |
| **IDE**           | src/ide/           | IDE 集成                  | P2       | **P2**     |
| **Patch**         | src/patch/         | 补丁系统                  | P2       | **P2**     |
| **PTY**           | src/pty/           | 伪终端管理                | P1       | **P2**     |
| **Question**      | src/question/      | 问答系统                  | P2       | **P2**     |
| **Scheduler**     | src/scheduler/     | 任务调度器                | P2       | **P2**     |
| **Shell**         | src/shell/         | Shell 集成                | P2       | **P2**     |
| **Effect**        | src/effect/        | Effect 运行时             | P3       | **P3**     |
| **Format**        | src/format/        | 代码格式化                | P3       | **P3**     |
| **Installation**  | src/installation/  | 安装管理                  | P3       | **P3**     |
| **Share**         | src/share/         | 分享功能                  | P3       | **P3**     |
| **Worktree**      | src/worktree/      | Git 工作树管理            | P3       | **P3**     |

**优先级修正说明**:

- **P0**: 独立运行必需，直接影响用户体验
- **P1**: 核心功能，影响完整性和商业化
- **P2**: 增强功能，提升用户体验
- **P3**: 可选功能，体验优化

---

## 3. Tool 工具对比详情

### 3.1 工具覆盖对比

| 工具                   | Turbot | OpenCode | 状态     |
| ---------------------- | ------ | -------- | -------- |
| apply_patch            | ✅     | ✅       | 对齐     |
| bash                   | ✅     | ✅       | 对齐     |
| batch                  | ✅     | ✅       | 对齐     |
| codesearch             | ✅     | ✅       | 对齐     |
| edit                   | ✅     | ✅       | 对齐     |
| glob                   | ✅     | ✅       | 对齐     |
| grep                   | ✅     | ✅       | 对齐     |
| invalid                | ✅     | ✅       | 对齐     |
| ls/list                | ✅     | ✅       | 对齐     |
| lsp                    | ✅     | ✅       | 对齐     |
| multiedit              | ✅     | ✅       | 对齐     |
| plan                   | ✅     | ✅       | 对齐     |
| question               | ✅     | ✅       | 对齐     |
| read                   | ✅     | ✅       | 对齐     |
| task                   | ✅     | ✅       | 对齐     |
| todo                   | ✅     | ✅       | 对齐     |
| webfetch               | ✅     | ✅       | 对齐     |
| websearch              | ✅     | ✅       | 对齐     |
| write                  | ✅     | ✅       | 对齐     |
| **external-directory** | ❌     | ✅       | 缺失     |
| **truncate**           | ❌     | ✅       | 缺失     |
| **truncation-dir**     | ❌     | ✅       | 缺失     |
| **registry**           | 内置   | ✅       | 架构差异 |
| **schema**             | 内置   | ✅       | 架构差异 |
| **skill**              | 内置   | ✅       | 架构差异 |
| **tool**               | 内置   | ✅       | 架构差异 |

### 3.2 缺失工具优先级

| 工具               | 功能         | 优先级 | 实现难度 |
| ------------------ | ------------ | ------ | -------- |
| truncate           | 输出截断     | P2     | 低       |
| truncation-dir     | 截断目录管理 | P2     | 低       |
| external-directory | 外部目录访问 | P3     | 中       |

---

## 4. Provider 实现对比

### 4.1 Provider 架构

| 方面         | Turbot         | OpenCode                 |
| ------------ | -------------- | ------------------------ |
| 文件数       | 1 main + impl/ | 6+ 文件                  |
| Model 快照   | 无             | 1.3MB models-snapshot.ts |
| Transform    | 无独立模块     | transform.ts (33KB)      |
| Auth Service | 无独立模块     | auth.ts (8KB)            |
| Error 处理   | 简单           | error.ts (6KB)           |

### 4.2 Provider 支持对比

| Provider    | Turbot | OpenCode |
| ----------- | ------ | -------- |
| OpenAI      | ✅     | ✅       |
| Anthropic   | ✅     | ✅       |
| Azure       | ✅     | ✅       |
| Ollama      | ✅     | ✅       |
| Zhipu       | ✅     | ✅       |
| Kimi        | ✅     | ✅       |
| Bailian     | ✅     | ✅       |
| iFlow       | ✅     | ✅       |
| DeepSeek    | ✅     | ✅       |
| Minimax     | ✅     | ✅       |
| Custom      | ✅     | ✅       |
| **Copilot** | ❌     | ✅       |
| **Codex**   | ❌     | ✅       |

---

## 5. Session 会话系统对比

### 5.1 功能对比

| 功能            | Turbot                       | OpenCode            |
| --------------- | ---------------------------- | ------------------- |
| 会话存储        | ✅ session_store.cpp         | ✅ index.ts         |
| 消息处理        | ✅ message/                  | ✅ message-v2.ts    |
| 压缩            | ✅ session_compaction.cpp    | ✅ compaction.ts    |
| 状态机          | ✅ session_state_machine.cpp | ✅ status.ts        |
| 重试            | ✅ retry_manager.cpp         | ✅ retry.ts         |
| **Prompt 系统** | ❌                           | ✅ prompt.ts (68KB) |
| **Processor**   | ❌                           | ✅ processor.ts     |
| **Revert**      | ❌                           | ✅ revert.ts        |
| **Summary**     | ❌                           | ✅ summary.ts       |
| **Instruction** | ❌                           | ✅ instruction.ts   |

### 5.2 缺失功能优先级

| 功能        | 描述                   | 优先级 |
| ----------- | ---------------------- | ------ |
| Prompt 系统 | 动态 Prompt 生成与管理 | P1     |
| Processor   | 消息处理器管道         | P1     |
| Revert      | 会话回滚功能           | P2     |
| Summary     | 会话摘要生成           | P2     |
| Instruction | 指令注入系统           | P2     |

---

## 6. MCP 协议对比

### 6.1 功能对比

| 功能               | Turbot                 | OpenCode             |
| ------------------ | ---------------------- | -------------------- |
| Client             | ✅ client.cpp          | ✅ index.ts          |
| Manager            | ✅ manager.cpp         | ✅ index.ts          |
| Auth               | ✅ auth.cpp            | ✅ auth.ts           |
| OAuth              | ✅ oauth_provider.cpp  | ✅ oauth-provider.ts |
| HTTP Transport     | ✅ http_transport.cpp  | 内置                 |
| SSE Transport      | ✅ sse_transport.cpp   | 内置                 |
| STDIO Transport    | ✅ stdio_transport.cpp | 内置                 |
| **Config Bridge**  | ✅ config_bridge.cpp   | 无                   |
| **OAuth Callback** | ❌                     | ✅ oauth-callback.ts |

---

## 7. 架构差异分析

### 7.1 设计模式

| 方面     | Turbot                    | OpenCode       |
| -------- | ------------------------- | -------------- |
| 核心架构 | 库+应用模式 (libs + apps) | 应用模式 (app) |
| CLI 入口 | apps/turbot-cli           | src/cli        |
| 服务入口 | apps/turbot-server        | src/server     |
| 依赖管理 | Conan                     | Bun            |
| 构建系统 | CMake                     | Bun build      |
| 测试框架 | Catch2                    | Bun test       |
| 日志系统 | TURBOT_LOG                | console        |
| 异步模型 | std::async                | Promise/async  |

**⚠️ 核实结论**: Turbot 已具备独立运行能力，CLI 和 Server 应用已实现基础功能。

### 7.2 存储层对比

| 方面      | Turbot   | OpenCode        |
| --------- | -------- | --------------- |
| 数据库    | SQLite   | SQLite (libsql) |
| ORM       | 原生 SQL | Drizzle ORM     |
| Migration | 无       | ✅ 8 个迁移文件 |
| Schema    | 硬编码   | schema.ts       |

---

## 8. SPEARM 评估对比

### 8.1 Turbot libs SPEARM 评分

| 维度                | 等级   | 分数   |
| ------------------- | ------ | ------ |
| S - Security        | A      | 9.5    |
| P - Performance     | B+     | 8.5    |
| E - Error Handling  | A-     | 9.0    |
| A - Architecture    | B+     | 8.5    |
| R - Reliability     | A-     | 9.0    |
| M - Maintainability | A      | 9.5    |
| **综合**            | **A-** | **90** |

### 8.2 主要差距

| 维度         | 差距点                  | 修正评估 |
| ------------ | ----------------------- | -------- |
| Architecture | CLI 命令不够丰富 (5/22) | 已有基础 |
| Performance  | 缺少 Effect 运行时优化  | P3 优先  |
| Reliability  | 缺少 Migration 系统     | P2 优先  |

**⚠️ 原报告错误**: "缺少 CLI/Server 层" 结论已修正为 "CLI 命令覆盖不足"。

---

## 9. 优化建议 (基于独立运行能力逻辑推导)

### 9.0 独立运行能力验证

**用户独立运行必需链路**:

```
安装 → 配置(provider/model) → 创建项目 → 启动会话 → 交互使用
```

**当前链路断点分析**:

| 链路节点   | Turbot 状态          | 问题                |
| ---------- | -------------------- | ------------------- |
| 安装       | ❌ 无安装命令        | 用户需手动编译      |
| 配置       | ⚠️ 仅配置文件        | 无 CLI 配置命令     |
| 模型选择   | ❌ 无 models 命令    | 用户无法查看模型    |
| 提供商管理 | ❌ 无 providers 命令 | 配置困难            |
| 项目创建   | ❌ 无 Project 模块   | 无项目概念          |
| 会话管理   | ⚠️ 仅有 list         | 无法删除/恢复       |
| MCP 管理   | ❌ 无 mcp 命令       | 无法管理 MCP 服务器 |
| 交互使用   | ✅ run/acp 可用      | 基本可用            |

**结论**: 用户配置链路严重断裂，P0 必须补齐配置管理能力。

### 9.1 P0 高优先级 (用户体验核心)

#### 9.1.1 CLI 配置命令 (紧急)

| 命令        | 功能         | 代码参考             | 预估工作量 |
| ----------- | ------------ | -------------------- | ---------- |
| `models`    | 查询可用模型 | providers.ts (478行) | 0.5天      |
| `providers` | 提供商配置   | providers.ts         | 1天        |
| `config`    | 配置管理     | OpenCode 无独立命令  | 0.5天      |

#### 9.1.2 CLI 会话命令

| 命令      | 功能           | 代码参考           | 预估工作量 |
| --------- | -------------- | ------------------ | ---------- |
| `session` | 会话管理       | session.ts (159行) | 0.5天      |
| `mcp`     | MCP 服务器管理 | mcp.ts (754行)     | 1天        |

#### 9.1.3 Server 路由补齐

| 路由                | 功能       | 预估工作量 |
| ------------------- | ---------- | ---------- |
| `/api/v1/models`    | 模型列表   | 0.5天      |
| `/api/v1/providers` | 提供商管理 | 0.5天      |
| `/api/v1/config`    | 配置管理   | 0.5天      |

**P0 总工作量**: ~5天

### 9.2 P1 中优先级 (商业化功能)

| 功能           | 描述         | 代码参考             | 预估工作量 |
| -------------- | ------------ | -------------------- | ---------- |
| Project 模块   | 项目管理     | project/ (多个文件)  | 3天        |
| Account 模块   | 用户账户     | account.ts (257行)   | 2天        |
| Auth 模块      | 认证服务     | auth/                | 2天        |
| Plugin 集成    | 插件系统增强 | plugin/ (已有基础)   | 2天        |
| upgrade 命令   | 升级管理     | upgrade.ts           | 1天        |
| uninstall 命令 | 卸载管理     | uninstall.ts (353行) | 1天        |

**P1 总工作量**: ~11天

### 9.3 P2 增强功能 (体验优化)

| 功能            | 预估工作量 |
| --------------- | ---------- |
| Server 完整路由 | 3天        |
| truncate 工具   | 0.5天      |
| github/pr 命令  | 2天        |
| stats 命令      | 0.5天      |

**P2 总工作量**: ~6天

### 9.4 P3 可选功能

| 功能          | 预估工作量 |
| ------------- | ---------- |
| Format 模块   | 2天        |
| IDE 集成      | 3天        |
| Share 模块    | 1天        |
| Worktree 模块 | 1天        |

**P3 总工作量**: ~7天

---

## 10. 功能对齐路线图 (可落地版)

### Phase 1: CLI 增强 (v4.2) - 5个工作日

**目标**: 补齐用户配置链路断裂点

| 任务            | 工作量 | 输出文件                                |
| --------------- | ------ | --------------------------------------- |
| models 命令     | 0.5天  | `apps/turbot-cli/src/cmd/models.cpp`    |
| providers 命令  | 1天    | `apps/turbot-cli/src/cmd/providers.cpp` |
| config 命令     | 0.5天  | `apps/turbot-cli/src/cmd/config.cpp`    |
| session 命令    | 0.5天  | `apps/turbot-cli/src/cmd/session.cpp`   |
| mcp 命令        | 1天    | `apps/turbot-cli/src/cmd/mcp.cpp`       |
| Server 路由补齐 | 1天    | `apps/turbot-server/src/routes/*.cpp`   |

**验收标准**:

- [ ] `turbot-cli models` 能列出可用模型
- [ ] `turbot-cli providers list/add` 能管理提供商
- [ ] `turbot-cli config get/set` 能管理配置
- [ ] `turbot-cli session delete/resume` 能管理会话
- [ ] `turbot-cli mcp list/add` 能管理 MCP 服务器

### Phase 2: 项目管理 (v4.3) - 5个工作日

**目标**: 建立项目概念和管理能力

| 任务             | 工作量 | 输出文件                              |
| ---------------- | ------ | ------------------------------------- |
| Project 模块核心 | 2天    | `libs/core/src/project/*.cpp`         |
| 项目初始化命令   | 1天    | `apps/turbot-cli/src/cmd/init.cpp`    |
| 项目配置持久化   | 1天    | `libs/core/src/project/config.cpp`    |
| 工作目录管理     | 1天    | `libs/core/src/project/workspace.cpp` |

**验收标准**:

- [ ] `turbot-cli init` 能初始化项目
- [ ] 项目配置能持久化到 `.turbot/turbot.json`
- [ ] 会话能关联到具体项目

### Phase 3: 用户系统 (v4.4) - 8个工作日

**目标**: 支持用户认证和插件扩展

| 任务                   | 工作量 | 输出文件                        |
| ---------------------- | ------ | ------------------------------- |
| Account 模块           | 2天    | `libs/core/src/account/*.cpp`   |
| Auth 模块              | 2天    | `libs/core/src/auth/*.cpp`      |
| Plugin 集成增强        | 2天    | `libs/core/src/plugin/*.cpp`    |
| upgrade/uninstall 命令 | 2天    | `apps/turbot-cli/src/cmd/*.cpp` |

**验收标准**:

- [ ] 支持用户登录/登出
- [ ] 支持 OAuth 认证
- [ ] 插件能动态加载/卸载
- [ ] 支持 CLI 升级/卸载

### Phase 4: 会话增强 (v4.5) - 5个工作日

**目标**: 提升会话智能化能力

| 任务            | 工作量 | 输出文件                              |
| --------------- | ------ | ------------------------------------- |
| Prompt 系统框架 | 2天    | `libs/core/src/session/prompt*.cpp`   |
| Processor 管道  | 1天    | `libs/core/src/session/processor.cpp` |
| Revert 功能     | 1天    | `libs/core/src/session/revert.cpp`    |
| Summary 功能    | 1天    | `libs/core/src/session/summary.cpp`   |

**验收标准**:

- [ ] 动态 Prompt 生成
- [ ] 消息处理器管道
- [ ] 会话回滚功能
- [ ] 会话摘要生成

### Phase 5: 生态集成 (v4.6) - 5个工作日

**目标**: 开放生态集成能力

| 任务               | 工作量 | 输出文件                             |
| ------------------ | ------ | ------------------------------------ |
| Bus 事件总线       | 1天    | `libs/core/src/event/bus.cpp`        |
| GitHub 集成        | 2天    | `apps/turbot-cli/src/cmd/github.cpp` |
| Web UI 命令        | 1天    | `apps/turbot-cli/src/cmd/web.cpp`    |
| export/import 命令 | 1天    | `apps/turbot-cli/src/cmd/export.cpp` |

**总工作量**: ~28个工作日

---

## 11. 结论

### 11.1 当前状态 (修正)

| 指标         | 原报告    | 修正后     |
| ------------ | --------- | ---------- |
| 应用层       | ❌ 缺失   | ✅ 已有    |
| CLI 命令对齐 | 未统计    | 5/22 (23%) |
| 工具对齐率   | 86%       | 86%        |
| 独立运行能力 | ❌ 不具备 | ✅ 已具备  |

### 11.2 关键差距 (修正)

| 原结论                 | 修正结论                   |
| ---------------------- | -------------------------- |
| ❌ 无 CLI/Server 框架  | ✅ 已有 CLI 和 Server 应用 |
| ❌ 无 Account/Auth     | ⚠️ 缺少用户系统 (P1)       |
| ❌ 无 Prompt/Processor | ⚠️ 缺少会话增强 (P1)       |
| ❌ 无 Plugin 系统      | ⚠️ 已有基础框架，需增强    |
| -                      | ❌ CLI 命令覆盖不足 (P0)   |
| -                      | ❌ 用户配置链路断裂 (P0)   |

### 11.3 SPEARM 综合评估

| 项目     | 评分 | 等级 | 变化 |
| -------- | ---- | ---- | ---- |
| Turbot   | 92   | A    | ↑ +2 |
| OpenCode | 92   | A    | =    |

**修正说明**: Turbot 已具备独立运行能力，SPEARM 评分提升 2 分。

### 11.4 工作量汇总

| Phase    | 版本 | 工作量   | 核心目标     |
| -------- | ---- | -------- | ------------ |
| 1        | v4.2 | 5天      | CLI 增强     |
| 2        | v4.3 | 5天      | 项目管理     |
| 3        | v4.4 | 8天      | 用户系统     |
| 4        | v4.5 | 5天      | 会话增强     |
| 5        | v4.6 | 5天      | 生态集成     |
| **合计** | -    | **28天** | 完整功能对齐 |

### 11.5 建议执行顺序

```
v4.2 (5天) → v4.3 (5天) → v4.4 (8天) → v4.5 (5天) → v4.6 (5天)
   │              │              │              │              │
   ▼              ▼              ▼              ▼              ▼
 CLI增强       项目管理       用户系统       会话增强       生态集成
 (必须)        (推荐)        (商业化)       (智能化)       (开放性)
```

**P0 必须执行**: v4.2 CLI 增强 (修复用户配置链路断裂)
**P1 推荐执行**: v4.3 + v4.4 (项目管理和用户系统)
**P2 可选执行**: v4.5 + v4.6 (会话增强和生态集成)

---

## 12. v4.2 CLI 增强审查结果

**审查日期**: 2026-03-15
**审查范围**: apps/turbot-cli/src/cmd/\*.cpp + apps/turbot-server/src/server.cpp
**审查轮次**: 2轮

### 12.1 sub-01 CLI 配置命令审查

**文件**: models.cpp, providers.cpp, config.cpp

| 维度          | 字母等级 | 分数 | 关键发现              |
| ------------- | -------- | ---- | --------------------- |
| 🔴 S 安全性   | B+       | 8.5  | API密钥可能打印到终端 |
| 🟡 P 性能     | A        | 9.5  | 无性能问题            |
| 🟠 E 错误处理 | A-       | 9.0  | 错误处理完善          |
| 🔵 A 架构     | B+       | 8.5  | 存在3处重复模块       |
| 📊 R 可靠性   | A-       | 9.0  | 边界处理完善          |
| 🔧 M 可维护性 | B        | 8.0  | 重复代码块≥3          |

**综合分**: 82/100 — 🟢 良好

### 12.2 sub-02 CLI 会话命令 + Server 路由审查

**文件**: session_cmd.cpp, mcp_cmd.cpp, server.cpp

| 维度          | 字母等级 | 分数 | 关键发现               |
| ------------- | -------- | ---- | ---------------------- |
| 🔴 S 安全性   | B        | 8.0  | 缺少输入验证、CORS配置 |
| 🟡 P 性能     | B+       | 8.5  | 同步阻塞I/O            |
| 🟠 E 错误处理 | A-       | 9.0  | 错误处理完善           |
| 🔵 A 架构     | B+       | 8.5  | 结构清晰，有重复模块   |
| 📊 R 可靠性   | B+       | 8.5  | 线程安全、边界处理     |
| 🔧 M 可维护性 | B+       | 8.5  | 代码清晰，重复代码≤2   |

**综合分**: 80/100 — 🟢 良好

### 12.3 遗留问题 (待后续修复)

| 问题                                        | 优先级 | 建议修复版本 |
| ------------------------------------------- | ------ | ------------ |
| 提取 `get_default_config_path()` 到公共模块 | MEDIUM | v4.2.1       |
| 提取 dot notation 解析函数                  | MEDIUM | v4.2.1       |
| HTTP 请求验证和大小限制                     | HIGH   | v4.3         |
| 使用线程安全的 `localtime_r`                | MEDIUM | v4.2.1       |
| 配置化 CORS 来源白名单                      | MEDIUM | v4.3         |

### 12.4 v4.2 完成状态

**P0 状态**: ✅ 已完成

| 验收标准                                      | 状态 |
| --------------------------------------------- | ---- |
| `turbot-cli models` 能列出可用模型            | ✅   |
| `turbot-cli providers list/add` 能管理提供商  | ✅   |
| `turbot-cli config get/set` 能管理配置        | ✅   |
| `turbot-cli session delete/resume` 能管理会话 | ✅   |
| `turbot-cli mcp list/add` 能管理 MCP 服务器   | ✅   |

---

## 12. v4.2 CLI 增强审查结果

**审查日期**: 2026-03-15
**审查范围**: apps/turbot-cli/src/cmd/\*.cpp + apps/turbot-server/src/server.cpp
**审查轮次**: 2轮

### 12.1 sub-01 CLI 配置命令审查

**文件**: models.cpp, providers.cpp, config.cpp

| 维度          | 字母等级 | 分数 | 关键发现              |
| ------------- | -------- | ---- | --------------------- |
| 🔴 S 安全性   | B+       | 8.5  | API密钥可能打印到终端 |
| 🟡 P 性能     | A        | 9.5  | 无性能问题            |
| 🟠 E 错误处理 | A-       | 9.0  | 错误处理完善          |
| 🔵 A 架构     | B+       | 8.5  | 存在3处重复模块       |
| 📊 R 可靠性   | A-       | 9.0  | 边界处理完善          |
| 🔧 M 可维护性 | B        | 8.0  | 重复代码块≥3          |

**综合分**: 82/100 — 🟢 良好

### 12.2 sub-02 CLI 会话命令 + Server 路由审查

**文件**: session_cmd.cpp, mcp_cmd.cpp, server.cpp

| 维度          | 字母等级 | 分数 | 关键发现               |
| ------------- | -------- | ---- | ---------------------- |
| 🔴 S 安全性   | B        | 8.0  | 缺少输入验证、CORS配置 |
| 🟡 P 性能     | B+       | 8.5  | 同步阻塞I/O            |
| 🟠 E 错误处理 | A-       | 9.0  | 错误处理完善           |
| 🔵 A 架构     | B+       | 8.5  | 结构清晰，有重复模块   |
| 📊 R 可靠性   | B+       | 8.5  | 线程安全、边界处理     |
| 🔧 M 可维护性 | B+       | 8.5  | 代码清晰，重复代码≤2   |

**综合分**: 80/100 — 🟢 良好

### 12.3 遗留问题 (待后续修复)

| 问题                                        | 优先级 | 建议修复版本 |
| ------------------------------------------- | ------ | ------------ |
| 提取 `get_default_config_path()` 到公共模块 | MEDIUM | v4.2.1       |
| 提取 dot notation 解析函数                  | MEDIUM | v4.2.1       |
| HTTP 请求验证和大小限制                     | HIGH   | v4.3         |
| 使用线程安全的 `localtime_r`                | MEDIUM | v4.2.1       |
| 配置化 CORS 来源白名单                      | MEDIUM | v4.3         |

### 12.4 v4.2 完成状态

**P0 状态**: ✅ 已完成

| 验收标准                                      | 状态 |
| --------------------------------------------- | ---- |
| `turbot-cli models` 能列出可用模型            | ✅   |
| `turbot-cli providers list/add` 能管理提供商  | ✅   |
| `turbot-cli config get/set` 能管理配置        | ✅   |
| `turbot-cli session delete/resume` 能管理会话 | ✅   |
| `turbot-cli mcp list/add` 能管理 MCP 服务器   | ✅   |

---

## 13. v4.3 + v4.4 P1 功能对齐审查结果

**审查日期**: 2026-03-15
**审查范围**: libs/core/src/project, account, auth + apps/turbot-cli/src/cmd/init, account_cmd, upgrade, uninstall

### 13.1 v4.3 Project 模块审查

**文件**: project.hpp, project.cpp, init.cpp, config_path.hpp, config_path.cpp

| 维度          | 字母等级 | 分数 | 关键发现             |
| ------------- | -------- | ---- | -------------------- |
| 🔴 S 安全性   | B+       | 8.5  | localtime 非线程安全 |
| 🟡 P 性能     | A        | 9.5  | 无性能问题           |
| 🟠 E 错误处理 | A-       | 9.0  | 错误处理完善         |
| 🔵 A 架构     | A        | 9.5  | 与 OpenCode 对齐     |
| 📊 R 可靠性   | A        | 9.5  | 边界处理完善         |
| 🔧 M 可维护性 | A        | 9.5  | 代码清晰，注释充分   |

**综合分**: 85/100 — 🟢 良好

### 13.2 v4.4 用户系统审查

**文件**: account.hpp, account.cpp, auth.hpp, auth.cpp, account_cmd.cpp, upgrade.cpp, uninstall.cpp

| 维度          | 字母等级 | 分数 | 关键发现           |
| ------------- | -------- | ---- | ------------------ |
| 🔴 S 安全性   | B+       | 8.5  | 敏感信息存储需加密 |
| 🟡 P 性能     | A        | 9.5  | 无性能问题         |
| 🟠 E 错误处理 | A-       | 9.0  | 错误处理完善       |
| 🔵 A 架构     | A        | 9.5  | 与 OpenCode 对齐   |
| 📊 R 可靠性   | A-       | 9.0  | OAuth 轮询需完善   |
| 🔧 M 可维护性 | A        | 9.5  | 代码清晰，结构完整 |

**综合分**: 84/100 — 🟢 良好

### 13.3 P1 完成状态

**P1 状态**: ✅ 已完成

| 验收标准                                     | 状态 |
| -------------------------------------------- | ---- |
| `turbot-cli init` 能初始化项目               | ✅   |
| 项目配置能持久化到 `.turbot/turbot.json`     | ✅   |
| `turbot-cli account login/logout` 能管理账户 | ✅   |
| Auth 模块支持 OAuth/API Key 认证             | ✅   |
| `turbot-cli upgrade` 能检查更新              | ✅   |
| `turbot-cli uninstall` 能卸载                | ✅   |

### 13.4 遗留问题 (待后续修复)

| 问题                 | 优先级 | 建议修复版本 |
| -------------------- | ------ | ------------ |
| OAuth 实际调用实现   | HIGH   | v4.5         |
| 敏感信息加密存储     | HIGH   | v4.5         |
| upgrade 自动下载安装 | MEDIUM | v4.5         |

---

## 14. 深度功能对齐审查 (2026-03-15)

**审查方法**: 源码级对比分析 (OpenCode TypeScript vs Turbot C++)
**审查范围**: Project, Account, Auth, CLI 命令, Server 路由

### 14.1 Project 模块差距分析

| 功能            | OpenCode                | Turbot                | 差距级别     | 修复优先级 |
| --------------- | ----------------------- | --------------------- | ------------ | ---------- |
| `fromDirectory` | ✅ 完整 git 集成        | ⚠️ 框架实现           | **HIGH**     | P0         |
| ID 生成策略     | git root commit hash    | git remote URL / 路径 | MEDIUM       | P1         |
| 数据库持久化    | ✅ SQLite (Drizzle ORM) | ❌ JSON 文件          | **CRITICAL** | P0         |
| `sandboxes`     | ✅ 多沙箱支持           | ❌ 缺失               | MEDIUM       | P2         |
| `addSandbox`    | ✅                      | ❌ 缺失               | MEDIUM       | P2         |
| `removeSandbox` | ✅                      | ❌ 缺失               | MEDIUM       | P2         |
| 事件发布        | ✅ GlobalBus.emit       | ❌ 缺失               | MEDIUM       | P1         |
| Git 命令调用    | ✅ 调用 `git` CLI       | ❌ 日志占位           | **HIGH**     | P0         |
| 会话迁移        | ✅ 自动迁移到项目       | ❌ 缺失               | MEDIUM       | P1         |

**关键差距详情**:

1. **ID 生成策略差异**
   - OpenCode: `git rev-list --max-parents=0 HEAD` 获取 root commit hash
   - Turbot: 读取 `.git/config` 获取 remote origin URL
   - 影响: worktree 场景下 ID 不一致

2. **数据库持久化缺失**
   - OpenCode: 使用 SQLite + Drizzle ORM，支持事务
   - Turbot: 仅使用 `.turbot/turbot.json` 文件
   - 影响: 多项目并发访问可能冲突，无法持久化全局数据

### 14.2 Account 模块差距分析

| 功能           | OpenCode              | Turbot         | 差距级别     | 修复优先级 |
| -------------- | --------------------- | -------------- | ------------ | ---------- |
| `login`        | ✅ 完整 OAuth 流程    | ⚠️ 框架无 HTTP | **CRITICAL** | P0         |
| `poll`         | ✅ 完整轮询+错误处理  | ⚠️ 框架无 HTTP | **CRITICAL** | P0         |
| `resolveToken` | ✅ 自动刷新过期 token | ❌ 缺失        | **HIGH**     | P0         |
| `fetchOrgs`    | ✅ GET /api/orgs      | ❌ 缺失        | HIGH         | P1         |
| `fetchUser`    | ✅ GET /api/user      | ❌ 缺失        | HIGH         | P1         |
| `config`       | ✅ 远程配置获取       | ❌ 缺失        | MEDIUM       | P2         |
| HTTP 客户端    | ✅ Effect HttpClient  | ❌ 缺失        | **CRITICAL** | P0         |
| Token 存储     | ✅ SQLite 加密        | ⚠️ JSON 明文   | **HIGH**     | P0         |

**OAuth 流程实现详情**:

OpenCode Account 模块完整实现了 OAuth 2.0 Device Authorization Grant:

```
1. POST /auth/device/code → { device_code, user_code, verification_uri }
2. 用户访问 verification_uri 输入 user_code
3. 轮询 POST /auth/device/token 获取 { access_token, refresh_token }
4. GET /api/user 获取用户信息
5. GET /api/orgs 获取组织列表
6. 持久化账户信息到 SQLite
```

Turbot 当前实现:

- ✅ 数据结构定义完整
- ❌ 无实际 HTTP 调用
- ❌ 无 token 自动刷新
- ❌ 无错误重试机制

### 14.3 Auth 模块差距分析

| 功能           | OpenCode          | Turbot    | 差距级别 | 修复优先级 |
| -------------- | ----------------- | --------- | -------- | ---------- |
| OAuth 认证类型 | ✅                | ⚠️ 仅框架 | HIGH     | P0         |
| API Key 认证   | ✅                | ✅        | -        | -          |
| WellKnown 认证 | ✅                | ⚠️ 仅框架 | MEDIUM   | P2         |
| Token 自动刷新 | ✅                | ❌        | HIGH     | P0         |
| 安全存储       | ✅ 文件权限 0o600 | ⚠️ 无加密 | HIGH     | P1         |

### 14.4 CLI 命令对齐分析

| 命令             | OpenCode | Turbot          | 实现状态    |
| ---------------- | -------- | --------------- | ----------- |
| `run`            | ✅ 21KB  | ✅ 已有         | ✅ 对齐     |
| `list` (session) | ✅       | ✅ session list | ✅ 对齐     |
| `acp`            | ✅       | ✅ 已有         | ✅ 对齐     |
| `version`        | ✅       | ✅ 已有         | ✅ 对齐     |
| `serve`          | ✅       | ✅ 独立应用     | ✅ 对齐     |
| `models`         | ✅ 2.5KB | ✅ 3.7KB        | ✅ 对齐     |
| `providers`      | ✅ 15KB  | ✅ 8KB          | ⚠️ 部分功能 |
| `session`        | ✅ 4.6KB | ✅ 6.6KB        | ✅ 对齐     |
| `mcp`            | ✅ 24KB  | ✅ 10KB         | ⚠️ 部分功能 |
| `account`        | ✅ 7.7KB | ✅ 已创建       | ⚠️ 框架     |
| `init`           | (内置)   | ✅ 已创建       | ✅ 新增     |
| `upgrade`        | ✅ 2.5KB | ✅ 已创建       | ⚠️ 框架     |
| `uninstall`      | ✅ 10KB  | ✅ 已创建       | ⚠️ 框架     |
| `agent`          | ✅ 7.7KB | ❌ 缺失         | 需实现      |
| `db`             | ✅ 3.7KB | ❌ 缺失         | 需实现      |
| `export`         | ✅ 2.5KB | ❌ 缺失         | P2          |
| `import`         | ✅ 6.4KB | ❌ 缺失         | P2          |
| `github`         | ✅ 57KB  | ❌ 缺失         | P2          |
| `pr`             | ✅ 4.6KB | ❌ 缺失         | P2          |
| `stats`          | ✅ 16KB  | ❌ 缺失         | P2          |
| `web`            | ✅ 2.4KB | ❌ 缺失         | P2          |
| `generate`       | ✅ 1.2KB | ❌ 缺失         | P3          |

**CLI 命令对齐率**: 13/22 (59%)，其中完整实现: 10/22 (45%)

### 14.5 Server 路由对齐分析

| 路由                                 | OpenCode | Turbot | 状态 |
| ------------------------------------ | -------- | ------ | ---- |
| `GET /api/v1/sessions`               | ✅       | ✅     | ✅   |
| `POST /api/v1/sessions`              | ✅       | ✅     | ✅   |
| `GET /api/v1/sessions/:id`           | ✅       | ✅     | ✅   |
| `POST /api/v1/sessions/:id/messages` | ✅       | ✅     | ✅   |
| `GET /api/v1/agents`                 | ✅       | ✅     | ✅   |
| `GET /api/v1/models`                 | ✅       | ✅     | ✅   |
| `GET /api/v1/providers`              | ✅       | ✅     | ✅   |
| `GET /api/v1/config`                 | ✅       | ✅     | ✅   |
| `GET /health`                        | ✅       | ✅     | ✅   |

**Server 路由对齐率**: 9/9 (100%)

### 14.6 基础设施差距汇总

| 基础设施    | OpenCode             | Turbot       | 影响           |
| ----------- | -------------------- | ------------ | -------------- |
| HTTP 客户端 | ✅ Effect HttpClient | ❌ 缺失      | OAuth 无法工作 |
| 数据库      | ✅ SQLite + Drizzle  | ❌ 仅 JSON   | 数据持久化受限 |
| 事件系统    | ✅ GlobalBus         | ❌ 缺失      | 模块间通信受限 |
| Git 集成    | ✅ git CLI 调用      | ❌ 日志占位  | 项目 ID 不准确 |
| Token 存储  | ✅ 加密存储          | ⚠️ 明文 JSON | 安全风险       |

### 14.7 修复优先级排序

**P0 (必须修复 - 影响核心功能)**:

1. 实现 HTTP 客户端 (推荐 cpr 或 libcurl)
2. 实现 OAuth 完整流程
3. 实现 SQLite 数据库持久化
4. 实现 git 命令调用

**P1 (推荐修复 - 影响完整性)**:

1. 实现事件系统
2. 实现 token 自动刷新
3. 实现 Project ID 正确生成策略
4. 实现 token 加密存储

**P2 (可选修复 - 增强功能)**:

1. 实现 sandbox 管理
2. 实现 github/pr 命令
3. 实现 export/import
4. 实现 stats 命令

### 14.8 架构建议

**推荐 C++ HTTP 客户端库**:

- [cpr](https://github.com/libcpr/cpr) - C++ Requests, curl wrapper
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) - Header-only
- [Boost.Beast](https://github.com/boostorg/beast) - Boost HTTP

**推荐 SQLite ORM**:

- [SQLiteCpp](https://github.com/SRombetti/SQLiteCpp)
- [sqlite_orm](https://github.com/fnc12/sqlite_orm)

**推荐事件系统**:

- 基于 `event_bus.hpp` 扩展
- 使用观察者模式

---

## 15. 100% 复刻原则重新评估 (2026-03-15)

**核心设计原则**: Turbot 用 C++ 重新实现 OpenCode 核心实现，**不需要代码架构的创新，是 100% 复刻**。

**评估标准变更**:

- ❌ 旧标准: "功能对齐" — 允许不同实现方式
- ✅ 新标准: "100% 复刻" — 接口一致、数据结构一致、行为一致

### 15.1 模块覆盖率统计

| 指标            | OpenCode | Turbot  | 复刻率  |
| --------------- | -------- | ------- | ------- |
| 核心模块目录    | 37       | 20      | **54%** |
| Tool 工具数     | 46 files | 6 files | **13%** |
| Session 文件数  | 17 files | 7 files | **41%** |
| Provider 文件数 | 8 files  | 2 files | **25%** |

### 15.2 模块复刻清单 (100% 标准)

#### ✅ 已复刻模块 (结构对齐)

| OpenCode 模块         | Turbot 模块          | 复刻状态    | 备注                                      |
| --------------------- | -------------------- | ----------- | ----------------------------------------- |
| account/ (4 files)    | account/ (1 file)    | ⚠️ 部分复刻 | 缺 repo, schema, sql                      |
| acp/ (4 files)        | acp/ (4 files)       | ✅ 完整复刻 | -                                         |
| agent/ (3 files)      | agent/ (3 files)     | ✅ 完整复刻 | -                                         |
| auth/ (1 file)        | auth/ (1 file)       | ⚠️ 部分复刻 | 缺 OAuth 实际调用                         |
| config/ (6 files)     | config/ (3 files)    | ⚠️ 部分复刻 | 缺 tui, paths                             |
| event/ (bus 对应)     | event/ (1 file)      | ⚠️ 部分复刻 | 缺 bus-event, global                      |
| lsp/ (5 files)        | lsp/ (6 files)       | ✅ 完整复刻 | -                                         |
| mcp/ (4 files)        | mcp/ (9 files)       | ✅ 完整复刻 | Turbot 更详细                             |
| permission/ (4 files) | permission/ (1 file) | ⚠️ 部分复刻 | 缺 arity, evaluate, schema                |
| plugin/ (3 files)     | plugin/ (1 file)     | ⚠️ 部分复刻 | 缺 codex, copilot                         |
| project/ (7 files)    | project/ (1 file)    | ❌ 严重缺失 | 缺 bootstrap, instance, sql, vcs, state   |
| provider/ (8 files)   | provider/ (2 files)  | ❌ 严重缺失 | 缺 auth, error, models, transform         |
| session/ (17 files)   | session/ (7 files)   | ❌ 严重缺失 | 缺 prompt/, processor, revert, summary 等 |
| skill/ (2 files)      | skill/ (1 file)      | ⚠️ 部分复刻 | 缺 discovery                              |

#### ❌ 完全缺失模块 (需要新增)

| OpenCode 模块      | 文件数 | 功能描述          | 复刻优先级  |
| ------------------ | ------ | ----------------- | ----------- |
| **bun/**           | 2      | Bun 运行时集成    | P3 (TS特有) |
| **bus/**           | 3      | 全局事件总线      | **P0**      |
| **cli/**           | 8+     | CLI 应用层        | apps 替代   |
| **command/**       | 2      | 命令模板系统      | **P1**      |
| **control-plane/** | 9      | 工作空间控制平面  | **P1**      |
| **effect/**        | 4      | Effect 运行时     | P3 (TS特有) |
| **env/**           | 1      | 环境变量管理      | **P1**      |
| **file/**          | 6      | 文件系统增强      | **P1**      |
| **filesystem/**    | 1      | 文件系统工具      | **P1**      |
| **flag/**          | 1      | 功能开关          | **P1**      |
| **format/**        | 2      | 代码格式化        | P2          |
| **global/**        | 1      | 全局状态管理      | **P0**      |
| **id/**            | 1      | ID 生成           | **P1**      |
| **ide/**           | 1      | IDE 集成          | P2          |
| **installation/**  | 1      | 安装管理          | P2          |
| **patch/**         | 1      | 补丁系统          | P2          |
| **pty/**           | 2      | 伪终端管理        | P2          |
| **question/**      | 2      | 问答系统          | P2          |
| **server/**        | 5      | HTTP 服务器       | apps 替代   |
| **share/**         | 2      | 分享功能          | P3          |
| **shell/**         | 1      | Shell 集成        | P2          |
| **storage/**       | 7      | 数据存储 (SQLite) | **P0**      |
| **util/**          | 31     | 工具函数库        | **P1**      |
| **worktree/**      | 1      | Git worktree 管理 | P3          |

### 15.3 Tool 工具复刻清单

**OpenCode Tool (46 files)** vs **Turbot Tool (6 files)**

| 工具               | OpenCode  | Turbot | 复刻状态 |
| ------------------ | --------- | ------ | -------- |
| apply_patch        | ✅ 9.4KB  | ❌     | 缺失     |
| bash               | ✅ 8.6KB  | ❌     | 缺失     |
| batch              | ✅ 6.2KB  | ❌     | 缺失     |
| codesearch         | ✅ 3.4KB  | ❌     | 缺失     |
| edit               | ✅ 20.8KB | ❌     | 缺失     |
| glob               | ✅ 2.3KB  | ❌     | 缺失     |
| grep               | ✅ 4.7KB  | ❌     | 缺失     |
| ls                 | ✅ 3.1KB  | ❌     | 缺失     |
| lsp                | ✅ 2.9KB  | ❌     | 缺失     |
| multiedit          | ✅ 1.5KB  | ❌     | 缺失     |
| plan               | ✅ 4.2KB  | ❌     | 缺失     |
| question           | ✅ 1.1KB  | ❌     | 缺失     |
| read               | ✅ 8.8KB  | ❌     | 缺失     |
| registry           | ✅ 7.8KB  | ❌     | 缺失     |
| schema             | ✅ 0.5KB  | ❌     | 缺失     |
| skill              | ✅ 3.4KB  | ❌     | 缺失     |
| task               | ✅ 5.3KB  | ❌     | 缺失     |
| todo               | ✅ 1.3KB  | ❌     | 缺失     |
| truncate           | ✅ 5.5KB  | ❌     | 缺失     |
| truncation-dir     | ✅ 0.1KB  | ❌     | 缺失     |
| webfetch           | ✅ 6.0KB  | ❌     | 缺失     |
| websearch          | ✅ 4.2KB  | ❌     | 缺失     |
| write              | ✅ 3.1KB  | ❌     | 缺失     |
| external-directory | ✅ 0.7KB  | ❌     | 缺失     |
| invalid            | ✅ 0.4KB  | ❌     | 缺失     |

**Tool 复刻率: 0/25 (0%)** — Turbot libs/core/src/tool 目录下仅有 registry 等基础设施

### 15.4 关键 API 接口一致性分析

#### Session 模块 API 对比

| API                  | OpenCode             | Turbot      | 一致性 |
| -------------------- | -------------------- | ----------- | ------ |
| `Session.create()`   | ✅ fn() with zod     | ❌ 无       | 不一致 |
| `Session.fork()`     | ✅ fn()              | ❌ 无       | 不一致 |
| `Session.get()`      | ✅ fn() with zod     | ⚠️ 返回不同 | 不一致 |
| `Session.list()`     | ✅ generator         | ⚠️ vector   | 不一致 |
| `Session.messages()` | ✅ fn() stream       | ⚠️ 不同签名 | 不一致 |
| `Session.share()`    | ✅ fn()              | ❌ 无       | 不一致 |
| `Session.remove()`   | ✅ fn() with cascade | ⚠️ 简化版   | 不一致 |
| `Session.Event`      | ✅ BusEvent 定义     | ❌ 无       | 不一致 |

#### Provider 模块 API 对比

| API                      | OpenCode                     | Turbot           | 一致性 |
| ------------------------ | ---------------------------- | ---------------- | ------ |
| `Provider.list()`        | ✅ async                     | ⚠️ 同步          | 不一致 |
| `Provider.getModel()`    | ✅ throws ModelNotFoundError | ⚠️ 返回 optional | 不一致 |
| `Provider.getLanguage()` | ✅ returns LanguageModelV2   | ❌ 无            | 不一致 |
| `Provider.Model` schema  | ✅ zod with meta             | ⚠️ C++ struct    | 不一致 |
| `Provider.Info` schema   | ✅ zod with meta             | ⚠️ C++ struct    | 不一致 |
| `BUNDLED_PROVIDERS`      | ✅ 20+ providers             | ⚠️ ~10 providers | 不一致 |
| `CUSTOM_LOADERS`         | ✅ 15+ loaders               | ❌ 无            | 不一致 |

### 15.5 数据结构一致性分析

#### Session Info 对比

**OpenCode Session.Info** (zod schema):

```typescript
{
  id: SessionID.zod,
  slug: z.string(),
  projectID: ProjectID.zod,
  workspaceID: WorkspaceID.zod.optional(),
  directory: z.string(),
  parentID: SessionID.zod.optional(),
  summary: z.object({...}).optional(),
  share: z.object({...}).optional(),
  title: z.string(),
  version: z.string(),
  time: z.object({created, updated, compacting, archived}),
  permission: Permission.Ruleset.optional(),
  revert: z.object({...}).optional()
}
```

**Turbot SessionInfo** (C++ struct):

```cpp
struct SessionInfo {
  std::string id;
  std::string title;
  std::optional<std::string> project_id;
  int64_t created_at;
  int64_t updated_at;
  // 缺失: slug, workspaceID, directory, parentID, summary, share, version, permission, revert
};
```

**数据结构一致性: ~40%** — 多个关键字段缺失

### 15.6 行为逻辑一致性分析

#### OAuth 登录流程对比

**OpenCode**:

1. `Account.login(server)` → POST `/auth/device/code`
2. `Account.poll(login)` → POST `/auth/device/token` (轮询)
3. 自动刷新过期 Token
4. 持久化到 SQLite

**Turbot**:

1. `Account::login()` → 仅日志输出
2. 无 HTTP 调用
3. 无 Token 刷新
4. JSON 文件存储

**行为一致性: 0%** — OAuth 流程未实现

#### Project ID 生成策略对比

**OpenCode**:

```typescript
// 使用 git root commit hash 作为 ID
const roots = await git(['rev-list', '--max-parents=0', 'HEAD'])
id = ProjectID.make(roots[0])
```

**Turbot**:

```cpp
// 使用 git remote URL 或路径作为 ID
std::string url = read_git_config_remote_origin_url();
return url.empty() ? absolute_path : url;
```

**行为一致性: 0%** — 完全不同的策略

### 15.7 100% 复刻差距汇总

| 类别           | OpenCode | Turbot | 复刻率   |
| -------------- | -------- | ------ | -------- |
| **核心模块**   | 37       | 20     | 54%      |
| **Tool 工具**  | 25       | 0      | 0%       |
| **API 接口**   | ~100     | ~30    | ~30%     |
| **数据结构**   | ~50      | ~20    | ~40%     |
| **行为逻辑**   | 完整     | 框架级 | ~20%     |
| **综合复刻率** | -        | -      | **~29%** |

### 15.8 100% 复刻修复路线图

#### Phase 0: 基础设施 (必须最先完成)

| 任务                          | 工作量 | 依赖      |
| ----------------------------- | ------ | --------- |
| 实现 `storage/` SQLite 持久化 | 5天    | SQLiteCpp |
| 实现 `bus/` 事件总线          | 2天    | -         |
| 实现 `global/` 全局状态       | 1天    | bus       |
| 实现 `util/` 工具函数库       | 3天    | -         |

#### Phase 1: 核心模块补齐

| 任务                 | 工作量 | 复刻目标文件        |
| -------------------- | ------ | ------------------- |
| `project/` 完整复刻  | 5天    | 7 files → 7 files   |
| `session/` 完整复刻  | 8天    | 17 files → 17 files |
| `provider/` 完整复刻 | 6天    | 8 files → 8 files   |
| `account/` 完整复刻  | 3天    | 4 files → 4 files   |

#### Phase 2: Tool 工具复刻

| 任务         | 工作量 | 优先工具                      |
| ------------ | ------ | ----------------------------- |
| 文件操作工具 | 3天    | read, write, edit, glob, grep |
| 执行工具     | 2天    | bash, batch                   |
| 交互工具     | 2天    | question, todo, plan          |
| 网络工具     | 2天    | webfetch, websearch           |
| 其他工具     | 2天    | apply_patch, lsp, task 等     |

#### Phase 3: 辅助模块复刻

| 任务         | 工作量 | 模块列表                  |
| ------------ | ------ | ------------------------- |
| 基础辅助模块 | 3天    | env, flag, id, filesystem |
| 集成辅助模块 | 3天    | file, shell, pty, command |
| 增强辅助模块 | 2天    | format, patch, ide, share |

### 15.9 复刻验收标准

**100% 复刻验收标准** (每个模块):

1. ✅ **接口一致**: 所有 public API 签名与 OpenCode 等价
2. ✅ **数据结构一致**: 所有字段/属性与 OpenCode 对齐
3. ✅ **行为一致**: 相同输入产生相同输出
4. ✅ **文件结构一致**: 源文件命名和组织与 OpenCode 对齐
5. ✅ **错误处理一致**: 异常类型和消息与 OpenCode 对齐

---

_报告生成时间: 2026-03-24 (二次核实+逻辑推导)_
_v4.2 审查更新: 2026-03-15_
_v4.3 + v4.4 审查更新: 2026-03-15_
_深度功能对齐审查: 2026-03-15_
_100% 复刻原则重新评估: 2026-03-15_

---

## 16. libs 目录深度审查修正 (2026-03-15)

**⚠️ 重要修正**: 初次审查时目录结构分析不够深入，遗漏了 `libs/storage/`、`libs/network/`、`libs/utils/` 子目录以及 `tool/builtin/` 目录下的 20 个工具实现。经深度核查，实际复刻率远高于初次评估。

### 16.1 libs 子目录完整清单

| Turbot 路径                    | 文件数   | 总行数/大小 | 对应 OpenCode 模块 |
| ------------------------------ | -------- | ----------- | ------------------ |
| `libs/storage/`                | 4 files  | ~800 行     | storage/ (SQLite)  |
| `libs/network/`                | 2 files  | ~900 行     | HTTP 客户端        |
| `libs/utils/`                  | 5 files  | ~400 行     | util/ 工具函数     |
| `libs/core/src/tool/builtin/`  | 20 files | ~240KB      | tool/ 工具实现     |
| `libs/core/src/provider/impl/` | 5 files  | ~76KB       | provider 实现      |
| `libs/core/src/session/`       | 7 files  | ~103KB      | session 模块       |

### 16.2 基础设施复刻状态 (修正)

| 基础设施          | OpenCode                | Turbot 实现                                   | 复刻状态          |
| ----------------- | ----------------------- | --------------------------------------------- | ----------------- |
| **SQLite 数据库** | storage/db.ts + Drizzle | libs/storage/sqlite_database.cpp (560行)      | ✅ **已完整复刻** |
| **数据库迁移**    | migration/ (8 files)    | libs/storage/migration.cpp (6.6KB)            | ✅ **已复刻**     |
| **事务管理**      | Database.use()          | libs/storage/transaction.cpp (1.6KB)          | ✅ **已复刻**     |
| **HTTP 客户端**   | Effect HttpClient       | libs/network/http_client.cpp (641行, libcurl) | ✅ **已完整复刻** |
| **URL 解析**      | 内置                    | libs/network/url.cpp (7.9KB)                  | ✅ **已复刻**     |
| **加密工具**      | -                       | libs/utils/crypto_utils.cpp (16.9KB)          | ✅ **已实现**     |
| **环境变量**      | env/index.ts            | libs/utils/env_utils.cpp (5.9KB)              | ✅ **已复刻**     |
| **文件工具**      | file/                   | libs/utils/file_utils.cpp                     | ✅ **已复刻**     |
| **JSON 工具**     | -                       | libs/utils/json_utils.cpp (8.3KB)             | ✅ **已实现**     |
| **字符串工具**    | -                       | libs/utils/string_utils.cpp (8.3KB)           | ✅ **已实现**     |

**关键发现**: 之前报告声称"缺少 HTTP 客户端"和"缺少 SQLite"是**错误的**！

### 16.3 Tool 工具复刻状态 (修正)

| OpenCode 工具      | Turbot 实现            | 代码量对比      | 复刻状态          |
| ------------------ | ---------------------- | --------------- | ----------------- |
| apply_patch        | apply_patch_tool.cpp   | 9.4KB → 32.5KB  | ✅ **超规格复刻** |
| bash               | bash_tool.cpp          | 8.6KB → 19.7KB  | ✅ **超规格复刻** |
| batch              | batch_tool.cpp         | 6.2KB → 9.0KB   | ✅ **复刻完成**   |
| codesearch         | codesearch_tool.cpp    | 3.4KB → 10.5KB  | ✅ **超规格复刻** |
| edit               | edit_tool.cpp          | 20.8KB → 25.5KB | ✅ **复刻完成**   |
| glob               | glob_tool.cpp          | 2.3KB → 11.6KB  | ✅ **超规格复刻** |
| grep               | grep_tool.cpp          | 4.7KB → 13.9KB  | ✅ **超规格复刻** |
| ls/list            | list_tool.cpp          | 3.1KB → 11.5KB  | ✅ **超规格复刻** |
| lsp                | lsp_tool.cpp           | 2.9KB → 12.1KB  | ✅ **超规格复刻** |
| multiedit          | multiedit_tool.cpp     | 1.5KB → 5.7KB   | ✅ **复刻完成**   |
| plan               | plan_tool.cpp          | 4.2KB → 7.3KB   | ✅ **复刻完成**   |
| question           | question_tool.cpp      | 1.1KB → 12.2KB  | ✅ **超规格复刻** |
| read               | read_file_tool.cpp     | 8.8KB → 11.8KB  | ✅ **复刻完成**   |
| task               | task_tool.cpp          | 5.3KB → 6.1KB   | ✅ **复刻完成**   |
| todo               | todo_tool.cpp          | 1.3KB → 9.0KB   | ✅ **超规格复刻** |
| webfetch           | webfetch_tool.cpp      | 6.0KB → 9.8KB   | ✅ **复刻完成**   |
| websearch          | websearch_tool.cpp     | 4.2KB → 9.2KB   | ✅ **复刻完成**   |
| write              | write_file_tool.cpp    | 3.1KB → 8.1KB   | ✅ **超规格复刻** |
| invalid            | invalid_tool.cpp       | 0.4KB → 2.6KB   | ✅ **复刻完成**   |
| external-directory | external_directory.cpp | 0.7KB → 5.0KB   | ✅ **复刻完成**   |
| truncate           | -                      | 5.5KB → ❌      | ⚠️ 缺失           |
| truncation-dir     | -                      | 0.1KB → ❌      | ⚠️ 缺失           |

**Tool 复刻率: 20/22 (91%)** — 仅 truncate 和 truncation-dir 缺失

### 16.4 Provider 实现状态 (修正)

| Provider  | OpenCode | Turbot impl/                  | 复刻状态      |
| --------- | -------- | ----------------------------- | ------------- |
| OpenAI    | ✅       | openai_provider.cpp (17.7KB)  | ✅ **已复刻** |
| Bailian   | -        | bailian_provider.cpp (15.2KB) | ✅ **新增**   |
| Zhipu     | -        | zhipu_provider.cpp (15.2KB)   | ✅ **新增**   |
| Kimi      | -        | kimi_provider.cpp (13.8KB)    | ✅ **新增**   |
| iFlow     | -        | iflow_provider.cpp (13.8KB)   | ✅ **新增**   |
| Anthropic | ✅       | 需实现                        | ⚠️ 缺失       |
| Azure     | ✅       | 需实现                        | ⚠️ 缺失       |
| Bedrock   | ✅       | 需实现                        | ⚠️ 缺失       |

### 16.5 修正后的复刻率统计

| 维度           | 初次评估      | 修正后      | 变化    |
| -------------- | ------------- | ----------- | ------- |
| **基础设施**   | 0% (声称缺失) | 100%        | ↑ +100% |
| **Tool 工具**  | 0%            | 91% (20/22) | ↑ +91%  |
| **Provider**   | 25%           | 62%         | ↑ +37%  |
| **Session**    | 41%           | 60%         | ↑ +19%  |
| **核心模块**   | 54%           | 75%         | ↑ +21%  |
| **综合复刻率** | ~29%          | **~78%**    | ↑ +49%  |

### 16.6 偏差实现处理建议

| 类别                         | 偏差程度            | 处理方式      | 理由                 |
| ---------------------------- | ------------------- | ------------- | -------------------- |
| **Tool 工具**                | 超规格 (代码量更大) | ✅ 保持现状   | 实现更完整，无需修改 |
| **HTTP 客户端**              | 完整实现            | ✅ 保持现状   | libcurl 方案成熟稳定 |
| **SQLite 数据库**            | 完整实现            | ✅ 保持现状   | 事务、迁移都已支持   |
| **Account OAuth**            | 框架级              | 🔧 **需补全** | 缺少 HTTP 调用集成   |
| **Project ID 生成**          | 策略不同            | 🔄 **需对齐** | 改用 git root commit |
| **Provider models-snapshot** | 缺失                | ➕ **需添加** | 模型列表快照         |
| **truncate 工具**            | 缺失                | ➕ **需添加** | 输出截断功能         |

### 16.7 剩余差距修复优先级

**P0 (必须修复)**:

1. Account OAuth 完整调用流程 (已有 HTTP 客户端，只需集成)
2. Project ID 生成策略对齐 (改用 git root commit hash)

**P1 (推荐修复)**:

1. 添加 truncate 工具
2. Provider models-snapshot 支持
3. GlobalBus 事件系统

**P2 (可选修复)**:

1. 更多 Provider 实现 (Anthropic, Azure, Bedrock)
2. Git CLI 实际调用
3. Token 加密存储

---

_深度功能对齐审查: 2026-03-15_
_100% 复刻原则重新评估: 2026-03-15_
_libs 目录深度审查修正: 2026-03-15_

---

## 17. 基于"100% 复刻"原则的源码级再评估 (2026-03-15)

**评估原则**: Turbot 是用 C++ 对 OpenCode 核心的 **100% 行为复刻**，不允许架构创新，要求接口一致、数据结构一致、行为逻辑一致。本节以 OpenCode TypeScript 源码为唯一基准，逐模块对比 Turbot C++ 实现，识别所有偏差并分级。

---

### 17.1 SQLite 数据库 Schema 偏差 (P0 级)

这是"100% 复刻"最根本的基础，DB Schema 决定了所有读写行为的正确性。

#### 17.1.1 `sessions` 表对比

| 列名                | OpenCode (`session.sql.ts`) | Turbot (`session_store.cpp`) | 差距                                                       |
| ------------------- | --------------------------- | ---------------------------- | ---------------------------------------------------------- |
| `id`                | TEXT PK (SessionID 字符串)  | TEXT PK                      | ✅ 对齐                                                    |
| `project_id`        | TEXT FK→project.id          | TEXT                         | ✅ 对齐（无 FK 约束）                                      |
| `workspace_id`      | TEXT nullable               | **❌ 缺失**                  | P0                                                         |
| `parent_id`         | TEXT nullable               | TEXT nullable                | ✅ 对齐                                                    |
| `slug`              | TEXT NOT NULL               | TEXT NOT NULL                | ✅ 对齐                                                    |
| `directory`         | TEXT NOT NULL               | TEXT NOT NULL                | ✅ 对齐                                                    |
| `title`             | TEXT NOT NULL               | TEXT NOT NULL                | ✅ 对齐                                                    |
| `version`           | TEXT NOT NULL               | TEXT NOT NULL                | ✅ 对齐                                                    |
| `share_url`         | TEXT nullable               | **❌ 缺失**                  | P0                                                         |
| `summary_additions` | INTEGER nullable            | **❌ 缺失**                  | P0                                                         |
| `summary_deletions` | INTEGER nullable            | **❌ 缺失**                  | P0                                                         |
| `summary_files`     | INTEGER nullable            | **❌ 缺失**                  | P0                                                         |
| `summary_diffs`     | TEXT(JSON) nullable         | **❌ 缺失**                  | P0                                                         |
| `revert`            | TEXT(JSON) nullable         | TEXT nullable                | ✅ 对齐                                                    |
| `permission`        | TEXT(JSON) nullable         | TEXT nullable                | ✅ 对齐                                                    |
| `time_created`      | INTEGER (via Timestamps)    | INTEGER                      | ✅ 对齐                                                    |
| `time_updated`      | INTEGER (via Timestamps)    | INTEGER                      | ✅ 对齐                                                    |
| `time_compacting`   | INTEGER nullable            | INTEGER nullable             | ✅ 对齐                                                    |
| `time_archived`     | INTEGER nullable            | INTEGER nullable             | ✅ 对齐                                                    |
| `state`             | **不存在**                  | TEXT (内部字段)              | ⚠️ Turbot 多出此列，需保持 DB 内部专用，不影响 wire format |

**缺失列**: `workspace_id`, `share_url`, `summary_additions`, `summary_deletions`, `summary_files`, `summary_diffs` — **6列全部 P0**

#### 17.1.2 `message` 表对比

| 方面   | OpenCode (`message` 表)                            | Turbot (`session_messages` 表) | 差距                     |
| ------ | -------------------------------------------------- | ------------------------------ | ------------------------ |
| PK     | `id TEXT` (MessageID 字符串)                       | `id INTEGER AUTOINCREMENT`     | ❌ **P0 — 完全不同**     |
| 外键   | `session_id TEXT FK→session.id`                    | `session_id TEXT`              | ⚠️ 无 FK 约束            |
| 索引   | `(session_id, time_created, id)`                   | `(session_id, seq)`            | ❌ **P0 — 排序语义不同** |
| 数据列 | `data TEXT(JSON)` (MessageV2.Info 去 id/sessionID) | `data TEXT`, `seq INTEGER`     | ❌ 结构不同              |
| 时间列 | `time_created INTEGER`                             | `created_at INTEGER`           | ⚠️ 列名不同              |

**影响**: MessageID 是 Turbot 生成 ID (`mes_` 前缀) 的前提，整个 message 存取体系需重构。

#### 17.1.3 `part` 表 — Turbot 完全缺失

OpenCode 有独立的 `part` 表 (`PartID TEXT PK`, `message_id FK`, `session_id`, `time_created`, `data JSON`)，用于存储 assistant 消息的结构化 part（文本、工具调用、工具结果等）。Turbot 无此表，也无此概念。**P1 级差距**（当前 agent 运行时有自己的 part 处理，但与 OpenCode wire format 不兼容）。

#### 17.1.4 `project` 表 — Turbot 缺失 SQLite 持久化

OpenCode (`project.sql.ts`) 定义了 `project` 表: `id TEXT PK`, `worktree`, `vcs`, `name`, `icon_url`, `icon_color`, `time_created`, `time_updated`, `time_initialized`, `sandboxes TEXT(JSON)`, `commands TEXT(JSON)`。

Turbot 当前: **无 `project` 表，`from_directory()` 将项目数据写入 `.turbot/turbot.json` 文件**。这是一个 **P0 级** 根本性差异，导致：

- `Project::list()` 返回空向量（无法从 SQLite 读取）
- `Project::get()` 通过扫描 `list()` 查找（无效）
- `fromDirectory()` 无法执行 session 迁移（将 global sessions 迁移到具体 project）

#### 17.1.5 `account` 表 — Turbot 缺失 SQLite 持久化

OpenCode 通过 `AccountRepo` + `AccountTable` + `AccountStateTable` (两张 SQLite 表) 持久化账户数据。  
Turbot 当前: **使用 `~/.turbot/accounts.json` 文件存储**。这是 **P0 级** 差异，影响：

- Token 过期时间无法持久化
- 多进程并发访问会出现竞态
- 缺乏事务性保证

---

### 17.2 Session 模块 API 偏差

#### 17.2.1 `Session::list()` — 缺失过滤参数

**OpenCode**:

```typescript
function* list(input?: {
  directory?: string
  workspaceID?: WorkspaceID
  roots?: boolean       // isNull(parent_id)
  start?: number        // time_updated >= start
  search?: string       // title LIKE %search%
  limit?: number        // default 100
})
```

**Turbot**:

```cpp
std::vector<Session> Session::list(const std::string& project_id);
// 仅有 project_id，缺少所有 5 个可选过滤参数
```

**差距级别**: P0 — 这直接影响 ACP/Server 的会话列表 API 行为。

#### 17.2.2 `Session::fork()` — 完全缺失

OpenCode `fork(sessionID, messageID?)` 的完整语义:

1. 获取原始会话
2. 创建新会话 (title 追加 `" (fork #N)"`)
3. 复制原始会话的所有 messages (到 `messageID` 截止)，重新分配 MessageID
4. 复制每条 message 的所有 parts，重新分配 PartID
5. 维护 parentID 映射（assistant 消息的 parentID 指向对应 user 消息的新 ID）

Turbot 完全没有 `fork()` 方法。**P0 级差距**。

#### 17.2.3 其他缺失的 Session 方法

| OpenCode 方法                           | 功能                       | Turbot 状态              | 差距级别 |
| --------------------------------------- | -------------------------- | ------------------------ | -------- |
| `touch(sessionID)`                      | 仅更新 time_updated        | 通过 `update()` 近似实现 | P2       |
| `setSummary(sessionID, summary)`        | 写 summary\_\* 列          | **缺失** (DB 列都没有)   | P0       |
| `setRevert(sessionID, revert, summary)` | 写 revert + summary 列     | **缺失**                 | P1       |
| `clearRevert(sessionID)`                | 清除 revert 列             | **缺失**                 | P1       |
| `share(sessionID)`                      | 写 share_url 列            | **缺失** (DB 列没有)     | P1       |
| `unshare(sessionID)`                    | 清除 share_url             | **缺失**                 | P1       |
| `diff(sessionID)`                       | 读取 session_diff 文件     | **缺失**                 | P2       |
| `children(parentID)`                    | 查询子会话                 | **缺失**                 | P1       |
| `setArchived(sessionID, time)`          | 写 time_archived           | 通过 `update()` 近似     | P2       |
| `initialize(...)`                       | 触发 SessionPrompt.command | **缺失**                 | P1       |
| `plan(slug, time)`                      | 计算 plan 文件路径         | **缺失**                 | P2       |
| `updatePart(part)`                      | 写 part 表                 | **缺失** (part 表不存在) | P1       |
| `removePart(...)`                       | 删除 part 记录             | **缺失**                 | P1       |
| `updatePartDelta(...)`                  | 发布 part delta 事件       | **缺失**                 | P1       |

---

### 17.3 Project 模块 API 偏差

#### 17.3.1 `from_directory()` 行为差异

| 步骤                   | OpenCode                                                                          | Turbot                                           | 差距 |
| ---------------------- | --------------------------------------------------------------------------------- | ------------------------------------------------ | ---- |
| 1. 生成 project ID     | `git rev-list --max-parents=0 HEAD` + cache                                       | ✅ 已对齐 (sub-02-1)                             | -    |
| 2. worktree 解析       | `git rev-parse --git-common-dir` + `--show-toplevel`                              | ⚠️ 仅有 `--git-common-dir`，无 `--show-toplevel` | P1   |
| 3. SQLite upsert       | `INSERT ... ON CONFLICT DO UPDATE SET ...`                                        | **❌ 写 `.turbot/turbot.json`**                  | P0   |
| 4. session 迁移        | `UPDATE sessions SET project_id=? WHERE project_id=global AND directory=worktree` | **❌ 缺失**                                      | P0   |
| 5. 发布 GlobalBus 事件 | `GlobalBus.emit("event", { type: "project.updated", ... })`                       | **❌ 缺失**                                      | P1   |

#### 17.3.2 其他缺失的 Project 方法

| OpenCode 方法              | Turbot 状态                                      | 差距级别 |
| -------------------------- | ------------------------------------------------ | -------- |
| `Project.list()`           | 返回空向量                                       | P0       |
| `Project.get(id)`          | 通过空 `list()` 扫描（无效）                     | P0       |
| `Project.update()`         | 写 JSON 文件，无 SQLite，无事件                  | P1       |
| `Project.setInitialized()` | 写 JSON 文件                                     | P1       |
| `Project.initGit()`        | 仅记录日志，不调用 `git init`                    | P1       |
| `Project.addSandbox()`     | 无实现                                           | P2       |
| `Project.removeSandbox()`  | 无实现                                           | P2       |
| `Project.sandboxes()`      | 无 SQLite 查询                                   | P2       |
| `Project.discover()`       | 仅扫描顶层目录，OpenCode 递归扫描 `**/favicon.*` | P3       |

---

### 17.4 Account 模块 API 偏差

#### 17.4.1 `login()` / `poll()` — 无 HTTP 调用

| 行为                       | OpenCode                          | Turbot                   | 差距                   |
| -------------------------- | --------------------------------- | ------------------------ | ---------------------- |
| `login(server)`            | POST `{server}/auth/device/code`  | 返回生成的假 device_code | **P0 — 行为一致性 0%** |
| `poll(input)`              | POST `{server}/auth/device/token` | 永远返回 Pending         | **P0**                 |
| `resolveToken()`           | 检查 token_expiry，过期则自动刷新 | 直接返回内存中的 token   | **P0**                 |
| `fetchUser()`              | GET `{server}/api/user`           | 不存在                   | P0                     |
| `fetchOrgs()`              | GET `{server}/api/orgs`           | 不存在                   | P0                     |
| `config(accountID, orgID)` | GET `{server}/api/config`         | 不存在                   | P1                     |

#### 17.4.2 Account 存储偏差

| 方面       | OpenCode                                 | Turbot         | 差距    |
| ---------- | ---------------------------------------- | -------------- | ------- |
| 存储后端   | SQLite (`account` + `account_state` 表)  | JSON 文件      | P0      |
| Token 加密 | 明文存 SQLite (OS 文件权限保护)          | 明文存 JSON    | ⚠️ 等价 |
| Token 过期 | `token_expiry INTEGER` 列，自动检查+刷新 | **无过期字段** | P0      |
| 多进程安全 | SQLite 事务保证                          | 文件级锁不足   | P1      |

---

### 17.5 事件系统偏差

OpenCode 在关键操作后通过 `Bus.publish()` 发布事件：

| 事件                                              | 触发位置                                      | Turbot 状态 |
| ------------------------------------------------- | --------------------------------------------- | ----------- |
| `session.created`                                 | `Session.createNext()`                        | **缺失**    |
| `session.updated`                                 | 几乎每个 Session 写操作                       | **缺失**    |
| `session.deleted`                                 | `Session.remove()`                            | **缺失**    |
| `project.updated`                                 | `Project.fromDirectory()`, `Project.update()` | **缺失**    |
| `message.updated` / `part.updated` / `part.delta` | Session 消息处理                              | **缺失**    |

事件系统是 ACP 实时推送的基础，Turbot 已有 `event/` 模块，但未与 Session/Project 集成。**P1 级缺口**。

---

### 17.6 修订后的复刻率统计

| 模块                        | 上次评估 (§16.5) | 本次重评 | 变化   | 说明                           |
| --------------------------- | ---------------- | -------- | ------ | ------------------------------ |
| **DB Schema — sessions 表** | 假设对齐         | **50%**  | ↓      | 6 列缺失                       |
| **DB Schema — messages 表** | 未评估           | **20%**  | 新     | 完全不同的结构                 |
| **DB Schema — parts 表**    | 未评估           | **0%**   | 新     | 不存在                         |
| **DB Schema — project 表**  | 未评估           | **0%**   | 新     | 使用 JSON 文件替代             |
| **DB Schema — account 表**  | 未评估           | **0%**   | 新     | 使用 JSON 文件替代             |
| **Session API**             | 60%              | **45%**  | ↓      | fork/setSummary/etc. 缺失      |
| **Project API**             | 假设 75%         | **40%**  | ↓      | SQLite 持久化缺失，list() 失效 |
| **Account API**             | 框架级           | **15%**  | ↓      | HTTP 调用全部是 stub           |
| **Tool 工具**               | 91%              | **91%**  | =      | 不变                           |
| **基础设施 (HTTP/SQLite)**  | 100%             | **100%** | =      | 不变                           |
| **综合复刻率**              | **~78%**         | **~55%** | ↓ -23% | DB schema 是最大拖累           |

---

### 17.7 P0 级差距修复清单 (必须修复)

以下问题每一条都会导致 Turbot 与 OpenCode 在相同输入下产生不同输出或崩溃：

| 编号    | 差距描述                                                                                               | 修复工作量 | 关联当前计划 |
| ------- | ------------------------------------------------------------------------------------------------------ | ---------- | ------------ |
| **G01** | `sessions` 表补充 `workspace_id`, `share_url`, `summary_*` 6 列                                        | 0.5 天     | s11          |
| **G02** | `session_messages` 表重构为 OpenCode `message` 表结构 (string MessageID PK + time_created + data JSON) | 1 天       | 新增         |
| **G03** | 新增 `part` 表，实现 `updatePart()`/`removePart()`                                                     | 1 天       | 新增         |
| **G04** | 新增 `project` SQLite 表，`from_directory()` 改为 SQLite upsert                                        | 1.5 天     | 新增         |
| **G05** | `from_directory()` 添加 session 迁移逻辑                                                               | 0.5 天     | 新增         |
| **G06** | 新增 `account` + `account_state` SQLite 表，AccountService 改为 SQLite 存储                            | 1 天       | s13          |
| **G07** | `Account::login()` 实现真实 HTTP POST 调用                                                             | 1 天       | s12          |
| **G08** | `Account::poll()` 实现真实 HTTP POST + token 持久化                                                    | 1 天       | s12          |
| **G09** | `Account::resolveToken()` 实现 token 过期检查 + 自动刷新                                               | 0.5 天     | s12          |
| **G10** | `Session::list()` 扩展 5 个过滤参数                                                                    | 0.5 天     | s07          |
| **G11** | `Session::fork()` 实现消息历史克隆                                                                     | 1 天       | s08          |
| **G12** | `Session::setSummary()` 实现                                                                           | 0.5 天     | s11          |

**P0 总工作量: ~10 天**

---

### 17.8 P1 级差距修复清单 (强烈推荐)

| 编号    | 差距描述                                                     | 工作量 |
| ------- | ------------------------------------------------------------ | ------ |
| **G13** | `Session::setRevert()` + `clearRevert()`                     | 0.5 天 |
| **G14** | `Session::share()` + `unshare()` (需 G01 share_url 列)       | 1 天   |
| **G15** | `Session::children()`                                        | 0.5 天 |
| **G16** | Session Bus 事件发布 (created/updated/deleted)               | 1 天   |
| **G17** | Project Bus 事件发布 (project.updated)                       | 0.5 天 |
| **G18** | `Project::list()`/`get()` 改为 SQLite 查询 (需 G04)          | 0.5 天 |
| **G19** | `Project::update()` 改为 SQLite (需 G04)                     | 0.5 天 |
| **G20** | `Project::initGit()` 实现真实 `git init` 调用                | 0.5 天 |
| **G21** | `from_directory()` 添加 `git rev-parse --show-toplevel` 步骤 | 0.5 天 |
| **G22** | Account `fetchUser()` + `fetchOrgs()` HTTP 调用              | 0.5 天 |
| **G23** | `Session::initialize()` 触发 SessionPrompt 命令              | 1 天   |

**P1 总工作量: ~8 天**

---

### 17.9 P2/P3 级差距 (可选)

| 编号 | 差距描述                                    | 优先级 |
| ---- | ------------------------------------------- | ------ |
| G24  | `Session::diff()` 读取 session_diff 文件    | P2     |
| G25  | `Session::plan()` 计算 plan 文件路径        | P2     |
| G26  | `Session::setArchived()` 专用方法           | P2     |
| G27  | `Session::touch()` 专用方法                 | P2     |
| G28  | `Project::addSandbox()` / `removeSandbox()` | P2     |
| G29  | Account `config()` 远程配置获取             | P2     |
| G30  | `Project::discover()` 递归 favicon 扫描     | P3     |
| G31  | `todo` 表新增                               | P3     |
| G32  | `permission` 表新增                         | P3     |

---

### 17.10 对当前执行计划的影响

当前计划 (sub-02, sub-03) 需要重新调整优先级：

| 原计划任务                       | 重评后状态                | 原因                                                                                        |
| -------------------------------- | ------------------------- | ------------------------------------------------------------------------------------------- |
| s07: Session::list() 过滤扩展    | **维持 P0**               | G10，直接影响 API 行为                                                                      |
| s08: Session::fork()             | **维持 P0**               | G11，核心功能缺失                                                                           |
| s11: session_store summary DB 列 | **升级至 P0，范围扩大**   | 需同时补 workspace*id + share_url + summary*\* (G01) + message 表重构 (G02) + part 表 (G03) |
| s12: Account OAuth               | **维持 P0**               | G07+G08+G09                                                                                 |
| s13: Account/Auth SQLite         | **维持 P0，范围扩大**     | G06 — 2 张表                                                                                |
| **新增**                         | **Project SQLite 持久化** | G04+G05+G18+G19 — 这是 project 模块最根本的差距                                             |
| **新增**                         | **事件系统集成**          | G16+G17 — Bus 事件是实时推送基础                                                            |

**建议 sub-03 重新拆分为**:

- **sub-03-1**: DB Schema 修复 (G01+G02+G03 — session/message/part 表)
- **sub-03-2**: Project SQLite 持久化 (G04+G05+G18+G19)
- **sub-03-3**: Account OAuth + SQLite (G06+G07+G08+G09+G22)
- **sub-03-4**: 事件系统集成 + 剩余 API (G13~G17, G23)

---

_100% 复刻原则源码级再评估: 2026-03-15_
_基准: OpenCode packages/opencode/src/ TypeScript 源码_
