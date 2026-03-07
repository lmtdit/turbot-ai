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
│   │   └── config.hpp       # 配置管理
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
│   │       └── bash_tool.hpp
│   ├── agent/               # Agent系统 ✓ 已实现
│   │   ├── agent.hpp        # Agent抽象基类、AgentRegistry
│   │   └── builtin/         # 内置代理
│   │       ├── build_agent.hpp
│   │       ├── plan_agent.hpp
│   │       └── explore_agent.hpp
│   └── session/             # 会话系统 ✓ 已实现
│       ├── session.hpp      # Session、SessionInfo、SessionState
│       └── session_state_machine.hpp
└── src/
    ├── common/
    │   ├── logger.cpp
    │   └── version.cpp
    ├── config/
    │   └── config.cpp
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
    │       └── bash_tool.cpp
    ├── agent/
    │   ├── agent.cpp
    │   └── builtin/
    │       ├── build_agent.cpp
    │       ├── plan_agent.cpp
    │       └── explore_agent.cpp
    └── session/
        ├── session.cpp
        └── session_state_machine.cpp
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
├── turbot-cli/              # 命令行应用
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp
│       └── commands.cpp
└── turbot-server/           # 服务器应用
    ├── CMakeLists.txt
    └── src/
        └── main.cpp
```

## 5. 测试目录结构 (tests/)

```
tests/
├── CMakeLists.txt
├── unit/                    # 单元测试
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
│   └── tool_test.cpp        ✓ tool模块测试
│   ├── agent_test.cpp       ✓ agent模块测试
│   └── session_test.cpp     ✓ session模块测试
├── integration/             # 集成测试 (计划中)
│   └── ...
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
    ├── crypto_utils_test.cpp
    ├── file_utils_test.cpp
    ├── json_utils_test.cpp
    ├── string_utils_test.cpp
    ├── config_test.cpp
    ├── event_bus_test.cpp
    ├── logger_test.cpp
    └── version_test.cpp

Plan 1.2 - 存储层 ✅ 完成
├── libs/storage/         # 独立存储模块
│   ├── database.hpp      # 数据库抽象接口
│   ├── sqlite_database.hpp # SQLite实现
│   ├── transaction.hpp   # 事务接口
│   └── migration.hpp     # 迁移系统
└── tests/unit/
    └── storage_test.cpp

Plan 1.3 - 网络层 ✅ 完成
├── libs/network/
│   ├── http_client.hpp   # HTTP客户端
│   └── url.hpp           # URL解析
└── tests/unit/
    ├── http_client_test.cpp
    └── url_test.cpp

Plan 1.4 - 核心数据模型 ✅ 完成
├── libs/core/message/    # 消息系统（独立子模块）
│   ├── message.hpp
│   ├── part.hpp
│   └── token_usage.hpp
└── tests/unit/
    └── message_test.cpp

Plan 1.5 - Provider系统 ✅ 完成
├── libs/core/provider/   # AI提供商（依赖 network）
│   ├── provider.hpp      # 抽象接口
│   ├── provider_manager.hpp
│   └── impl/             # Provider实现
│       ├── openai_provider.hpp
│       ├── bailian_provider.hpp
│       ├── zhipu_provider.hpp
│       ├── kimi_provider.hpp
│       └── iflow_provider.hpp
└── tests/unit/
    └── provider_test.cpp

Plan 1.6 - 权限和工具系统 ✅ 完成
├── libs/core/permission/ # 权限系统（独立）
│   └── permission.hpp    # PermissionSystem, PermissionRule, Ruleset, PermissionReply
├── libs/core/tool/       # 工具系统（独立）
│   ├── tool.hpp          # Tool接口, ToolResult, ToolContext
│   ├── tool_registry.hpp # ToolRegistry 注册表
│   └── builtin/          # 内置工具实现
│       ├── read_file_tool.hpp
│       ├── write_file_tool.hpp
│       └── bash_tool.hpp
└── tests/unit/
    ├── permission_test.cpp
    └── tool_test.cpp

Plan 1.7 - Agent和会话系统 ✅ 完成
├── libs/core/agent/      # Agent系统
│   ├── agent.hpp
│   └── builtin/
│       ├── build_agent.hpp
│       ├── plan_agent.hpp
│       └── explore_agent.hpp
├── libs/core/session/    # 会话系统
│   ├── session.hpp
│   └── session_state_machine.hpp
└── tests/unit/
    ├── agent_test.cpp
    └── session_test.cpp

Plan 1.8 - 多Agent协作
├── libs/core/session/    # 扩展会话循环
│   └── sub_session.hpp
├── apps/turbot-cli/      # CLI应用
└── apps/turbot-server/   # Server应用
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

| 模块              | 状态      | 说明                                                                |
| ----------------- | --------- | ------------------------------------------------------------------- |
| utils             | ✅ 完成   | crypto_utils, file_utils, json_utils, string_utils                  |
| core/common       | ✅ 完成   | export, logger, version                                             |
| core/config       | ✅ 完成   | 配置管理                                                            |
| core/event        | ✅ 完成   | 事件总线                                                            |
| core/message      | ✅ 完成   | message, part, token_usage                                          |
| core/provider     | ✅ 完成   | provider, provider_manager, 5 个 provider 实现                      |
| storage           | ✅ 完成   | database, sqlite_database, transaction, migration                   |
| network           | ✅ 完成   | http_client, url                                                    |
| core/permission   | ✅ 完成   | 权限系统：PermissionRule、Ruleset、PermissionSystem                 |
| core/tool         | ✅ 完成   | 工具系统：Tool、ToolRegistry、ReadFileTool、WriteFileTool、BashTool |
| core/agent        | ✅ 完成   | Agent系统：Agent、AgentRegistry、BuildAgent、PlanAgent、ExploreAgent |
| core/session      | ✅ 完成   | 会话系统：Session、SessionStateMachine、SessionState |
| network/websocket | 📋 计划中 | WebSocket 客户端                                                    |
| network/async     | 📋 计划中 | 异步 IO                                                             |
| storage/pool      | 📋 计划中 | 连接池                                                              |

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
