# Turbot ↔ OpenCode 对齐报告 Round 12

> 生成时间：2026-03-25  
> 范围：`libs/core` — opencode 最新更新 diff 扫描  
> 基准：Round 11 综合分 94/100（G104-G110 已补全，git commit `040fb9b`）  
> opencode 新增 commits：`7123aad5a`（ZlibError）、`da1d37274`（gpt prompt）、`66a56551b`（todowrite perm）

---

## 一、opencode 最新更新 diff 分析

### 扫描的 commits（相关）

| Commit | 说明 | 影响 turbot |
|--------|------|------------|
| `7123aad5a` | ZlibError 分类为可重试错误 | ✅ 有差距 |
| `da1d37274` | 新增 gpt.txt 系统提示词，非 codex gpt 模型走新分支 | ✅ 有差距 |
| `66a56551b` | TaskTool 尊重 agent 的 todowrite 权限 | ✅ 有差距 |
| `38450443b` | 移除 WorkspaceContext，架构重构 | ⚪ 架构层差异，turbot EventBus 已等效 |
| `b0017bf1b` | SyncEvent 初始实现 | ⚪ 架构层差异，不影响核心功能对齐 |
| `77fc88c8a` | 移除 todoread 工具死代码 | ⚪ 清理性变更 |

---

## 二、差距清单

### G111 — `provider_prompt()` gpt 分支逻辑错误

**文件**: `libs/core/src/llm/system_prompt.cpp`  
**优先级**: HIGH  

```
opencode system.ts provider() 最新逻辑:
  if (model.api.id.includes("gpt-4") || model.api.id.includes("o1") || model.api.id.includes("o3"))
    return [PROMPT_BEAST]
  if (model.api.id.includes("gpt")) {
    if (model.api.id.includes("codex")) {
      return [PROMPT_CODEX]   // gpt+codex → codex prompt
    }
    return [PROMPT_GPT]       // gpt（非codex）→ 新 gpt.txt prompt
  }
  ...

turbot system_prompt.cpp provider_prompt() 当前逻辑:
  if (gpt-5) → prompt_codex()        // 错误：gpt-5 无 codex，应走 prompt_gpt()
  if (gpt- || o1 || o3) → prompt_beast()  // 合并了所有 gpt 和 o1/o3
  // 缺少：非 gpt-4/o1/o3 的普通 gpt 模型（如 gpt-4o-mini）走 prompt_gpt()
  // 缺少：gpt+codex → prompt_codex() 的精确匹配
```

**修复**:
1. 新增 `prompt_gpt()` 方法（加载 gpt.md prompt 文件）
2. 创建 `libs/core/prompts/gpt.md`（从 opencode gpt.txt 适配，替换品牌名）
3. 调整 `provider_prompt()` 分支顺序：
   - `gpt-4 / o1 / o3` → beast（不变）
   - `gpt` + `codex` → codex
   - `gpt`（其他）→ gpt（新分支）

---

### G112 — 缺少 ZlibError 可重试处理

**文件**: `libs/core/src/session/session_loop.cpp`  
**优先级**: HIGH  

```
opencode message-v2.ts fromError() 新增（commit 7123aad5a）:
  case e instanceof Error && (e as FetchDecompressionError).code === "ZlibError":
    if (ctx.aborted) {
      return AbortedError(...)
    }
    return APIError({
      message: "Response decompression failed",
      isRetryable: true,
      metadata: { code: "ZlibError", message: e.message }
    })

turbot session_loop.cpp: 无 ZlibError 检测，解压失败会被当作未知错误处理
```

**修复**: 在 `session_loop.cpp` 的异常处理链中添加 ZlibError 检测；或在 `provider_error.cpp` 的 `parse_api_call_error()` 中添加。实际上 turbot 的 HTTP 层不用 Bun，但需要在 llm.cpp/session_loop.cpp 中捕获解压错误并标记为 retryable。

---

### G113 — `task_tool.cpp` 缺少 `hasTodoWritePermission` 检查

**文件**: `libs/core/src/tool/builtin/task_tool.cpp`  
**优先级**: MEDIUM  

```
opencode task.ts 最新逻辑（commit 66a56551b）:
  const hasTodoWritePermission = agent.permission.some(r => r.permission === "todowrite")
  
  permission: [
    ...(hasTodoWritePermission ? [] : [{ permission: "todowrite", action: "deny" }]),
    ...
  ]
  
  tools: {
    ...(hasTodoWritePermission ? {} : { todowrite: false }),
    ...
  }

turbot task_tool.cpp build_subagent_permission():
  // 只 deny task，没有 todowrite 的条件性 deny
  ruleset.push_back(PermissionRule{"task", "*", PermissionAction::Deny});
  // 缺少: if (!has_todowrite_permission) deny todowrite
```

**修复**: `build_subagent_permission()` 检查 agent 是否有 `todowrite` 权限，如无则 deny todowrite

---

## 三、第一次 SPEARM 评分（初始，补全前）

| 维度 | 等级 | 说明 |
|------|------|------|
| S | A | 无安全敏感差距 |
| P | B+ | gpt 分支逻辑错误影响 prompt 质量，ZlibError 导致不必要的失败 |
| E | B | ZlibError 缺失导致可重试错误被当作不可重试处理 |
| A | A- | 架构整体对齐，agent permission 逻辑有差距 |
| R | B+ | ZlibError + gpt prompt 影响可靠性 |
| M | A- | 代码整洁，待补充 gpt.md |

| 维度 | 等级 | 分值 | 权重 | 加权分 |
|------|------|------|------|--------|
| S | A | 9 | 3 | 27 |
| P | B+ | 7.5 | 2 | 15 |
| E | B | 7 | 2 | 14 |
| A | A- | 8.5 | 1.5 | 12.75 |
| R | B+ | 7.5 | 1.5 | 11.25 |
| M | A- | 8.5 | 1 | 8.5 |

