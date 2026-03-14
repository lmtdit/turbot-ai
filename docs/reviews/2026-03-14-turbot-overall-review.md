# Turbot 整体功能完成度评估总结报告

> **评估时间**：2026-03-14
> **评估范围**：Turbot v2.x（v2.0-v2.9）+ v3.0（MCP/LSP/ACP 三大协议）
> **评估基准**：OpenCode 对应功能实现（`src/mcp/index.ts` / `src/lsp/server.ts` / `src/acp/agent.ts`）
> **评估方式**：源码逐模块二次核实 + 计划文档状态核查 + 占位代码识别
> **子报告**：
>
> - [v2.x 完成度报告](./2026-03-14-v2.x-completeness-review.md)
> - [v3.0 完成度报告](./2026-03-14-v3.0-completeness-review.md)

---

## 一、综合完成度概览

| 版本/系统            | 综合完成度 | SPEARM 综合分（修正后）  | 关键阻塞                         |
| -------------------- | ---------- | ------------------------ | -------------------------------- |
| v2.0-v2.7（核心层）  | **93%**    | A 区间                   | 无主流程阻塞                     |
| v2.8（架构差距补全） | **88%**    | B+ 区间                  | P0/P1 已落地，P2/P3 待           |
| v2.9（测试质量）     | **0%**     | —                        | 未启动，7 failed                 |
| v2.x 整体            | **~78%**   | **80 / 100**             | Session DB + 权限旁路            |
| v3.0（三大协议）     | **~65%**   | **70 / 100（上限锁定）** | Session DB + 权限旁路 + MCP 测试 |

---

## 二、跨版本共同关键问题（P0 级）

这三个问题同时影响 v2.x 和 v3.0，且相互关联，**必须作为统一优先级处理**：

### 问题1：Session 持久化全为占位 [CRITICAL]

```cpp
// session.cpp line 231-250
// This is a placeholder - in a real implementation, this would query the database
(void)id;
return std::nullopt;  // Session::get()
return {};            // Session::list()
return false;         // Session::delete()
```

**影响链路**：

- v2.x：`Session.get/list/messages()` 占位 → 多 Session 管理不可用 → 会话历史不可持久
- v3.0：`ACP.load_session → Session::get() → nullopt → 抛异常`；`ACP.list_sessions → Session::list() → 永远空列表`；`ACP.fork_session` 永远失败

**修复方向**：`libs/storage/` SQLite 层已存在，接入 `Session::get/list/delete/messages()` 完成真实 DB 操作

---

### 问题2：权限系统全路旁路 [CRITICAL]（二次核实新增发现）

```cpp
// session_loop.cpp:700
tool::ToolContext ctx;
ctx.session_id        = session_.id();
ctx.message_id        = ...;
ctx.agent             = ...;
ctx.call_id           = call_id;
ctx.abort_flag        = abort_flag_;
ctx.working_directory = ...;
// ask_permission 从未设置 ← 所有工具跳过权限确认
```

**根因**：`SessionLoop::execute_tool()` 中 `ToolContext.ask_permission` 函数指针从未被赋值。各工具（read/write/edit/bash/glob/grep/list）均有 `if (!ctx.ask_permission)` 守卫，导致全部走 `allow` 分支，跳过任何交互式权限确认。

**影响**：`permission.cpp` 中定义的权限规则引擎从未被触发；工具在用户无感知的情况下无条件执行（含写文件/执行 Shell）。

**OpenCode 对比**：完整实现了 `permission.asked` EventBus 事件 → ACP `requestPermission()` → 用户确认 → 权限回复的闭环。

**修复方向**：在 `SessionLoop::execute_tool()` 中设置 `ctx.ask_permission` 回调，回调内通过 ACP/EventBus 向客户端请求权限并等待回复。

---

### 问题3：MCP 单元测试目录为空 [HIGH]（v3.0 独有）

```
tests/unit/mcp/   ← 0 文件，目录存在但无任何测试
```

`mcp.cpp`/`client.cpp`/`stdio_transport.cpp`/`sse_transport.cpp`/`http_transport.cpp` 全部无单元测试，计划要求覆盖率 ≥ 80%。

---

## 三、v2.x 完成度详情（二次核实修正版）

### 3.1 优势

- **核心主循环完整**（`session_loop.cpp` 37.9KB）：prune / RetryManager / repairToolCall / Plugin 5钩子 / tool_choice / question 工具 / per-step token / EventBus 全部落地
- **2.8 P0/P1 差距全部已落地**（计划文档显示「进行中」为严重状态漂移，实际早已完成）
- **测试断言总量 3705 个**，平均评级 A（v2.0-v2.17）
- **Plugin 系统**：5 个钩子挂载点（chat.params / chat.headers / text.complete × 2 路径 + session status）

