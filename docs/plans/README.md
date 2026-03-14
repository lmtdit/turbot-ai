# 项目规划文档

本目录包含产品规划、产品愿景、发展路线图、重构计划、模版设计等文档。

## 文档索引

### v1.0 多 Agent 架构实现

| 文档                                                                                                                                 | 描述                      | 状态    |
| ------------------------------------------------------------------------------------------------------------------------------------ | ------------------------- | ------- |
| [1.0-turbot-ai-multi-agent-architecture-implementation-plan.md](./1.0/1.0-turbot-ai-multi-agent-architecture-implementation-plan.md) | 多 Agent 架构总体实现方案 | ✅ 完成 |
| [1.1-turbot-ai-foundation-infra-plan.md](./1.0/1.1-turbot-ai-foundation-infra-plan.md)                                               | 基础设施实现计划          | ✅ 完成 |
| [1.2-turbot-ai-storage-layer-plan.md](./1.0/1.2-turbot-ai-storage-layer-plan.md)                                                     | 存储层实现计划            | ✅ 完成 |
| [1.3-turbot-ai-network-layer-plan.md](./1.0/1.3-turbot-ai-network-layer-plan.md)                                                     | 网络层实现计划            | ✅ 完成 |
| [1.4-turbot-ai-core-data-models-plan.md](./1.0/1.4-turbot-ai-core-data-models-plan.md)                                               | 核心数据模型计划          | ✅ 完成 |
| [1.5-turbot-ai-provider-system-plan.md](./1.0/1.5-turbot-ai-provider-system-plan.md)                                                 | Provider 系统计划         | ✅ 完成 |
| [1.6-turbot-ai-permission-tool-system-plan.md](./1.0/1.6-turbot-ai-permission-tool-system-plan.md)                                   | 权限与工具系统计划        | ✅ 完成 |
| [1.7-turbot-ai-agent-session-system-plan.md](./1.0/1.7-turbot-ai-agent-session-system-plan.md)                                       | Agent 与会话系统计划      | ✅ 完成 |
| [1.8-turbot-ai-multiagent-app-plan.md](./1.0/1.8-turbot-ai-multiagent-app-plan.md)                                                   | 多 Agent 应用计划         | ✅ 完成 |

### v2.0 核心流程完善

**总览**: [2.0-turbot-ai-follow-up-development-plan.md](./2.0/2.0-turbot-ai-follow-up-development-plan.md)

| 文档                                                               | 描述            | 依赖          | 工时   |
| ------------------------------------------------------------------ | --------------- | ------------- | ------ |
| [2.1-agent-fields-plan.md](./2.0/2.1-agent-fields-plan.md)         | Agent 字段补充  | 无            | 0.5 天 |
| [2.2-system-prompt-plan.md](./2.0/2.2-system-prompt-plan.md)       | 系统提示词      | 2.1           | 1 天   |
| [2.3-tool-schema-plan.md](./2.0/2.3-tool-schema-plan.md)           | 工具 Schema     | 无            | 1 天   |
| [2.4-message-builder-plan.md](./2.0/2.4-message-builder-plan.md)   | 消息构建器      | 无            | 1 天   |
| [2.5-prompt-build-plan.md](./2.0/2.5-prompt-build-plan.md)         | Prompt 构建     | 2.2, 2.3, 2.4 | 1 天   |
| [2.6-stream-event-plan.md](./2.0/2.6-stream-event-plan.md)         | 流式事件类型    | 无            | 0.5 天 |
| [2.7-llm-stream-plan.md](./2.0/2.7-llm-stream-plan.md)             | LLM 流式调用    | 2.6           | 2 天   |
| [2.8-agent-loop-plan.md](./2.0/2.8-agent-loop-plan.md)             | Agent Loop      | 2.5, 2.7      | 2 天   |
| [2.9-part-system-plan.md](./2.0/2.9-part-system-plan.md)           | Part 系统扩展   | 无            | 1 天   |
| [2.10-token-usage-plan.md](./2.0/2.10-token-usage-plan.md)         | Token 统计      | 2.6           | 0.5 天 |
| [2.11-doom-loop-plan.md](./2.0/2.11-doom-loop-plan.md)             | 死循环检测      | 2.8           | 0.5 天 |
| [2.12-retry-plan.md](./2.0/2.12-retry-plan.md)                     | 错误重试        | 2.7           | 1 天   |
| [2.13-compaction-plan.md](./2.0/2.13-compaction-plan.md)           | 会话压缩        | 2.5, 2.10     | 1.5 天 |
| [2.14-skill-discovery-plan.md](./2.0/2.14-skill-discovery-plan.md) | Skill 发现      | 无            | 1 天   |
| [2.15-skill-tool-plan.md](./2.0/2.15-skill-tool-plan.md)           | Skill 工具      | 2.14, 2.3     | 0.5 天 |
| [2.16-builtin-agent-plan.md](./2.0/2.16-builtin-agent-plan.md)     | 内置 Agent 完善 | 2.1           | 1 天   |
| [2.17-agent-loader-plan.md](./2.0/2.17-agent-loader-plan.md)       | Agent 动态加载  | 2.1, 2.16     | 1.5 天 |

### v2.5 核心流程串联验证

**总览**: [2.5-core-flow-integration-plan.md](./2.5/2.5-core-flow-integration-plan.md)

