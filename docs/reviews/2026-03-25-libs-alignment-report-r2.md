# Turbot libs 核心实现对齐报告（Round 2）& 补全计划

> **评审日期**：2026-03-25（第二轮全量扫描）  
> **评审原则**：100% 复刻 OpenCode（接口一致 / 数据结构一致 / 行为逻辑一致）  
> **评审范围**：`turbot-ai/libs/core/` ↔ `opencode/packages/opencode/src/`  
> **评审方法**：SPEARM 六维度 + 源码级逐模块对比  
> **基准版本**：OpenCode 当前 main（2026-03-25 快照，含所有已合并 PR）  
> **前序参考**：[Round 1 报告](./2026-03-25-libs-alignment-report.md)（T1~T20 全部完成）

---

## 一、执行摘要

T1~T20 全部完成后，本轮对 turbot libs 做全量重新扫描。

| 指标                   | Round 1（T1~T20前）| Round 2（当前）| 目标   |
| ---------------------- | ------------------ | -------------- | ------ |
| **综合复刻率**         | ~68%               | **~78%**       | 100%   |
| **模块覆盖**           | 19/39（49%）       | **29/39（74%）** | 39/39 |
| **Session 模块**       | ~65%               | **~75%**       | 100%   |
| **Provider 实现**      | ~45%               | **~52%**       | 100%   |
| **LSP 服务器**         | 27/37（73%）       | **29/37（78%）** | 37/37 |
| **Tool 工具**          | 21/23（91%）       | **22/23（96%）** | 23/23 |
| **事件系统集成**       | 0%                 | **~85%**（Session/Project 已集成）| 100% |

> **本轮新增（T1~T20 成果）**：  
> ide/（IDE检测）、flag/（功能开关）、file/（文件系统增强）、id/（ID生成）、global/（路径单例）、instance（项目单例）、session_todo、session_instruction、truncate工具、Anthropic Provider、fetchUser/Orgs、models-snapshot、Oxlint/Gleam LSP、addSandbox、question/ 模块

---

## 二、SPEARM 评分（以"100% 复刻"为评分基准）

| 维度                       | 等级 | 分数 | 关键发现                                                                                       |
| -------------------------- | ---- | ---- | ---------------------------------------------------------------------------------------------- |
| 🔴 **S — Security**        | A-   | 9.0  | HTTP 客户端 libcurl；SQLite 事务完整；popen 路径注入已修复                                    |
| 🟡 **P — Performance**     | B+   | 8.5  | 同步 HTTP 调用在 account/provider 有改善空间；Session 无 PartDelta 流式写入                   |
| 🟠 **E — Error Handling**  | B+   | 8.5  | 基础路径好；`session/summary.ts` 无等价实现（SessionSummary.summarize 缺）；PartID 流缺失     |
| 🔵 **A — Architecture**    | B+   | 8.5  | 事件系统已集成 Session/Project；`control-plane` 完全缺失（工作空间路由）；plugin 无实质实现   |
| 📊 **R — Reliability**     | B+   | 8.5  | parts 表已建；message TEXT PK 已对齐；SessionSummary diff 统计未实现；revert PartID 逻辑不完整 |
| 🔧 **M — Maintainability** | A-   | 9.0  | 模块结构清晰；transform.ts/provider error.ts 缺失降低可维护性                                 |

**综合评分**：**87 / 100 — 🟢 良好**

> **评分计算**：S(9×3) + P(8.5×2) + E(8.5×2) + A(8.5×1.5) + R(8.5×1.5) + M(9×1) = 27+17+17+12.75+12.75+9 = 95.5 / (3+2+2+1.5+1.5+1)=11 → 95.5/11 = 8.68 → 87/100  
> **主要短板**：SessionSummary 差异统计缺失 + control-plane 完全缺失 + Provider transform/auth/error 层缺失

---

## 三、逐模块对齐状态（Round 2）

### 3.1 基础设施层（`libs/network`, `libs/storage`, `libs/utils`）

