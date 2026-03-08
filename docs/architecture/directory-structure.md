# Turbot-AI 目录架构规范

## 1. 项目根目录结构

```
turbot-ai/
├── apps/                    # 应用程序
├── libs/                    # 核心库
├── tests/                   # 测试代码
├── third_party/             # 第三方依赖
├── docs/                    # 文档
├── build/                   # 构建配置
├── cmake/                   # CMake模块
├── CMakeLists.txt           # 根CMake配置
├── CMakePresets.json        # CMake预设
├── conanfile.py             # Conan依赖配置
├── .clang-format            # 代码格式配置
└── .clang-tidy              # 静态分析配置
```

## 2. 库目录结构 (libs/)

### 2.1 总体结构

```
libs/
├── core/                    # 核心库 - 业务核心逻辑
├── storage/                 # 存储层 - 数据持久化
├── network/                 # 网络层 - HTTP/WebSocket通信
└── utils/                   # 工具库 - 通用工具函数
```

### 2.2 单个库的标准结构

每个库遵循以下标准结构：

```
libs/<library>/
├── CMakeLists.txt           # 库的CMake配置
├── include/
│   └── turbot/<library>/    # 公共头文件
│       ├── export.hpp       # 导出宏定义
│       └── ...              # 其他头文件
└── src/                     # 源文件实现
    └── ...
```

**命名规范**：

- 命名空间：`turbot::<library>`
- 头文件路径：`#include <turbot/<library>/xxx.hpp>`
- CMake 目标：`turbot_<library>`

## 3. 库详细规范

### 3.1 core - 核心库

