# Docs Governance & Home

本文档用于规范当前文档目录 `./docs/` 的组织方式，提供文档的导航索引。

## 目录分类

- `./decisions`
  - 核心决策记录（core decision），保存不可逆或长期有效的决策文档。
- `./architecture`
  - 架构设计、核心概念、技术 PRD、技术流程等文档。
- `./plans`
  - 产品规划、产品愿景、发展路线图、重构计划、模版设计等。
- `./researchs`
  - 对外部项目的调研分析、竞品对比、决策分析、架构分析等文档。
- `./reviews`
  - 代码 review 的文档记录。

## 文档存放规则

- 先判断文档类型，再落目录，不能把新文档直接放在 `./docs/` 根目录。
- 同一主题的文档，优先补充已有文档，避免同义的重复文档；
- 文档命名规则：
  - 需要使用与内容语义相同，方便理解的单词词汇来命名。
  - 文档的语义命名的开头必须以 序号来开头。
  - 如果被要求生成不同版本的文档，则相同主题/命名的文档则逐步累加 `_v2/v3/...`等后缀。
- 增加新文档后，需要同步更新 `./docs/README.md` 的 `快速导航` 章节。

## 推荐命名规则

- 调研分析：`<n.n>-<topic>-<research|analysis>.md`，保存到 `docs/researchs/`目录。
- 竞品对比：`<n.n>-<topic>-comparison-<projects>.md`，保存到 `docs/researchs/`目录。
- 方案设计：`<n.n>-<topic>-design.md`，保存到 `docs/plans/`目录。
- 项目计划：`<n.n>-<topic>-plan.md`，保存到 `docs/plans/`目录。
- 技术重构：`<n.n>-<topic>-refactor.md`，保存到 `docs/plans/`目录。
- review：`<yyyy-mm-dd>-<module>-code-review.md`

## 快速导航

### 调研分析

- [1.0-opencode-project-architecture-analysis.md](./researchs/1.0-opencode-project-architecture-analysis.md) - OpenCode 项目架构设计/核心概念/核心流程技术分析报告
- [1.1-opencode-core-concepts-multi-agent-design-analysis.md](./researchs/1.1-opencode-core-concepts-multi-agent-design-analysis.md) - OpenCode 核心概念/多代理协作架构深度分析

### 项目计划

**总体规划**

- [1.0-turbot-ai-multi-agent-architecture-implementation-plan.md](./plans/1.0-turbot-ai-multi-agent-architecture-implementation-plan.md) - Turbot-AI 多 Agent 协作架构技术实现方案（原始总体规划）

**分阶段实现计划**

- [1.1-turbot-ai-foundation-infra-plan.md](./plans/1.1-turbot-ai-foundation-infra-plan.md) - 基础架构实现计划（15-25 天）
- [1.2-turbot-ai-storage-layer-plan.md](./plans/1.2-turbot-ai-storage-layer-plan.md) - 存储层实现计划（13-20 天）
- [1.3-turbot-ai-network-layer-plan.md](./plans/1.3-turbot-ai-network-layer-plan.md) - 网络层实现计划（12-18 天）
- [1.4-turbot-ai-core-data-models-plan.md](./plans/1.4-turbot-ai-core-data-models-plan.md) - 核心数据模型实现计划（12-18 天）
- [1.5-turbot-ai-provider-system-plan.md](./plans/1.5-turbot-ai-provider-system-plan.md) - Provider 系统实现计划（8-12 天）
- [1.6-turbot-ai-permission-tool-system-plan.md](./plans/1.6-turbot-ai-permission-tool-system-plan.md) - 权限和工具系统实现计划（9-14 天）
- [1.7-turbot-ai-agent-session-system-plan.md](./plans/1.7-turbot-ai-agent-session-system-plan.md) - Agent 和会话系统实现计划（14-19 天）
- [1.8-turbot-ai-multiagent-app-plan.md](./plans/1.8-turbot-ai-multiagent-app-plan.md) - 多 Agent 协作和应用层实现计划（21-28 天）

**总体估算**: 104-154 天（约 3.5-5 个月）

**v2.9 测试质量提升计划**

- [2.9/2.9-test-coverage-improvement-plan.md](./plans/2.9/2.9-test-coverage-improvement-plan.md) - 单元测试覆盖率提升计划（目标：整体行覆盖率 ≥85%）

### 代码审查

- [2026-03-07-libs-code-review.md](./reviews/2026-03-07-libs-code-review.md) - libs 目录全面代码审核报告
- [2026-03-08-config-refactor-review.md](./reviews/2026-03-08-config-refactor-review.md) - 配置系统重构代码审查
- [2026-03-11-v2.5-core-flow-review.md](./reviews/2026-03-11-v2.5-core-flow-review.md) - v2.5 核心流程代码审查
- [2026-03-11-v2.6-quality-hardening-review.md](./reviews/2026-03-11-v2.6-quality-hardening-review.md) - v2.6 质量加固代码审查
- [2026-03-13-unit-test-coverage-report.md](./reviews/2026-03-13-unit-test-coverage-report.md) - 单元测试覆盖率报告（整体 70.33%，7 failed）

<!-- ToDo -->