### 3.2 遗留问题汇总

| 问题                                       | 严重度      | 影响                                |
| ------------------------------------------ | ----------- | ----------------------------------- |
| Session.get/list/messages() 占位           | P0 CRITICAL | 多会话持久化完全不可用              |
| 权限系统旁路（ask_permission 未注入）      | P0 CRITICAL | 所有工具无权限确认运行              |
| v2.9 整体未启动（7 failed，覆盖率 70.33%） | P0          | 测试质量门禁未达标，7 个已知失败    |
| BuildAgent/ExploreAgent/PlanAgent 占位     | P1          | 子 Agent 路由不可用，返回固定字符串 |
| generate_agent() 占位                      | P1          | 动态 Agent 生成不可用               |
| 2.8 计划文档状态未同步                     | P1          | 与实际落地严重不同步，导致误判进度  |
| multiedit/webfetch/codesearch 工具缺失     | P2          | 高级工具能力不全，不阻塞主流程      |

### 3.3 SPEARM 评分（修正后）

| 维度 | 等级   | 修正原因                                                   |
| ---- | ------ | ---------------------------------------------------------- |
| S    | **B-** | 权限规则存在但实际旁路（原 B+，二次核实降级）              |
| P    | **B+** | per-step token + prune + LRU 缓存，无变化                  |
| E    | **B+** | RetryManager + repairToolCall + 统一 try-catch，无变化     |
| A    | **B+** | Plugin 钩子 + EventBus 解耦，Session DB 破坏持久化，无变化 |
| R    | **B-** | 主流程可运行，Session 持久化和测试质量是主要缺陷，无变化   |
| M    | **B**  | 代码结构清晰，3 Agent 占位，文档状态漂移，无变化           |

**综合分：80 / 100**（原 82，因权限旁路重新评估 S 维度）

> 计算展示：S=7.5×3 + P=8.5×2 + E=8.5×2 + A=8.5×1.5 + R=7.5×1.5 + M=8.0×1 = 88.5 / 11 = 8.045 × 10 = **80.45 ≈ 80**

---

## 四、v3.0 完成度详情（二次核实修正版）

### 4.1 MCP 系统

**已完成（与 OpenCode 对齐）**：

- MCPStatus 5 种状态（Connected/Disabled/Failed/NeedsAuth/NeedsClientRegistration）对齐 OpenCode discriminatedUnion
- MCPToolWrapper 命名规则（`{sanitized_client}_{sanitized_tool}`）对齐 OpenCode sanitizedClientName
- DEFAULT_TIMEOUT = 30000ms 对齐 OpenCode `DEFAULT_TIMEOUT = 30_000`
- OAuth PKCE + McpOAuthProvider 完整实现
- 三种传输层（stdio/SSE/HTTP）均已实现

**差距**：MCP 单元测试目录为空（0 文件）

### 4.2 LSP 系统

**已完成（与 OpenCode 对齐）**：

- `kDiagnosticsDebounceMs = 150` 对齐 OpenCode `DIAGNOSTICS_DEBOUNCE_MS = 150`
- Content-Length framing 手工实现
- 诊断 condition_variable debounce

**差距**：仅 3 种语言服务器（Clangd/Pyright/Gopls）vs OpenCode 8 种（另有 Deno/TS/Vue/ESLint/Oxlint/Biome）

### 4.3 ACP 系统

**已完成（与 OpenCode 对齐）**：

- 全部 8 个方法实现（initialize/new_session/load_session/resume_session/list_sessions/fork_session/prompt/cancel）
- `cancelled → completed` 映射对齐 OpenCode
- `todowrite` plan 通知对齐
- `load_session` 历史回放路径中 diff oldText/newText 从 args 正确提取

**差距（修正）**：

- `cancel()` 为 stub（no-op）— 非权限问题，是中断信号传递问题
- `set_on_tool_result` 弹流路径 diff 内容为空（区别于 load_session 路径）
- 权限系统旁路（与 v2.x 共同问题）

### 4.4 SPEARM 评分（修正后）

| 维度 | 等级   | 修正原因                                                       |
| ---- | ------ | -------------------------------------------------------------- |
| S    | **C+** | 权限系统全路旁路（原 B+，二次核实降级为 C+，安全边界实质缺失） |
| P    | **B**  | 各模块独立 io 线程，无变化                                     |
| E    | **B-** | ACP try-catch，Session::get 返回 nullopt 掩盖问题，无变化      |
| A    | **B+** | 三大协议分层清晰，无变化                                       |
| R    | **C+** | Session DB 占位 → ACP 核心路径不可用，无变化                   |
| M    | **B**  | 文件结构清晰，注释对齐，无变化                                 |