```
libs/core/
├── CMakeLists.txt
├── include/turbot/core/
│   ├── common/              # 通用模块 ✓ 已实现
│   │   ├── export.hpp       # 导出宏
│   │   ├── logger.hpp       # 日志系统
│   │   └── version.hpp      # 版本信息
│   ├── config/              # 配置模块 ✓ 已实现
│   │   ├── config.hpp       # 配置管理
│   │   └── config_manager.hpp # 配置分级管理
│   ├── event/               # 事件模块 ✓ 已实现
│   │   └── event_bus.hpp    # 事件总线
│   ├── message/             # 消息系统 ✓ 已实现
│   │   ├── message.hpp      # Message模型
│   │   ├── part.hpp         # Part系统
│   │   └── token_usage.hpp  # Token统计
│   ├── provider/            # AI提供商系统 ✓ 已实现
│   │   ├── provider.hpp     # Provider接口
│   │   ├── provider_manager.hpp
│   │   └── impl/            # Provider实现
│   │       ├── openai_provider.hpp
│   │       ├── bailian_provider.hpp
│   │       ├── zhipu_provider.hpp
│   │       ├── kimi_provider.hpp
│   │       └── iflow_provider.hpp
│   ├── permission/          # 权限系统 ✓ 已实现
│   │   └── permission.hpp   # 权限规则、PermissionSystem
│   ├── tool/                # 工具系统 ✓ 已实现
│   │   ├── tool.hpp         # Tool 抽象基类、ToolContext、ToolResult
│   │   ├── tool_registry.hpp # ToolRegistry 单例
│   │   └── builtin/         # 内置工具
│   │       ├── read_file_tool.hpp
│   │       ├── write_file_tool.hpp
│   │       ├── bash_tool.hpp
│   │       └── task_tool.hpp    # Task工具（多Agent协作）
│   ├── agent/               # Agent系统 ✓ 已实现
│   │   ├── agent.hpp        # Agent抽象基类、AgentRegistry
│   │   └── builtin/         # 内置代理
│   │       ├── build_agent.hpp
│   │       ├── plan_agent.hpp
│   │       └── explore_agent.hpp
│   ├── session/             # 会话系统 ✓ 已实现
│   │   ├── session.hpp      # Session、SessionInfo、SessionState
│   │   ├── session_state_machine.hpp
│   │   └── session_loop.hpp # SessionLoop（会话主循环）
│   │
│   │ === v2.0 核心流程完善 ===
│   │
│   ├── prompt/              # Prompt生成系统 ✓ v2.0
│   │   ├── system_prompt.hpp    # 系统提示词
│   │   ├── prompt_builder.hpp   # Prompt构建器
│   │   └── message_builder.hpp  # 消息构建器
│   ├── llm/                 # LLM调用层 ✓ v2.0
│   │   ├── stream_event.hpp     # 流式事件类型
│   │   ├── llm.hpp              # LLM流式调用
│   │   ├── provider_adapter.hpp # Provider适配器
│   │   └── tool_schema.hpp      # 工具Schema定义
│   ├── session/             # 会话系统扩展 ✓ v2.0
│   │   ├── session_loop.hpp     # Agent主循环（含死循环检测）
│   │   ├── usage_tracker.hpp    # Token使用追踪
│   │   ├── retry.hpp            # 错误重试 📋
│   │   └── compaction.hpp       # 会话压缩 📋
│   ├── skill/               # Skill系统 ✓ v2.0
│   │   ├── skill.hpp            # Skill管理器
│   │   └── skill_tool.hpp       # Skill工具
│   │
│   │ === v3.0 协议系统 ===
│   │
│   ├── mcp/                 # MCP协议 📋 v3.0
│   │   ├── mcp.hpp              # MCP核心
│   │   ├── client.hpp           # MCP客户端
│   │   ├── resource.hpp         # 资源定义
│   │   ├── transport/           # 传输层
│   │   │   ├── transport.hpp
│   │   │   ├── stdio_transport.hpp
│   │   │   ├── sse_transport.hpp
│   │   │   └── http_transport.hpp
│   │   └── auth/                # 认证
│   │       ├── oauth_provider.hpp
│   │       └── auth_manager.hpp
│   ├── lsp/                 # LSP协议 📋 v3.0
│   │   ├── lsp.hpp              # LSP核心
│   │   ├── client.hpp           # LSP客户端
│   │   ├── server.hpp           # LSP服务器管理
│   │   └── builtin/             # 内置LSP服务器
│   │       ├── typescript_server.hpp
│   │       ├── python_server.hpp
│   │       └── clangd_server.hpp
│   └── acp/                 # ACP协议 📋 v3.0
│       ├── acp.hpp              # ACP核心
│       ├── agent.hpp            # ACP Agent
│       ├── session.hpp          # ACP会话
│       └── server.hpp           # ACP服务器
│
└── src/
    ├── common/
    │   ├── logger.cpp
    │   └── version.cpp
    ├── config/
    │   ├── config.cpp
    │   └── config_manager.cpp
    ├── event/
    │   └── event_bus.cpp
    ├── message/
    │   ├── message.cpp
    │   └── part.cpp
    ├── provider/
    │   ├── provider.cpp
    │   └── impl/
    │       ├── openai_provider.cpp
    │       ├── bailian_provider.cpp
    │       ├── zhipu_provider.cpp
    │       ├── kimi_provider.cpp
    │       └── iflow_provider.cpp
    ├── permission/
    │   └── permission.cpp
    ├── tool/
    │   ├── tool.cpp
    │   ├── tool_registry.cpp
    │   └── builtin/
    │       ├── read_file_tool.cpp
    │       ├── write_file_tool.cpp
    │       ├── bash_tool.cpp
    │       └── task_tool.cpp
    ├── agent/
    │   ├── agent.cpp
    │   └── builtin/
    │       ├── build_agent.cpp
    │       ├── plan_agent.cpp
    │       └── explore_agent.cpp
    ├── session/
    │   ├── session.cpp
    │   ├── session_state_machine.cpp
    │   └── session_loop.cpp
    │
    │ === v2.0 实现 ===
    │
    ├── prompt/
    │   ├── system_prompt.cpp
    │   ├── prompt_builder.cpp
    │   └── message_builder.cpp
    ├── llm/
    │   ├── stream_event.cpp
    │   ├── llm_stream.cpp
    │   └── tool_schema.cpp
    ├── session/              # 扩展
    │   ├── agent_loop.cpp
    │   ├── retry.cpp
    │   ├── compaction.cpp
    │   └── doom_loop.cpp
    ├── skill/
    │   ├── skill.cpp
    │   ├── discovery.cpp
    │   └── skill_tool.cpp
    │
    │ === v3.0 实现 ===
    │
    ├── mcp/
    │   ├── mcp.cpp
    │   ├── client.cpp
    │   ├── transport/
    │   │   ├── stdio_transport.cpp
    │   │   ├── sse_transport.cpp
    │   │   └── http_transport.cpp
    │   └── auth/
    │       ├── oauth_provider.cpp
    │       └── auth_manager.cpp
    ├── lsp/
    │   ├── lsp.cpp
    │   ├── client.cpp
    │   ├── server.cpp
    │   └── builtin/
    │       ├── typescript_server.cpp
    │       ├── python_server.cpp
    │       └── clangd_server.cpp
    └── acp/
        ├── acp.cpp
        ├── agent.cpp
        ├── session.cpp
        └── server.cpp
```