| 模块          | Turbot 实现                                 | OpenCode 对标            | 复刻状态             |
| ------------- | ------------------------------------------- | ------------------------ | -------------------- |
| HTTP 客户端   | `network/http_client.cpp`（641行，libcurl） | Effect HttpClient        | ✅ 功能等价          |
| URL 解析      | `network/url.cpp`                           | 内置                     | ✅ 完整              |
| SQLite 数据库 | `storage/sqlite_database.cpp`（560行）      | `storage/db.ts` + libsql | ✅ 功能等价          |
| 数据库迁移    | `storage/migration.cpp`                     | `migration/` 9个迁移文件 | ⚠️ 迁移版本差异（+5个新迁移）|
| 事务管理      | `storage/transaction.cpp`                   | `Database.transaction()` | ✅ 对齐              |
| 加密工具      | `utils/crypto_utils.cpp`                    | 无直接对应               | ✅ 超规格            |
| 环境变量      | `utils/env_utils.cpp`                       | `env/index.ts`           | ✅ 对齐              |
| 文件工具      | `utils/file_utils.cpp`                      | `util/filesystem.ts` 等  | ⚠️ 覆盖度约 60%      |
| JSON 工具     | `utils/json_utils.cpp`                      | 无直接对应               | ✅ 额外实现          |

**基础设施层复刻率：~88%** — 迁移版本仍不严格对齐（新增5个 migration）。

---

### 3.2 Session 模块（核心差距所在）

**OpenCode 文件**：17个（含 revert.ts 独立）  
**Turbot 文件**：9个 .cpp

| OpenCode 文件               | Turbot 等价实现                                    | 复刻状态                                          |
| --------------------------- | -------------------------------------------------- | ------------------------------------------------- |
| `index.ts`（Session API）   | `session.cpp` + `session_store.cpp`                | ✅ ~85%                                           |
| `compaction.ts`             | `session_compaction.cpp`                           | ✅ 对齐                                           |
| `status.ts`                 | `session_state_machine.cpp`                        | ✅ 对齐                                           |
| `retry.ts`                  | `retry_manager.cpp`                                | ✅ 对齐                                           |
| `llm.ts`                    | `session_loop.cpp` + `llm/`                        | ✅ 对齐                                           |
| `message-v2.ts`（989行）    | `message/message.cpp` + `session_store.cpp`        | ⚠️ ~70%（PartDelta 流缺失；OutputFormat 类型缺）  |
| `message.ts`（旧版）        | —                                                  | ✅ 不需要（v2 覆盖）                              |
| **`processor.ts`（431行）** | `session_loop.cpp`（含 doom loop）                 | ⚠️ ~60%（reasoning/PartID part 精细管理缺失）     |
| **`prompt.ts`（66.6KB）**   | `llm/system_prompt.cpp` + `llm/prompt_builder.cpp` | ⚠️ ~35%（SessionPrompt.assertNotBusy 等 API 缺）  |
| `instruction.ts`            | `session_instruction.cpp`                          | ✅ 已实现（T5）                                   |
| **`summary.ts`（171行）**   | **无独立实现**                                     | ❌ **完全缺失**（diff统计 / summarize / Bus.Diff） |
| `todo.ts`                   | `session_todo.cpp`                                 | ✅ 已实现（T20）                                  |
| **`revert.ts`（139行）**    | `session.cpp::revert()/unrevert()`                 | ⚠️ ~60%（PartID 级别 revert + cleanup() 缺失）    |
| `system.ts`                 | `llm/system_prompt.cpp`                            | ✅ 部分对齐                                       |
| `schema.ts`                 | `session.hpp` + `session_store.hpp`                | ✅ 结构对齐                                       |
| `session.sql.ts`            | `session_store.cpp ensure_schema()`                | ✅ schema 完整                                    |

**Session 模块复刻率：~75%**

**仍然缺失/不完整**：
- `summary.ts` — `SessionSummary.summarize()`、`SessionSummary.diff()`、`computeDiff()`（git diff 统计）、Bus `Session.Event.Diff` 事件
- `processor.ts` — reasoning 流（reasoning-start/delta/end）、PartID 精细管理（updatePartDelta）
- `revert.ts` — `cleanup()` 函数（PartID 级别删除 + Bus.MessageV2.Event.PartRemoved）
- `prompt.ts` — `SessionPrompt.assertNotBusy()` + 完整提示词组合系统

