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
- 方案设计：`<n.n>-<topic>-design.md`，保存到 `docs/architecture/`目录。
- 项目计划：`<n.n>-<topic>-plan.md`，保存到 `docs/plans/<X.Y>/`目录。
- 技术重构：`<n.n>-<topic>-refactor.md`，保存到 `docs/plans/<X.Y>/`目录。
- ADR：`<NNN>-<topic>.md`，保存到 `docs/decisions/`目录。
- 代码审查：`<yyyy-mm-dd>-<module>-code-review.md`，保存到 `docs/reviews/`目录。
- API 文档：`<topic>-api.md` 或 `<topic>-protocol.md`，保存到 `docs/apis/`目录。

## 快速导航

### 调研分析

- [1.0-opencode-project-architecture-analysis.md](./researchs/1.0-opencode-project-architecture-analysis.md) - OpenCode 项目架构设计/核心概念/核心流程技术分析报告
- [1.1-opencode-core-concepts-multi-agent-design-analysis.md](./researchs/1.1-opencode-core-concepts-multi-agent-design-analysis.md) - OpenCode 核心概念/多代理协作架构深度分析

### 架构设计

- [2.x-architecture.md](./architecture/2.x-architecture.md) - Turbot-AI 2.x 整体架构文档（v2.0 起）
- [config-hierarchy.md](./architecture/config-hierarchy.md) - 配置分级机制设计文档
- [directory-structure.md](./architecture/directory-structure.md) - 目录架构规范

### 核心决策（ADR）

- [001-agent-definition-and-generation.md](./decisions/001-agent-definition-and-generation.md) - ADR-001：Agent 定义与动态生成模块架构
- [002-config-hierarchy-architecture.md](./decisions/002-config-hierarchy-architecture.md) - ADR-002：配置分级机制架构
- [003-directory-structure-architecture.md](./decisions/003-directory-structure-architecture.md) - ADR-003：目录架构规范
- [004-prompt-template-independence.md](./decisions/004-prompt-template-independence.md) - ADR-004：Prompt 模板文件独立

### 项目计划

**v1.0 初始架构实现**

- [1.0/1.0-turbot-ai-multi-agent-architecture-implementation-plan.md](./plans/1.0/1.0-turbot-ai-multi-agent-architecture-implementation-plan.md) - Turbot-AI 多 Agent 协作架构技术实现方案（原始总体规划）
- [1.0/1.1-turbot-ai-foundation-infra-plan.md](./plans/1.0/1.1-turbot-ai-foundation-infra-plan.md) - 基础架构实现计划（15-25 天）
- [1.0/1.2-turbot-ai-storage-layer-plan.md](./plans/1.0/1.2-turbot-ai-storage-layer-plan.md) - 存储层实现计划（13-20 天）
- [1.0/1.3-turbot-ai-network-layer-plan.md](./plans/1.0/1.3-turbot-ai-network-layer-plan.md) - 网络层实现计划（12-18 天）
- [1.0/1.4-turbot-ai-core-data-models-plan.md](./plans/1.0/1.4-turbot-ai-core-data-models-plan.md) - 核心数据模型实现计划（12-18 天）
- [1.0/1.5-turbot-ai-provider-system-plan.md](./plans/1.0/1.5-turbot-ai-provider-system-plan.md) - Provider 系统实现计划（8-12 天）
- [1.0/1.6-turbot-ai-permission-tool-system-plan.md](./plans/1.0/1.6-turbot-ai-permission-tool-system-plan.md) - 权限和工具系统实现计划（9-14 天）
- [1.0/1.7-turbot-ai-agent-session-system-plan.md](./plans/1.0/1.7-turbot-ai-agent-session-system-plan.md) - Agent 和会话系统实现计划（14-19 天）
- [1.0/1.8-turbot-ai-multiagent-app-plan.md](./plans/1.0/1.8-turbot-ai-multiagent-app-plan.md) - 多 Agent 协作和应用层实现计划（21-28 天）

**v2.0 后续功能开发**