### 3.2 storage - 存储层

```
libs/storage/
├── CMakeLists.txt
├── include/turbot/storage/
│   ├── export.hpp           ✓ 已实现
│   ├── database.hpp         # Database接口 ✓ 已实现
│   ├── sqlite_database.hpp  # SQLite实现 ✓ 已实现
│   ├── transaction.hpp      # 事务接口 ✓ 已实现
│   └── migration.hpp        # 迁移系统 ✓ 已实现
└── src/
    ├── database.cpp         ✓ 已实现
    ├── sqlite_database.cpp  ✓ 已实现
    ├── transaction.cpp      ✓ 已实现
    └── migration.cpp        ✓ 已实现
```

### 3.3 network - 网络层

```
libs/network/
├── CMakeLists.txt
├── include/turbot/network/
│   ├── export.hpp           ✓ 已实现
│   ├── http_client.hpp      # HTTP客户端 ✓ 已实现
│   └── url.hpp              # URL解析 ✓ 已实现
└── src/
    ├── http_client.cpp      ✓ 已实现
    └── url.cpp              ✓ 已实现
```

**计划中扩展**：

- `websocket/` - WebSocket 模块
- `async/` - 异步 IO
- `http_request.hpp` - HTTP 请求模型
- `http_response.hpp` - HTTP 响应模型
- `http_headers.hpp` - HTTP 头部

### 3.4 utils - 工具库

```
libs/utils/
├── CMakeLists.txt
├── include/turbot/utils/
│   ├── export.hpp           ✓ 已实现
│   ├── crypto_utils.hpp     # 加密工具 ✓ 已实现
│   ├── file_utils.hpp       # 文件工具 ✓ 已实现
│   ├── json_utils.hpp       # JSON工具 ✓ 已实现
│   └── string_utils.hpp     # 字符串工具 ✓ 已实现
└── src/
    ├── crypto_utils.cpp     ✓ 已实现
    ├── file_utils.cpp       ✓ 已实现
    ├── json_utils.cpp       ✓ 已实现
    └── string_utils.cpp     ✓ 已实现
```

## 4. 应用目录结构 (apps/)

```
apps/
├── turbot-cli/              # 命令行应用 ✓ 已实现
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp
│       └── commands.cpp
└── turbot-server/           # 服务器应用 ✓ 已实现
    ├── CMakeLists.txt
    └── src/
        ├── main.cpp
        ├── server.cpp
        └── server.hpp
```

## 5. 测试目录结构 (tests/)