---

### 3.3 DB Schema 一致性

| 方面                   | OpenCode                                         | Turbot                                    | 差距      |
| ---------------------- | ------------------------------------------------ | ----------------------------------------- | --------- |
| 消息 PK                | `id TEXT`（`msg_` 前缀）                         | `id TEXT`（`msg_` 前缀）                  | ✅ 一致   |
| ID 前缀体系            | `ses_`/`msg_`/`prt_`（`Identifier` 命名空间）   | `ses_`/`msg_`/`prt_`（`id::` 命名空间）  | ✅ 对齐   |
| 消息排序索引           | `message_session_time_created_id_idx`           | ✅ 已建立同名索引                          | ✅ 对齐   |
| Part 表                | `PartTable`（PartID TEXT PK）+ `part_message_id_id_idx` | ✅ `parts` 表 + 同名索引           | ✅ 对齐   |
| Session 字段           | `workspace_id` + `share_url` + `summary_diffs` + `permission` | ✅ 全部字段已补全          | ✅ 对齐   |
| **Todo 表名**          | `todo`（单数）                                   | `todos`（复数）                           | ❌ **P1** |
| **`session_diff` 存储** | `Storage.write(["session_diff", sessionID])` JSON 文件系统 | ❌ 无等价 key-value 存储           | ❌ **P1** |
| **updatePartDelta**    | `PartDelta` 写入（field + delta，增量更新）      | ❌ 仅有 updatePart（全量写入）             | ❌ **P1** |

> **校验说明**：  
> - OpenCode `id/id.ts` 确认前缀 `ses_`/`msg_`/`prt_`，与 Turbot 完全一致 ✅  
> - Migration `20260312043431_session_message_cursor` 仅重建了索引（非新增字段），Turbot 已有同名索引 ✅  
> - `summary_diffs`（Snapshot.FileDiff[]）和 `permission` 字段 Turbot 均已实现 ✅

---

### 3.4 Project 模块

| OpenCode 文件     | Turbot 等价实现                    | 复刻状态                                      |
| ----------------- | ---------------------------------- | --------------------------------------------- |
| `project.ts`      | `project.cpp`（27.8KB）            | ✅ ~85%（addSandbox/removeSandbox 已实现）     |
| `project.sql.ts`  | `project_store.cpp`                | ✅ 对齐                                       |
| `bootstrap.ts`    | `project.cpp::from_directory()`    | ✅ 已集成                                     |
| `vcs.ts`          | `project.cpp::init_git()`          | ✅ 已实现真实调用（T15）                      |
| `instance.ts`     | `instance.cpp`                     | ✅ 已实现（T7）                               |
| **`state.ts`**    | **无**                             | ❌ **P2**（State.create/dispose 生命周期管理） |
| `schema.ts`       | `project.hpp` 结构体               | ✅ 对齐                                       |

**Project 模块复刻率：~88%** — 仅 `state.ts` 缺失。

---

### 3.5 Account + Auth 模块

| 功能                     | OpenCode               | Turbot                               | 复刻状态       |
| ------------------------ | ---------------------- | ------------------------------------ | -------------- |
| `login()` / `poll()`     | ✅                     | ✅ 已实现                            | ✅             |
| `resolveToken()`         | ✅                     | ✅ 已实现                            | ✅             |
| `fetchUser()`            | ✅                     | ✅ 已实现（T9）                      | ✅             |
| `fetchOrgs()`            | ✅                     | ✅ 已实现（T9）                      | ✅             |
| `config()` 远程配置      | ✅                     | ❌ 缺失                              | P2             |
| `repo.ts`（数据访问层）  | ✅ 独立 repo 模块      | ⚠️ 内联在 account.cpp 中            | ⚠️ 结构差异   |
| `account.sql.ts`         | ✅ 2张表               | ✅ `account_store.cpp`               | ✅             |

**Account 模块复刻率：~85%**

---

### 3.6 Provider 模块

