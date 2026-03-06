# Docs Governance & Home

本文档用于规范当前文档目录 `./docs/` 的组织方式，提供文档的导航索引。

## 目录分类

- `./decisions`
  - 核心决策记录（core decision），保存不可逆或长期有效的决策。
- `./architecture`
  - 架构设计、核心概念、技术 PRD、技术流程等文档。
- `./plans`
  - 产品规划、产品愿景、发展路线图、重构计划、模版设计等。
- `./researchs`
  - 对外部项目的调研分析、竞品对比、决策分析、架构分析等。

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

## 快速导航

### 调研分析

- [1.0-opencode-project-architecture-analysis.md](./researchs/1.0-opencode-project-architecture-analysis.md) - OpenCode 项目架构设计/核心概念/核心流程技术分析报告
- [1.1-opencode-core-concepts-multi-agent-design-analysis.md](./researchs/1.1-opencode-core-concepts-multi-agent-design-analysis.md) - OpenCode 核心概念/多代理协作架构深度分析
- [2.0-turbot-ai-multi-agent-architecture-implementation-plan.md](./researchs/2.0-turbot-ai-multi-agent-architecture-implementation-plan.md) - Turbot-AI 多Agent协作架构技术实现方案

<!-- ToDo -->