```
tests/
├── CMakeLists.txt
├── unit/                    # 单元测试
│   │
│   │ === v1.0 已实现 ===
│   │
│   ├── config_test.cpp      ✓ config模块测试
│   ├── event_bus_test.cpp   ✓ event_bus模块测试
│   ├── logger_test.cpp      ✓ logger模块测试
│   ├── version_test.cpp     ✓ version模块测试
│   ├── message_test.cpp     ✓ message模块测试
│   ├── provider_test.cpp    ✓ provider模块测试
│   ├── crypto_utils_test.cpp ✓ crypto_utils模块测试
│   ├── json_utils_test.cpp  ✓ json_utils模块测试
│   ├── string_utils_test.cpp ✓ string_utils模块测试
│   ├── file_utils_test.cpp  ✓ file_utils模块测试
│   ├── url_test.cpp         ✓ url模块测试
│   ├── http_client_test.cpp ✓ http_client模块测试
│   ├── storage_test.cpp     ✓ storage模块测试
│   ├── permission_test.cpp  ✓ permission模块测试
│   ├── tool_test.cpp        ✓ tool模块测试
│   ├── agent_test.cpp       ✓ agent模块测试
│   ├── session_test.cpp     ✓ session模块测试
│   ├── task_tool_test.cpp   ✓ task_tool模块测试 (1.8新增)
│   └── session_loop_test.cpp ✓ session_loop模块测试 (1.8新增)
│   │
│   │ === v2.0 已实现 ===
│   │
│   ├── system_prompt_test.cpp  ✓ system_prompt模块测试
│   ├── prompt_builder_test.cpp  ✓ prompt_builder模块测试
│   ├── message_builder_test.cpp ✓ message_builder模块测试
│   ├── tool_schema_test.cpp     ✓ tool_schema模块测试
│   ├── stream_event_test.cpp    ✓ stream_event模块测试
│   ├── llm_test.cpp             ✓ llm模块测试
│   ├── session_loop_test.cpp    ✓ session_loop模块测试
│   ├── skill_test.cpp           ✓ skill模块测试
│   ├── task_tool_test.cpp       ✓ task_tool模块测试
│   │
│   │ === v2.0 待实现 ===
│   │
│   ├── retry_test.cpp           📋 v2.0
│   ├── compaction_test.cpp      📋 v2.0
│   │
│   │ === v3.0 计划中 ===
│   │
│   ├── mcp/                 📋 v3.0
│   │   ├── mcp_test.cpp
│   │   ├── client_test.cpp
│   │   ├── transport_test.cpp
│   │   └── auth_test.cpp
│   ├── lsp/                 📋 v3.0
│   │   ├── lsp_test.cpp
│   │   ├── client_test.cpp
│   │   └── server_test.cpp
│   └── acp/                 📋 v3.0
│       ├── acp_test.cpp
│       ├── agent_test.cpp
│       └── session_test.cpp
│
├── integration/             # 集成测试
│   ├── agent_loop_integration_test.cpp  📋 v2.0
│   ├── prompt_integration_test.cpp      📋 v2.0
│   ├── mcp_playwright_test.cpp          📋 v3.0
│   └── acp_ide_test.cpp                 📋 v3.0
│
└── test_integration.cpp     # 测试入口
```

**测试架构规范**：

- 所有单元测试集中存放在 `tests/unit/` 目录
- 不在各库目录下创建 tests 子目录
- 测试文件命名：`<模块名>_test.cpp`
- 测试标签：`[<library>][<module>]`，如 `[core][event_bus]`

## 6. 文档目录结构 (docs/)

```
docs/
├── README.md                # 文档索引
├── architecture/            # 架构文档
│   └── directory-structure.md
├── plans/                   # 计划文档
│   ├── README.md
│   ├── 1.0-xxx.md
│   └── ...
├── decisions/               # 技术决策记录
│   └── README.md
└── researchs/               # 研究文档
    └── ...
```

## 7. 模块解耦设计原则

### 7.1 核心原则

每个 Plan 阶段实现的模块必须满足以下条件：

1. **独立编译**：模块可独立编译，不依赖未实现的模块
2. **独立测试**：模块有完整的单元测试，测试覆盖率 ≥ 85%
3. **接口隔离**：模块间通过接口交互，不直接依赖实现
4. **Mock 友好**：支持依赖注入，便于测试时 Mock

### 7.2 Plan 与模块映射

#### v1.0 已完成

```
Plan 1.1 - 基础架构 ✅ 完成
├── libs/utils/           # 独立模块，无外部依赖
│   ├── crypto_utils      # 加密工具
│   ├── file_utils        # 文件工具
│   ├── json_utils        # JSON工具
│   └── string_utils      # 字符串工具
├── libs/core/            # 核心基础
│   ├── common/           # 通用模块
│   │   ├── export        # 导出宏
│   │   ├── logger        # 日志系统
│   │   └── version       # 版本信息
│   ├── config/           # 配置管理
│   └── event/            # 事件总线
└── tests/unit/           # 配套单元测试

Plan 1.2 - 存储层 ✅ 完成
├── libs/storage/         # 独立存储模块
│   ├── database.hpp      # 数据库抽象接口
│   ├── sqlite_database.hpp # SQLite实现
│   ├── transaction.hpp   # 事务接口
│   └── migration.hpp     # 迁移系统
└── tests/unit/

Plan 1.3 - 网络层 ✅ 完成
├── libs/network/
│   ├── http_client.hpp   # HTTP客户端
│   └── url.hpp           # URL解析
└── tests/unit/

Plan 1.4 - 核心数据模型 ✅ 完成
├── libs/core/message/    # 消息系统（独立子模块）
│   ├── message.hpp
│   ├── part.hpp
│   └── token_usage.hpp
└── tests/unit/

Plan 1.5 - Provider系统 ✅ 完成
├── libs/core/provider/   # AI提供商（依赖 network）
│   ├── provider.hpp      # 抽象接口
│   ├── provider_manager.hpp
│   └── impl/             # Provider实现
└── tests/unit/

Plan 1.6 - 权限和工具系统 ✅ 完成
├── libs/core/permission/ # 权限系统（独立）
├── libs/core/tool/       # 工具系统（独立）
└── tests/unit/

Plan 1.7 - Agent和会话系统 ✅ 完成
├── libs/core/agent/      # Agent系统
├── libs/core/session/    # 会话系统
└── tests/unit/

Plan 1.8 - 多Agent协作 ✅ 完成
├── libs/core/session/    # 扩展会话循环
├── apps/turbot-cli/      # CLI应用
└── apps/turbot-server/   # Server应用
```