| OpenCode 文件               | Turbot 等价实现               | 复刻状态                           |
| --------------------------- | ----------------------------- | ---------------------------------- |
| `provider.ts`（52.6KB）     | `provider.cpp`（23.4KB）      | ⚠️ ~55%（大量辅助 API 未实现）     |
| `schema.ts`                 | `provider.hpp` 结构体         | ✅ 基本对齐                        |
| `models.ts`                 | `provider.cpp` 内联           | ⚠️ 动态发现能力弱                  |
| `models-snapshot.ts`        | `provider.cpp` ModelsDev 类   | ✅ 已实现（T10）                   |
| **`transform.ts`（32.8KB）**| **无**                        | ❌ **P1** — 响应转换层缺失         |
| **`auth.ts`（8.1KB）**      | **无独立模块**                | ❌ **P1** — Provider Auth 缺失     |
| **`error.ts`（6.5KB）**     | **无独立模块**                | ❌ **P1** — ProviderError 类型缺失 |
| `impl/` Anthropic           | `anthropic_provider.cpp`      | ✅ 已实现（T8）                    |
| `impl/` Azure               | ❌ 缺失                       | ❌ **P1**                          |
| `impl/` Google/Gemini       | ❌ 缺失                       | ❌ **P1**                          |
| `sdk/copilot/`              | ❌ 缺失                       | P2                                 |
| `impl/` bailian/kimi/zhipu  | ✅ 3个额外实现                | ✅ 超规格                          |

**Provider 模块复刻率：~52%**（核心 transform/auth/error 层仍缺失）

---

### 3.7 Tool 工具系统

| 工具          | OpenCode | Turbot                       | 状态          |
| ------------- | -------- | ---------------------------- | ------------- |
| apply_patch   | ✅       | ✅ `apply_patch_tool.cpp`    | ✅ 超规格     |
| bash          | ✅       | ✅ `bash_tool.cpp`           | ✅ 超规格     |
| batch         | ✅       | ✅ `batch_tool.cpp`          | ✅ 对齐       |
| codesearch    | ✅       | ✅ `codesearch_tool.cpp`     | ✅ 超规格     |
| edit          | ✅       | ✅ `edit_tool.cpp`           | ✅ 对齐       |
| glob          | ✅       | ✅ `glob_tool.cpp`           | ✅ 超规格     |
| grep          | ✅       | ✅ `grep_tool.cpp`           | ✅ 超规格     |
| ls            | ✅       | ✅ `list_tool.cpp`           | ✅ 超规格     |
| lsp           | ✅       | ✅ `lsp_tool.cpp`            | ✅ 超规格     |
| multiedit     | ✅       | ✅ `multiedit_tool.cpp`      | ✅ 对齐       |
| plan          | ✅       | ✅ `plan_tool.cpp`           | ✅ 对齐       |
| question      | ✅       | ✅ `question_tool.cpp`       | ✅ 超规格     |
| read          | ✅       | ✅ `read_file_tool.cpp`      | ✅ 对齐       |
| task          | ✅       | ✅ `task_tool.cpp`           | ✅ 对齐       |
| todo          | ✅       | ✅ `todo_tool.cpp`           | ✅ 超规格     |
| webfetch      | ✅       | ✅ `webfetch_tool.cpp`       | ✅ 对齐       |
| websearch     | ✅       | ✅ `websearch_tool.cpp`      | ✅ 对齐       |
| write         | ✅       | ✅ `write_file_tool.cpp`     | ✅ 超规格     |
| invalid       | ✅       | ✅ `invalid_tool.cpp`        | ✅ 对齐       |
| external-dir  | ✅       | ✅ `external_directory.cpp`  | ✅ 对齐       |
| skill         | ✅       | ✅ `skill_tool.cpp`          | ✅ 对齐       |
| truncate      | ✅       | ✅ `truncate_tool.cpp`       | ✅ 已实现（T11）|
| **truncation-dir** | ✅  | ❌ 缺失                      | ❌ **P1**      |

> truncation-dir（0.1KB）非常简单，是 `truncate.ts` 的目录伴随文件，仅需几行实现。

**Tool 复刻率：22/23（96%）** — 仅 `truncation-dir` 缺失

---

### 3.8 LSP 模块