- 总权重：11
- 加权总分：88.5
- **百分制综合分：~89/100**

---

## 四、第二次 SPEARM 校验（复核）

**G111 影响重估**：`gpt-4o-mini`、`gpt-3.5-turbo` 等模型在旧逻辑中走 `gpt-` → beast prompt，而 opencode 新逻辑走 gpt prompt。Beast prompt 是 "keep going until solved" 风格，GPT prompt 是更精细的协作型提示词，差异显著。E 维度不降级（无功能失效），但 P 和 R 维度影响确认。

**G112 影响重估**：ZlibError 在高负载网络环境（特别是启用 gzip 的 API）中可能频繁触发，丢失重试可能导致用户体验下降。E 维度 B 确认。

**G113 影响重估**：中等影响，仅在需要 todowrite 权限的子 agent 场景触发，影响范围有限。

### 二次评分确认

| 维度 | 初次 | 复核 | 变化 |
|------|------|------|------|
| S | A | A | — |
| P | B+ | B+ | — |
| E | B | B | — |
| A | A- | A- | — |
| R | B+ | B+ | — |
| M | A- | A- | — |

**二次综合分：89/100（确认）**

---

## 五、补全计划

| ID | 优先级 | 修改文件 | 说明 |
|----|--------|---------|------|
| G111-a | HIGH | `libs/core/prompts/gpt.md`（新建） | gpt.txt 内容适配 |
| G111-b | HIGH | `system_prompt.hpp`（添加 prompt_gpt 声明） | 新方法声明 |
| G111-c | HIGH | `system_prompt.cpp`（修改 provider_prompt + 添加 prompt_gpt） | 核心逻辑修复 |
| G112 | HIGH | `session_loop.cpp`（添加 ZlibError 处理） | 可重试错误识别 |
| G113 | MEDIUM | `task_tool.cpp`（hasTodoWritePermission 检查） | agent 权限尊重 |

### 执行顺序

1. **G111**: 创建 `gpt.md` → 修改 `system_prompt.hpp` → 修改 `system_prompt.cpp`
2. **G112**: 修改 `session_loop.cpp` ZlibError 处理
3. **G113**: 修改 `task_tool.cpp` todowrite permission

---

## 六、终审 SPEARM 评分（补全后）

> 补全执行完成时间：2026-03-25  
> 变更集：G111（gpt.md + system_prompt 修复）、G112（ZlibError 重试）、G113（todowrite 权限）

### 第一次终审

| 维度 | 等级 | 说明 |
|------|------|------|
| S | A | 无安全差距 |
| P | A- | gpt 分支精确对齐，ZlibError 现在可重试，性能损耗减少 |
| E | A- | ZlibError 现在正确识别为 retryable，abort 时切换为 AbortRetryException |
| A | A | build_subagent_permission 完整对齐 opencode 语义 |
| R | A- | 解压失败重试 + prompt 分支正确 = 可靠性提升 |
| M | A | gpt.md 完整创建，代码注释清晰 |

| 维度 | 等级 | 分值 | 权重 | 加权分 |
|------|------|------|------|--------|
| S | A | 9 | 3 | 27 |
| P | A- | 8.5 | 2 | 17 |
| E | A- | 8.5 | 2 | 17 |
| A | A | 9 | 1.5 | 13.5 |
| R | A- | 8.5 | 1.5 | 12.75 |
| M | A | 9 | 1 | 9 |

- 总权重：11
- 加权总分：96.25
- **百分制综合分：~96/100**

### 第二次终审（复核）

**G111 完整性确认**：
- `gpt.md` 创建完成（117行，与 opencode gpt.txt 完全对齐，品牌 OpenCode→Turbot）
- `system_prompt.hpp` 添加 `prompt_gpt()` 声明
- `system_prompt.cpp` 修复 `provider_prompt()` 分支：`gpt-4/o1/o3 → beast`，`gpt+codex → codex`，`gpt(其他) → gpt`
- 旧的错误分支 `gpt-5 → codex` 已移除

**G112 完整性确认**：
- lambda 内先 `catch(APIError) → rethrow` 确保 `APIError` 不被误捕
- `catch(std::runtime_error)` 检查 `ZlibError` 关键字，匹配时 abort 走 `AbortRetryException`，否则抛出 `is_retryable` 的 `APIError`
- `RetryManager::with_retry` 的 `is_retryable()` 会识别并重试该错误

**G113 完整性确认**：
- `std::any_of` 遍历 `agent->info().permission`，检查是否有 `todowrite` 规则
- 无 todowrite 权限时 deny todowrite，有则跳过
- `task` deny 保持不变（防止递归子 agent）

**构建/测试结果**：
- 编译：0 错误，仅存量 warning
- 测试：99% 通过（3361/3378），17 个存量失败（ERR-01/02/05/06/07 等，Round 11 之前已存在）
- 新增相关测试：45 个（system_prompt/retry_manager/task_tool 相关）全部 PASS

### 终审复核评分确认

| 维度 | 初审 | 复核 | 变化 |
|------|------|------|------|
| S | A | A | — |
| P | A- | A- | — |
| E | A- | A- | — |
| A | A | A | — |
| R | A- | A- | — |
| M | A | A | — |

**终审综合分：96/100（确认）**

---

## 七、Round 12 总结

| 项目 | 结果 |
|------|------|
| 扫描 commits | 3 个有效差距（G111/G112/G113） |
| 补全文件 | 4 个修改 + 1 个新建 |
| 综合分变化 | 89/100 → 96/100（+7分） |
| 累计进度 | R1-R12 持续提升，从初始 ~70 到当前 96 |
| git commit | 待执行 |