#### v2.0 核心流程完善 (✅ 大部分完成)

```
Plan 2.1 - Agent字段补充 ✅ 完成
├── libs/core/include/turbot/core/agent/agent.hpp
│   ├── 新增字段: prompt, temperature, top_p, steps, color, variant
│   ├── 新增 ModelRef 结构体 (model_id + provider_id)
│   ├── 新增 AgentRegistry 方法: default_agent(), list_visible(), list_primary()
│   └── 新增 agent_loader/agent_generator 命名空间
└── tests/unit/agent_test.cpp

Plan 2.2 - 系统提示词 ✅ 完成
├── libs/core/include/turbot/core/llm/system_prompt.hpp
└── tests/unit/system_prompt_test.cpp

Plan 2.3 - 工具Schema ✅ 完成
├── libs/core/include/turbot/core/llm/tool_schema.hpp
└── tests/unit/tool_schema_test.cpp

Plan 2.4 - 消息构建器 ✅ 完成
├── libs/core/include/turbot/core/llm/message_builder.hpp
└── tests/unit/message_builder_test.cpp

Plan 2.5 - Prompt构建 ✅ 完成
├── libs/core/include/turbot/core/llm/prompt_builder.hpp
└── tests/unit/prompt_builder_test.cpp

Plan 2.6 - 流式事件类型 ✅ 完成
├── libs/core/include/turbot/core/llm/stream_event.hpp
└── tests/unit/stream_event_test.cpp

Plan 2.7 - LLM流式调用 ✅ 完成
├── libs/core/include/turbot/core/llm/llm.hpp
├── libs/core/include/turbot/core/llm/provider_adapter.hpp
└── tests/unit/llm_test.cpp

Plan 2.8 - Agent Loop ✅ 完成
├── libs/core/include/turbot/core/session/session_loop.hpp
│   ├── SessionLoop 主循环
│   ├── StepInfo 步骤信息
│   └── Doom Loop 检测（集成）
└── tests/unit/session_loop_test.cpp

Plan 2.9 - Part系统扩展 ✅ 完成
├── libs/core/include/turbot/core/message/part.hpp
│   ├── ImagePart 图片附件
│   ├── ErrorPart 错误信息
│   └── SourcePart 源码引用
└── tests/unit/message_test.cpp

Plan 2.10 - Token统计 ✅ 完成
├── libs/core/include/turbot/core/session/usage_tracker.hpp
└── tests/unit/usage_tracker_test.cpp

Plan 2.11 - 死循环检测 ✅ 完成（集成在 2.8）
├── 集成在 SessionLoop 中
│   └── doom_loop_threshold 配置项
└── 无单独测试文件

Plan 2.12 - 错误重试 📋 待实现
├── libs/core/session/retry.hpp
└── tests/unit/retry_test.cpp

Plan 2.13 - 会话压缩 📋 待实现
├── libs/core/session/compaction.hpp
└── tests/unit/compaction_test.cpp

Plan 2.14 - Skill发现 ✅ 完成
├── libs/core/include/turbot/core/skill/skill.hpp
└── tests/unit/skill_test.cpp

Plan 2.15 - Skill工具 ✅ 完成
├── libs/core/include/turbot/core/tool/skill_tool.hpp
└── tests/unit/skill_tool_test.cpp

Plan 2.16 - 内置Agent完善 ✅ 完成
├── libs/core/src/agent/builtin/
│   ├── build_agent.cpp
│   ├── plan_agent.cpp
│   └── explore_agent.cpp
└── tests/unit/agent_test.cpp

Plan 2.17 - Agent动态加载 ✅ 完成
├── libs/core/src/agent/agent_loader.cpp
│   ├── agent_loader::initialize_builtin_agents()
│   ├── agent_loader::load_from_config()
│   ├── agent_loader::reload()
│   └── agent_generator::generate()
└── tests/unit/agent_test.cpp
```