| LSP 服务器              | OpenCode | Turbot                  | 状态             |
| ----------------------- | -------- | ----------------------- | ---------------- |
| Clangd                  | ✅       | ✅                      | ✅               |
| Pyright                 | ✅       | ✅                      | ✅               |
| Ty（Python 类型检查）   | ✅       | ❌                      | P3               |
| Gopls                   | ✅       | ✅                      | ✅               |
| TypeScript              | ✅       | ✅                      | ✅               |
| Vue (volar)             | ✅       | ✅                      | ✅               |
| ESLint                  | ✅       | ✅                      | ✅               |
| **Oxlint**              | ✅       | ✅ `make_oxlint_server` | ✅ 已实现（T17） |
| Biome                   | ✅       | ✅                      | ✅               |
| Deno                    | ✅       | ✅                      | ✅               |
| Rust Analyzer           | ✅       | ✅                      | ✅               |
| Svelte                  | ✅       | ✅                      | ✅               |
| Astro                   | ✅       | ✅                      | ✅               |
| BashLS                  | ✅       | ✅                      | ✅               |
| Java (JDTLS)            | ✅       | ✅                      | ✅               |
| Kotlin (KotlinLS)       | ✅       | ✅                      | ✅               |
| C# (CSharp)             | ✅       | ✅                      | ✅               |
| F# (FSharp)             | ✅       | ❌                      | P3               |
| Swift (SourceKit)       | ✅       | ✅                      | ✅               |
| Clojure                 | ✅       | ✅                      | ✅               |
| Dart                    | ✅       | ✅                      | ✅               |
| Elixir (ElixirLS)       | ✅       | ✅                      | ✅               |
| Haskell (HLS)           | ✅       | ✅                      | ✅               |
| Lua (LuaLS)             | ✅       | ✅                      | ✅               |
| Nix (Nixd)              | ✅       | ✅                      | ✅               |
| OCaml                   | ✅       | ✅                      | ✅               |
| PHP (PHPIntelephense)   | ✅       | ✅                      | ✅               |
| Ruby (Rubocop)          | ✅       | ✅                      | ✅               |
| Terraform (TerraformLS) | ✅       | ✅                      | ✅               |
| Zig (Zls)               | ✅       | ✅                      | ✅               |
| YAML (YamlLS)           | ✅       | ❌                      | P3               |
| Prisma                  | ✅       | ❌                      | P3               |
| Dockerfile (DockerfileLS) | ✅     | ❌                      | P3               |
| **Gleam**               | ✅       | ✅ `make_gleam_server`  | ✅ 已实现（T17） |
| Tinymist (Typst)        | ✅       | ❌                      | P3               |
| Tex (TexLab)            | ✅       | ❌                      | P3               |
| Julia (JuliaLS)         | ✅       | ❌                      | P3               |

**LSP 服务器复刻率：29/37（78%）** — P3 级别8个剩余服务器

---

### 3.9 ACP 模块

| OpenCode 文件       | Turbot 等价实现           | 复刻状态                                         |
| ------------------- | ------------------------- | ------------------------------------------------ |
| `agent.ts`（58.1KB）| `acp/agent.cpp`（27.7KB） | ⚠️ ~55%（大量消息流处理 API 未完整对齐）         |
| `session.ts`        | `acp/session.cpp`         | ⚠️ ~70%                                          |
| `types.ts`          | 内联在头文件              | ✅ 基本对齐                                      |

**ACP 模块复刻率：~60%**

---

### 3.10 Config 模块

| OpenCode 文件        | Turbot 等价实现         | 复刻状态                                           |
| -------------------- | ----------------------- | -------------------------------------------------- |
| `config.ts`（54.8KB）| `config_manager.cpp`    | ⚠️ ~60%（TUI schema / markdown config 未实现）    |
| `paths.ts`           | `config_path.cpp`       | ✅ 基本对齐                                        |
| `markdown.ts`        | ❌ 无                   | ❌ P2（Markdown 格式配置解析）                    |
| `tui-schema.ts`      | ❌ 无                   | P3（TUI 特有）                                     |
| `tui.ts`             | ❌ 无                   | P3（TUI 特有）                                     |
| `migrate-tui-config.ts` | ❌ 无               | P3（TUI 特有）                                     |

**Config 模块复刻率：~65%**

---

### 3.11 完全缺失模块（OpenCode 有，Turbot 无）

