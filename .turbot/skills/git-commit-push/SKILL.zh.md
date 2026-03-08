# Git 提交与推送 — 中文参考

自动按照 Conventional Commits 规范提交更改并推送到远程仓库。

## 提交信息格式

遵循 Conventional Commits 规范：

```
<类型>(<范围>): <描述>

[可选的正文]
```

### 类型说明

| 类型       | 用途                     |
| ---------- | ------------------------ |
| `feat`     | 新功能                   |
| `fix`      | Bug 修复                 |
| `docs`     | 仅文档更改               |
| `style`    | 格式调整，无代码逻辑变更 |
| `refactor` | 代码重构                 |
| `test`     | 添加/更新测试            |
| `chore`    | 构建、配置、依赖         |
| `perf`     | 性能优化                 |
| `ci`       | CI/CD 更改               |

### 示例

```
feat(core): 添加用户认证模块
fix(storage): 修复 SQLite 连接泄漏
docs(api): 更新接口文档
refactor(provider): 提取公共 HTTP 客户端逻辑
test(crypto): 添加加密边界测试
```

## 按模块分批提交规则

当变更涉及多个模块时，**必须**按模块拆分提交，不得将所有变更合并为一次提交。

### 模块识别策略

根据文件路径识别所属模块：

| 路径特征           | 模块名称     |
| ------------------ | ------------ |
| `packages/app/`    | `app`        |
| `packages/ui/`     | `ui`         |
| `packages/sdk/`    | `sdk`        |
| `internal/agent/`  | `agent`      |
| `internal/db/`     | `db`         |
| `internal/config/` | `config`     |
| `libs/core/`       | `core`       |
| `libs/utils/`      | `utils`      |
| `libs/network/`    | `network`    |
| 根目录配置文件     | `chore`      |
| 其他路径           | 取顶层目录名 |

### 分批提交流程

1. 运行 `git diff --name-only HEAD`（或 `git status`）获取所有变更文件
2. 按模块分组文件，列出各模块的变更清单
3. 向用户展示分组结果，确认提交顺序
4. 按模块**逐批**执行以下操作：
   - `git add <该模块的文件列表>`
   - `git diff --cached` 分析变更内容
   - 生成针对该模块的提交信息
   - `git commit -m "<类型>(<模块>): <描述>"`
5. 所有模块提交完毕后，执行一次 `git push`

### 分批提交示例

```bash
# 第 1 批：core 模块
git add libs/core/src/xxx.cpp libs/core/include/xxx.h
git commit -m "feat(core): 添加 token 流式处理支持"

# 第 2 批：network 模块
git add libs/network/src/client.cpp
git commit -m "fix(network): 修复 HTTP 连接超时问题"

# 第 3 批：config 变更
git add internal/config/config.go
git commit -m "chore(config): 更新默认超时配置"

# 统一推送
git push
```

## 工作流程

1. **检查状态**: 运行 `git status` 查看更改
2. **模块分组**: 按路径识别模块，对变更文件进行分组
3. **确认顺序**: 向用户展示分组结果，确认提交批次和顺序
4. **逐批暂存**: 每批只 `git add` 该模块的文件
5. **生成信息**: 针对当前批次 `git diff --cached` 生成对应模块的提交信息
6. **提交当前批次**: 执行 `git commit -m "信息"`
7. **重复 4-6**: 直到所有模块提交完毕
8. **统一推送**: 所有批次完成后执行一次 `git push`

## 提交信息语言规则

提交信息的语言（中文/英文）应与该仓库的历史提交保持一致，**禁止**在同一仓库中混用语言。

### 语言检测流程

1. 执行以下命令获取最近 10 条提交信息：
   ```bash
   git log --oneline -10
   ```
2. 统计历史 log 中中文与英文描述的比例：
   - **中文占多数** → 使用中文生成提交信息
   - **英文占多数** → 使用英文生成提交信息
   - **数量相当（5:5）** → 优先沿用最近一条提交的语言
   - **无历史提交（空仓库）** → 默认使用英文
3. 按检测结果统一本次所有批次的提交信息语言

### 语言示例对比

```bash
# 中文仓库风格
git commit -m "feat(core): 添加 token 流式处理支持"
git commit -m "fix(network): 修复 HTTP 连接超时问题"

# 英文仓库风格
git commit -m "feat(core): add token streaming support"
git commit -m "fix(network): fix HTTP connection timeout issue"
```

## 重要规则

- **禁止推送到 main/master** 分支，除非用户明确确认
- **禁止使用 --force**，除非用户明确要求
- 提交前始终向用户展示提交信息
- 检测到合并冲突时，停止并通知用户
- **提交语言必须与历史 log 保持一致**，不得擅自切换语言