#### v3.0 协议系统开发 (📋 计划中)

```
Plan 3.1 - MCP核心框架 📋
├── libs/core/mcp/
│   ├── mcp.hpp             # MCP核心
│   ├── client.hpp          # MCP客户端
│   └── resource.hpp        # 资源定义
└── tests/unit/mcp/

Plan 3.2 - MCP传输层 📋
├── libs/core/mcp/transport/
│   ├── stdio_transport.hpp
│   ├── sse_transport.hpp
│   └── http_transport.hpp
└── tests/unit/mcp/

Plan 3.3 - MCP OAuth认证 📋
├── libs/core/mcp/auth/
│   ├── oauth_provider.hpp
│   └── auth_manager.hpp
└── tests/unit/mcp/

Plan 3.4 - MCP Playwright集成 📋
├── config/mcp.json         # Playwright MCP配置
└── tests/integration/mcp_playwright_test.cpp

Plan 3.5 - LSP Client 📋
├── libs/core/lsp/
│   ├── lsp.hpp             # LSP核心
│   └── client.hpp          # LSP客户端
└── tests/unit/lsp/

Plan 3.6 - LSP Server管理 📋
├── libs/core/lsp/
│   └── server.hpp          # LSP服务器管理
└── tests/unit/lsp/

Plan 3.7 - LSP内置服务器 📋
├── libs/core/lsp/builtin/
│   ├── typescript_server.hpp
│   ├── python_server.hpp
│   └── clangd_server.hpp
└── tests/integration/

Plan 3.8 - ACP Agent 📋
├── libs/core/acp/
│   ├── acp.hpp             # ACP核心
│   └── agent.hpp           # ACP Agent
└── tests/unit/acp/

Plan 3.9 - ACP Session 📋
├── libs/core/acp/
│   └── session.hpp         # ACP会话管理
└── tests/unit/acp/

Plan 3.10 - ACP IDE集成 📋
├── libs/core/acp/
│   └── server.hpp          # ACP服务器
└── tests/e2e/acp_ide_test.cpp
```

### 7.3 模块依赖隔离策略

**接口抽象层**：

```cpp
// 模块A依赖模块B时，通过接口隔离
// libs/core/provider/provider.hpp

namespace turbot::core::provider {

// 定义抽象接口
class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    virtual Task<HttpResponse> request(const HttpRequest& req) = 0;
};

class Provider {
public:
    // 通过接口注入依赖
    explicit Provider(std::shared_ptr<IHttpClient> http_client)
        : http_client_(std::move(http_client)) {}

private:
    std::shared_ptr<IHttpClient> http_client_;
};

} // namespace turbot::core::provider
```

**Mock 测试**：

```cpp
// tests/unit/provider_test.cpp

class MockHttpClient : public IHttpClient {
public:
    MOCK_METHOD(Task<HttpResponse>, request, (const HttpRequest&), (override));
};

TEST_CASE("Provider::chat", "[core][provider]") {
    auto mock_http = std::make_shared<MockHttpClient>();
    Provider provider(mock_http);

    // 使用 Mock 测试，不依赖真实网络
    // ...
}
```

### 7.4 模块验收标准

每个 Plan 完成时必须满足：

| 验收项   | 标准               |
| -------- | ------------------ |
| 编译     | 模块独立编译通过   |
| 单元测试 | 测试覆盖率 ≥ 85%   |
| 测试通过 | 100% 测试通过      |
| 静态分析 | clang-tidy 无警告  |
| 内存检查 | ASan/UBSan 无问题  |
| 文档更新 | 目录架构文档已更新 |

## 8. 依赖关系

### 8.1 库依赖关系