| OpenCode 模块              | 功能描述                                   | 复刻优先级     |
| -------------------------- | ------------------------------------------ | -------------- |
| **`control-plane/`（9文件）** | 工作空间控制平面、workspace 路由、多实例管理 | **P1**       |
| **`session/summary.ts`**   | 会话 diff 统计（git diff 聚合 + Bus 推送） | **P1**         |
| **`provider/transform.ts`**| 响应转换管道（ProviderTransform）          | **P1**         |
| **`provider/auth.ts`**     | Provider 级别认证流程                      | **P1**         |
| **`provider/error.ts`**    | ProviderError 类型化错误体系               | **P1**         |
| **`provider/impl/azure`**  | Azure OpenAI Provider                      | **P1**         |
| **`provider/impl/gemini`** | Google Gemini Provider                     | **P1**         |
| `project/state.ts`         | 有状态单例生命周期管理                     | P2             |
| `config/markdown.ts`       | Markdown 格式配置解析                      | P2             |
| `pty/`（2文件）            | 伪终端管理                                 | P2             |
| `shell/`（1文件）          | Shell 集成（Shell.expand 等）              | P2             |
| `format/`（2文件）         | 代码格式化集成                             | P2             |
| `share/`（2文件）          | 会话分享（share-next + DB）                | P2             |
| `plugin/copilot.ts`        | Copilot 插件实现（12.3KB）                 | P2             |
| `plugin/codex.ts`          | Codex 插件实现（20.0KB）                   | P2             |
| `tool/truncation-dir.ts`   | Truncate 目录工具（0.1KB）                 | **P1**（很小） |
| `worktree/`（1文件）       | Git worktree 支持                          | P3             |
| `bun/`（2文件）            | Bun 特有运行时（TS 特有，可跳过）          | Skip           |
| `effect/`（5文件）         | Effect 运行时（TS 特有，可跳过）           | Skip           |

---

### 3.12 Session prompt 对齐

| OpenCode `session/prompt/`          | Turbot `libs/core/prompts/`  | 状态                          |
| ----------------------------------- | ---------------------------- | ----------------------------- |
| `default.txt`（8.5KB）              | `openai.md` / `qwen.md`      | ⚠️ 内容差异（非官方 default） |
| `anthropic.txt`（8.0KB）            | `anthropic.md`（3.8KB）      | ⚠️ 内容覆盖约 50%             |
| `gemini.txt`（15.0KB）              | `gemini.md`（3.5KB）         | ⚠️ 内容覆盖约 30%             |
| `beast.txt`（10.8KB）               | `beast.md`（3.8KB）          | ⚠️ 内容覆盖约 40%             |
| `codex.txt`（7.2KB）                | `codex.md`（3.8KB）          | ⚠️ 内容覆盖约 50%             |
| `copilot-gpt-5.txt`（13.9KB）       | ❌ 无                         | ❌ P2                         |
| `trinity.txt`（7.6KB）              | `trinity.md`（0.5KB）        | ❌ 几乎空文件                  |
| `plan.txt`（1.4KB）                 | ❌ 无                         | ❌ P2                         |
| `plan-reminder-anthropic.txt`（4.0KB）| ❌ 无                       | ❌ P2                         |
| `max-steps.txt`（0.7KB）            | ❌ 无                         | ❌ P2                         |
| `build-switch.txt`（0.2KB）         | ❌ 无                         | ❌ P2                         |

**Prompt 复刻率：~45%**（文件数对齐但内容严重不足）

---

## 四、补全计划（Round 2，按优先级排序）

### P1 级补全（影响功能正确性和主流程完整性）

#### T21：`session/summary.ts` — 会话 diff 统计（预估 1.5天）

**目标**：实现 `SessionSummary::summarize()`，在 LLM 完成后统计 git diff（additions/deletions/files），通过 `Session.set_summary()` 持久化并发布 `session.diff` 事件。

```
新增文件：
- libs/core/include/turbot/core/session/session_summary.hpp
- libs/core/src/session/session_summary.cpp

关键逻辑（对齐 opencode/src/session/summary.ts）：
- summarize(session_id, message_id): 统计 git diff，调用 set_summary()，publish session.diff
- diff(session_id): 读取存储的 diff 结果
- compute_diff(messages): 扫描 step-start/step-finish snapshot，调用 Snapshot::diff()
```

