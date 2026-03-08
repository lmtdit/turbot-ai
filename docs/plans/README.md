# 项目规划文档

本目录包含产品规划、产品愿景、发展路线图、重构计划、模版设计等文档。

## 文档索引

### 总体规划

| 文档                                                                                                                             | 描述                      | 更新日期   |
| -------------------------------------------------------------------------------------------------------------------------------- | ------------------------- | ---------- |
| [1.0-turbot-ai-multi-agent-architecture-implementation-plan.md](./1.0-turbot-ai-multi-agent-architecture-implementation-plan.md) | 多 Agent 架构总体实现方案 | 2026-03-08 |
| [2.0-turbot-ai-follow-up-development-plan.md](./2.0-turbot-ai-follow-up-development-plan.md)                                     | 后续开发总览              | 2026-03-08 |
| [3.0-protocol-systems-plan.md](./3.0-protocol-systems-plan.md)                                                                   | 协议系统开发计划          | 2026-03-08 |

### 2.x 分拆计划（独立可测试）

| 文档                                                           | 描述           | 依赖          | 工时   |
| -------------------------------------------------------------- | -------------- | ------------- | ------ |
| [2.1-agent-fields-plan.md](./2.1-agent-fields-plan.md)         | Agent 字段补充 | 无            | 0.5 天 |
| [2.2-system-prompt-plan.md](./2.2-system-prompt-plan.md)       | 系统提示词     | 2.1           | 1 天   |
| [2.3-tool-schema-plan.md](./2.3-tool-schema-plan.md)           | 工具 Schema    | 无            | 1 天   |
| [2.4-message-builder-plan.md](./2.4-message-builder-plan.md)   | 消息构建器     | 无            | 1 天   |
| [2.5-prompt-build-plan.md](./2.5-prompt-build-plan.md)         | Prompt 构建    | 2.2, 2.3, 2.4 | 1 天   |
| [2.6-stream-event-plan.md](./2.6-stream-event-plan.md)         | 流式事件类型   | 无            | 0.5 天 |
| [2.7-llm-stream-plan.md](./2.7-llm-stream-plan.md)             | LLM 流式调用   | 2.6           | 2 天   |
| [2.8-agent-loop-plan.md](./2.8-agent-loop-plan.md)             | Agent Loop     | 2.5, 2.7      | 2 天   |
| [2.9-part-system-plan.md](./2.9-part-system-plan.md)           | Part 系统扩展  | 无            | 1 天   |
| [2.10-token-usage-plan.md](./2.10-token-usage-plan.md)         | Token 统计     | 2.6           | 0.5 天 |
| [2.11-doom-loop-plan.md](./2.11-doom-loop-plan.md)             | 死循环检测     | 2.8           | 0.5 天 |
| [2.12-retry-plan.md](./2.12-retry-plan.md)                     | 错误重试       | 2.7           | 1 天   |
| [2.13-compaction-plan.md](./2.13-compaction-plan.md)           | 会话压缩       | 2.5, 2.10     | 1.5 天 |
| [2.14-skill-discovery-plan.md](./2.14-skill-discovery-plan.md) | Skill 发现     | 无            | 1 天   |
| [2.15-skill-tool-plan.md](./2.15-skill-tool-plan.md)           | Skill 工具     | 2.14, 2.3     | 0.5 天 |

### 3.x 分拆计划（协议系统）

| 文档 | 描述 | 依赖 | 工时 |
| ---- | ---- | ---- | ---- |
| 3.1-mcp-core-plan.md | MCP 核心框架 | 无 | 2 天 |
| 3.2-mcp-transport-plan.md | MCP 传输层 | 3.1 | 1.5 天 |
| 3.3-mcp-oauth-plan.md | MCP OAuth 认证 | 3.1 | 1 天 |
| 3.4-mcp-playwright-plan.md | MCP Playwright 集成 | 3.2 | 1 天 |
| 3.5-lsp-client-plan.md | LSP Client 实现 | 无 | 2 天 |
| 3.6-lsp-server-plan.md | LSP Server 管理 | 3.5 | 1.5 天 |
| 3.7-lsp-builtin-plan.md | LSP 内置服务器 | 3.6 | 1 天 |
| 3.8-acp-agent-plan.md | ACP Agent 实现 | 2.x 完成 | 2 天 |
| 3.9-acp-session-plan.md | ACP Session 管理 | 3.8 | 1 天 |
| 3.10-acp-ide-plan.md | ACP IDE 集成 | 3.8, 3.9 | 1 天 |

### 归档文档

已完成的阶段性计划文档，详见 [archive/](./archive/) 目录：

| 文档                            | 描述                 | 状态      |
| ------------------------------- | -------------------- | --------- |
| 1.1-foundation-infra-plan       | 基础设施实现计划     | ✅ 已完成 |
| 1.2-storage-layer-plan          | 存储层实现计划       | ✅ 已完成 |
| 1.3-network-layer-plan          | 网络层实现计划       | ✅ 已完成 |
| 1.4-core-data-models-plan       | 核心数据模型计划     | ✅ 已完成 |
| 1.5-provider-system-plan        | Provider 系统计划    | ✅ 已完成 |
| 1.6-permission-tool-system-plan | 权限与工具系统计划   | ✅ 已完成 |
| 1.7-agent-session-system-plan   | Agent 与会话系统计划 | ✅ 已完成 |
| 1.8-multiagent-app-plan         | 多 Agent 应用计划    | ✅ 已完成 |

## 当前进度

- **总体进度**: 70% (35/50 任务完成)
- **测试覆盖**: 464 test cases, 2124 assertions
- **代码规模**: 91 个库文件 + 5 个应用文件

## 核心差距分析

基于 OpenCode 项目架构分析，详细对比见 [2.0 后续开发计划](./2.0-turbot-ai-follow-up-development-plan.md#12-核心差距分析)。

**关键缺失**:

- ❌ Prompt 生成系统
- ❌ LLM 调用抽象层
- ⚠️ Agent Loop 执行引擎（框架存在，核心缺失）
- ⚠️ 消息 Part 流式处理（部分实现）
- ❌ Skill 系统（动态加载技能）

详细实现计划见 [2.0-turbot-ai-follow-up-development-plan.md](./2.0-turbot-ai-follow-up-development-plan.md)。