```
                    ┌─────────────┐
                    │    apps     │
                    └──────┬──────┘
                           │
        ┌──────────────────┼──────────────────┐
        │                  │                  │
        ▼                  ▼                  ▼
┌───────────────┐  ┌───────────────┐  ┌───────────────┐
│     core      │  │   storage     │  │   network     │
│ (业务核心)    │  │ (存储层)      │  │ (网络层)      │
└───────┬───────┘  └───────┬───────┘  └───────┬───────┘
        │                  │                  │
        └──────────────────┼──────────────────┘
                           │
                           ▼
                    ┌───────────────┐
                    │    utils      │
                    │ (工具库)      │
                    └───────────────┘
```

**依赖规则**：

- `utils` 是基础层，不依赖其他库
- `core`、`storage`、`network` 可依赖 `utils`
- `apps` 可依赖所有库
- 库之间尽量避免横向依赖

### 8.2 Core 内部模块依赖

```
┌─────────────────────────────────────────────────────────────────┐
│                         core 模块依赖                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────┐     ┌─────────┐     ┌─────────┐                   │
│  │ common  │     │ config  │     │  event  │                   │
│  │ (基础)  │     │ (配置)  │     │ (事件)  │                   │
│  └────┬────┘     └────┬────┘     └────┬────┘                   │
│       │               │               │                         │
│       └───────────────┼───────────────┘                         │
│                       │                                          │
│                       ▼                                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    message (消息)                        │    │
│  └─────────────────────────────────────────────────────────┘    │
│                       │                                          │
│       ┌───────────────┼───────────────┐                         │
│       │               │               │                         │
│       ▼               ▼               ▼                         │
│  ┌─────────┐    ┌──────────┐    ┌──────────┐                   │
│  │provider │    │permission│    │   tool   │                   │
│  │(AI提供) │    │ (权限)   │    │ (工具)   │                   │
│  └────┬────┘    └────┬─────┘    └────┬─────┘                   │
│       │              │               │                          │
│       └──────────────┼───────────────┘                          │
│                      │                                          │
│                      ▼                                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    agent (代理)                          │    │
│  └─────────────────────────────────────────────────────────┘    │
│                      │                                          │
│                      ▼                                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                   session (会话)                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ════════════════ v2.0 新增依赖 ════════════════                │
│                                                                  │
│                      ▼                                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │              prompt (提示词生成) 📋                      │    │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐      │    │
│  │  │system_prompt│  │  message_   │  │   prompt_   │      │    │
│  │  │             │  │  builder    │  │   builder   │      │    │
│  │  └─────────────┘  └─────────────┘  └─────────────┘      │    │
│  └─────────────────────────────────────────────────────────┘    │
│                      │                                          │
│                      ▼                                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │               llm (LLM调用层) 📋                         │    │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐      │    │
│  │  │stream_event │  │ llm_stream  │  │ tool_schema │      │    │
│  │  └─────────────┘  └─────────────┘  └─────────────┘      │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │              skill (技能系统) 📋                         │    │
│  │  ┌─────────────┐  ┌─────────────┐                       │    │
│  │  │    skill    │  │  discovery  │                       │    │
│  │  └─────────────┘  └─────────────┘                       │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ════════════════ v3.0 新增依赖 ════════════════                │
│                                                                  │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐                   │
│  │    mcp    │  │    lsp    │  │    acp    │  📋              │
│  │(工具协议) │  │(语言协议) │  │(IDE协议)  │                   │
│  └─────┬─────┘  └─────┬─────┘  └─────┬─────┘                   │
│        │              │              │                          │
│        └──────────────┼──────────────┘                          │
│                       │                                          │
│                       ▼                                          │
│              ┌─────────────┐                                     │
│              │   session   │ (扩展)                              │
│              └─────────────┘                                     │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## 9. CMake 目标命名

| 库      | CMake 目标       | 命名空间          |
| ------- | ---------------- | ----------------- |
| core    | `turbot_core`    | `turbot::core`    |
| storage | `turbot_storage` | `turbot::storage` |
| network | `turbot_network` | `turbot::network` |
| utils   | `turbot_utils`   | `turbot::utils`   |

## 10. 文件命名规范

| 类型     | 规范              | 示例                 |
| -------- | ----------------- | -------------------- |
| 头文件   | 小写下划线        | `event_bus.hpp`      |
| 源文件   | 小写下划线        | `event_bus.cpp`      |
| 测试文件 | `<模块>_test.cpp` | `event_bus_test.cpp` |
| CMake    | `CMakeLists.txt`  | -                    |

## 11. 实现状态

### v1.0 多 Agent 架构实现 (✅ 已完成)

| 模块            | 状态    | 说明                                                                          |
| --------------- | ------- | ----------------------------------------------------------------------------- |
| utils           | ✅ 完成 | crypto_utils, file_utils, json_utils, string_utils                            |
| core/common     | ✅ 完成 | export, logger, version                                                       |
| core/config     | ✅ 完成 | 配置管理、配置分级                                                            |
| core/event      | ✅ 完成 | 事件总线                                                                      |
| core/message    | ✅ 完成 | message, part, token_usage                                                    |
| core/provider   | ✅ 完成 | provider, provider_manager, 5 个 provider 实现                                |
| storage         | ✅ 完成 | database, sqlite_database, transaction, migration                             |
| network         | ✅ 完成 | http_client, url                                                              |
| core/permission | ✅ 完成 | 权限系统：PermissionRule、Ruleset、PermissionSystem                           |
| core/tool       | ✅ 完成 | 工具系统：Tool、ToolRegistry、ReadFileTool、WriteFileTool、BashTool、TaskTool |
| core/agent      | ✅ 完成 | Agent 系统：Agent、AgentRegistry、BuildAgent、PlanAgent、ExploreAgent         |
| core/session    | ✅ 完成 | 会话系统：Session、SessionStateMachine、SessionState、SessionLoop             |
| apps/cli        | ✅ 完成 | 命令行应用：交互式会话、命令处理                                              |
| apps/server     | ✅ 完成 | 服务器应用：HTTP API、会话管理                                                |

### v2.0 核心流程完善 (✅ 88% 完成)

| 模块              | 状态      | 说明                                                     | 计划编号        |
| ----------------- | --------- | -------------------------------------------------------- | --------------- |
| core/llm          | ✅ 完成   | 流式事件、LLM 流式调用、Provider 适配器、工具 Schema     | 2.3, 2.6, 2.7   |
| core/prompt       | ✅ 完成   | 系统提示词、Prompt 构建器、消息构建器（集成在 llm 目录） | 2.2, 2.4, 2.5   |
| core/session 扩展 | ✅ 完成   | SessionLoop（含死循环检测）、UsageTracker                | 2.8, 2.10, 2.11 |
| core/skill        | ✅ 完成   | Skill 管理器、Skill 工具                                 | 2.14, 2.15      |
| core/agent 扩展   | ✅ 完成   | Agent 字段补充、内置 Agent 完善、动态加载、生成器        | 2.1, 2.16, 2.17 |
| core/message 扩展 | ✅ 完成   | Part 系统扩展（Image/Error/Source）                      | 2.9             |
| core/session 重试 | 📋 待实现 | 错误重试机制                                             | 2.12            |
| core/session 压缩 | 📋 待实现 | 会话压缩                                                 | 2.13            |

### v3.0 协议系统开发 (📋 计划中)

| 模块     | 状态      | 说明                                 | 计划编号 |
| -------- | --------- | ------------------------------------ | -------- |
| core/mcp | 📋 计划中 | MCP 协议：客户端、传输层、OAuth 认证 | 3.1-3.4  |
| core/lsp | 📋 计划中 | LSP 协议：客户端、服务器管理         | 3.5-3.7  |
| core/acp | 📋 计划中 | ACP 协议：Agent、Session、IDE 集成   | 3.8-3.10 |

### 网络层扩展 (📋 计划中)

| 模块              | 状态      | 说明             |
| ----------------- | --------- | ---------------- |
| network/websocket | 📋 计划中 | WebSocket 客户端 |
| network/async     | 📋 计划中 | 异步 IO          |
| storage/pool      | 📋 计划中 | 连接池           |

**状态图例**：

- ✅ 完成：已实现并通过测试
- 🔄 进行中：正在实现
- 📋 计划中：已规划，待实现

## 12. 扩展指南

添加新模块时，请遵循以下步骤：

1. 在对应库的 `include/turbot/<library>/` 下创建头文件
2. 在 `src/` 下创建实现文件
3. 更新库的 `CMakeLists.txt`
4. 在 `tests/unit/` 下创建测试文件
5. 更新本文档的实现状态