---

#### T22：`provider/transform.ts` — 响应转换层（预估 2天）

**目标**：实现 `ProviderTransform` 管道，统一处理不同 Provider 的响应格式差异（function calling 格式 / streaming 差异 / 工具调用 input 反序列化）。

```
新增文件：
- libs/core/include/turbot/core/provider/provider_transform.hpp
- libs/core/src/provider/provider_transform.cpp

对齐：opencode/src/provider/transform.ts（32.8KB）
```

---

#### T23：`provider/error.ts` + `provider/auth.ts` — Provider 错误/认证（预估 1天）

**目标**：实现 `ProviderError` 类型化错误体系（对齐 `NamedError` 模式），以及 Provider 级别认证（API Key 验证 / OAuth）。

```
新增文件：
- libs/core/include/turbot/core/provider/provider_error.hpp
- libs/core/include/turbot/core/provider/provider_auth.hpp
- libs/core/src/provider/provider_error.cpp
- libs/core/src/provider/provider_auth.cpp
```

---

#### T24：Azure OpenAI Provider（预估 1天）

```
新增文件：
- libs/core/include/turbot/core/provider/impl/azure_provider.hpp
- libs/core/src/provider/impl/azure_provider.cpp

关键：Azure endpoint 格式（https://{resource}.openai.azure.com）+ api-version header
```

---

#### T25：Google Gemini Provider（预估 1.5天）

```
新增文件：
- libs/core/include/turbot/core/provider/impl/gemini_provider.hpp
- libs/core/src/provider/impl/gemini_provider.cpp

关键：Gemini API v1beta，contents 格式，function_declarations 结构
```

---

#### T26：`todos` 表名修正（预估 0.25天）

**目标**：将 `session_store.cpp` 中的 `todos` 表重命名为 `todo`（对齐 OpenCode `TodoTable` 表名），并更新所有相关 SQL 查询。

```
修改文件：
- libs/core/src/session/session_store.cpp（CREATE TABLE + 所有 SQL 引用）
- libs/core/src/session/session_todo.cpp（如有直接 SQL）
```

---

#### T27：`truncation-dir` 工具（预估 0.25天）

**目标**：对齐 `opencode/src/tool/truncation-dir.ts`（仅0.1KB，返回 turbot 截断输出目录路径）。

```
新增/修改：
- libs/core/src/tool/builtin/truncate_tool.cpp（追加 TruncationDir 工具）
- 或新增 truncation_dir_tool.cpp（极简实现）
```

---

#### T27：`control-plane/` 工作空间控制平面（预估 4天）

**目标**：实现多实例工作空间管理，是 ACP 协议多客户端接入的基础。

```
新增文件（对齐 opencode/src/control-plane/ 9个文件）：
- libs/core/include/turbot/core/control_plane/ (workspace.hpp, sse.hpp, schema.hpp 等)
- libs/core/src/control_plane/ (workspace.cpp, sse.cpp, workspace_server.cpp 等)

关键：Workspace 注册/路由 + SSE 事件推送 + 多实例协调
```

---

### P2 级补全（功能完整性增强）

| 任务编号 | 内容                                      | 新增/修改文件                                     | 预估工作量 |
| -------- | ----------------------------------------- | ------------------------------------------------- | ---------- |
| T28      | `session/revert.ts` — PartID 级 cleanup() | `session.cpp` 扩展 cleanup() + PartRemoved 事件  | 1天        |
| T29      | `session/processor.ts` — reasoning 流    | `session_loop.cpp` 追加 reasoning-start/delta/end | 1天        |
| T30      | `session/prompt.ts` 完整对齐              | `llm/prompt_builder.cpp` 扩展 assertNotBusy 等   | 1.5天      |
| T31      | `project/state.ts` — 状态生命周期管理     | `libs/core/src/project/state.cpp`                | 0.5天      |
| T32      | Session prompt 内容对齐（trinity/plan等） | `libs/core/prompts/` 补全                        | 1天        |
| T33      | `config/markdown.ts` — Markdown 配置      | `libs/core/src/config/config_markdown.cpp`       | 1天        |
| T34      | `plugin/copilot.ts` + `codex.ts` 实现    | `libs/core/src/plugin/` 扩展                     | 3天        |
| T35      | `pty/` 伪终端管理                         | `libs/core/src/pty/`                             | 2天        |
| T36      | `shell/` Shell 集成                       | `libs/core/src/shell/shell.cpp`                  | 1天        |
| T37      | `format/` 代码格式化集成                  | `libs/core/src/format/`                          | 1.5天      |
| T38      | DB migration 版本对齐（+5个迁移）         | `storage/migration.cpp`                          | 1天        |
| T39      | `message cursor` 字段补全                 | `session_store.cpp` DDL + 迁移                   | 0.5天      |
| T40      | updatePartDelta 流式写入                  | `session_store.cpp` + `session.hpp`              | 1天        |

