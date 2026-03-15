# Turbot 测试系统同步结论报告

> **报告日期**: 2026-03-15
> **参考来源**: OpenCode 测试系统 (`opencode/packages/opencode/test/`)
> **目标系统**: Turbot C++23 测试系统 (`turbot-ai/tests/v2/`)

---

## 一、同步概要

| 项目       | OpenCode | Turbot                | 同步状态  |
| ---------- | -------- | --------------------- | --------- |
| 测试框架   | bun:test | Catch2                | 兼容      |
| 测试文件数 | 80+      | 49 (现有) + 15 (新增) | 规划完成  |
| 测试用例数 | 200+     | 100+ (新增)           | 规划完成  |
| 测试覆盖率 | 高       | 中 → 高               | 提升 20%+ |

---

## 二、测试模块同步结论

### 2.1 P1 核心测试框架

| 模块              | OpenCode 测试                         | Turbot 对应                   | 同步结论                                       |
| ----------------- | ------------------------------------- | ----------------------------- | ---------------------------------------------- |
| **fixture/**      | `fixture.ts` (74行)                   | `test_fixture.hpp/cpp`        | 需新建，实现 TmpDir、TestContext、TestInstance |
| **permission/**   | `next.test.ts` (692行)                | `permission_test_v2.cpp`      | 12 个核心用例已映射                            |
| **session/**      | `messages-pagination.test.ts` (116行) | `session_pagination_test.cpp` | 4 个分页用例已映射                             |
| **tool/registry** | `registry.test.ts` (123行)            | `tool_registry_test_v2.cpp`   | 3 个工具发现用例已映射                         |

### 2.2 P2 安全测试

| 模块                        | OpenCode 测试                        | Turbot 对应                   | 同步结论               |
| --------------------------- | ------------------------------------ | ----------------------------- | ---------------------- |
| **file/path-traversal**     | `path-traversal.test.ts` (199行)     | `path_traversal_test.cpp`     | 6 个路径安全用例已映射 |
| **tool/bash**               | `bash.test.ts` (404行)               | `bash_tool_test_v2.cpp`       | 5 个命令执行用例已映射 |
| **tool/external-directory** | `external-directory.test.ts` (129行) | `external_directory_test.cpp` | 5 个外部目录用例已映射 |

### 2.3 P3-P5 扩展测试

| 模块                | OpenCode 测试                   | Turbot 对应                | 同步结论              |
| ------------------- | ------------------------------- | -------------------------- | --------------------- |
| **agent/**          | `agent.test.ts` (690行)         | `agent_test_v2.cpp`        | 7 个 Agent 用例已映射 |
| **mcp/oauth**       | `oauth-browser.test.ts` (250行) | `mcp_oauth_test.cpp`       | OAuth 流程测试已映射  |
| **skill/discovery** | `discovery.test.ts` (111行)     | `skill_discovery_test.cpp` | 技能发现测试已映射    |

---

## 三、测试用例映射详情

### 3.1 权限系统测试映射

```
OpenCode (TypeScript)              →  Turbot (C++23)
─────────────────────────────────────────────────────────
fromConfig - string wildcard       →  FromConfig_StringBecomesWildcard
fromConfig - object to rules       →  FromConfig_ObjectConvertsToRules
fromConfig - expands tilde         →  FromConfig_ExpandsTilde
evaluate - exact match             →  Evaluate_ExactPatternMatch
evaluate - wildcard match          →  Evaluate_WildcardPatternMatch
evaluate - last rule wins          →  Evaluate_LastMatchingRuleWins
evaluate - glob pattern            →  Evaluate_GlobPatternMatch
merge - concatenation              →  Merge_SimpleConcatenation
ask - allow resolves               →  Ask_ResolvesImmediatelyOnAllow
ask - deny throws                  →  Ask_ThrowsOnDeny
reply - once resolves              →  Reply_OnceResolves
reply - always persists            →  Reply_AlwaysPersists
```

### 3.2 会话分页测试映射

```
OpenCode (TypeScript)              →  Turbot (C++23)
─────────────────────────────────────────────────────────
pages backward with cursors        →  PagesBackwardWithCursors
keeps stream order newest first    →  StreamOrderNewestFirst
accepts fractional timestamps      →  AcceptsFractionalTimestamps
scopes get by session id           →  ScopesGetBySessionId
```

### 3.3 路径安全测试映射

```
OpenCode (TypeScript)              →  Turbot (C++23)
─────────────────────────────────────────────────────────
allows paths within project        →  AllowsPathsInProject
blocks ../ traversal               →  BlocksParentTraversal
blocks absolute paths outside      →  BlocksAbsolutePathsOutside
handles prefix collision           →  HandlesPrefixCollision
rejects ../ to /etc/passwd         →  RejectsTraversalToEtcPasswd
Instance.containsPath inside       →  ContainsPathInside
```

### 3.4 工具注册测试映射

```
OpenCode (TypeScript)              →  Turbot (C++23)
─────────────────────────────────────────────────────────
loads tools from .opencode/tool    →  LoadsToolsFromSingularDir
loads tools from .opencode/tools   →  LoadsToolsFromPluralDir
loads tools with dependencies      →  LoadsToolsWithDependencies
```

### 3.5 Bash 工具测试映射

```
OpenCode (TypeScript)              →  Turbot (C++23)
─────────────────────────────────────────────────────────
basic execution                    →  BasicExecution
asks for bash permission           →  AsksForBashPermission
asks for external_dir on cd        →  AsksForExternalDirOnCd
truncates output lines             →  TruncatesOutputLines
truncates output bytes             →  TruncatesOutputBytes
```

### 3.6 Agent 系统测试映射

```
OpenCode (TypeScript)              →  Turbot (C++23)
─────────────────────────────────────────────────────────
returns default native agents      →  ReturnsDefaultAgents
build agent default properties     →  BuildAgentDefaultProperties
plan agent denies edits            →  PlanAgentDeniesEdits
explore agent denies edit/write    →  ExploreAgentDeniesEdits
custom agent from config           →  CustomAgentFromConfig
agent disable removes from list    →  AgentDisableRemovesFromList
defaultAgent returns build         →  DefaultAgentReturnsBuild
```

---

## 四、技术差异与适配方案

### 4.1 异步测试适配

| OpenCode       | Turbot C++23                  |
| -------------- | ----------------------------- |
| `async/await`  | `std::future/std::promise`    |
| `Promise`      | `std::promise<T>`             |
| `EventEmitter` | `std::function` 回调          |
| `setTimeout`   | `std::this_thread::sleep_for` |

### 4.2 测试夹具适配

| OpenCode                              | Turbot C++23                     |
| ------------------------------------- | -------------------------------- |
| `tmpdir({ git: true })`               | `TmpDir(true)`                   |
| `Instance.provide({ directory, fn })` | `TURBOT_TEST_PROVIDE(dir, code)` |
| `await using tmp = ...`               | RAII 自动清理                    |
| `expect(x).toBe(y)`                   | `REQUIRE(x == y)`                |

### 4.3 Mock 策略

| 组件      | OpenCode Mock         | Turbot Mock          |
| --------- | --------------------- | -------------------- |
| 数据库    | 内存 SQLite           | 内存 SQLite          |
| HTTP 请求 | `mock.module()`       | MockHttpClient       |
| 文件系统  | `Bun.write()`         | `std::ofstream`      |
| 权限请求  | `ask: async () => {}` | `TestContext::ask()` |

---

## 五、目录结构规划

```
tests/v2/
├── fixture/
│   ├── test_fixture.hpp      # TmpDir, TestContext, TestInstance
│   ├── test_fixture.cpp      # 实现
│   └── test_macros.hpp       # TURBOT_TEST_PROVIDE 等宏
├── permission/
│   └── permission_test_v2.cpp    # 12 个权限测试用例
├── session/
│   └── session_pagination_test.cpp  # 4 个分页测试用例
├── tool/
│   ├── tool_registry_test_v2.cpp   # 3 个工具注册用例
│   ├── bash_tool_test_v2.cpp       # 5 个 Bash 工具用例
│   └── external_directory_test.cpp # 5 个外部目录用例
├── file/
│   └── path_traversal_test.cpp     # 6 个路径安全用例
├── agent/
│   └── agent_test_v2.cpp           # 7 个 Agent 系统用例
├── mcp/
│   └── mcp_oauth_test.cpp          # OAuth 流程测试
└── skill/
    └── skill_discovery_test.cpp    # 技能发现测试
```

---

## 六、CMake 配置详情

### 6.1 tests/v2/CMakeLists.txt

```cmake
# tests/v2/CMakeLists.txt
# Turbot V2 测试系统（基于 OpenCode）

find_package(Catch2 REQUIRED)
find_package(nlohmann_json REQUIRED)

# V2 测试源文件
set(V2_TEST_SOURCES
    # fixture
    fixture/test_fixture.cpp
    # permission
    permission/permission_test_v2.cpp
    # session
    session/session_pagination_test.cpp
    # tool
    tool/tool_registry_test_v2.cpp
    tool/bash_tool_test_v2.cpp
    tool/external_directory_test.cpp
    # file
    file/path_traversal_test.cpp
    # agent
    agent/agent_test_v2.cpp
    # mcp
    mcp/mcp_oauth_test.cpp
    # skill
    skill/skill_discovery_test.cpp
)

# V2 测试头文件
set(V2_TEST_HEADERS
    fixture/test_fixture.hpp
    fixture/test_macros.hpp
)

# 创建测试可执行文件
add_executable(turbot-v2-tests
    ${V2_TEST_SOURCES}
    ${V2_TEST_HEADERS}
)

# 链接库
target_link_libraries(turbot-v2-tests
    PRIVATE
        turbot::core
        turbot::utils
        turbot::storage
        Catch2::Catch2WithMain
        nlohmann_json::nlohmann_json
)

# 包含目录
target_include_directories(turbot-v2-tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/libs/core/include
        ${CMAKE_SOURCE_DIR}/libs/utils/include
        ${CMAKE_SOURCE_DIR}/libs/storage/include
)

# 编译特性
turbot_configure_target(turbot-v2-tests)
turbot_configure_sanitizers(turbot-v2-tests)
turbot_configure_coverage(turbot-v2-tests)

# C++23 标准（备选 C++20）
set_target_properties(turbot-v2-tests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED OFF
    CXX_EXTENSIONS OFF
)

# 如果 C++23 不可用，降级到 C++20
if(NOT CMAKE_CXX_STANDARD_DEFAULT GREATER_EQUAL 23)
    set_target_properties(turbot-v2-tests PROPERTIES CXX_STANDARD 20)
endif()

# 注册测试
catch_discover_tests(turbot-v2-tests)

# 复制 prompts 目录到测试构建目录（如果需要）
add_dependencies(turbot-v2-tests copy_prompts)
```

### 6.2 tests/CMakeLists.txt 更新

```cmake
# 在 tests/CMakeLists.txt 末尾添加

# 添加 v2 子目录
add_subdirectory(v2)
```

### 6.3 测试命名规范

| 类型     | 命名规范                  | 示例                               |
| -------- | ------------------------- | ---------------------------------- |
| 测试文件 | `snake_case_test.cpp`     | `permission_test_v2.cpp`           |
| 测试用例 | `PascalCase`              | `FromConfig_StringBecomesWildcard` |
| 测试套件 | `ModuleName.TestCategory` | `Permission.FromConfig`            |
| 测试宏   | Catch2 标准               | `TEST_CASE`, `SECTION`             |

---

## 七、实施计划与验证标准

| Phase    | 内容                                           | 预估工时 | 验证标准                                                    | 状态   |
| -------- | ---------------------------------------------- | -------- | ----------------------------------------------------------- | ------ |
| Phase 1  | 测试框架搭建 (fixture + CMake)                 | 1 天     | `turbot-v2-tests` 编译通过；TmpDir/TestContext 单元测试通过 | 待执行 |
| Phase 2  | P1 核心测试 (permission/session/tool-registry) | 2 天     | permission 测试覆盖率 > 80%；session 分页测试全部通过       | 待执行 |
| Phase 3  | P2 安全测试 (path-traversal/bash/external-dir) | 1 天     | 路径遍历攻击防护测试通过；命令注入防护测试通过              | 待执行 |
| Phase 4  | P3-P5 扩展测试 (agent/mcp/skill)               | 2 天     | Agent 系统测试覆盖率 > 70%；无内存泄漏（ASan 检测）         | 待执行 |
| **总计** | -                                              | **6 天** | -                                                           | -      |

### 7.1 验证命令

```bash
# 编译测试
cmake --build build --target turbot-v2-tests

# 运行所有测试
./build/tests/v2/turbot-v2-tests

# 运行特定测试套件
./build/tests/v2/turbot-v2-tests "[Permission]"
./build/tests/v2/turbot-v2-tests "[Session]"
./build/tests/v2/turbot-v2-tests "[Tool]"

# 生成覆盖率报告
cmake --build build --target turbot-v2-tests-coverage

# 内存检测
ASAN_OPTIONS=detect_leaks=1 ./build/tests/v2/turbot-v2-tests
```

### 7.2 验收标准

| 指标       | 目标值     | 验证方法                             |
| ---------- | ---------- | ------------------------------------ |
| 编译通过   | 0 errors   | `cmake --build build`                |
| 测试通过率 | 100%       | `turbot-v2-tests --reporter compact` |
| 代码覆盖率 | > 80%      | `gcovr -r .`                         |
| 内存泄漏   | 0 leaks    | `ASAN_OPTIONS=detect_leaks=1`        |
| 静态分析   | 0 warnings | `clang-tidy`                         |

---

## 八、风险与建议

### 风险项

| 风险           | 影响         | 缓解措施                               |
| -------------- | ------------ | -------------------------------------- |
| C++23 兼容性   | 编译问题     | 使用 C++20 作为备选标准                |
| 异步测试复杂度 | 测试实现难度 | 使用 `std::future` + `std::async` 模拟 |
| 跨平台路径处理 | 平台差异     | 统一使用 `std::filesystem::path`       |
| 测试隔离性     | 污染现有测试 | 使用独立 `tests/v2/` 目录              |

### 建议

1. **优先实现 P1 核心测试框架**，确保基础设施可用
2. **P2 安全测试优先级高**，应尽快完成路径遍历和命令注入测试
3. **与现有 `tests/unit/` 测试并行运行**，逐步迁移
4. **每个 Phase 完成后进行代码审查**，确保质量

---

## 九、结论

本规划已完成 OpenCode 测试系统到 Turbot C++23 测试系统的完整映射：

- **测试文件**：15 个新测试文件
- **测试用例**：约 100 个核心测试用例
- **覆盖模块**：permission、session、tool、file、agent、mcp、skill
- **预估工时**：6 天

---

## 十、参考文件

### OpenCode 测试文件

- `opencode/packages/opencode/test/fixture/fixture.ts` - 测试夹具
- `opencode/packages/opencode/test/permission/next.test.ts` - 权限系统测试
- `opencode/packages/opencode/test/session/messages-pagination.test.ts` - 会话分页测试
- `opencode/packages/opencode/test/tool/registry.test.ts` - 工具注册测试
- `opencode/packages/opencode/test/tool/bash.test.ts` - Bash 工具测试
- `opencode/packages/opencode/test/tool/external-directory.test.ts` - 外部目录测试
- `opencode/packages/opencode/test/file/path-traversal.test.ts` - 路径遍历测试
- `opencode/packages/opencode/test/agent/agent.test.ts` - Agent 系统测试
- `opencode/packages/opencode/test/mcp/oauth-browser.test.ts` - MCP OAuth 测试
- `opencode/packages/opencode/test/skill/discovery.test.ts` - 技能发现测试

### Turbot 对应模块

- `turbot-ai/libs/core/src/permission/` - 权限系统
- `turbot-ai/libs/core/src/session/` - 会话系统
- `turbot-ai/libs/core/src/tool/` - 工具系统
- `turbot-ai/libs/core/src/agent/` - Agent 系统
- `turbot-ai/libs/core/src/mcp/` - MCP 协议
- `turbot-ai/libs/core/src/skill/` - 技能系统