- [2.0/2.0-turbot-ai-follow-up-development-plan.md](./plans/2.0/2.0-turbot-ai-follow-up-development-plan.md) - Turbot-AI 后续开发总计划 (v2.0)
- [2.0/2.1-agent-fields-plan.md](./plans/2.0/2.1-agent-fields-plan.md) - Agent 字段补充
- [2.0/2.2-system-prompt-plan.md](./plans/2.0/2.2-system-prompt-plan.md) - 系统提示词
- [2.0/2.3-tool-schema-plan.md](./plans/2.0/2.3-tool-schema-plan.md) - 工具 Schema
- [2.0/2.4-message-builder-plan.md](./plans/2.0/2.4-message-builder-plan.md) - 消息构建器
- [2.0/2.5-prompt-build-plan.md](./plans/2.0/2.5-prompt-build-plan.md) - Prompt 构建
- [2.0/2.6-stream-event-plan.md](./plans/2.0/2.6-stream-event-plan.md) - 流式事件类型
- [2.0/2.7-llm-stream-plan.md](./plans/2.0/2.7-llm-stream-plan.md) - LLM 流式调用
- [2.0/2.8-agent-loop-plan.md](./plans/2.0/2.8-agent-loop-plan.md) - Agent Loop
- [2.0/2.9-part-system-plan.md](./plans/2.0/2.9-part-system-plan.md) - Part 系统扩展
- [2.0/2.10-token-usage-plan.md](./plans/2.0/2.10-token-usage-plan.md) - Token 统计
- [2.0/2.11-doom-loop-plan.md](./plans/2.0/2.11-doom-loop-plan.md) - 死循环检测
- [2.0/2.12-retry-plan.md](./plans/2.0/2.12-retry-plan.md) - 错误重试
- [2.0/2.13-compaction-plan.md](./plans/2.0/2.13-compaction-plan.md) - 会话压缩
- [2.0/2.14-skill-discovery-plan.md](./plans/2.0/2.14-skill-discovery-plan.md) - Skill 发现系统
- [2.0/2.15-skill-tool-plan.md](./plans/2.0/2.15-skill-tool-plan.md) - Skill 工具
- [2.0/2.16-builtin-agent-plan.md](./plans/2.0/2.16-builtin-agent-plan.md) - 内置 Agent 完善
- [2.0/2.17-agent-loader-plan.md](./plans/2.0/2.17-agent-loader-plan.md) - Agent 动态加载
- [2.0/2.18-config-manager-implementation-plan.md](./plans/2.0/2.18-config-manager-implementation-plan.md) - ConfigManager 配置分级机制实现计划

**v2.5 核心流程串联验证**

- [2.5/2.5-core-flow-integration-plan.md](./plans/2.5/2.5-core-flow-integration-plan.md) - 核心流程串联验证总计划 (v2.5)
- [2.5/2.5.1-e2e-flow-verification.md](./plans/2.5/2.5.1-e2e-flow-verification.md) - 端到端流程验证
- [2.5/2.5.2-snapshot-tracking-system.md](./plans/2.5/2.5.2-snapshot-tracking-system.md) - 快照追踪系统
- [2.5/2.5.3-message-persistence-verification.md](./plans/2.5/2.5.3-message-persistence-verification.md) - 消息持久化验证
- [2.5/2.5.4-multi-agent-collaboration.md](./plans/2.5/2.5.4-multi-agent-collaboration.md) - 多 Agent 协作验证
- [2.5/2.5.5-error-recovery-verification.md](./plans/2.5/2.5.5-error-recovery-verification.md) - 错误恢复流程验证

**v2.6 质量加固**

- [2.6/2.6-quality-hardening-plan.md](./plans/2.6/2.6-quality-hardening-plan.md) - 质量加固总计划 (v2.6)
- [2.6/2.6.1-critical-bug-fixes.md](./plans/2.6/2.6.1-critical-bug-fixes.md) - Critical Bug 修复
- [2.6/2.6.2-role-tool-extension.md](./plans/2.6/2.6.2-role-tool-extension.md) - Role::Tool 扩展与工具消息语义修复
- [2.6/2.6.3-utils-consolidation.md](./plans/2.6/2.6.3-utils-consolidation.md) - 公共模块提升（utils 整合）
- [2.6/2.6.4-snapshot-performance.md](./plans/2.6/2.6.4-snapshot-performance.md) - SnapshotManager 性能优化
- [2.6/2.6.5-test-quality-improvement.md](./plans/2.6/2.6.5-test-quality-improvement.md) - 测试质量补强