---

### P3 级补全（可选/低优先级）

| 任务             | 内容                               | 预估工作量 |
| ---------------- | ---------------------------------- | ---------- |
| LSP P3 补全（8个）| YAML/Prisma/Dockerfile/Ty/FSharp等 | 1.5天     |
| `share/`         | 会话分享功能                       | 2天        |
| `worktree/`      | Git worktree 支持                  | 1.5天      |
| ACP agent.ts 深度对齐 | 消息流处理完整对齐             | 3天        |

---

## 五、工作量汇总

| 优先级                    | 任务数     | 总预估工作量 | 预期复刻率提升   |
| ------------------------- | ---------- | ------------ | ---------------- |
| **P1（T21~T27）**         | 7          | **~11.25天** | 78% → ~90%       |
| **P2（T28~T41）**         | 14         | **~17天**    | 90% → ~96%       |
| **P3**                    | 4          | **~8天**     | 96% → ~99%       |
| **合计**                  | 25         | **~36天**    | 100% 复刻目标     |

---

## 六、执行建议

### 推荐执行顺序（按依赖关系）

```
Sprint 1（T21+T23+T26）：Session summary + Provider error/auth + truncation-dir（短平快）
Sprint 2（T22+T24+T25）：Provider transform + Azure + Gemini
Sprint 3（T27）：control-plane（较复杂，独立 sprint）
Sprint 4（T28~T32）：Session 深度对齐（revert cleanup + reasoning + prompt）
Sprint 5（T33~T40）：周边增强（config/plugin/pty/db迁移）
```

### 验收标准（100% 复刻判定）

1. ✅ **接口一致**：所有 public API 签名与 OpenCode 等价
2. ✅ **数据结构一致**：所有字段与 OpenCode 对齐
3. ✅ **行为一致**：相同输入产生相同输出（包括错误场景）
4. ✅ **事件一致**：写操作后能触发等价的事件推送
5. ✅ **测试覆盖**：每个模块有对应的单元测试

---

## 七、Round 1 vs Round 2 进度对比

| 模块          | Round 1 复刻率 | Round 2 复刻率 | 提升 | 主要成果                              |
| ------------- | -------------- | -------------- | ---- | ------------------------------------- |
| 基础设施层    | ~90%           | ~88%           | -    | 新发现5个迁移文件差距                 |
| Session       | ~65%           | ~75%           | +10% | instruction/todo/EventBus 集成        |
| Project       | ~70%           | ~88%           | +18% | addSandbox/instance/initGit           |
| Account       | ~75%           | ~85%           | +10% | fetchUser/fetchOrgs                   |
| Provider      | ~45%           | ~52%           | +7%  | Anthropic/models-snapshot             |
| Tool          | 91%            | 96%            | +5%  | truncate                              |
| LSP           | 73%            | 78%            | +5%  | Oxlint/Gleam                          |
| 新增模块      | —              | ✅ ide/flag/file/id/global/question | —  | 全新实现          |
| **综合**      | **~68%**       | **~78%**       | **+10%** | —                                 |

---

_报告生成时间：2026-03-25（Round 2）_  
_评审基准：OpenCode packages/opencode/src/ TypeScript 源码（2026-03-25 快照）_  
_评审人：code-reviewer（SPEARM 六维度框架）_