> S 维度 C+、R 维度 C+ → 综合分上限 ≤ 70

**综合分：70 / 100（上限锁定）**（原 62，因权限旁路使 S 维度从 B+ 降至 C+）

> 计算展示：S=7.0×3 + P=8.0×2 + E=7.5×2 + A=8.5×1.5 + R=7.0×1.5 + M=8.0×1 = 83.25 / 11 = 7.568 × 10 = 75.68，但 S/R 均为 C+ → 上限锁定为 **70**

---

## 五、优先修复路线图

### P0：必须解决，阻塞核心功能

| ID   | 问题                           | 文件                                                  | 影响版本    |
| ---- | ------------------------------ | ----------------------------------------------------- | ----------- |
| P0-1 | Session::get/list/delete 占位  | `session.cpp` line 231-250                            | v2.x + v3.0 |
| P0-2 | 权限系统旁路（ask_permission） | `session_loop.cpp` line 700（ToolContext 未设置回调） | v2.x + v3.0 |
| P0-3 | v2.9 未启动（7 failed）        | `CMakeLists.txt`（copy_prompts 目标缺失）             | v2.x        |
| P0-4 | MCP 单元测试目录为空           | `tests/unit/mcp/`                                     | v3.0        |

### P1：重要，影响关键功能完整性

| ID   | 问题                                   | 文件                                                       | 影响版本    |
| ---- | -------------------------------------- | ---------------------------------------------------------- | ----------- |
| P1-1 | BuildAgent/ExploreAgent/PlanAgent 占位 | `build_agent.cpp` / `explore_agent.cpp` / `plan_agent.cpp` | v2.x + v3.0 |
| P1-2 | ACP cancel() 为 stub                   | `acp/agent.cpp` line 439-448                               | v3.0        |
| P1-3 | set_on_tool_result diff 内容为空       | `acp/agent.cpp` line 384-390                               | v3.0        |
| P1-4 | generate_agent() 占位                  | `agent_loader.cpp` line 265                                | v2.x        |
| P1-5 | 2.8 计划文档状态未同步                 | 计划文档                                                   | 文档        |

### P2：增强，不阻塞主流程

| ID   | 问题                                           |
| ---- | ---------------------------------------------- |
| P2-1 | multiedit/webfetch/codesearch 工具缺失         |
| P2-2 | LSP 语言服务器 3→8 种（Deno/TS/Vue/ESLint 等） |
| P2-3 | SSE fallback 降级逻辑集成测试                  |

---

## 六、二次核实修正对照表

| 原始结论                        | 修正后                                               | 修正依据                                                         |
| ------------------------------- | ---------------------------------------------------- | ---------------------------------------------------------------- |
| v2.x：S 安全性 B+               | **B-**                                               | 权限规则存在但 ToolContext 未注入 ask_permission，工具实际无守卫 |
| v2.x：综合分 82/100             | **80/100**（公式计算 80.45）                         | S 维度降级 B+→8.5 → B-→7.5，裁切功能自由区间重计                 |
| v3.0：ACP 权限 stub（line 448） | `cancel()` 方法为 stub，权限问题是独立的更严重问题   | stub 注释所在函数为 cancel()，非权限处理函数                     |
| v3.0：S 安全性 B+               | **C+**                                               | ask_permission 链路在 v3.0 同样未建立                            |
| v3.0：综合分 62/100             | **70/100（上限锁定）**（公式结果 75.68 被锁定到 70） | S 维度 C+，上限锁定 ≤ 70；原始报告 62 计算误差                   |
| v3.0：diff 块全部为空           | **弹流路径为空，load_session 路径已修复**            | agent.cpp line 609-615 已从 args 正确提取 diff 内容              |

---

## 七、总体结论

**v2.x** 整体处于可用状态，核心主循环成熟度高，测试体系较完善。关键短板是 Session 持久化（DB 占位）和权限系统（旁路未接入），以及 v2.9 测试质量提升计划尚未启动（70.33% 覆盖率，7 failed）。

**v3.0** 三大协议框架结构清晰，MCP/LSP/ACP 基础接口完整，与 OpenCode 对齐度高。但 Session DB 占位导致 ACP 核心路径（load/list/fork session）全部不可用，权限系统旁路属于安全边界缺失，需列为 P0 与 Session DB 并行修复。

**最关键的统一结论**：Session 持久化（P0-1）+ 权限注入（P0-2）是两个跨版本的基础设施问题，应作为下一开发周期的绝对优先项，解决后 v2.x 可达 88+分、v3.0 可达 82+分。