**v2.7 二次加固**

- [2.7/2.7-post-review-hardening-plan.md](./plans/2.7/2.7-post-review-hardening-plan.md) - 二次加固总计划 (v2.7)
- [2.7/2.7.1-retry-integration-clarification.md](./plans/2.7/2.7.1-retry-integration-clarification.md) - RetryManager 集成确认与测试修正
- [2.7/2.7.2-error-handling-hardening.md](./plans/2.7/2.7.2-error-handling-hardening.md) - 错误处理强化
- [2.7/2.7.3-concurrency-safety-fix.md](./plans/2.7/2.7.3-concurrency-safety-fix.md) - 并发安全修复（agent\_ 数据竞争）
- [2.7/2.7.4-snapshot-memory-strategy.md](./plans/2.7/2.7.4-snapshot-memory-strategy.md) - Snapshot 内存策略优化
- [2.7/2.7.5-test-assertion-completion.md](./plans/2.7/2.7.5-test-assertion-completion.md) - 测试断言补全（2.6.5 遗留项）

**v2.8 架构差距分析**

- [2.8/2.8-opencode-gap-analysis-plan.md](./plans/2.8/2.8-opencode-gap-analysis-plan.md) - Turbot vs OpenCode 架构差距分析与补全计划

**v2.9 测试质量提升**

- [2.9/2.9-test-coverage-improvement-plan.md](./plans/2.9/2.9-test-coverage-improvement-plan.md) - 测试质量提升计划（目标：整体行覆盖率 ≥85%）

**v3.0 协议系统**

- [3.0/3.0-protocol-systems-plan.md](./plans/3.0/3.0-protocol-systems-plan.md) - Turbot-AI 协议系统开发计划 (v3.0)

### 代码审查

- [2026-03-07-libs-code-review.md](./reviews/2026-03-07-libs-code-review.md) - libs 目录全面代码审核报告
- [2026-03-08-config-refactor-review.md](./reviews/2026-03-08-config-refactor-review.md) - 配置系统重构代码审查
- [2026-03-10-v2.5-code-review.md](./reviews/2026-03-10-v2.5-code-review.md) - v2.5 代码审核报告（快照系统 + E2E 测试框架）
- [2026-03-11-v2.5-core-flow-review.md](./reviews/2026-03-11-v2.5-core-flow-review.md) - v2.5 核心流程代码审查
- [2026-03-11-v2.6-quality-hardening-review.md](./reviews/2026-03-11-v2.6-quality-hardening-review.md) - v2.6 质量加固代码审查
- [2026-03-13-unit-test-coverage-report.md](./reviews/2026-03-13-unit-test-coverage-report.md) - 单元测试覆盖率报告（整体 70.33%，7 failed）
- [2026-03-14-v3.0-completeness-review.md](./reviews/2026-03-14-v3.0-completeness-review.md) - v3.0 功能完成度评估报告（综合完成度 65%，SPEARM 70/100 上限锁定）
- [2026-03-14-v2.x-completeness-review.md](./reviews/2026-03-14-v2.x-completeness-review.md) - v2.x 功能完成度评估报告（综合完成度 78%，SPEARM 80/100）
- [2026-03-14-turbot-overall-review.md](./reviews/2026-03-14-turbot-overall-review.md) - Turbot 整体功能完成度总结报告（二次核实综合评估）
- [2026-03-15-turbot-opencode-alignment-review.md](./reviews/2026-03-15-turbot-opencode-alignment-review.md) - Turbot-AI 与 OpenCode 功能对齐审查（85% 对齐度，SPEARM 74/100）

### API 文档

（暂无）

### 其他文档

（暂无）
