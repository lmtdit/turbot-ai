# 架构文档

本目录包含 Turbot-AI 项目的架构设计文档。

## 文档索引

| 文档                                               | 说明         |
| -------------------------------------------------- | ------------ |
| [directory-structure.md](./directory-structure.md) | 目录架构规范 |

## 架构概述

Turbot-AI 采用分层架构设计：

```
┌─────────────────────────────────────┐
│            应用层 (apps)            │
│    turbot-cli    turbot-server     │
└──────────────────┬──────────────────┘
                   │
┌──────────────────┴──────────────────┐
│           业务层 (libs/core)         │
│  message  provider  permission      │
│  tool     agent     session         │
└──────────────────┬──────────────────┘
                   │
┌──────────────────┴──────────────────┐
│         基础设施层 (libs)            │
│   storage    network    utils       │
└─────────────────────────────────────┘
```

## 核心模块

- **core**: 业务核心逻辑（消息、会话、Agent、工具、权限、Provider）
- **storage**: 数据持久化（数据库抽象、SQLite、迁移系统）
- **network**: 网络通信（HTTP、WebSocket、异步 IO）
- **utils**: 通用工具（加密、文件、JSON、字符串）

## 相关文档

- [实现计划](../plans/README.md)
- [技术决策](../decisions/README.md)
- [研究文档](../researchs/README.md)