| 文档                                                                         | 描述             | 依赖     | 工时   |
| ---------------------------------------------------------------------------- | ---------------- | -------- | ------ |
| [2.5-core-flow-integration-plan.md](./2.5/2.5-core-flow-integration-plan.md) | 核心流程串联验证 | 2.0 全部 | 5 天   |
| 2.5.1 端到端流程验证                                                         | E2E 测试         | 2.0 全部 | 1 天   |
| 2.5.2 快照追踪系统                                                           | 文件变更追踪     | 无       | 1.5 天 |
| 2.5.3 消息持久化验证                                                         | 数据库持久化     | 2.5.1    | 1 天   |
| 2.5.4 多 Agent 协作验证                                                      | Task 工具验证    | 2.5.1    | 1 天   |
| 2.5.5 错误恢复流程验证                                                       | 重试/恢复验证    | 2.5.1    | 0.5 天 |

### v3.0 协议系统开发

**总览**: [3.0-protocol-systems-plan.md](./3.0/3.0-protocol-systems-plan.md)

| 文档                       | 描述                | 依赖     | 工时   |
| -------------------------- | ------------------- | -------- | ------ |
| 3.1-mcp-core-plan.md       | MCP 核心框架        | 无       | 2 天   |
| 3.2-mcp-transport-plan.md  | MCP 传输层          | 3.1      | 1.5 天 |
| 3.3-mcp-oauth-plan.md      | MCP OAuth 认证      | 3.1      | 1 天   |
| 3.4-mcp-playwright-plan.md | MCP Playwright 集成 | 3.2      | 1 天   |
| 3.5-lsp-client-plan.md     | LSP Client 实现     | 无       | 2 天   |
| 3.6-lsp-server-plan.md     | LSP Server 管理     | 3.5      | 1.5 天 |
| 3.7-lsp-builtin-plan.md    | LSP 内置服务器      | 3.6      | 1 天   |
| 3.8-acp-agent-plan.md      | ACP Agent 实现      | 2.x 完成 | 2 天   |
| 3.9-acp-session-plan.md    | ACP Session 管理    | 3.8      | 1 天   |
| 3.10-acp-ide-plan.md       | ACP IDE 集成        | 3.8, 3.9 | 1 天   |

### v4.0 修复与加固

**总览**: [4.0-fix-and-hardening-plan.md](./4.0/4.0-fix-and-hardening-plan.md)

| ID   | 问题                                   | 优先级 | 影响版本    | 工时   |
| ---- | -------------------------------------- | ------ | ----------- | ------ |
| P0-1 | Session::get/list/delete 占位          | P0     | v2.x + v3.0 | 3 天   |
| P0-2 | 权限系统旁路（ask_permission 未注入）  | P0     | v2.x + v3.0 | 2 天   |
| P0-3 | v2.9 修复 7 个失败测试（CMakeLists）   | P0     | v2.x        | 0.5 天 |
| P0-4 | MCP 单元测试目录为空                   | P0     | v3.0        | 2 天   |
| P1-1 | BuildAgent/ExploreAgent/PlanAgent 占位 | P1     | v2.x + v3.0 | 3 天   |
| P1-2 | ACP cancel() 中断信号为 stub           | P1     | v3.0        | 1 天   |
| P1-3 | set_on_tool_result diff 内容为空       | P1     | v3.0        | 1 天   |
| P1-4 | generate_agent() LLM 接入              | P1     | v2.x        | 1.5 天 |
| P1-5 | v2.8 计划文档状态未同步                | P1     | 文档        | 0.5 天 |
| P1-6 | Provider 初始化与端到端 LLM 验证       | P1     | v2.x + v3.0 | 1.5 天 |
| P2-1 | multiedit/webfetch/codesearch 工具缺失 | P2     | v2.x        | 3 天   |
| P2-2 | LSP 语言服务器 3→8 种扩展              | P2     | v3.0        | 2 天   |
| P2-3 | SSE fallback 降级逻辑集成测试          | P2     | v3.0        | 1 天   |

**总工时估算**：约 22 天

## 当前进度

- **总体进度**: 75% (40/53 任务完成)
- **v1.0**: ✅ 100% (9/9)
- **v2.0**: ✅ 100% (17/17)
- **v2.5**: 📋 计划中 (0/5)
- **v3.0**: 📋 计划中 (0/10)
- **v4.0**: 📋 待启动 (0/13)
- **测试覆盖**: 680 test cases, 3705 assertions
- **代码规模**: 102 个库文件 + 5 个应用文件

## 核心差距分析

基于 OpenCode 项目架构分析，详细对比见 [2.0 后续开发计划](./2.0/2.0-turbot-ai-follow-up-development-plan.md)。

**v2.0 已完成**:

- ✅ Prompt 生成系统
- ✅ LLM 调用抽象层
- ✅ Agent Loop 执行引擎
- ✅ 消息 Part 流式处理
- ✅ Token 统计
- ✅ 死循环检测
- ✅ 错误重试
- ✅ 会话压缩
- ✅ Skill 发现与加载
- ✅ Agent 动态加载

**v2.5 待验证**:

- ⚠️ 端到端流程串联
- ❌ 快照追踪系统
- ⚠️ 多 Agent 协作

**v3.0 协议系统缺失**:

- ❌ MCP (Model Context Protocol) - 外部工具集成
- ❌ LSP (Language Server Protocol) - 代码智能
- ❌ ACP (Agent Client Protocol) - IDE 集成

详细实现计划见 [2.5 核心流程串联验证](./2.5/2.5-core-flow-integration-plan.md)。
